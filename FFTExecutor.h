#pragma once

/// <summary>
/// �G��FFT������
/// size��2�̏搔�Ɍ���
/// size * sizeof(int) + size / 2 * sizeof(complex&lt;T&gt;) + size * sizeof(T)
/// = 12 * size���̃��������m��
/// </summary>
/**
 * @brief �����t�[���G�ϊ� (FFT) �����s����N���X�B
 * 
 * @tparam T FFT �v�Z�p�̕��������_�^�B
 */
template<std::floating_point T>
class FFTExecutor
{
	/**
	 * @brief FFT�̌v�Z�p�f�[�^���L���b�V�����邽�߂̃l�X�g�N���X�B
	 *
	 * @tparam U FFT �v�Z�p�̕��������_�^�B
	 */
	template<std::floating_point U>
	class FFTCache {
		std::vector<std::complex<U>> weight;	/// FFT�̏d�݂�vector�B
		std::vector<int> reverce_indexes;		/// index���r�b�g���]����vector
		std::vector<U> han_windows;				/// �n������vector

	public:
		int N;	/// FFT�̃T�C�Y

		/**
		 * @brief �d�݁A�r�b�g���]�C���f�b�N�X�A�n�j���O�����\�z����B
		 *
		 * @param size FFT�̃T�C�Y�B
		 */
		FFTCache(int size) : N(size), weight(size / 2), reverce_indexes(size), han_windows(size / 2 + 1) {
			// �d�݂̏�����
			for (int i = 0; i < weight.size(); i++) {
				weight[i] = std::polar(1.0f, -i * std::numbers::pi_v<U> *2 / N);
			}

			// �r�b�g���]�̏�����
			for (int j = 0; auto & x : reverce_indexes)
			{
				x = j;
				int k = N;
				while (k > (j ^= (k >>= 1)));
			}

			// �n�����̏������A�㔼��O���ő�p
			for (int i = 0; i < han_windows.size(); i++) {
				han_windows[i] = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<U> *i / N);
			}
		}

		/**
		 * @brief �d�݂��擾����B
		 * �d�݂̌v�Z�ɕK�v��value/stage�̏����l���K�v�B
		 * 
		 * @param value 0 <= value < stage
		 * @param stage 2�̏搔�Ɍ���B
		 * @return ���f��
		 */
		std::complex<U> get_weight(int value, int stage) const {
			return weight[value * reverce_indexes[stage]];
			// return weight[(value * N) / (stage * 2)]
		}
		/**
		 * @brief �l�̃r�b�g���]�l�𓾂�B
		 *
		 * @param value �C���f�b�N�X
		 * @return �r�b�g���]�����C���f�b�N�X
		 */
		int get_reverce_index(int value) const {
			return reverce_indexes[value];
		}
		/**
		 * @brief �C���f�b�N�X�ɉ������n�����̒l�𓾂�B
		 *
		 * @param value �C���f�b�N�X
		 * @return �n�j���O���̒l
		 */
		U get_han_window(int value) const {
			return han_windows[value <= N / 2 ? value : N - value];
		}
	};
	const FFTCache<T> data;	/// FFT�v�Z�p�L���b�V���B
public:
	/**
	 * @param size FFT�̃T�C�Y�B2�̏搔�Ɍ���
	 */
	FFTExecutor(int size) : data(size) {}

	/**
	 * @brief �w�肳�ꂽ�f�[�^�ɑ΂���FFT���s���B
	 *
	 * @param pcm ���͂���f�[�^�ւ̃|�C���^�B
	 * @param result FFT�̌��ʂ̏o�͐�ւ̃|�C���^�B
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
	 * @return FFT�̃T�C�Y�B
	 */
	int size() const { return data.N; }

};
