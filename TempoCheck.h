#pragma once

template<std::floating_point T>
class TempoCheck
{
	std::vector<T> han_windows; 
	T get_han_window(int value) const {
		return han_windows[value < (N >> 1) ? value : N - value];
	}

	void to_cut_under_volume(T* volume) {
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
	const int N;
	const int frame_size;
	const int sample_rate;
	const T frame_sample_rate;
	TempoCheck(int size, int frame_size, int sample_rate) : N(size), frame_size(frame_size), sample_rate(sample_rate), frame_sample_rate(T(sample_rate) / T(frame_size)), han_windows(size / 2 + 1) {
		for (int i = 0; i < han_windows.size(); i++) {
			han_windows[i] = T(0.5) - T(0.5) * std::cos(T(2.0) * std::numbers::pi_v<T> *i / N);
		}
	}

	int get_BPM(T* wave, int lower, int upper) {
		to_cut_under_volume(wave);

		int max_idx = 0;
		T max = 0;
		for (int i = lower; i < upper; ++i) {
			T sum1 = 0, sum2 = 0;
			T b = T(i) / lower;
			for (int n = 0; n < N; ++n) {
				sum1 += wave[n] * std::cos(T(2.0) * std::numbers::pi_v<T> * b * n / frame_sample_rate) * get_han_window(n);
				sum2 += wave[n] * std::sin(T(2.0) * std::numbers::pi_v<T> * b * n / frame_sample_rate) * get_han_window(n);
			}
			T avg1 = sum1 / N;
			T avg2 = sum2 / N;
			T temp = std::sqrt((avg1 * avg1) + (avg2 * avg2));
			if (max < temp) {
				max = temp;
				max_idx = i;
			}
			std::wcout << temp << '\n';
		}
		return max_idx;
	}
};

