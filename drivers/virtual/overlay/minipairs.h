// Copyright (c) 2026 DPS Audio Engine contributors. MIT.
#pragma once
#include "speakertopo.h"
#include "speakertoptable.h"
#include "dpswavtable.h"
NTSTATUS CreateMiniportWaveRTSYSVAD(_Out_ PUNKNOWN*,_In_ REFCLSID,_In_opt_ PUNKNOWN,
    _In_ POOL_FLAGS,_In_ PUNKNOWN,_In_opt_ PVOID,_In_ PENDPOINT_MINIPAIR);
NTSTATUS CreateMiniportTopologySYSVAD(_Out_ PUNKNOWN*,_In_ REFCLSID,_In_opt_ PUNKNOWN,
    _In_ POOL_FLAGS,_In_ PUNKNOWN,_In_opt_ PVOID,_In_ PENDPOINT_MINIPAIR);
static PHYSICALCONNECTIONTABLE DpsPhysicalConnections[] = {
    {KSPIN_TOPO_WAVEOUT_SOURCE,KSPIN_WAVE_RENDER3_SOURCE,CONNECTIONTYPE_WAVE_OUTPUT}
};
static ENDPOINT_MINIPAIR DpsSpeakers = {
    eSpeakerDevice,L"TopologyDps",NULL,CreateMiniportTopologySYSVAD,
    &SpeakerTopoMiniportFilterDescriptor,0,NULL,
    L"WaveDps",NULL,CreateMiniportWaveRTSYSVAD,&DpsWaveDescriptor,0,NULL,
    2,DpsPinFormats,SIZEOF_ARRAY(DpsPinFormats),DpsPhysicalConnections,
    SIZEOF_ARRAY(DpsPhysicalConnections),ENDPOINT_NO_FLAGS,NULL,0,NULL
};
static PENDPOINT_MINIPAIR g_RenderEndpoints[] = {&DpsSpeakers};
// Storage is nonempty for standard C++; the published capture count is zero.
static PENDPOINT_MINIPAIR g_CaptureEndpoints[] = {NULL};
#define g_cRenderEndpoints SIZEOF_ARRAY(g_RenderEndpoints)
#define g_cCaptureEndpoints 0u
#define g_MaxMiniports 2u
