#include "aeffect_minimal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <algorithm>
#include <cstring>
#include <new>

namespace {
constexpr int kEditorWidth = 460, kEditorHeight = 150, kOpenButtonId = 1001;
constexpr wchar_t kWindowClass[] = L"MP3BridgePlayerEditor";
struct ProbeState {
    AEffect effect{};
    HWND editor = nullptr, fileLabel = nullptr, hostParent = nullptr;
    wchar_t selectedFile[MAX_PATH]{};
};
LRESULT CALLBACK editorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM) {
    auto* state = reinterpret_cast<ProbeState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_COMMAND && LOWORD(wParam) == kOpenButtonId && state) {
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
            lstrcpynW(state->selectedFile, path, MAX_PATH);
            const wchar_t* name = wcsrchr(path, L'\\');
            SetWindowTextW(state->fileLabel, name ? name + 1 : path);
        }
        return 0;
    }
    if (message == WM_NCDESTROY) SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    return DefWindowProcW(hwnd, message, 0, 0);
}
bool registerEditorClass() {
    static bool registered = false;
    if (registered) return true;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc); wc.lpfnWndProc = editorProc;
    wc.hInstance = GetModuleHandleW(nullptr); wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWindowClass;
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    return registered;
}
VstIntPtr VSTCALLBACK dispatch(AEffect* effect, VstInt32 opcode, VstInt32, VstIntPtr, void* ptr, float) {
    auto* state = effect ? static_cast<ProbeState*>(effect->object) : nullptr;
    switch (opcode) {
    case effOpen: return 1;
    case effClose:
        if (state && state->editor) DestroyWindow(state->editor);
        delete state; return 1;
    case effSetSampleRate: case effSetBlockSize: case effMainsChanged: return 1;
    case effEditGetRect:
        if (ptr) { static ERect rect{0, kEditorHeight, 0, kEditorWidth}; *static_cast<ERect**>(ptr) = &rect; return 1; }
        return 0;
    case effEditOpen:
        if (!state || !ptr || !registerEditorClass()) return 0;
        state->hostParent = static_cast<HWND>(ptr);
        state->editor = CreateWindowExW(0, kWindowClass, L"MP3 Bridge Player",
            WS_CHILD | WS_VISIBLE, 0, 0, kEditorWidth, kEditorHeight,
            state->hostParent, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!state->editor) return 0;
        SetWindowLongPtrW(state->editor, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        CreateWindowW(L"STATIC", L"MP3 Bridge Player - teste da interface",
            WS_CHILD | WS_VISIBLE, 16, 14, 420, 24, state->editor, nullptr, GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"Abrir MP3",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, 52, 120, 32, state->editor,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOpenButtonId)), GetModuleHandleW(nullptr), nullptr);
        state->fileLabel = CreateWindowW(L"STATIC", L"Nenhum arquivo selecionado",
            WS_CHILD | WS_VISIBLE, 150, 58, 285, 24, state->editor, nullptr, GetModuleHandleW(nullptr), nullptr);
        return 1;
    case effEditClose:
        if (state && state->editor) { DestroyWindow(state->editor); state->editor = nullptr; state->fileLabel = nullptr; state->hostParent = nullptr; }
        return 1;
    case effGetEffectName:
        if (ptr) { std::strncpy(static_cast<char*>(ptr), "MP3 Bridge Player Preview", 63); return 1; } return 0;
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
// Audio remains transparent in this UI-validation build. MP3 decoding comes next.
void VSTCALLBACK processReplacing(AEffect*, float** inputs, float** outputs, VstInt32 frames) {
    for (int ch = 0; ch < 2; ++ch) {
        if (!outputs || !outputs[ch]) continue;
        const float* in = inputs ? inputs[ch] : nullptr;
        if (!in) std::fill(outputs[ch], outputs[ch] + frames, 0.0f);
        else std::copy(in, in + frames, outputs[ch]);
    }
}
}
extern "C" __declspec(dllexport) AEffect* VSTCALLBACK VSTPluginMain(audioMasterCallback host) {
    if (!host || host(nullptr, audioMasterVersion, 0, 0, nullptr, 0.0f) == 0) return nullptr;
    auto* state = new (std::nothrow) ProbeState();
    if (!state) return nullptr;
    auto& e = state->effect;
    e.magic = kEffectMagic; e.dispatcher = dispatch; e.process = nullptr;
    e.setParameter = nullptr; e.getParameter = nullptr; e.numPrograms = 1; e.numParams = 0;
    e.numInputs = 2; e.numOutputs = 2; e.flags = effFlagsCanReplacing | effFlagsHasEditor;
    e.initialDelay = 0; e.ioRatio = 1.0f; e.object = state; e.uniqueID = 0x4D503250;
    e.version = 1002; e.processReplacing = processReplacing; e.processDoubleReplacing = nullptr;
    return &e;
}