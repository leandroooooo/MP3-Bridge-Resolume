#pragma once
#include <cstdint>
#if defined(_WIN32)
#define VSTCALLBACK __cdecl
#else
#define VSTCALLBACK
#endif
using VstInt32 = int32_t;
using VstIntPtr = intptr_t;
struct AEffect;
using audioMasterCallback = VstIntPtr (VSTCALLBACK *)(AEffect*, VstInt32, VstInt32, VstIntPtr, void*, float);
using AEffectDispatcherProc = VstIntPtr (VSTCALLBACK *)(AEffect*, VstInt32, VstInt32, VstIntPtr, void*, float);
using AEffectProcessProc = void (VSTCALLBACK *)(AEffect*, float**, float**, VstInt32);
using AEffectSetParameterProc = void (VSTCALLBACK *)(AEffect*, VstInt32, float);
using AEffectGetParameterProc = float (VSTCALLBACK *)(AEffect*, VstInt32);
struct AEffect {
    VstInt32 magic; AEffectDispatcherProc dispatcher; AEffectProcessProc process;
    AEffectSetParameterProc setParameter; AEffectGetParameterProc getParameter;
    VstInt32 numPrograms, numParams, numInputs, numOutputs, flags;
    VstIntPtr resvd1, resvd2;
    VstInt32 initialDelay, realQualities, offQualities;
    float ioRatio; void* object; void* user;
    VstInt32 uniqueID, version;
    AEffectProcessProc processReplacing, processDoubleReplacing;
    char future[56];
};
// VST2 ERect field order is top, left, bottom, right.
struct ERect { int16_t top, left, bottom, right; };
constexpr VstInt32 kEffectMagic = 0x56737450;
constexpr VstInt32 effOpen = 0, effClose = 1;
constexpr VstInt32 effSetSampleRate = 10, effSetBlockSize = 11, effMainsChanged = 12;
constexpr VstInt32 effEditGetRect = 13, effEditOpen = 14, effEditClose = 15;
constexpr VstInt32 effGetEffectName = 45, effGetVendorString = 47, effGetProductString = 48;
constexpr VstInt32 effGetVendorVersion = 49, effCanDo = 51, effGetVstVersion = 58;
constexpr VstInt32 audioMasterVersion = 1;
constexpr VstInt32 effFlagsHasEditor = 1 << 0, effFlagsCanReplacing = 1 << 4;
