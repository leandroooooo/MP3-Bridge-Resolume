#include "aeffect_minimal.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace {
struct ProbeState {
    AEffect effect{};
    audioMasterCallback host = nullptr;
    float gain = 1.0f;
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

void VSTCALLBACK setParameter(AEffect* effect, VstInt32 index, float value) {
    if (effect && index == 0) {
        auto* state = static_cast<ProbeState*>(effect->object);
        if (state) state->gain = std::clamp(value, 0.0f, 1.0f);
    }
}

float VSTCALLBACK getParameter(AEffect* effect, VstInt32 index) {
    if (!effect || index != 0) return 0.0f;
    auto* state = static_cast<ProbeState*>(effect->object);
    return state ? state->gain : 1.0f;
}

void VSTCALLBACK processReplacing(AEffect* effect, float** inputs, float** outputs, VstInt32 frames) {
    auto* state = effect ? static_cast<ProbeState*>(effect->object) : nullptr;
    const float gain = state ? state->gain : 1.0f;
    for (int channel = 0; channel < 2; ++channel) {
        if (!outputs || !outputs[channel]) continue;
        const float* input = (inputs ? inputs[channel] : nullptr);
        if (!input) {
            std::fill(outputs[channel], outputs[channel] + frames, 0.0f);
            continue;
        }
        for (VstInt32 i = 0; i < frames; ++i) outputs[channel][i] = input[i] * gain;
    }
}
}

extern "C" __declspec(dllexport) AEffect* VSTCALLBACK VSTPluginMain(audioMasterCallback host) {
    if (!host || host(nullptr, audioMasterVersion, 0, 0, nullptr, 0.0f) == 0) return nullptr;

    auto* state = new (std::nothrow) ProbeState();
    if (!state) return nullptr;
    state->host = host;
    auto& e = state->effect;
    e.magic = kEffectMagic;
    e.dispatcher = dispatch;
    e.process = nullptr;
    e.setParameter = setParameter;
    e.getParameter = getParameter;
    e.numPrograms = 1;
    e.numParams = 1;
    e.numInputs = 2;
    e.numOutputs = 2;
    e.flags = effFlagsCanReplacing;
    e.initialDelay = 0;
    e.ioRatio = 1.0f;
    e.object = state;
    e.uniqueID = 0x4D503242; // 'MP2B'
    e.version = 1000;
    e.processReplacing = processReplacing;
    e.processDoubleReplacing = nullptr;
    return &e;
}

extern "C" __declspec(dllexport) AEffect* VSTCALLBACK main(audioMasterCallback host) {
    return VSTPluginMain(host);
}
