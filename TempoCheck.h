#pragma once

template<std::floating_point T>
class TempoCheck
{
	std::vector<T> han_windows; 
	T get_han_window(uint32_t value) const {
		return han_windows[value < (N >> 1) ? value : N - value];
	}

	void to_volume_diff(T* volume) {
		for (int n = N - 1; n > 0; --n) {
			auto temp = volume[n] - volume[n - 1];
			if (temp > 0) {
				volume[n] = temp;
			}
			else {
				volume[n] = 0;
			}
		}
	}

public:
	const uint32_t N;
	const uint32_t frame_size;
	const uint32_t sample_rate;
	const T frame_sample_rate;
	TempoCheck(uint32_t size, uint32_t frame_size, uint32_t sample_rate) : N(size), frame_size(frame_size), sample_rate(sample_rate), frame_sample_rate(T(sample_rate) / T(frame_size)), han_windows(size / 2 + 1) {
		for (int i = 0; i < han_windows.size(); i++) {
			han_windows[i] = T(0.5) - T(0.5) * std::cos(T(2.0) * std::numbers::pi_v<T> *i / N);
		}
	}

	template <std::size_t S>
	std::array<uint32_t, S> get_BPM(T* volume, uint32_t lower, uint32_t upper) {
		to_volume_diff(volume);

		std::map<T, int> max;
		T b_result = 0;
		T b_slope = 0;
		for (uint32_t i = lower; i < upper; ++i) {
			T sum1 = 0, sum2 = 0;
			T b = T(i) / lower;
			for (uint32_t n = 0; n < N; ++n) {
				sum1 += volume[n] * std::cos(T(2.0) * std::numbers::pi_v<T> * b * n / frame_sample_rate) * get_han_window(n);
				sum2 += volume[n] * std::sin(T(2.0) * std::numbers::pi_v<T> * b * n / frame_sample_rate) * get_han_window(n);
			}
			T avg1 = sum1 / N;
			T avg2 = sum2 / N;
			T result = std::sqrt((avg1 * avg1) + (avg2 * avg2));

			T slope = result - b_result;
			if (b_slope > 0 && slope <= 0) {
				max.try_emplace(result, i);
				if (max.size() > S) max.erase(max.begin());
			}
			b_result = result;
			b_slope = slope;
		}

		std::array<uint32_t, S> arr{};
		int cnt = 0;
		for (auto i = max.crbegin(); i != max.crend(); ++i) {
			arr[cnt] = i->second;
			cnt++;
		}
		return arr;
	}
};

