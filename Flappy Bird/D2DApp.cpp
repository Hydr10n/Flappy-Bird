module;
#include "pch.h"

#include "StepTimer.h"

#include "box2d/box2d.h"

#include "Random.h"

#include <queue>

module D2DApp;

import SharedData;

using namespace D2D1;
using namespace DX;
using namespace Microsoft::WRL;
using namespace std;
using namespace WindowHelpers;

struct D2DApp::Impl : b2ContactListener {
	Impl(const shared_ptr<WindowModeHelper>& windowModeHelper) noexcept(false) : m_windowModeHelper(windowModeHelper) {
		CreateDeviceDependentResources();

		CreateWindowSizeDependentResources();

		InitializeWorld();
	}

	SIZE GetOutputSize() const noexcept {
		const auto size = m_d2dDeviceContext->GetPixelSize();
		return { static_cast<LONG>(size.width), static_cast<LONG>(size.height) };
	}

	void Tick() {
		m_stepTimer.Tick([&] { Update(); });

		Render();
	}

	void OnWindowSizeChanged() {
		const auto outputSize = GetOutputSize();
		if (const auto resolution = m_windowModeHelper->GetResolution(); resolution.cx != outputSize.cx || resolution.cy != outputSize.cy) {
			ComPtr<ID2D1HwndRenderTarget> hwndRenderTarget;
			ThrowIfFailed(m_d2dDeviceContext.As(&hwndRenderTarget));
			ThrowIfFailed(hwndRenderTarget->Resize({ static_cast<UINT32>(resolution.cx), static_cast<UINT32>(resolution.cy) }));

			CreateWindowSizeDependentResources();
		}
	}

	void OnResuming() { m_stepTimer.ResetElapsedTime(); }

	void OnSuspending() {}

	void OnActivated() {}

	void OnDeactivated() {}

	void ProcessKeyboardMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_KEYDOWN: {
			if (m_state == State::Over) Reset();
			else if (wParam == VK_SPACE && !(HIWORD(lParam) & KF_REPEAT)) FlyUp();
		} break;
		}
	}

	void ProcessMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN: {
			if (m_state == State::Over) Reset();
			else if (wParam & MK_LBUTTON) FlyUp();
		} break;
		}
	}

private:
	const shared_ptr<WindowModeHelper> m_windowModeHelper;

	ComPtr<ID2D1Factory> m_d2dFactory;
	ComPtr<IDWriteFactory> m_dWriteFactory;
	ComPtr<ID2D1DeviceContext> m_d2dDeviceContext;

	StepTimer m_stepTimer;

	D2D1_MATRIX_3X2_F m_transform{};

	struct Images {
		ComPtr<ID2D1ImageBrush> Background, Pawn, Barrier;

		Images() = default;

		Images(ID2D1DeviceContext* pDeviceContext) {
			const auto CreateImageBrush = [&](D2D1_SIZE_F size, ID2D1ImageBrush** ppBrush) {
				ComPtr<ID2D1BitmapRenderTarget> bitmapRenderTarget;
				ThrowIfFailed(pDeviceContext->CreateCompatibleRenderTarget(size, &bitmapRenderTarget));

				ComPtr<ID2D1Bitmap> bitmap;
				ThrowIfFailed(bitmapRenderTarget->GetBitmap(&bitmap));
				ThrowIfFailed(pDeviceContext->CreateImageBrush(bitmap.Get(), ImageBrushProperties({ 0, 0, size.width, size.height }), BrushProperties(1, Matrix3x2F::Scale(1 / size.width, 1 / size.height)), ppBrush));

				return bitmapRenderTarget;
			};

			const auto Render = [&](initializer_list<D2D1_GRADIENT_STOP> gradientStops, const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES& gradientBrushProperties, D2D1_SIZE_F size, ID2D1ImageBrush** ppBrush) {
				const auto renderTarget = CreateImageBrush(size, ppBrush);

				renderTarget->BeginDraw();

				ComPtr<ID2D1GradientStopCollection> gradientStopCollection;
				ThrowIfFailed(pDeviceContext->CreateGradientStopCollection(gradientStops.begin(), static_cast<UINT32>(gradientStops.size()), &gradientStopCollection));

				ComPtr<ID2D1LinearGradientBrush> linearGradientBrush;
				ThrowIfFailed(pDeviceContext->CreateLinearGradientBrush(gradientBrushProperties, gradientStopCollection.Get(), &linearGradientBrush));

				renderTarget->FillRectangle({ 0, 0, size.width, size.height }, linearGradientBrush.Get());

				ThrowIfFailed(renderTarget->EndDraw());
			};

			const auto deviceContextSize = pDeviceContext->GetSize();

			Render(
				{
					GradientStop(0, ColorF(ColorF::WhiteSmoke)),
					GradientStop(1, ColorF(ColorF::LightSkyBlue))
				},
				LinearGradientBrushProperties({}, { 0, deviceContextSize.height }),
				deviceContextSize,
				&Background
			);

			const D2D1_SIZE_F imageSize{ deviceContextSize.width * 0.1f, deviceContextSize.height * 0.1f };

			Render(
				{
					GradientStop(0, ColorF(ColorF::WhiteSmoke)),
					GradientStop(1, ColorF(ColorF::DarkCyan))
				},
				LinearGradientBrushProperties({}, { imageSize.width, 0 }),
				imageSize,
				&Pawn
			);

			Render(
				{
					GradientStop(0, ColorF(ColorF::WhiteSmoke)),
					GradientStop(1, ColorF(ColorF::LightSkyBlue))
				},
				LinearGradientBrushProperties({}, { 0, imageSize.height }),
				imageSize,
				&Barrier
			);
		}
	};
	Images m_images;

	enum class ObjectType { Unknown, Pawn, BarrierTop, BarrierBottom };

	b2Vec2 m_worldSize{ 0, 12 };
	b2World m_world = decltype(m_world)({ 0, 0 });

	b2Body* m_ground{};

	static constexpr float PawnRadius = 0.5f;
	b2Body* m_pawn{};

	static constexpr float BarrierWidth = PawnRadius * 2 * 1.7f, BarrierDistance = 5;
	queue<b2Body*> m_barriers;

	enum class State { NotStarted, Running, Over } m_state{};

	uint32_t m_score{};

	Random m_random;

	void CreateDeviceDependentResources() {
		D2D1_FACTORY_OPTIONS factoryOptions{};
#ifdef _DEBUG
		factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
		ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, factoryOptions, m_d2dFactory.ReleaseAndGetAddressOf()));

		ThrowIfFailed(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(*m_dWriteFactory.Get()), &m_dWriteFactory));

		ComPtr<ID2D1HwndRenderTarget> hwndRenderTarget;
		const auto resolution = m_windowModeHelper->GetResolution();
		ThrowIfFailed(m_d2dFactory->CreateHwndRenderTarget({}, HwndRenderTargetProperties(m_windowModeHelper->hWnd, { static_cast<UINT32>(resolution.cx), static_cast<UINT32>(resolution.cy) }), &hwndRenderTarget));
		ThrowIfFailed(hwndRenderTarget.As(&m_d2dDeviceContext));
	}

	void CreateWindowSizeDependentResources() {
		m_images = m_d2dDeviceContext.Get();

		const auto deviceContextSize = m_d2dDeviceContext->GetSize();

		const auto worldWidth = m_worldSize.y * deviceContextSize.width / deviceContextSize.height;

		m_world.ShiftOrigin({ (m_worldSize.x - worldWidth) / 2, 0 });

		m_worldSize.x = worldWidth;

		const auto scale = deviceContextSize.height / m_worldSize.y;
		m_transform = Matrix3x2F::Scale(scale, -scale) * Matrix3x2F::Translation((deviceContextSize.width - m_worldSize.x * scale) / 2, deviceContextSize.height);
	}

	void Update() {
		const auto elapsedSeconds = static_cast<float>(m_stepTimer.GetElapsedSeconds());

		if (m_state == State::NotStarted) {
			constexpr auto CalculateSpringOscillatorVelocity = [](float a, float ω, float t, float φ) { return -a * ω * sin(ω * t - φ); };

			auto v = m_pawn->GetLinearVelocity();
			v.y = CalculateSpringOscillatorVelocity(0.1f, 2 * b2_pi / 0.8f, static_cast<float>(m_stepTimer.GetTotalSeconds()), 0);
			m_pawn->SetLinearVelocity(v);
		}

		const auto pawnPositionX = m_pawn->GetPosition().x;

		m_world.Step(elapsedSeconds, 8, 3);

		const auto pawnDisplacementX = m_pawn->GetPosition().x - pawnPositionX;

		auto groundPosition = m_ground->GetPosition();
		groundPosition.x += m_pawn->GetPosition().x - pawnPositionX;
		m_ground->SetTransform(groundPosition, 0);

		m_world.ShiftOrigin({ pawnDisplacementX, 0 });

		if (m_state == State::Running) {
			if (const auto barrier = m_barriers.front(); barrier->GetPosition().x - BarrierWidth / 2 + BarrierDistance < 0) {
				AddBarrier();
				m_world.DestroyBody(barrier);
				m_barriers.pop();
			}
		}
	}

	void Render() {
		if (!m_stepTimer.GetFrameCount()) return;

		m_d2dDeviceContext->BeginDraw();

		RenderBackground();

		RenderWorld();

		RenderUI();

		ThrowIfFailed(m_d2dDeviceContext->EndDraw());
	}

	void FlyUp() {
		switch (m_state) {
		case State::NotStarted: {
			m_world.SetGravity({ 0, -10 });

			for (auto count = static_cast<uint32_t>(ceil((m_worldSize.x + BarrierDistance) / (BarrierDistance + BarrierWidth))); count; count--) AddBarrier();

			m_state = State::Running;
		} [[fallthrough]];

		case State::Running: {
			const auto x = PawnRadius * 2 * 1.7f, g = -m_world.GetGravity().y, t = sqrt(2 * x / g);
			m_pawn->SetLinearVelocity({ m_pawn->GetLinearVelocity().x, g * t });
		} break;
		}
	}

	void InitializeWorld() {
		m_world.SetContactListener(this);

		CreateGround();

		Spawn();
	}

	void Reset() {
		m_state = {};

		m_score = {};

		m_barriers = {};

		m_world.~b2World();
		new (&m_world) decltype(m_world)({ 0, 0 });

		InitializeWorld();
	}

	void CreateGround() {
		const b2Vec2 halfSize{ 50, 0.1f };

		b2BodyDef bodyDef;
		bodyDef.position.Set(m_worldSize.x / 2, -halfSize.y);
		const auto body = m_world.CreateBody(&bodyDef);

		b2PolygonShape shape;
		shape.SetAsBox(halfSize.x, halfSize.y);
		b2FixtureDef fixtureDef;
		fixtureDef.shape = &shape;
		fixtureDef.friction = 0.6f;
		body->CreateFixture(&fixtureDef);

		m_ground = body;
	}

	void Spawn() {
		b2BodyDef bodyDef;
		bodyDef.type = b2_dynamicBody;
		bodyDef.position = { m_worldSize.x / 2 - PawnRadius, m_worldSize.y / 2 };
		bodyDef.linearVelocity.x = 2;
		const auto body = m_world.CreateBody(&bodyDef);

		b2CircleShape shape;
		shape.m_radius = PawnRadius;
		b2FixtureDef fixtureDef;
		fixtureDef.shape = &shape;
		fixtureDef.density = 1;
		fixtureDef.friction = 0.5f;
		fixtureDef.userData.pointer = static_cast<uintptr_t>(ObjectType::Pawn);
		body->CreateFixture(&fixtureDef);

		m_pawn = body;
	}

	void AddBarrier() {
		const auto
			worldHalfHeight = m_worldSize.y / 2,
			gapHalfHeight = PawnRadius * 2.8f,
			bottomHalfHeight = worldHalfHeight * m_random.Float(0.3f, 0.5f), topHalfHeight = worldHalfHeight - gapHalfHeight - bottomHalfHeight;

		b2BodyDef bodyDef;
		bodyDef.position.x = m_barriers.empty() ? m_worldSize.x + BarrierWidth / 2 + 1 : m_barriers.back()->GetPosition().x + BarrierDistance;
		const auto body = m_world.CreateBody(&bodyDef);

		const auto CreateFixture = [&](float halfHeight, float positionY, ObjectType objectType) {
			b2PolygonShape shape;
			shape.SetAsBox(BarrierWidth / 2, halfHeight, { 0, positionY }, 0);
			b2FixtureDef fixtureDef;
			fixtureDef.shape = &shape;
			fixtureDef.friction = 0.3f;
			fixtureDef.isSensor = objectType == ObjectType::Unknown;
			fixtureDef.userData.pointer = static_cast<uintptr_t>(objectType);
			body->CreateFixture(&fixtureDef);
		};
		CreateFixture(bottomHalfHeight, bottomHalfHeight, ObjectType::BarrierBottom);
		CreateFixture(gapHalfHeight, bottomHalfHeight * 2 + gapHalfHeight, ObjectType::Unknown);
		CreateFixture(topHalfHeight, m_worldSize.y - topHalfHeight, ObjectType::BarrierTop);

		m_barriers.emplace(body);
	}

	void BeginContact(b2Contact* contact) override {
		if (!contact->GetFixtureA()->IsSensor() && !contact->GetFixtureB()->IsSensor()) m_state = State::Over;
	}

	void EndContact(b2Contact* contact) override {
		if ((contact->GetFixtureA()->IsSensor() || contact->GetFixtureB()->IsSensor()) && m_state == State::Running) m_score++;
	}

	void RenderBackground() const {
		ComPtr<ID2D1Image> image;
		m_images.Background->GetImage(&image);
		m_d2dDeviceContext->DrawImage(image.Get());
	}

	void RenderWorld() const {
		D2D1_MATRIX_3X2_F transform;
		m_d2dDeviceContext->GetTransform(&transform);

		for (auto body = m_world.GetBodyList(); body != nullptr; body = body->GetNext()) {
			const auto bodyTransform = body->GetTransform();

			for (auto fixture = body->GetFixtureList(); fixture != nullptr; fixture = fixture->GetNext()) {
				if (fixture->IsSensor()) continue;

				ID2D1Brush* pBrush;
				auto angleDelta = 0.0f;
				switch (static_cast<ObjectType>(const_cast<b2Fixture*>(fixture)->GetUserData().pointer)) {
				case ObjectType::Pawn: pBrush = m_images.Pawn.Get(); break;

				case ObjectType::BarrierTop: angleDelta = b2_pi; [[fallthrough]];
				case ObjectType::BarrierBottom: pBrush = m_images.Barrier.Get(); break;

				default: continue;
				}

				const auto shape = fixture->GetShape();
				const auto shapeType = shape->GetType();

				D2D1_RECT_F rect{ bodyTransform.p.x, bodyTransform.p.y, bodyTransform.p.x, bodyTransform.p.y };
				switch (shapeType) {
				case b2Shape::e_circle: {
					const auto radius = dynamic_cast<const b2CircleShape*>(shape)->m_radius;
					rect.left += -radius;
					rect.top += radius;
					rect.right += radius;
					rect.bottom += -radius;
				} break;

				case b2Shape::e_polygon: {
					const auto& vertices = dynamic_cast<const b2PolygonShape*>(shape)->m_vertices;
					rect.left += vertices[3].x;
					rect.top += vertices[3].y;
					rect.right += vertices[1].x;
					rect.bottom += vertices[1].y;
				} break;

				default: continue;
				}

				const D2D1_SIZE_F scale{ rect.right - rect.left, rect.bottom - rect.top };
				m_d2dDeviceContext->SetTransform(Matrix3x2F::Scale(scale) * Matrix3x2F::Rotation((bodyTransform.q.GetAngle() + angleDelta) * 180 / b2_pi, { scale.width / 2, scale.height / 2 }) * Matrix3x2F::Translation(rect.left, rect.top) * m_transform);

				if (shapeType == b2Shape::e_polygon) m_d2dDeviceContext->FillRectangle({ 0, 0, 1, 1 }, pBrush);
				else m_d2dDeviceContext->FillEllipse({ { 0.5f, 0.5f }, 0.5f, 0.5f }, pBrush);
			}
		}

		m_d2dDeviceContext->SetTransform(transform);
	}

	void RenderUI() const {
		const auto deviceContextSize = m_d2dDeviceContext->GetSize();

		const auto RenderText = [&](LPCWSTR text, float fontSize, float positionY, UINT32 rgb) {
			fontSize *= deviceContextSize.height;
			positionY *= deviceContextSize.height;

			ComPtr<IDWriteTextFormat> dWriteTextFormat;
			ThrowIfFailed(m_dWriteFactory->CreateTextFormat(L"Comic Sans MS", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"", &dWriteTextFormat));
			ThrowIfFailed(dWriteTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
			ThrowIfFailed(dWriteTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));

			ComPtr<ID2D1SolidColorBrush> solidColorBrush;
			ThrowIfFailed(m_d2dDeviceContext->CreateSolidColorBrush(ColorF(rgb), &solidColorBrush));

			m_d2dDeviceContext->DrawTextW(text, lstrlenW(text), dWriteTextFormat.Get(), RectF(0, positionY, deviceContextSize.width, positionY + fontSize), solidColorBrush.Get());
		};

		RenderText(to_wstring(m_score).c_str(), 0.08f, 0.1f, 0x0063b1);

		if (m_state == State::Over) {
			RenderText(L"Game Over", 0.1f, 0.4f, 0xea005e);

			RenderText(L"Press any key to restart.", 0.04f, 0.6f, ColorF::Teal);
		}
	}
};

D2DApp::D2DApp(const shared_ptr<WindowModeHelper>& windowModeHelper) : m_impl(make_unique<Impl>(windowModeHelper)) {}

D2DApp::~D2DApp() = default;

SIZE D2DApp::GetOutputSize() const noexcept { return m_impl->GetOutputSize(); }

void D2DApp::Tick() { m_impl->Tick(); }

void D2DApp::OnWindowSizeChanged() { m_impl->OnWindowSizeChanged(); }

void D2DApp::OnResuming() { m_impl->OnResuming(); }

void D2DApp::OnSuspending() { m_impl->OnSuspending(); }

void D2DApp::OnActivated() { return m_impl->OnActivated(); }

void D2DApp::OnDeactivated() { return m_impl->OnDeactivated(); }

void D2DApp::ProcessKeyboardMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) { m_impl->ProcessKeyboardMessage(uMsg, wParam, lParam); }

void D2DApp::ProcessMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) { m_impl->ProcessMouseMessage(uMsg, wParam, lParam); }
