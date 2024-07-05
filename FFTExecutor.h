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
	/**
	 * @brief FFTの計算用データをキャッシュするためのネストクラス。
	 *
	 * @tparam U FFT 計算用の浮動小数点型。
	 */
	template<std::floating_point U>
	class FFTCache {
		std::vector<std::complex<U>> weight;	/// FFTの重みのvector。
		std::vector<int> reverce_indexes;		/// indexをビット反転したvector
		std::vector<U> han_windows;				/// ハン窓のvector

	public:
		int N;	/// FFTのサイズ

		/**
		 * @brief 重み、ビット反転インデックス、ハニング窓を構築する。
		 *
		 * @param size FFTのサイズ。
		 */
		FFTCache(int size) : N(size), weight(size / 2), reverce_indexes(size), han_windows(size / 2 + 1) {
			// 重みの初期化
			for (int i = 0; i < weight.size(); i++) {
				weight[i] = std::polar(1.0f, -i * std::numbers::pi_v<U> *2 / N);
			}

			// ビット反転の初期化
			for (int j = 0; auto & x : reverce_indexes)
			{
				x = j;
				int k = N;
				while (k > (j ^= (k >>= 1)));
			}

			// ハン窓の初期化、後半を前半で代用
			for (int i = 0; i < han_windows.size(); i++) {
				han_windows[i] = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<U> *i / N);
			}
		}

		/**
		 * @brief 重みを取得する。
		 * 重みの計算に必要なvalue/stageの小数値が必要。
		 * 
		 * @param value 0 <= value < stage
		 * @param stage 2の乗数に限る。
		 * @return 複素数
		 */
		std::complex<U> get_weight(int value, int stage) const {
			return weight[value * reverce_indexes[stage]];
			// return weight[(value * N) / (stage * 2)]
		}
		/**
		 * @brief 値のビット反転値を得る。
		 *
		 * @param value インデックス
		 * @return ビット反転したインデックス
		 */
		int get_reverce_index(int value) const {
			return reverce_indexes[value];
		}
		/**
		 * @brief インデックスに応じたハン窓の値を得る。
		 *
		 * @param value インデックス
		 * @return ハニング窓の値
		 */
		U get_han_window(int value) const {
			return han_windows[value <= N / 2 ? value : N - value];
		}
	};
	const FFTCache<T> data;	/// FFT計算用キャッシュ。
public:
	/**
	 * @param size FFTのサイズ。2の乗数に限る
	 */
	FFTExecutor(int size) : data(size) {}

	/**
	 * @brief 指定されたデータに対してFFTを行う。
	 *
	 * @param pcm 入力するデータへのポインタ。
	 * @param result FFTの結果の出力先へのポインタ。
	 */
	void FFT(T* pcm, T* result)
	{
		using namespace std;

		unique_ptr<complex<T>[]> ans = make_unique_for_overwrite<complex<T>[]>(data.N);
		for (int i = 0; i < data.N; i++) {
			int ri = data.get_reverce_index(i);
			T hw = data.get_han_window(ri);
			ans[i] = complex<T>(pcm[ri] * hw, 0);
		}

		for (int stage = 1; stage < data.N; stage <<= 1) {
			for (int j = 0; j < stage; j++) {
				complex<T> w = data.get_weight(j, stage);
				for (int i = 0; i < data.N; i += stage << 1) {
					complex<T> temp = w * ans[j + i + stage];
					ans[j + i + stage] = ans[j + i] - temp;
					ans[j + i] += temp;
				}
			}
		}

		for (int i = 0; i < data.N; i++) {
			result[i] = abs(ans[i]);
		}
	}

	/**
	 * @return FFTのサイズ。
	 */
	int size() const { return data.N; }

};
