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
#include "DescriptorHeap.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"

//
// DescriptorAllocator implementation
//
//std::mutex DescriptorAllocator::sm_AllocationMutex;
//std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::sm_DescriptorHeapPool;
const uint32_t cNumDescriptorsPerHeap = 256;

void DescriptorAllocatorStatics::DestroyAll()
{
    m_DescriptorHeapPool.clear();
}

ID3D12DescriptorHeap* DescriptorAllocatorStatics::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
    std::lock_guard<std::mutex> LockGuard(this->m_AllocationMutex);

    D3D12_DESCRIPTOR_HEAP_DESC Desc;
    Desc.Type = Type;
    Desc.NumDescriptors = cNumDescriptorsPerHeap;
    Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Desc.NodeMask = 1;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
    ASSERT_SUCCEEDED(this->m_core.m_pDevice->CreateDescriptorHeap(&Desc, MY_IID_PPV_ARGS(&pHeap)));
    m_DescriptorHeapPool.emplace_back(pHeap);
    return pHeap.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate( uint32_t Count )
{
    if (m_CurrentHeap == nullptr || m_RemainingFreeHandles < Count)
    {
        m_CurrentHeap = this->m_descriptorAllocatorStatics.RequestNewHeap(m_Type);
        m_CurrentHandle = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
        m_RemainingFreeHandles = cNumDescriptorsPerHeap;

        if (m_DescriptorSize == 0)
            m_DescriptorSize = this->m_descriptorAllocatorStatics.m_core.m_pDevice->GetDescriptorHandleIncrementSize(m_Type);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
    m_CurrentHandle.ptr += Count * m_DescriptorSize;
    m_RemainingFreeHandles -= Count;
    return ret;
}

//
// UserDescriptorHeap implementation
//

//void UserDescriptorHeap::Create( const std::wstring& DebugHeapName )
//{
//    ASSERT_SUCCEEDED(Graphics::g_Core.g_Device->CreateDescriptorHeap(&m_HeapDesc, MY_IID_PPV_ARGS(m_Heap.ReleaseAndGetAddressOf())));
//#ifdef RELEASE
//    (void)DebugHeapName;
//#else
//    m_Heap->SetName(DebugHeapName.c_str());
//#endif
//
//    m_DescriptorSize = Graphics::g_Core.g_Device->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
//    m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;
//    m_FirstHandle = DescriptorHandle( m_Heap->GetCPUDescriptorHandleForHeapStart(),  m_Heap->GetGPUDescriptorHandleForHeapStart() );
//    m_NextFreeHandle = m_FirstHandle;
//}
//
//DescriptorHandle UserDescriptorHeap::Alloc( uint32_t Count )
//{
//    ASSERT(HasAvailableSpace(Count), "Descriptor Heap out of space.  Increase heap size.");
//    DescriptorHandle ret = m_NextFreeHandle;
//    m_NextFreeHandle += Count * m_DescriptorSize;
//    return ret;
//}
//
//bool UserDescriptorHeap::ValidateHandle( const DescriptorHandle& DHandle ) const
//{
//    if (DHandle.GetCpuHandle().ptr < m_FirstHandle.GetCpuHandle().ptr ||
//        DHandle.GetCpuHandle().ptr >= m_FirstHandle.GetCpuHandle().ptr + m_HeapDesc.NumDescriptors * m_DescriptorSize)
//        return false;
//
//    if (DHandle.GetGpuHandle().ptr - m_FirstHandle.GetGpuHandle().ptr !=
//        DHandle.GetCpuHandle().ptr - m_FirstHandle.GetCpuHandle().ptr)
//        return false;
//
//    return true;
//}
