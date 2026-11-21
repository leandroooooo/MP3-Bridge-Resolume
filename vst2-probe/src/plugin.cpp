#include "aeffect_minimal.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace {
struct ProbeState {
    AEffect effect{};
};

VstIntPtr VSTCALLBACK dispatch(AEffect* effect, VstInt32 opcode, VstInt32, VstIntPtr, void* ptr, float) {
    auto* state = effect ? static_cast<ProbeState*>(effect->object) : nullptr;
    switch (opcode) {
        case effOpen:
            return 1;
        case effClose:
            delete state;
            return 1;
        case effSetSampleRate:
        case effSetBlockSize:
        case effMainsChanged:
            return 1;
        case effGetEffectName:
            if (ptr) { std::strncpy(static_cast<char*>(ptr), "MP3 Bridge VST2 Probe", 63); return 1; }
            return 0;
        case effGetProductString:
            if (ptr) { std::strncpy(static_cast<char*>(ptr), "MP3 Bridge VST2 Probe", 63); return 1; }
            return 0;
        case effGetVendorString:
            if (ptr) { std::strncpy(static_cast<char*>(ptr), "Leandro", 63); return 1; }
            return 0;
        case effGetVendorVersion:
            return 1000;
        case effGetVstVersion:
            return 2400;
        case effCanDo:
            if (ptr && std::strcmp(static_cast<const char*>(ptr), "plugAsChannelInsert") == 0) return 1;
            if (ptr && std::strcmp(static_cast<const char*>(ptr), "plugAsSend") == 0) return 1;
            return 0;
        default:
            return 0;
    }
}

// Transparent stereo pass-through: the plugin does not change the clip audio.
void VSTCALLBACK processReplacing(AEffect*, float** inputs, float** outputs, VstInt32 frames) {
    for (int channel = 0; channel < 2; ++channel) {
        if (!outputs || !outputs[channel]) continue;
        const float* input = inputs ? inputs[channel] : nullptr;
        if (!input) {
            std::fill(outputs[channel], outputs[channel] + frames, 0.0f);
        } else {
            std::copy(input, input + frames, outputs[channel]);
        }
    }
}
}

extern "C" __declspec(dllexport) AEffect* VSTCALLBACK VSTPluginMain(audioMasterCallback host) {
    if (!host || host(nullptr, audioMasterVersion, 0, 0, nullptr, 0.0f) == 0) return nullptr;

    auto* state = new (std::nothrow) ProbeState();
    if (!state) return nullptr;

    auto& e = state->effect;
    e.magic = kEffectMagic;
    e.dispatcher = dispatch;
    e.process = nullptr;
    e.setParameter = nullptr;
    e.getParameter = nullptr;
    e.numPrograms = 1;
    e.numParams = 0;
    e.numInputs = 2;
    e.numOutputs = 2;
    e.flags = effFlagsCanReplacing;
    e.initialDelay = 0;
    e.ioRatio = 1.0f;
    e.object = state;
    e.uniqueID = 0x4D503242; // 'MP2B'
    e.version = 1001;
    e.processReplacing = processReplacing;
    e.processDoubleReplacing = nullptr;
    return &e;
}
