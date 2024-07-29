#pragma once

/**
 * @class Container
 * @brief T �^�̔z���ێ����A�������ݑ�����T�|�[�g����ėp�R���e�i�BMemoryUtil�Ŏg�p���邽�߂̃N���X�B
 *
 * @tparam T �R���e�i�Ɋi�[�����v�f�̃^�C�v�B
 */
template<typename T>
struct Container
{
	/**
	 * @brief �w�肳�ꂽ�T�C�Y�̃R���e�i���\�z����B
	 *
	 * @param size �R���e�i�̃T�C�Y�B
	 */
	Container(int size) : length(size) {
		data = new T[size];
	}

	/**
	 * @brief �R���e�i��j�����A���̃��\�[�X���������B
	 */
	~Container() {
		delete[] data;
	}

	/**
	 * @brief �R���e�i�ɒl���������ށB
	 * 
	 * @param value �������ޒl�B
	 */
	void write(T value) {
		data[index.load()] = value;
		index++;
	}

	/**
	 * @brief �R���e�i�������ς����ǂ������m�F����B
	 *
	 * @return �ő�T�C�Y�ɒB�����ꍇ��true�A�����łȂ��ꍇ��false
	 */
	bool is_max() {
		return index.load() == length;
	}	

	/**
	 * @brief �R���e�i���󂩂ǂ������m�F����B
	 *
	 * @return ��̏ꍇ true�A�����łȂ��ꍇ�� false.
	 */
	bool is_empty() {
		return index.load() == 0;
	}

	/**
	 * @brief �R���e�i�̃f�[�^��񓯊��I�ɏ�������B
	 *
	 * @param action �R���e�i �f�[�^�ɑ΂��Ď��s����A�N�V�����B
	 * @return �񓯊������\�� IAsyncAction�B
	 */
	winrt::Windows::Foundation::IAsyncAction Process(const std::function<void(T*)> action) {
		co_await winrt::resume_background();
		action(data);
		index.store(0);

		co_return;
	}
private:
	const int length;							/// �R���e�i�̃T�C�Y
	std::atomic_int index = std::atomic_int(0);	/// ���݂̃C���f�b�N�X
	T* data;									/// �f�[�^�̔z��

};

/**
 * @class MemoryUtil
 * @brief T �^�� Container �C���X�^���X�̃R���N�V�������Ǘ����A�X���b�h�Z�[�t�ȏ������ݑ���Ɣ񓯊��������s���B
 *
 * @tparam T �R���e�i�Ɋi�[�����v�f�̃^�C�v�B
 */
template<typename T>
class MemoryUtil
{
	int size;												/// �R���e�i�T�C�Y
	std::function<void(T*)> action;							/// �f�[�^�ɑ΂��čs������
	std::vector<std::shared_ptr<Container<T>>> resource;	/// �R���e�i�̃��\�[�X
 	std::shared_ptr<Container<T>> current;					/// ���݂̃R���e�i
	concurrency::concurrent_queue<std::weak_ptr<Container<T>>> exe_queue;	/// ���s�L���[
	std::binary_semaphore can_running{ 1 };					/// �񓯊������������Ǘ�����Z�}�t�H
	std::mutex mtx;											/// �������݂��Ǘ�����~���[�e�b�N�X

	/**
	 * @brief �񓯊������̊������������A�\�ȏꍇ�͎��̃R���e�i�̏������g���K�[����B
	 *
	 * @param asyncInfo ���������񓯊������\�� IAsyncAction�B
	 * @param asyncStatus ���������񓯊�����̃X�e�[�^�X�B
	 */
	void ProcessCompletedHandler(winrt::Windows::Foundation::IAsyncAction const& asyncInfo, winrt::Windows::Foundation::AsyncStatus const asyncStatus) {
		std::weak_ptr<Container<T>> next;

		_RPT1(_CRT_WARN, "%p : 1 Container Processed, %d Container Left.\n", this, exe_queue.unsafe_size());

		if (!exe_queue.try_pop(next)) {
			can_running.release();
			return;
		}

		if (auto n = next.lock()) {
			winrt::Windows::Foundation::IAsyncAction state = n->Process(action);
			state.Completed({ this, &MemoryUtil<T>::ProcessCompletedHandler });
		}
	};
public:
	/**
	 * @brief �w�肳�ꂽ�T�C�Y�Ə����A�N�V���������� MemoryUtil ���\�z����B
	 *
	 * @param n �e�R���e�i�̃T�C�Y�B
	 * @param act �ő�ɂȂ����R���e�i�̃f�[�^�ɑ΂��Ď��s����A�N�V�����B
	 */
	MemoryUtil(int n, std::function<void(T*)> act) : size(n), action(act) {
		current = std::make_shared<Container<T>>(size);
		resource.push_back(current);
	}

	/**
	 * @brief ���݂̃R���e�i�ɒl���������݁A���t�̃R���e�i���Ǘ����A�K�v�ɉ����Ĕ񓯊��������J�n����B
	 *
	 * @param value �R���e�i�ɏ������ޒl�B
	 */
	void write(T value) {
		using namespace winrt::Windows::Foundation;

		std::lock_guard<std::mutex> lock(mtx);

		current->write(value);

		if (current->is_max()) {

			// �����̊J�n�A�܂��͏����L���[�ւ̒ǉ�
			if (can_running.try_acquire()) {
				IAsyncAction state = current->Process(action);
				state.Completed({ this, &MemoryUtil<T>::ProcessCompletedHandler });
			}
			else {
				exe_queue.push(current);
			}
			
			auto result = std::find_if(resource.begin(), resource.end(), [](const std::shared_ptr<Container<T>>& c) { return c->is_empty(); });
			if (result == resource.end()) {

				current = std::make_shared<Container<T>>(size);
				resource.push_back(current);
			}
			else {
				current = *result;
			}
		}
	}

	/**
	 * @brief ���ׂĂ̔񓯊��v���Z�X����������܂őҋ@����B
	 *
	 * @return �񓯊������\�� IAsyncAction�B
	 */
	winrt::Windows::Foundation::IAsyncAction wait_all_processes_end() {
		co_await winrt::resume_background();
		can_running.acquire();
		can_running.release();
		co_return;
	}
};
