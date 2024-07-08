#include "pch.h"
#include "MusicAnalysis.h"

inline void MusicAnalysis::QuantumStartedHandler(winrt::Windows::Media::Audio::AudioGraph sender, winrt::Windows::Foundation::IInspectable args) {
    unsigned size = (unsigned int)out_nodes.size();
    for (unsigned i = 0; i < size; i++)
    {
        out_nodes[i].QuantumStartedHandler();
    }
}

MusicAnalysis::MusicAnalysis(winrt::Windows::Storage::StorageFile const& f) : file(f) {
    using namespace winrt::Windows::Media::Audio;
    using winrt::Windows::Media::Core::MediaSource;

    MediaSource source = MediaSource::CreateFromStorageFile(file);
    CreateMediaSourceAudioInputNodeResult result = audioGraph.CreateMediaSourceAudioInputNodeAsync(source).get();
    if (result.Status() != MediaSourceAudioInputNodeCreationStatus::Success)
    {
        winrt::throw_hresult(result.ExtendedError());
    }
    in_node = result.Node();
}

winrt::Windows::Foundation::IAsyncAction MusicAnalysis::execute() {
#if false    // ƒ|[ƒŠƒ“ƒO?
    bool flag = false;
    in_node.MediaSourceCompleted([&flag](winrt::Windows::Media::Audio::MediaSourceAudioInputNode, winrt::Windows::Foundation::IInspectable args) {
        flag = true;
    });

    audioGraph.Start();

    co_await resume_background();
    while (flag == false) {
        __nop();
    }
#else
    std::binary_semaphore flag(0);
    in_node.MediaSourceCompleted([&flag](winrt::Windows::Media::Audio::MediaSourceAudioInputNode, winrt::Windows::Foundation::IInspectable args) {
        flag.release();
    });

    audioGraph.Start();

    flag.acquire();
#endif
    audioGraph.Stop();
    co_return;
}

const winrt::Windows::Media::MediaProperties::AudioEncodingProperties MusicAnalysis::get_graph_properties()
{
    return audioGraph.EncodingProperties();
}

void MusicAnalysis::add_outnode(std::function<void(float*, uint32_t, winrt::Windows::Foundation::TimeSpan)> action, winrt::Windows::Media::MediaProperties::AudioEncodingProperties const& properties)
{
    winrt::Windows::Media::Audio::AudioFrameOutputNode frameOutputNode = audioGraph.CreateFrameOutputNode(properties);
    frameOutputNode.ConsumeInput(true);

    out_nodes.emplace_back(frameOutputNode, action);

    in_node.AddOutgoingConnection(frameOutputNode);
}

MusicAnalysis::~MusicAnalysis() {
    in_node.Close();
    unsigned size = (unsigned int)out_nodes.size();
    for (unsigned i = 0; i < size; i++)
    {
        out_nodes[i].out_node.Close();
    }
    audioGraph.Close();
}

inline const void MusicAnalysis::AudioFrameOutputNodeContorller::QuantumStartedHandler() {
    using namespace winrt::Windows::Media;
    using winrt::Windows::Foundation::IMemoryBufferReference;

    AudioFrame frame = out_node.GetFrame();

    AudioBuffer buffer = frame.LockBuffer(AudioBufferAccessMode::Read);
    IMemoryBufferReference reference = buffer.CreateReference();
    uint32_t capacity = reference.Capacity() * sizeof(std::byte) / sizeof(float);
    float* data = reinterpret_cast<float*>(reference.data());

    func(data, capacity, frame.RelativeTime().Value());

    reference.Close();
    buffer.Close();
    frame.Close();
}
