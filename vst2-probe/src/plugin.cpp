#include "aeffect_minimal.h"
#include <windows.h>
#include <commdlg.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <iterator>
#include <mutex>
#include <new>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
constexpr int kEditorWidth = 460, kEditorHeight = 150, kOpenButtonId = 1001, kPlayButtonId = 1002, kPauseButtonId = 1003, kStopButtonId = 1004, kLoopButtonId = 1005;
constexpr wchar_t kWindowClass[] = L"MP3BridgePlayerEditor";
constexpr UINT32 kFallbackSampleRate = 48000;
constexpr UINT32 kMaxPlaybackMinutes = 15;

struct AudioBuffer {
    std::vector<float> stereo;
    UINT32 sampleRate = 0;
};

struct ProbeState {
    AEffect effect{};
    audioMasterCallback host = nullptr;
    HWND editor = nullptr, fileLabel = nullptr, hostParent = nullptr;
    wchar_t selectedFile[MAX_PATH]{};
    std::shared_ptr<AudioBuffer> audio;
    std::atomic<uint64_t> playhead{0};
    std::atomic<uint64_t> playbackGeneration{0};
    std::atomic<bool> playing{false};
    std::atomic<bool> hostWasPlaying{false};
    std::atomic<bool> loopEnabled{true};
    std::atomic<float> hostSampleRate{static_cast<float>(kFallbackSampleRate)};
};

struct ComApartment {
    bool initialized = false;
    HRESULT start() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        initialized = SUCCEEDED(hr);
        return (hr == RPC_E_CHANGED_MODE) ? S_OK : hr;
    }
    ~ComApartment() { if (initialized) CoUninitialize(); }
};

std::once_flag gMfStartupOnce;
HRESULT gMfStartupResult = E_FAIL;

HINSTANCE pluginInstance() {
    static HINSTANCE instance = [] {
        HINSTANCE result = nullptr;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&pluginInstance), &result);
        return result;
    }();
    return instance;
}

std::wstring describeError(HRESULT hr) {
    wchar_t message[256]{};
    const DWORD count = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(hr), 0, message, static_cast<DWORD>(std::size(message)), nullptr);
    if (count) return message;
    wchar_t code[32]{};
    swprintf_s(code, L"0x%08lX", static_cast<unsigned long>(hr));
    return code;
}

HRESULT decodeAudioFile(const wchar_t* path, UINT32 requestedRate, std::shared_ptr<AudioBuffer>& result) {
    ComApartment com;
    HRESULT hr = com.start();
    if (FAILED(hr)) return hr;

    std::call_once(gMfStartupOnce, [] { gMfStartupResult = MFStartup(MF_VERSION, MFSTARTUP_FULL); });
    if (FAILED(gMfStartupResult)) return gMfStartupResult;

    ComPtr<IMFAttributes> attributes;
    hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) return hr;
    hr = attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);
    if (FAILED(hr)) return hr;

    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromURL(path, attributes.Get(), &reader);
    if (FAILED(hr)) return hr;

    hr = reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (FAILED(hr)) return hr;
    hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    if (FAILED(hr)) return hr;

    ComPtr<IMFMediaType> outputType;
    hr = MFCreateMediaType(&outputType);
    if (FAILED(hr)) return hr;
    if (FAILED(hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))) return hr;
    if (FAILED(hr = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float))) return hr;
    if (FAILED(hr = outputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32))) return hr;
    if (FAILED(hr = outputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2))) return hr;
    if (FAILED(hr = outputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, requestedRate))) return hr;
    if (FAILED(hr = outputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * sizeof(float)))) return hr;
    if (FAILED(hr = outputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, requestedRate * 2 * sizeof(float)))) return hr;
    hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, outputType.Get());
    if (FAILED(hr)) return hr;

    ComPtr<IMFMediaType> currentType;
    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &currentType);
    if (FAILED(hr)) return hr;
    UINT32 channels = 0, sampleRate = 0, blockAlign = 0;
    if (FAILED(hr = currentType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels))) return hr;
    if (FAILED(hr = currentType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate))) return hr;
    if (FAILED(hr = currentType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign))) return hr;
    if (channels != 2 || blockAlign != 2 * sizeof(float) || sampleRate == 0) return MF_E_INVALIDMEDIATYPE;

    auto decoded = std::make_shared<AudioBuffer>();
    decoded->sampleRate = sampleRate;
    const uint64_t maxFrames = static_cast<uint64_t>(sampleRate) * 60 * kMaxPlaybackMinutes;
    decoded->stereo.reserve(static_cast<size_t>(std::min<uint64_t>(maxFrames * 2, sampleRate * 20ull)));

    for (;;) {
        DWORD streamIndex = 0, flags = 0;
        LONGLONG timestamp = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &timestamp, &sample);
        if (FAILED(hr)) return hr;
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> mediaBuffer;
        hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
        if (FAILED(hr)) return hr;
        BYTE* bytes = nullptr;
        DWORD maxLength = 0, currentLength = 0;
        hr = mediaBuffer->Lock(&bytes, &maxLength, &currentLength);
        if (FAILED(hr)) return hr;
        if (currentLength % blockAlign != 0) {
            mediaBuffer->Unlock();
            return MF_E_INVALIDMEDIATYPE;
        }
        const size_t frameCount = currentLength / blockAlign;
        const uint64_t existingFrames = decoded->stereo.size() / 2;
        if (existingFrames + frameCount > maxFrames) {
            mediaBuffer->Unlock();
            return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        }
        const auto* samples = reinterpret_cast<const float*>(bytes);
        decoded->stereo.insert(decoded->stereo.end(), samples, samples + frameCount * 2);
        mediaBuffer->Unlock();
    }

    if (decoded->stereo.empty()) return MF_E_INVALIDMEDIATYPE;
    result = std::move(decoded);
    return S_OK;
}

void startPlayback(ProbeState* state, bool restartFromBeginning) {
    if (!state) return;
    const auto audio = std::atomic_load_explicit(&state->audio, std::memory_order_acquire);
    if (!audio || audio->stereo.empty()) return;
    if (restartFromBeginning || state->playhead.load(std::memory_order_relaxed) >= audio->stereo.size() / 2)
        state->playhead.store(0, std::memory_order_release);
    state->playing.store(true, std::memory_order_release);
    state->playbackGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void pausePlayback(ProbeState* state) {
    if (state) state->playing.store(false, std::memory_order_release);
}

void stopPlayback(ProbeState* state) {
    if (!state) return;
    state->playing.store(false, std::memory_order_release);
    state->playhead.store(0, std::memory_order_release);
    state->playbackGeneration.fetch_add(1, std::memory_order_acq_rel);
}

void syncHostTransport(ProbeState* state) {
    if (!state || !state->host) return;
    const auto raw = state->host(&state->effect, audioMasterGetTime, 0,
        kVstTransportPlaying, nullptr, 0.0f);
    if (raw == 0) return;
    const auto* timeInfo = reinterpret_cast<const VstTimeInfo*>(raw);
    const bool hostPlaying = (timeInfo->flags & kVstTransportPlaying) != 0;
    const bool wasPlaying = state->hostWasPlaying.exchange(hostPlaying, std::memory_order_acq_rel);
    if (hostPlaying && !wasPlaying) {
        state->playhead.store(0, std::memory_order_release);
        state->playing.store(true, std::memory_order_release);
        state->playbackGeneration.fetch_add(1, std::memory_order_acq_rel);
    } else if (!hostPlaying && wasPlaying) {
        state->playing.store(false, std::memory_order_release);
        state->playhead.store(0, std::memory_order_release);
        state->playbackGeneration.fetch_add(1, std::memory_order_acq_rel);
    }
}

LRESULT CALLBACK editorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<ProbeState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = static_cast<ProbeState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (message == WM_COMMAND && state) {
        switch (LOWORD(wParam)) {
        case kOpenButtonId: {
            wchar_t path[MAX_PATH]{};
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = hwnd;
            dialog.lpstrFilter = L"Áudio (*.mp3;*.wav)\0*.mp3;*.wav\0Todos (*.*)\0*.*\0";
            dialog.lpstrFile = path;
            dialog.nMaxFile = MAX_PATH;
            dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            dialog.lpstrTitle = L"Selecionar áudio";
            if (GetOpenFileNameW(&dialog)) {
                SetWindowTextW(state->fileLabel, L"Carregando áudio...");
                UpdateWindow(hwnd);
                const float hostRate = state->hostSampleRate.load(std::memory_order_relaxed);
                const auto requestedRate = hostRate >= 8000.0f && hostRate <= 192000.0f
                    ? static_cast<UINT32>(std::lround(hostRate)) : kFallbackSampleRate;
                std::shared_ptr<AudioBuffer> decoded;
                const HRESULT hr = decodeAudioFile(path, requestedRate, decoded);
                if (SUCCEEDED(hr)) {
                    std::atomic_store_explicit(&state->audio, std::move(decoded), std::memory_order_release);
                    state->playhead.store(0, std::memory_order_release);
                    state->playing.store(true, std::memory_order_release);
                    state->playbackGeneration.fetch_add(1, std::memory_order_acq_rel);
                    lstrcpynW(state->selectedFile, path, MAX_PATH);
                    const wchar_t* name = wcsrchr(path, L'\\');
                    SetWindowTextW(state->fileLabel, name ? name + 1 : path);
                } else {
                    const std::wstring error = L"Falha ao abrir áudio: " + describeError(hr);
                    SetWindowTextW(state->fileLabel, L"Falha ao carregar o áudio");
                    MessageBoxW(hwnd, error.c_str(), L"MP3 Bridge", MB_OK | MB_ICONERROR);
                }
            }
            return 0;
        }
        case kPlayButtonId:
            startPlayback(state, true);
            return 0;
        case kPauseButtonId:
            pausePlayback(state);
            return 0;
        case kStopButtonId:
            stopPlayback(state);
            return 0;
        case kLoopButtonId:
            state->loopEnabled.store(IsDlgButtonChecked(hwnd, kLoopButtonId) == BST_CHECKED,
                                     std::memory_order_release);
            return 0;
        default:
            break;
        }
    }
    if (message == WM_NCDESTROY) SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool registerEditorClass() {
    static bool registered = false;
    if (registered) return true;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = editorProc;
    wc.hInstance = pluginInstance();
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWindowClass;
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    return registered;
}

VstIntPtr VSTCALLBACK dispatch(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    auto* state = effect ? static_cast<ProbeState*>(effect->object) : nullptr;
    switch (opcode) {
    case effOpen: return 1;
    case effClose:
        if (state && state->editor) DestroyWindow(state->editor);
        delete state; return 1;
    case effSetSampleRate:
        if (state && opt >= 8000.0f && opt <= 192000.0f)
            state->hostSampleRate.store(opt, std::memory_order_relaxed);
        return 1;
    case effSetBlockSize: return 1;
    case effMainsChanged:
        if (state && value == 0) {
            state->hostWasPlaying.store(false, std::memory_order_release);
            stopPlayback(state);
        }
        return 1;
    case effEditIdle:
        syncHostTransport(state);
        return 1;
    case effEditGetRect:
        if (ptr) {
            static ERect rect{0, 0, kEditorHeight, kEditorWidth};
            *static_cast<ERect**>(ptr) = &rect;
            return 1;
        }
        return 0;
    case effEditOpen: {
        if (!state || !ptr || !registerEditorClass()) return 0;
        state->hostParent = static_cast<HWND>(ptr);
        state->editor = CreateWindowExW(
            0, kWindowClass, L"MP3 Bridge Player",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, kEditorWidth, kEditorHeight,
            state->hostParent, nullptr, pluginInstance(), state);
        if (!state->editor) return 0;
        CreateWindowExW(0, L"STATIC", L"MP3 Bridge Player",
            WS_CHILD | WS_VISIBLE, 16, 14, 420, 24, state->editor, nullptr, pluginInstance(), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Abrir MP3",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, 52, 120, 32, state->editor,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpenButtonId)), pluginInstance(), nullptr);
        state->fileLabel = CreateWindowExW(0, L"STATIC",
            state->selectedFile[0] ? state->selectedFile : L"Nenhum arquivo selecionado",
            WS_CHILD | WS_VISIBLE, 150, 58, 285, 24, state->editor, nullptr, pluginInstance(), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Play",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, 98, 78, 28, state->editor,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPlayButtonId)), pluginInstance(), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Pause",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 102, 98, 78, 28, state->editor,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPauseButtonId)), pluginInstance(), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Stop",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 188, 98, 78, 28, state->editor,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStopButtonId)), pluginInstance(), nullptr);
        HWND loopButton = CreateWindowExW(0, L"BUTTON", L"Loop",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, 274, 98, 100, 28, state->editor,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLoopButtonId)), pluginInstance(), nullptr);
        if (loopButton && state->loopEnabled.load(std::memory_order_acquire))
            SendMessageW(loopButton, BM_SETCHECK, BST_CHECKED, 0);
        ShowWindow(state->editor, SW_SHOW);
        UpdateWindow(state->editor);
        return 1;
    }
    case effEditClose:
        if (state && state->editor) { DestroyWindow(state->editor); state->editor = nullptr; state->fileLabel = nullptr; state->hostParent = nullptr; }
        return 1;
    case effGetEffectName:
        if (ptr) { std::strncpy(static_cast<char*>(ptr), "MP3 Bridge Player", 63); return 1; } return 0;
    case effGetProductString:
        if (ptr) { std::strncpy(static_cast<char*>(ptr), "MP3 Bridge Player", 63); return 1; } return 0;
    case effGetVendorString:
        if (ptr) { std::strncpy(static_cast<char*>(ptr), "Leandro", 63); return 1; } return 0;
    case effGetVendorVersion: return 1000;
    case effGetVstVersion: return 2400;
    case effCanDo:
        if (ptr && std::strcmp(static_cast<const char*>(ptr), "plugAsChannelInsert") == 0) return 1;
        if (ptr && std::strcmp(static_cast<const char*>(ptr), "plugAsSend") == 0) return 1;
        return 0;
    default: return 0;
    }
}

void VSTCALLBACK processReplacing(AEffect* effect, float** inputs, float** outputs, VstInt32 frames) {
    if (!outputs || frames <= 0) return;
    auto* state = effect ? static_cast<ProbeState*>(effect->object) : nullptr;
    syncHostTransport(state);
    const auto audio = state ? std::atomic_load_explicit(&state->audio, std::memory_order_acquire) : nullptr;
    if (!audio || audio->stereo.empty()) {
        for (int ch = 0; ch < 2; ++ch) {
            if (!outputs[ch]) continue;
            const float* in = inputs ? inputs[ch] : nullptr;
            if (!in) std::fill(outputs[ch], outputs[ch] + frames, 0.0f);
            else std::copy(in, in + frames, outputs[ch]);
        }
        return;
    }

    // The file selected in MP3 Bridge replaces the clip audio.
    // Silence is emitted while this player is paused or stopped.
    for (int ch = 0; ch < 2; ++ch)
        if (outputs[ch]) std::fill(outputs[ch], outputs[ch] + frames, 0.0f);

    if (!state->playing.load(std::memory_order_acquire)) return;

    const uint64_t totalFrames = audio->stereo.size() / 2;
    const uint64_t generation = state->playbackGeneration.load(std::memory_order_acquire);
    uint64_t cursor = state->playhead.load(std::memory_order_acquire);
    const bool loop = state->loopEnabled.load(std::memory_order_acquire);

    for (VstInt32 i = 0; i < frames; ++i) {
        if (cursor >= totalFrames) {
            if (loop) cursor = 0;
            else break;
        }
        for (int ch = 0; ch < 2; ++ch) {
            if (outputs[ch])
                outputs[ch][i] = audio->stereo[static_cast<size_t>(cursor) * 2 + ch];
        }
        ++cursor;
    }

    if (!loop && cursor >= totalFrames)
        state->playing.store(false, std::memory_order_release);

    // A Stop or file reload during this block owns the new playhead position.
    if (generation == state->playbackGeneration.load(std::memory_order_acquire))
        state->playhead.store(loop ? (cursor % totalFrames) : std::min(cursor, totalFrames),
                              std::memory_order_release);
}
}

extern "C" __declspec(dllexport) AEffect* VSTCALLBACK VSTPluginMain(audioMasterCallback host) {
    if (!host || host(nullptr, audioMasterVersion, 0, 0, nullptr, 0.0f) == 0) return nullptr;
    auto* state = new (std::nothrow) ProbeState();
    if (!state) return nullptr;
    state->host = host;
    state->loopEnabled.store(true, std::memory_order_relaxed);
    auto& e = state->effect;
    e.magic = kEffectMagic; e.dispatcher = dispatch; e.process = nullptr;
    e.setParameter = nullptr; e.getParameter = nullptr; e.numPrograms = 1; e.numParams = 0;
    e.numInputs = 2; e.numOutputs = 2; e.flags = effFlagsCanReplacing | effFlagsHasEditor;
    e.initialDelay = 0; e.ioRatio = 1.0f; e.object = state; e.uniqueID = 0x4D503250;
    e.version = 1003; e.processReplacing = processReplacing; e.processDoubleReplacing = nullptr;
    return &e;
}