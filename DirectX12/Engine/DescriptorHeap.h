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

#include <mutex>
#include <vector>
#include <queue>
#include <string>

class GraphicsCore;

class DescriptorAllocatorStatics
{
public:
    DescriptorAllocatorStatics(GraphicsCore& core) : m_core(core) {};
    DescriptorAllocatorStatics(const DescriptorAllocatorStatics&) = delete;
    DescriptorAllocatorStatics(DescriptorAllocatorStatics&&) = delete;

    void DestroyAll();
    ID3D12DescriptorHeap* RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type);

public:
    std::mutex m_AllocationMutex;
    std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_DescriptorHeapPool;
    GraphicsCore& m_core;
};

// This is an unbounded resource descriptor allocator.  It is intended to provide space for CPU-visible resource descriptors
// as resources are created.  For those that need to be made shader-visible, they will need to be copied to a UserDescriptorHeap
// or a DynamicDescriptorHeap.
class DescriptorAllocator
{
public:
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE Type, DescriptorAllocatorStatics& descriptorAllocatorStatics) :
        m_Type(Type), 
        m_CurrentHeap(nullptr),
        m_CurrentHandle{ 0 },
        m_descriptorAllocatorStatics(descriptorAllocatorStatics),
        m_DescriptorSize(0),
        m_RemainingFreeHandles(0)
    {}

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate( uint32_t Count );

    void Reset()
    {
        m_CurrentHeap = nullptr;
        m_CurrentHandle = { 0 };
    }

protected:
    DescriptorAllocatorStatics& m_descriptorAllocatorStatics;

    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    ID3D12DescriptorHeap* m_CurrentHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentHandle;
    uint32_t m_DescriptorSize;
    uint32_t m_RemainingFreeHandles;
};


class DescriptorHandle
{
public:
    DescriptorHandle()
    {
        m_CpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
    }

    DescriptorHandle( D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle )
        : m_CpuHandle(CpuHandle)
    {
        m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
    }

    DescriptorHandle( D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle )
        : m_CpuHandle(CpuHandle), m_GpuHandle(GpuHandle)
    {
    }

    DescriptorHandle operator+ ( INT OffsetScaledByDescriptorSize ) const
    {
        DescriptorHandle ret = *this;
        ret += OffsetScaledByDescriptorSize;
        return ret;
    }

    void operator += ( INT OffsetScaledByDescriptorSize )
    {
         if (m_CpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            m_CpuHandle.ptr += OffsetScaledByDescriptorSize;
         if (m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            m_GpuHandle.ptr += OffsetScaledByDescriptorSize;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return m_CpuHandle; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return m_GpuHandle; }

    bool IsNull() const { return m_CpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
    bool IsShaderVisible() const { return m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_CpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_GpuHandle;
};

//class UserDescriptorHeap
//{
//public:
//
//    UserDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount) :
//        m_HeapDesc{ Type, MaxCount, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 1 }
//    {
//        m_HeapDesc.Type = Type;
//        m_HeapDesc.NumDescriptors = MaxCount;
//        m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
//        m_HeapDesc.NodeMask = 1;
//    }
//
//    void Create( const std::wstring& DebugHeapName );
//
//    bool HasAvailableSpace( uint32_t Count ) const { return Count <= m_NumFreeDescriptors; }
//    DescriptorHandle Alloc( uint32_t Count = 1 );
//
//    DescriptorHandle GetHandleAtOffset( uint32_t Offset ) const { return m_FirstHandle + Offset * m_DescriptorSize; }
//
//    bool ValidateHandle( const DescriptorHandle& DHandle ) const;
//
//    ID3D12DescriptorHeap* GetHeapPointer() const { return m_Heap.Get(); }
//
//private:
//
//    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_Heap;
//    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
//    uint32_t m_DescriptorSize = 0;
//    uint32_t m_NumFreeDescriptors = 0;
//    DescriptorHandle m_FirstHandle;
//    DescriptorHandle m_NextFreeHandle;
//};
