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
class GraphicsCore;

class GpuResource
{
    friend class CommandContext;
    friend class GraphicsContext;
    friend class ComputeContext;

public:
    GpuResource(GraphicsCore& core) :
        m_UsageState(D3D12_RESOURCE_STATE_COMMON),
        m_TransitioningState((D3D12_RESOURCE_STATES)-1),
        m_GpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
        m_core(core),
        m_UserAllocatedMemory(nullptr)
    {
    }

    GpuResource(GraphicsCore& core, ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState) :
        m_pResource(pResource),
        m_UsageState(CurrentState),
        m_TransitioningState((D3D12_RESOURCE_STATES)-1),
        m_GpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL),
        m_core(core),
        m_UserAllocatedMemory(nullptr)
    {
    }

    virtual void Destroy()
    {
        this->m_pResource = nullptr;
        this->m_GpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
        if (this->m_UserAllocatedMemory != nullptr)
        {
            VirtualFree(this->m_UserAllocatedMemory, 0, MEM_RELEASE);
            this->m_UserAllocatedMemory = nullptr;
        }
    }

    ID3D12Resource* operator->() { return this->m_pResource.Get(); }
    const ID3D12Resource* operator->() const { return this->m_pResource.Get(); }

    ID3D12Resource* GetResource() { return this->m_pResource.Get(); }
    const ID3D12Resource* GetResource() const { return this->m_pResource.Get(); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return this->m_GpuVirtualAddress; }

protected:

    Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
    D3D12_RESOURCE_STATES m_UsageState;
    D3D12_RESOURCE_STATES m_TransitioningState;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
    GraphicsCore& m_core;

    // When using VirtualAlloc() to allocate memory directly, record the allocation here so that it can be freed.  The
    // GpuVirtualAddress may be offset from the true allocation start.
    void* m_UserAllocatedMemory;
};
