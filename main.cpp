#include "pch.h"
#include "FFTExecutor.h"
#include "MusicAnalysis.h"
#include "MemoryUtil.h"
#include "TempoCheck.h"

#include <chrono>


winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFolder> getCurrentStorageFolder()
{
    std::unique_ptr<wchar_t[]> lpBuffer = std::make_unique<wchar_t[]>(MAX_PATH);
    GetCurrentDirectory(MAX_PATH, lpBuffer.get());
    // wprintf(L"%ls\n", lpBuffer.get());
    return winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(lpBuffer.get());
}

void printTimeSpan(const winrt::Windows::Foundation::TimeSpan& ts)
{
    int64_t totalMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(ts).count();

    int minutes = static_cast<int>(totalMilliseconds / 60000);  // 60 * 1000
    int seconds = static_cast<int>((totalMilliseconds % 60000) / 1000);
    int milliseconds = static_cast<int>(totalMilliseconds % 1000);

    std::wcout << std::setw(2) << std::setfill(L'0') << minutes << ":"
        << std::setw(2) << std::setfill(L'0') << seconds << "."
        << std::setw(3) << std::setfill(L'0') << milliseconds
        << '\r' << std::flush;
}

constexpr int FFT_N = 1024;
constexpr int BPM_N = 480;

int wmain(int argc, wchar_t* argv[])
{
    using namespace winrt::Windows::Storage;
    using namespace winrt::Windows::Media::Core;
    using namespace winrt::Windows::Media::Audio;
    using namespace winrt::Windows::Media::MediaProperties;

    // 初期化
    winrt::init_apartment();

    // ファイル取得
    StorageFolder folder = getCurrentStorageFolder().get();
#if true // test
    StorageFile r = folder.GetFileAsync(L"120.mp3").get();
#else
    StorageFile r{ nullptr };
    if (argc >= 2) {
        r = folder.GetFileAsync(argv[1]).get();
    }
    else {
        wcerr << "Not enough arguments. Specify a relative path." << endl;
        return 1;
    }
#endif

    // FFT関連の初期化
    FFTExecutor<float> executor(FFT_N);
    std::unique_ptr<float[]> l_result = std::make_unique<float[]>(FFT_N);
    std::unique_ptr<float[]> r_result = std::make_unique<float[]>(FFT_N);

    
    // 出力先に関する初期化
    std::ofstream lStream((r.Name() + L"_fft_L.tmp").c_str(), std::ios::trunc | std::ios::binary);
    float lmax = 0; // 検証用
    MemoryUtil<float> l_pcm = MemoryUtil<float>(FFT_N, [&lStream, &executor, &l_result, &lmax](float *pcm) {
        executor.FFT(pcm, l_result.get());
        lStream.write((char*)l_result.get(), sizeof(float) * FFT_N);
        for(int i = 0; i < FFT_N; i++) if (l_result[i] > lmax) lmax = l_result[i];
    });    
    std::ofstream rStream((r.Name() + L"_fft_R.tmp").c_str(), std::ios::trunc | std::ios::binary);
    float rmax = 0;
    MemoryUtil<float> r_pcm = MemoryUtil<float>(FFT_N, [&rStream, &executor, &r_result, &rmax](float* pcm) {
        executor.FFT(pcm, r_result.get());
        rStream.write((char*)r_result.get(), sizeof(float) * FFT_N);
        for (int i = 0; i < FFT_N; i++) if (r_result[i] > rmax) rmax = r_result[i];
    });

    // 処理の作成
    MusicAnalysis ma(r);
    AudioEncodingProperties ep = ma.get_graph_properties();
    ep.ChannelCount(2);
    ep.SampleRate(30 * FFT_N);

    ma.add_outnode([&l_pcm, &r_pcm](float* pcm, uint32_t capacity, winrt::Windows::Foundation::TimeSpan ts) {
        uint32_t i = 0;
        while (i < capacity) {
            l_pcm.write(pcm[i]);
            i++;
            r_pcm.write(pcm[i]);
            i++;
        }
        // printTimeSpan(ts);
    }, ep);

    AudioEncodingProperties aep = ma.get_graph_properties();
    aep.ChannelCount(1);
    aep.SampleRate(30 * FFT_N);

    // std::ofstream tStream((r.Name() + L"_tempo.tmp").c_str(), std::ios::trunc | std::ios::binary);
    std::ofstream vStream((r.Name() + L"_volume.tmp").c_str(), std::ios::trunc | std::ios::binary);
    

    constexpr int bpmFFTSize = 512;
    std::unique_ptr<float[]> b_result = std::make_unique<float[]>(BPM_N);
    Container<float> vol_mem(BPM_N);
    TempoCheck<float> tempo(BPM_N, bpmFFTSize, 30 * FFT_N);
    auto bpm_func = [&vStream, &tempo, &b_result](float* pcm) {
        vStream.write((char*)pcm, sizeof(float) * BPM_N);
        
        auto [bpm1, bpm2] = tempo.get_BPM<2>(pcm, 60, 270);
        std::wcout << "bpm:" << bpm1 << ',' << bpm2 << std::endl;
    };
    std::unique_ptr<float[]> v_result = std::make_unique<float[]>(bpmFFTSize);
    FFTExecutor<float> bpmFFT(bpmFFTSize);
    MemoryUtil<float> t_pcm = MemoryUtil<float>(bpmFFTSize, [&vol_mem, &bpm_func, &bpmFFT, &v_result](float* pcm) {
        float sum = 0;
#if true
        bpmFFT.FFT(pcm, v_result.get());
        for (uint32_t i = 0; i < bpmFFTSize; ++i) {
            sum += v_result[i] * v_result[i];
        }
        float vol = std::sqrt(sum / bpmFFTSize);
#else 
        // 実行値
        for (uint32_t i = 0; i < bpmFFTSize; ++i) {
            sum += pcm[i] * pcm[i];
        }
        float vol = std::sqrt(sum / bpmFFTSize);
#endif
        vol_mem.write(vol);
        if (vol_mem.is_max()) {
            vol_mem.Process(bpm_func).get();
        }
    });
    ma.add_outnode([&t_pcm](float* pcm, uint32_t capacity, winrt::Windows::Foundation::TimeSpan ts) {
        for (uint32_t i = 0; i < capacity; ++i) {
            t_pcm.write(pcm[i]);
        }
    }, aep);

    // 実行
    ma.execute().get();
    l_pcm.wait_all_processes_end().get();
    r_pcm.wait_all_processes_end().get();
    t_pcm.wait_all_processes_end().get();

    lStream.close();
    rStream.close();

    std::wcout << std::endl << "Max FFT Value:" << (lmax > rmax ? lmax : rmax) << std::endl;

    return 0;
}