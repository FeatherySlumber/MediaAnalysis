#pragma once

/// <summary>
/// 雑にFFTをする
/// sizeは2の乗数に限る
/// size * sizeof(int) + size / 2 * sizeof(complex&lt;T&gt;) + size * sizeof(T)
/// = 12 * size分のメモリを確保
/// </summary>
/**
 * @brief 高速フーリエ変換 (FFT) を実行するクラス。
 * 
 * @tparam T FFT 計算用の浮動小数点型。
 */
template<std::floating_point T>
class FFTExecutor
{
	const std::vector<std::complex<T>> weight;
	const std::vector<std::uint_fast32_t> rindexes;
	const std::vector<T> han_windows;				/// ハン窓のvector

	// 重みの初期化
	static std::vector<std::complex<T>> init_weight(std::uint_fast32_t n) {
		std::vector<std::complex<T>> w(n >> 1);
		for (std::uint_fast32_t i = 0; i < w.size(); ++i)
		{
			w[i] = std::polar(T(1.0), i * std::numbers::pi_v<T> *-2 / n);
		}
		return w;
	}

	// ビット反転の初期化
	static std::vector<std::uint_fast32_t> init_rindexes(std::uint_fast32_t n) {
		std::vector<std::uint_fast32_t> ri(n);
		for (std::uint_fast32_t j = 0; auto & x : ri)
		{
			x = j;
			std::uint_fast32_t k = n;
			while (k > (j ^= (k >>= 1)));
		}
		return ri;
	}

	// ハン窓の初期化、後半を前半で代用
	static std::vector<T> init_windows(std::uint_fast32_t n) {
		std::vector<T> hw((n >> 1) + 1);
		for (std::uint_fast32_t i = 0; i < hw.size(); ++i) {
			hw[i] = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<T> *i / n);
		}
		return hw;
	}
public:
	const std::uint_fast32_t N;
	/**
	 * @param size FFTのサイズ。2の乗数に限る
	 */
	FFTExecutor(std::uint_fast32_t size) : N(size), weight{ init_weight(size) }, rindexes{ init_rindexes(size) }, han_windows{ init_windows(size) } {}

	/**
	 * @brief 指定されたデータに対してFFTを行う。
	 *
	 * @param pcm 入力するデータへのポインタ。
	 * @param result FFTの結果の出力先へのポインタ。
	 */
	void FFT(T* pcm, T* result)
	{
		using namespace std;

		unique_ptr<complex<T>[]> ans = make_unique_for_overwrite<complex<T>[]>(N);
		for (std::uint_fast32_t i = 0; i < N; ++i) {
			auto ri = rindexes[i];
			auto hw = han_windows[ri <= N >> 1 ? ri : N - ri];
			ans[i] = complex<T>(pcm[ri] * hw, 0);
		}

		std::uint_fast32_t harf = N >> 1;
		for (std::uint_fast32_t stage = 1; stage < N; stage <<= 1) {
			std::uint_fast32_t temp = (stage - 1);
			for (std::uint_fast32_t x = 0; x < harf; ++x)
			{
				std::uint_fast32_t i = (~temp & x) << 1;
				std::uint_fast32_t j = x & temp;

				complex<T> w = weight[j * rindexes[stage]];
				complex<T> t_ans = w * ans[j + i + stage];
				ans[j + i + stage] = ans[j + i] - t_ans;
				ans[j + i] += t_ans;
			}
		}

		for (std::uint_fast32_t i = 0; i < (N >> 1) + 1; ++i) {
			result[i] = abs(ans[i]);
		}
	}
};



template <>
inline void FFTExecutor<float>::FFT(float* pcm, float* result) {
	using namespace std;

	unique_ptr<complex<float>[]> ans = make_unique_for_overwrite<complex<float>[]>(N);
	for (std::uint_fast32_t i = 0; i < N; ++i) {
		auto ri = rindexes[i];
		auto hw = han_windows[ri <= N >> 1 ? ri : N - ri];
		ans[i] = complex<float>(pcm[ri] * hw, 0);
	}

#if true 
	const std::uint_fast32_t eighth = N >> 3;
	alignas(32) std::complex<float> temp_w[4];
	alignas(32) std::complex<float> temp_s[4];
	std::uint_fast32_t temp_ij[4];
	for (std::uint_fast32_t stage = 1; stage < N; stage <<= 1) {
		const std::uint_fast32_t temp = (stage - 1);
		for (std::uint_fast32_t x = 0; x < eighth; ++x)
		{

			auto y = x << 2;
			//#pragma unroll 4
			for (std::uint_fast32_t n = 0; n < 4; ++n)
			{
				const std::uint_fast32_t i = _andn_u32(temp, y) << 1; // (~temp & y) << 1;
				const std::uint_fast32_t j = y & temp;

				temp_w[n] = weight[j * rindexes[stage]];
				temp_s[n] = ans[i + j + stage];
				temp_ij[n] = i + j;
				++y;
			}

			__m256 temp256 = _mm256_load_ps(reinterpret_cast<float*>(temp_w));

			__m256 f1 = _mm256_moveldup_ps(temp256);
			__m256 f2 = _mm256_loadu_ps(reinterpret_cast<float*>(temp_s));
			__m256 f3 = _mm256_movehdup_ps(temp256);
			__m256 f4 = _mm256_permute_ps(f2, _MM_SHUFFLE(2, 3, 0, 1));
			__m256 res256 = _mm256_fmaddsub_ps(f1, f2, _mm256_mul_ps(f3, f4));

			_mm256_store_ps(reinterpret_cast<float*>(temp_s), res256);

			//#pragma unroll 4
			for (std::uint_fast32_t n = 0; n < 4; ++n) {
				ans[temp_ij[n] + stage] = ans[temp_ij[n]] - temp_s[n];
				ans[temp_ij[n]] += temp_s[n];
			}
		}
	}
#else 
	const std::uint_fast32_t quarter = N >> 2;
	for (std::uint_fast32_t stage = 1; stage < N; stage <<= 1) {
		const std::uint_fast32_t temp = (stage - 1);
		for (std::uint_fast32_t x = 0; x < quarter; ++x)
		{
			std::uint_fast32_t y = x << 1;
			const std::uint_fast32_t i1 = _andn_u32(temp, y) << 1; // (~temp & y) << 1;
			const std::uint_fast32_t j1 = y & temp;
			++y;
			const std::uint_fast32_t i2 = _andn_u32(temp, y) << 1;
			const std::uint_fast32_t j2 = y & temp;

			complex<float> w1 = weight[j1 * rindexes[stage]];
			complex<float> w2 = weight[j2 * rindexes[stage]];
			alignas(32) std::complex<float> temp[2] = { w1, w2 }; // 配列に直接 { weight[略], weight[略] }としようとしたら速度が2倍になった。要検証(g++-13 -std=c++20 -mavx2 -mfma -mbmi -mbmi2 -O2)
			complex<float> s1 = ans[j1 + i1 + stage];
			complex<float> s2 = ans[j2 + i2 + stage];

			__m128 temp128 = _mm_load_ps(reinterpret_cast<float*>(temp));

			alignas(32) complex<float> tc[2] = { s1, s2 };
			__m128 f1 = _mm_moveldup_ps(temp128);
			__m128 f2 = _mm_load_ps(reinterpret_cast<float*>(tc));
			__m128 f3 = _mm_movehdup_ps(temp128);
			__m128 f4 = _mm_permute_ps(f2, _MM_SHUFFLE(2, 3, 0, 1));
			__m128 result = _mm_fmaddsub_ps(f1, f2, _mm_mul_ps(f3, f4));

			_mm_store_ps(reinterpret_cast<float*>(tc), result);

			ans[j1 + i1 + stage] = ans[j1 + i1] - tc[0];
			ans[j1 + i1] += tc[0];
			ans[j2 + i2 + stage] = ans[j2 + i2] - tc[1];
			ans[j2 + i2] += tc[1];
		}
	}
#endif
	for (std::uint_fast32_t i = 0; i < (N >> 1) + 1; ++i) {
		result[i] = abs(ans[i]);
	}
}
