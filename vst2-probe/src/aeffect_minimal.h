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
    VstInt32 magic;
    AEffectDispatcherProc dispatcher;
    AEffectProcessProc process;
    AEffectSetParameterProc setParameter;
    AEffectGetParameterProc getParameter;
    VstInt32 numPrograms;
    VstInt32 numParams;
    VstInt32 numInputs;
    VstInt32 numOutputs;
    VstInt32 flags;
    VstIntPtr resvd1;
    VstIntPtr resvd2;
    VstInt32 initialDelay;
    VstInt32 realQualities;
    VstInt32 offQualities;
    float ioRatio;
    void* object;
    void* user;
    VstInt32 uniqueID;
    VstInt32 version;
    AEffectProcessProc processReplacing;
    AEffectProcessProc processDoubleReplacing;
    char future[56];
};

constexpr VstInt32 kEffectMagic = 0x56737450; // 'VstP'
constexpr VstInt32 effOpen = 0;
constexpr VstInt32 effClose = 1;
constexpr VstInt32 effSetSampleRate = 10;
constexpr VstInt32 effSetBlockSize = 11;
constexpr VstInt32 effMainsChanged = 12;
constexpr VstInt32 effGetEffectName = 45;
constexpr VstInt32 effGetVendorString = 47;
constexpr VstInt32 effGetProductString = 48;
constexpr VstInt32 effGetVendorVersion = 49;
constexpr VstInt32 effCanDo = 51;
constexpr VstInt32 effGetVstVersion = 58;
constexpr VstInt32 audioMasterVersion = 1;
constexpr VstInt32 effFlagsCanReplacing = 1 << 4;
