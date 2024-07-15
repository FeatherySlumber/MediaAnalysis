#include "pch.h"
#include "FFTExecutor.h"
#include "MusicAnalysis.h"
#include "MemoryUtil.h"
#include "TempoCheck.h"

#include <chrono>
#include <ratio>

winrt::Windows::Foundation::IAsyncAction FFTAndBPMOutput(winrt::Windows::Storage::StorageFile audioSource, winrt::Windows::Storage::StorageFolder output);

static winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFolder> getCurrentStorageFolder()
{
    const DWORD bufferSize = GetCurrentDirectory(0, NULL);
    if (bufferSize == 0) {
        winrt::throw_last_error();
    }
    std::unique_ptr<wchar_t[]> lpBuffer = std::make_unique<wchar_t[]>(bufferSize);
    winrt::check_bool(GetCurrentDirectory(bufferSize, lpBuffer.get()));
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

static void printChangeTimeSpan(const winrt::Windows::Foundation::TimeSpan& ts)
{
    static int64_t before = 0;
    int64_t count = std::chrono::duration_cast<std::chrono::duration<int_fast64_t, std::deci>>(ts).count();
    if (count != before) {
        before = count;

        int minutes = static_cast<int>(count / 600);  // 60 * 10
        int seconds = static_cast<int>((count % 60) / 10);
        int deciseconds = static_cast<int>(count % 10);

        std::wcout << std::setw(2) << std::setfill(L'0') << minutes << ":"
            << std::setw(2) << std::setfill(L'0') << seconds << '.'
            << deciseconds
            << '\r' << std::flush;
    }
}

constexpr int FFT_N = 1024;
constexpr int BPMDataSize = 480;
constexpr int BPMFFT_N = 512;
constexpr int BPMOutputCount = 3;
constexpr int DisplayFrameRate = 30;

int wmain(int argc, wchar_t* argv[])
{
    using namespace winrt::Windows::Storage;

    // 初期化
    winrt::init_apartment();

    // ファイル取得
    StorageFolder folder = getCurrentStorageFolder().get();
#if true // test
    StorageFile r = folder.GetFileAsync(L"120.mp3").get();
#else
    StorageFile r{ nullptr };
    if (argc >= 2) {
        r = StorageFile::GetFileFromPathAsync(argv[1]).get();
    }
    else {
        std::wcerr << "Not enough arguments. Specify a path." << endl;
        return 1;
    }
#endif

    FFTAndBPMOutput(r, folder).get();

    return 0;
}

winrt::Windows::Foundation::IAsyncAction FFTAndBPMOutput(winrt::Windows::Storage::StorageFile audioSource, winrt::Windows::Storage::StorageFolder output) 
{
    using namespace winrt::Windows::Media::Core;
    using namespace winrt::Windows::Media::Audio;
    using namespace winrt::Windows::Media::MediaProperties;

    MusicAnalysis ma(audioSource);

#pragma region /****** L,RチャンネルのFFTを出力する準備 ここから *******/
    /* FFT関連の初期化 */
    FFTExecutor<float> executor(FFT_N);
    std::unique_ptr<float[]> l_result = std::make_unique<float[]>(FFT_N);
    std::unique_ptr<float[]> r_result = std::make_unique<float[]>(FFT_N);

    /* FFT関連の出力先の作成 */
    // Lチャンネル
    std::ofstream lStream((audioSource.Name() + L"_fft_L.tmp").c_str(), std::ios::trunc | std::ios::binary);
    float lmax = 0; // 検証用
    MemoryUtil<float> l_pcm = MemoryUtil<float>(FFT_N, [&lStream, &executor, &l_result, &lmax](float* pcm) {
        executor.FFT(pcm, l_result.get());
        lStream.write(reinterpret_cast<const char*>(l_result.get()), sizeof(float) * FFT_N);
        for (int i = 0; i < FFT_N; i++) if (l_result[i] > lmax) lmax = l_result[i];
        });
    // Rチャンネル
    std::ofstream rStream((audioSource.Name() + L"_fft_R.tmp").c_str(), std::ios::trunc | std::ios::binary);
    float rmax = 0;
    MemoryUtil<float> r_pcm = MemoryUtil<float>(FFT_N, [&rStream, &executor, &r_result, &rmax](float* pcm) {
        executor.FFT(pcm, r_result.get());
        rStream.write(reinterpret_cast<const char*>(r_result.get()), sizeof(float) * FFT_N);
        for (int i = 0; i < FFT_N; i++) if (r_result[i] > rmax) rmax = r_result[i];
        });

    /* 処理の作成 */
    // PCMデータ出力形式の設定
    AudioEncodingProperties fft_aep = ma.get_graph_properties();
    fft_aep.ChannelCount(2);
    fft_aep.SampleRate(DisplayFrameRate * FFT_N);

    // PCMデータをL,Rに振分け
    ma.add_outnode([&l_pcm, &r_pcm](float* pcm, uint32_t capacity, winrt::Windows::Foundation::TimeSpan ts) {
        uint32_t i = 0;
        while (i < capacity) {
            l_pcm.write(pcm[i]);
            i++;
            r_pcm.write(pcm[i]);
            i++;
        }
        }, fft_aep);
    /****** L,RチャンネルのFFTを出力する準備 ここまで *******/
#pragma endregion

#pragma region /****** BPMと音量を出力する準備 ここから *******/
    std::ofstream tStream((audioSource.Name() + L"_tempo.tmp").c_str(), std::ios::trunc | std::ios::binary);
    std::ofstream vStream((audioSource.Name() + L"_volume.tmp").c_str(), std::ios::trunc | std::ios::binary);


    /* BPM関連の初期化 */
    // 秒間(samplerate(第3引数) / framesize(第2引数))データ
    // (size(第1引数) * framesize / samplerate)秒分のBPMを取得可能
    TempoCheck<float> tempo(BPMDataSize, BPMFFT_N, DisplayFrameRate * FFT_N);
    Container<float> vol_mem(BPMDataSize);  // BPMの取得、出力用バッファ

    auto bpm_func = [&vStream, &tStream, &tempo](float* pcm) {  // BPMの取得、出力用関数
        vStream.write(reinterpret_cast<const char*>(pcm), sizeof(float) * BPMDataSize);

        std::array<unsigned int, BPMOutputCount> bpms = tempo.get_BPM<BPMOutputCount>(pcm, 60, 270);
        tStream.write(reinterpret_cast<const char*>(bpms.data()), sizeof(unsigned int) * BPMOutputCount);
        };
    std::unique_ptr<float[]> bpmFFT_result = std::make_unique<float[]>(BPMFFT_N);
    FFTExecutor<float> bpmFFT(BPMFFT_N);
    // 音量への変換、BPM解析の実行
    MemoryUtil<float> t_pcm = MemoryUtil<float>(BPMFFT_N, [&vol_mem, &bpm_func, &bpmFFT, &bpmFFT_result](float* pcm) {
        /* 音量の生成 */
        float sum = 0;
#if true
        bpmFFT.FFT(pcm, bpmFFT_result.get());
        for (uint32_t i = 0; i < BPMFFT_N; ++i) {
            sum += bpmFFT_result[i] * bpmFFT_result[i];
}
        float vol = std::sqrt(sum / BPMFFT_N);
#else 
        // 実行値
        for (uint32_t i = 0; i < BPMFFT_N; ++i) {
            sum += pcm[i] * pcm[i];
        }
        float vol = std::sqrt(sum / BPMFFT_N);
#endif
        vol_mem.write(vol);
        if (vol_mem.is_max()) {
            vol_mem.Process(bpm_func).get();
        }
    });

    // PCMデータ出力形式の設定
    AudioEncodingProperties bpm_aep = ma.get_graph_properties();
    bpm_aep.ChannelCount(1);
    bpm_aep.SampleRate(DisplayFrameRate * FFT_N);

    // PCMデータを流す
    ma.add_outnode([&t_pcm](float* pcm, uint32_t capacity, winrt::Windows::Foundation::TimeSpan ts) {
        for (uint32_t i = 0; i < capacity; ++i) {
            t_pcm.write(pcm[i]);
        }
        printChangeTimeSpan(ts);  // 処理進捗の表示
        }, bpm_aep);
    /****** BPMと音量を出力する準備 ここまで *******/
#pragma endregion

    // 実行
    co_await ma.execute();
    co_await l_pcm.wait_all_processes_end();
    co_await r_pcm.wait_all_processes_end();
    co_await t_pcm.wait_all_processes_end();

    lStream.close();
    rStream.close();
    vStream.close();
    tStream.close();

    std::wcout << std::endl << "Max FFT Value:" << (lmax > rmax ? lmax : rmax) << std::endl;

    co_return;
}