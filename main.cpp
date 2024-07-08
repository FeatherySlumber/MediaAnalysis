#include "pch.h"
#include "FFTExecutor.h"
#include "MusicAnalysis.h"
#include "MemoryUtil.h"

#include <chrono>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::Audio;
using namespace winrt::Windows::Media::MediaProperties;

IAsyncOperation<StorageFolder> getCurrentStorageFolder()
{
    std::unique_ptr<wchar_t[]> lpBuffer = std::make_unique<wchar_t[]>(MAX_PATH);
    GetCurrentDirectory(MAX_PATH, lpBuffer.get());
    // wprintf(L"%ls\n", lpBuffer.get());
    return StorageFolder::GetFolderFromPathAsync(lpBuffer.get());
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

int wmain(int argc, wchar_t* argv[])
{
    // 初期化
    winrt::init_apartment();

    // ファイル取得
    StorageFolder folder = getCurrentStorageFolder().get();
#if true // test
    StorageFile r = folder.GetFileAsync(L"test.wav").get();
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
    int N = 1024;
    FFTExecutor<float> executor(N);
    std::unique_ptr<float[]> l_result = std::make_unique<float[]>(N);
    std::unique_ptr<float[]> r_result = std::make_unique<float[]>(N);

    
    // 出力先に関する初期化
    std::ofstream lStream((r.Name() + L"_fft_L.tmp").c_str(), std::ios::trunc | std::ios::binary);
    float lmax = 0; // 検証用
    MemoryUtil<float> l_pcm = MemoryUtil<float>(1024, [&lStream, &executor, &l_result, &lmax](float *pcm) {
        executor.FFT(pcm, l_result.get());
        lStream.write((char*)l_result.get(), sizeof(float) * 1024);
        for(int i = 0; i < 1024; i++) if (l_result[i] > lmax) lmax = l_result[i];
    });    
    std::ofstream rStream((r.Name() + L"_fft_R.tmp").c_str(), std::ios::trunc | std::ios::binary);
    float rmax = 0;
    MemoryUtil<float> r_pcm = MemoryUtil<float>(1024, [&rStream, &executor, &r_result, &rmax](float* pcm) {
        executor.FFT(pcm, r_result.get());
        rStream.write((char*)r_result.get(), sizeof(float) * 1024);
        for (int i = 0; i < 1024; i++) if (r_result[i] > rmax) rmax = r_result[i];
    });

    // 処理の作成
    MusicAnalysis ma(r);
    AudioEncodingProperties ep = ma.get_graph_properties();
    ep.ChannelCount(2);
    ep.SampleRate(30 * N);

    ma.add_outnode([&l_pcm, &r_pcm](float* pcm, uint32_t capacity, TimeSpan ts) {
        uint32_t i = 0;
        while (i < capacity) {
            l_pcm.write(pcm[i]);
            i++;
            r_pcm.write(pcm[i]);
            i++;
        }
        printTimeSpan(ts);
    }, ep);

    // 実行
    ma.execute().get();
    l_pcm.wait_all_processes_end().get();
    r_pcm.wait_all_processes_end().get();

    lStream.close();
    rStream.close();

    std::wcout << std::endl << "Max FFT Value:" << (lmax > rmax ? lmax : rmax) << std::endl;

    return 0;
}