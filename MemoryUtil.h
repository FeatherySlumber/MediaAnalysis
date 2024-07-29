#pragma once

/**
 * @class Container
 * @brief T 型の配列を保持し、書き込み操作をサポートする汎用コンテナ。MemoryUtilで使用するためのクラス。
 *
 * @tparam T コンテナに格納される要素のタイプ。
 */
template<typename T>
struct Container
{
	/**
	 * @brief 指定されたサイズのコンテナを構築する。
	 *
	 * @param size コンテナのサイズ。
	 */
	Container(int size) : length(size) {
		data = new T[size];
	}

	/**
	 * @brief コンテナを破棄し、そのリソースを解放する。
	 */
	~Container() {
		delete[] data;
	}

	/**
	 * @brief コンテナに値を書き込む。
	 * 
	 * @param value 書き込む値。
	 */
	void write(T value) {
		data[index.load()] = value;
		index++;
	}

	/**
	 * @brief コンテナがいっぱいかどうかを確認する。
	 *
	 * @return 最大サイズに達した場合はtrue、そうでない場合はfalse
	 */
	bool is_max() {
		return index.load() == length;
	}	

	/**
	 * @brief コンテナが空かどうかを確認する。
	 *
	 * @return 空の場合 true、そうでない場合は false.
	 */
	bool is_empty() {
		return index.load() == 0;
	}

	/**
	 * @brief コンテナのデータを非同期的に処理する。
	 *
	 * @param action コンテナ データに対して実行するアクション。
	 * @return 非同期操作を表す IAsyncAction。
	 */
	winrt::Windows::Foundation::IAsyncAction Process(const std::function<void(T*)> action) {
		co_await winrt::resume_background();
		action(data);
		index.store(0);

		co_return;
	}
private:
	const int length;							/// コンテナのサイズ
	std::atomic_int index = std::atomic_int(0);	/// 現在のインデックス
	T* data;									/// データの配列

};

/**
 * @class MemoryUtil
 * @brief T 型の Container インスタンスのコレクションを管理し、スレッドセーフな書き込み操作と非同期処理を行う。
 *
 * @tparam T コンテナに格納される要素のタイプ。
 */
template<typename T>
class MemoryUtil
{
	int size;												/// コンテナサイズ
	std::function<void(T*)> action;							/// データに対して行う処理
	std::vector<std::shared_ptr<Container<T>>> resource;	/// コンテナのリソース
 	std::shared_ptr<Container<T>> current;					/// 現在のコンテナ
	concurrency::concurrent_queue<std::weak_ptr<Container<T>>> exe_queue;	/// 実行キュー
	std::binary_semaphore can_running{ 1 };					/// 非同期処理中かを管理するセマフォ
	std::mutex mtx;											/// 書き込みを管理するミューテックス

	/**
	 * @brief 非同期処理の完了を処理し、可能な場合は次のコンテナの処理をトリガーする。
	 *
	 * @param asyncInfo 完了した非同期操作を表す IAsyncAction。
	 * @param asyncStatus 完了した非同期操作のステータス。
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
	 * @brief 指定されたサイズと処理アクションを持つ MemoryUtil を構築する。
	 *
	 * @param n 各コンテナのサイズ。
	 * @param act 最大になったコンテナのデータに対して実行するアクション。
	 */
	MemoryUtil(int n, std::function<void(T*)> act) : size(n), action(act) {
		current = std::make_shared<Container<T>>(size);
		resource.push_back(current);
	}

	/**
	 * @brief 現在のコンテナに値を書き込み、満杯のコンテナを管理し、必要に応じて非同期処理を開始する。
	 *
	 * @param value コンテナに書き込む値。
	 */
	void write(T value) {
		using namespace winrt::Windows::Foundation;

		std::lock_guard<std::mutex> lock(mtx);

		current->write(value);

		if (current->is_max()) {

			// 処理の開始、または処理キューへの追加
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
	 * @brief すべての非同期プロセスが完了するまで待機する。
	 *
	 * @return 非同期操作を表す IAsyncAction。
	 */
	winrt::Windows::Foundation::IAsyncAction wait_all_processes_end() {
		co_await winrt::resume_background();
		can_running.acquire();
		can_running.release();
		co_return;
	}
};
