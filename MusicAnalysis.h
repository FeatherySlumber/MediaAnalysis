#pragma once

/**
 * @class MusicAnalysis
 * @brief ������͂̂���AudioGraph�AMediaSourceAudioInputNode�AAudioFrameOutputNode�����܂Ƃ߂��N���X�B
 */
class MusicAnalysis
{
    /**
     * @brief AudioFrameOutputNode�Ƃ���𗘗p����R�[���o�b�N�֐����܂Ƃ߂��\����
     */
    struct AudioFrameOutputNodeContorller {
        const winrt::Windows::Media::Audio::AudioFrameOutputNode out_node;
        const std::function<void(float*, uint32_t, winrt::Windows::Foundation::TimeSpan)> func;

        AudioFrameOutputNodeContorller(winrt::Windows::Media::Audio::AudioFrameOutputNode const& node, std::function<void(float*, uint32_t, winrt::Windows::Foundation::TimeSpan)> action) : out_node(node), func(action) {};
        const void QuantumStartedHandler(winrt::Windows::Foundation::TimeSpan ts);
    };

    const winrt::Windows::Media::Audio::AudioGraph audioGraph = [this]() {  /// �I�[�f�B�I�O���t
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
    winrt::Windows::Media::Audio::MediaSourceAudioInputNode in_node{ nullptr };  /// �������̓m�[�h
    const winrt::Windows::Storage::StorageFile file;        /// �����t�@�C��
    std::vector<AudioFrameOutputNodeContorller> out_nodes;  /// �����t���[���o�̓m�[�h�ƃR�[���o�b�N�֐��̃x�N�^

    /**
     * @brief QuantumStarted�C�x���g�n���h���B
     * �o�^���ꂽ�eAudioFrameOutputNode�ƃR�[���o�b�N�֐������s����B
     * 
     * @param sender AudioGraph
     * @param args �C�x���g����
     */
    void QuantumStartedHandler(winrt::Windows::Media::Audio::AudioGraph sender, winrt::Windows::Foundation::IInspectable args);;
public:
    /**
     * @param f �����t�@�C��
     */
    MusicAnalysis(winrt::Windows::Storage::StorageFile const& f);;
    MusicAnalysis() = delete;
    MusicAnalysis(const MusicAnalysis&) = delete;
    MusicAnalysis(MusicAnalysis&&) = delete;

    /**
     * @brief ������͂����s����񓯊��֐�
     * @return �񓯊������\�� IAsyncAction�B���f�B�A�R���e���c�̖������B���Ɋ����B
     */
    winrt::Windows::Foundation::IAsyncAction execute();

    /**
     * @brief �I�[�f�B�I�O���t�̃G���R�[�f�B���O�v���p�e�B���擾����
     * @return �G���R�[�f�B���O�v���p�e�B
     */
    const winrt::Windows::Media::MediaProperties::AudioEncodingProperties get_graph_properties();

    /**
     * @brief �o�̓m�[�h��ǉ�����
     * @param action �R�[���o�b�N�֐�
     * @param properties �G���R�[�f�B���O�v���p�e�B
     */
    void add_outnode(std::function<void(float*, uint32_t, winrt::Windows::Foundation::TimeSpan)> action, winrt::Windows::Media::MediaProperties::AudioEncodingProperties const& properties);

    ~MusicAnalysis();
};