#pragma once

/**
 * @class MusicAnalysis
 * @brief 音声解析のためAudioGraph、MediaSourceAudioInputNode、AudioFrameOutputNodeを取りまとめたクラス。
 */
class MusicAnalysis
{
    /**
     * @brief AudioFrameOutputNodeとそれを利用するコールバック関数をまとめた構造体
     */
    struct AudioFrameOutputNodeContorller {
        const winrt::Windows::Media::Audio::AudioFrameOutputNode out_node;
        const std::function<void(float*, uint32_t, winrt::Windows::Foundation::TimeSpan)> func;

        AudioFrameOutputNodeContorller(winrt::Windows::Media::Audio::AudioFrameOutputNode const& node, std::function<void(float*, uint32_t, winrt::Windows::Foundation::TimeSpan)> action) : out_node(node), func(action) {};
        const void QuantumStartedHandler(winrt::Windows::Foundation::TimeSpan ts);
    };

    const winrt::Windows::Media::Audio::AudioGraph audioGraph = [this]() {  /// オーディオグラフ
        using namespace winrt::Windows::Media::Audio;
        CreateAudioGraphResult result = AudioGraph::CreateAsync(
            AudioGraphSettings(
                winrt::Windows::Media::Render::AudioRenderCategory::Other
            )
        ).get();
        if (result.Status() != AudioGraphCreationStatus::Success) {
            winrt::throw_hresult(result.ExtendedError());
        }
        AudioGraph ag = result.Graph();
        ag.QuantumStarted({ this, &MusicAnalysis::QuantumStartedHandler });
        return ag;
	}();    
    winrt::Windows::Media::Audio::MediaSourceAudioInputNode in_node{ nullptr };  /// 音声入力ノード
    const winrt::Windows::Storage::StorageFile file;        /// 音声ファイル
    std::vector<AudioFrameOutputNodeContorller> out_nodes;  /// 音声フレーム出力ノードとコールバック関数のベクタ

    /**
     * @brief QuantumStartedイベントハンドラ。
     * 登録された各AudioFrameOutputNodeとコールバック関数を実行する。
     * 
     * @param sender AudioGraph
     * @param args イベント引数
     */
    void QuantumStartedHandler(winrt::Windows::Media::Audio::AudioGraph sender, winrt::Windows::Foundation::IInspectable args);;
public:
    /**
     * @param f 音声ファイル
     */
    MusicAnalysis(winrt::Windows::Storage::StorageFile const& f);;
    MusicAnalysis() = delete;
    MusicAnalysis(const MusicAnalysis&) = delete;
    MusicAnalysis(MusicAnalysis&&) = delete;

    /**
     * @brief 音声解析を実行する非同期関数
     * @return 非同期操作を表す IAsyncAction。メディアコンテンツの末尾到達時に完了。
     */
    winrt::Windows::Foundation::IAsyncAction execute();

    /**
     * @brief オーディオグラフのエンコーディングプロパティを取得する
     * @return エンコーディングプロパティ
     */
    const winrt::Windows::Media::MediaProperties::AudioEncodingProperties get_graph_properties();

    /**
     * @brief 出力ノードを追加する
     * @param action コールバック関数
     * @param properties エンコーディングプロパティ
     */
    void add_outnode(std::function<void(float*, uint32_t, winrt::Windows::Foundation::TimeSpan)> action, winrt::Windows::Media::MediaProperties::AudioEncodingProperties const& properties);

    ~MusicAnalysis();
};