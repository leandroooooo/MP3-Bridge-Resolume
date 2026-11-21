#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <iostream>
#include <iomanip>

using Microsoft::WRL::ComPtr;

static void PrintHr(const wchar_t* step, HRESULT hr) {
    std::wcerr << step << L" falhou. HRESULT=0x"
               << std::hex << std::uppercase
               << static_cast<unsigned long>(hr) << std::dec << L"\n";
}

static int DecodeAndCheck(const wchar_t* path) {
    ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(path, nullptr, &reader);
    if (FAILED(hr)) {
        PrintHr(L"Abrir MP3 no Media Foundation", hr);
        return 1;
    }

    // Solicita PCM. O Source Reader tenta carregar automaticamente
    // o decoder apropriado para o fluxo MP3.
    ComPtr<IMFMediaType> pcm;
    hr = MFCreateMediaType(&pcm);
    if (FAILED(hr)) { PrintHr(L"MFCreateMediaType", hr); return 1; }

    hr = pcm->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) { PrintHr(L"Definir tipo Audio", hr); return 1; }

    hr = pcm->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) { PrintHr(L"Definir PCM", hr); return 1; }

    hr = reader->SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcm.Get());
    if (FAILED(hr)) { PrintHr(L"Configurar decodificacao PCM", hr); return 1; }

    ComPtr<IMFMediaType> actual;
    hr = reader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actual);
    if (FAILED(hr)) { PrintHr(L"Ler formato PCM", hr); return 1; }

    UINT32 sampleRate = 0, channels = 0, bits = 0;
    actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);

    std::wcout << L"Arquivo: " << path << L"\n"
               << L"Formato decodificado: PCM\n"
               << L"Taxa: " << sampleRate << L" Hz | Canais: " << channels
               << L" | Bits: " << bits << L"\n"
               << L"Lendo o audio para validar a decodificacao...\n";

    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    unsigned long long totalBytes = 0;
    for (;;) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
            nullptr, &flags, nullptr, &sample);
        if (FAILED(hr)) { PrintHr(L"ReadSample", hr); return 1; }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;

        if (sample) {
            DWORD length = 0;
            hr = sample->GetTotalLength(&length);
            if (FAILED(hr)) { PrintHr(L"GetTotalLength", hr); return 1; }
            totalBytes += length;
        }
    }

    if (totalBytes == 0) {
        std::wcerr << L"Nenhum dado PCM foi produzido. Verifique o arquivo.\n";
        return 1;
    }

    std::wcout << L"\nSUCESSO: MP3 aberto e decodificado pelo Windows.\n"
               << L"Dados PCM lidos: " << totalBytes << L" bytes\n"
               << L"Este teste valida o decoder do Windows, nao a importacao no Resolume.\n";
    return 0;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 2) {
        std::wcerr << L"Uso: Mp3DecodeCheck.exe \"C:\\caminho\\arquivo.mp3\"\n";
        return 2;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        PrintHr(L"CoInitializeEx", hr);
        return 1;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        PrintHr(L"MFStartup", hr);
        CoUninitialize();
        return 1;
    }

    const int result = DecodeAndCheck(argv[1]);
    MFShutdown();
    CoUninitialize();
    return result;
}
