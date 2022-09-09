#pragma once

#include <random>

struct Random {
	float Float(float min = 0, float max = 1) { return min + (max - min) * m_distribution(m_generator); }

private:
	std::uniform_real_distribution<float> m_distribution = decltype(m_distribution)(0, 1);
	std::mt19937 m_generator = decltype(m_generator)(std::random_device()());
};
