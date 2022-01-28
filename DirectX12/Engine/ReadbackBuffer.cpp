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

#include "pchDirectX.h"
#include "ReadbackBuffer.h"
#include "GraphicsCore.h"

void ReadbackBuffer::Create( const std::wstring& name, uint32_t NumElements, uint32_t ElementSize )
{
    this->Destroy();

    this->m_name = name;
    this->m_ElementCount = NumElements;
    this->m_ElementSize = ElementSize;
    this->m_BufferSize = NumElements * ElementSize;
    this->m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;

    // Create a readback buffer large enough to hold all texel data
    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask = 1;
    HeapProps.VisibleNodeMask = 1;

    // Readback buffers must be 1-dimensional, i.e. "buffer" not "texture2d"
    D3D12_RESOURCE_DESC ResourceDesc = {};
    ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ResourceDesc.Width = this->m_BufferSize;
    ResourceDesc.Height = 1;
    ResourceDesc.DepthOrArraySize = 1;
    ResourceDesc.MipLevels = 1;
    ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    ResourceDesc.SampleDesc.Count = 1;
    ResourceDesc.SampleDesc.Quality = 0;
    ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ASSERT_SUCCEEDED(this->m_core.m_pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, MY_IID_PPV_ARGS(&this->m_pResource)) );

    this->m_GpuVirtualAddress = this->m_pResource->GetGPUVirtualAddress();

#ifdef RELEASE
    (name);
#else
    this->m_pResource->SetName(name.c_str());
#endif
}


void* ReadbackBuffer::Map(void)
{
    void* memory;
    auto readRange = CD3DX12_RANGE(0, this->m_BufferSize);
    auto hr = this->m_pResource->Map(0, &readRange, &memory);

    if (FAILED(hr))
    {
        throw L"NewPipelineQuery max num pipelines";
    }

    return memory;
}

void ReadbackBuffer::Unmap(void)
{
    auto readRange = CD3DX12_RANGE(0, 0);
    this->m_pResource->Unmap(0, &readRange);
}
