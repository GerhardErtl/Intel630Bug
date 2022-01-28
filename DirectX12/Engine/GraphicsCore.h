//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#pragma once

#include <map>
#include <thread>
#include <mutex>

#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "RootSignature.h"
#include "SamplerManager.h"
#include "GraphicsCommon.h"
#include "DynamicDescriptorHeap.h"
#include "CommandSignature.h"
#include "pchDirectX.h"

class ColorBuffer;
class DepthBuffer;
class GraphicsPSO;
class CommandListManager;
class CommandSignature;
class LinearAllocatorStatics;
class ContextManager;
class CommandListManager;
class GpuTimeManager;

using Microsoft::WRL::ComPtr;

class GraphicsCore
{
public:
    static GraphicsCore* Reserve(HWND g_hWnd);
    void Release();

private:
    static GraphicsCore* s_pCore;

    GraphicsCore(HWND g_hWnd);
    ~GraphicsCore();

    GraphicsCore(const GraphicsCore&) = delete;
    GraphicsCore(GraphicsCore&&) = delete;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_RecommendedAdapter;
    int refCount;
    void Initialize(HWND g_hWnd);
    void Shutdown(void);

public:
    Microsoft::WRL::ComPtr<IDXGIAdapter1> GetRecommendedAdapter() const;
    void Resize(uint32_t width, uint32_t height);
    
    void HandleDeviceRemoved() const;

    void Present(void);

    int GetMultiSampleQuality();

    const unsigned int SWAP_CHAIN_BUFFER_COUNT = 3;

    // device
    ID3D12Device4* m_pDevice = nullptr;

    // Statics and managers
    ContextManager* m_pContextManager = nullptr;
    CommandListManager* m_pCommandManager = nullptr;
    DynamicDescriptorHeapStatics* m_pDynamicDescriptorHeapStatics = nullptr;
    LinearAllocatorStatics* m_pLinearAllocatorStatics = nullptr;
    
    // Swap Chain Buffers
    IDXGISwapChain1* m_pSwapChain1 = nullptr;
    ColorBuffer* m_pDisplayPlanes;
    UINT m_CurrentBufferIndex;

    // root signature and pso for mip map calculation
    RootSignature m_GenerateMipsRS;
    ComputePSO* m_pGenerateMipsLinearPSOs;
    ComputePSO* m_pGenerateMipsGammaPSOs;

    GpuTimeManager* m_pGpuTimeManager;
    int m_GpuTimerIndexRendering;
    int m_GpuTimerIndexTexture;
    int m_GpuTimerPipelineQueryIndex;

    // allocator
    DescriptorAllocatorStatics* m_pDescriptorAllocatorStatics;
    DescriptorAllocator* m_pDescriptorAllocators;
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
    {
        return m_pDescriptorAllocators[Type].Allocate(Count);
    }

    uint32_t m_DisplayWidth;
    uint32_t m_DisplayHeight;

    // PSO Statics
    std::map< size_t, ComPtr<ID3D12PipelineState> > m_GraphicsPSOHashMap;
    std::map< size_t, ComPtr<ID3D12PipelineState> > m_ComputePSOHashMap;
    std::mutex m_HashMapMutex;

    // root signature statics
    std::map< size_t, ComPtr<ID3D12RootSignature> > m_RootSignatureHashMap;

    // Sampler Descriptor statics
    std::map< size_t, D3D12_CPU_DESCRIPTOR_HANDLE > m_SamplerCache;

    // from GraphicsCommon
    SamplerDesc SamplerLinearClampDesc;

    D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearClamp;

    CommandSignature DispatchIndirectCommandSignature;
    CommandSignature DrawIndirectCommandSignature;

    void InitializeCommonState();
    void DestroyCommonState(void);
};

