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
#include "DynamicDescriptorHeap.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"
#include "RootSignature.h"

//
// DynamicDescriptorHeap Implementation
//

ID3D12DescriptorHeap* DynamicDescriptorHeapStatics::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
{
    std::lock_guard<std::mutex> LockGuard(this->m_Mutex);

    uint32_t idx = HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;

    while (!this->m_RetiredDescriptorHeaps[idx].empty() && this->m_core.m_pCommandManager->IsFenceComplete(this->m_RetiredDescriptorHeaps[idx].front().first))
    {
        this->m_AvailableDescriptorHeaps[idx].push(this->m_RetiredDescriptorHeaps[idx].front().second);
        this->m_RetiredDescriptorHeaps[idx].pop();
    }

    if (!this->m_AvailableDescriptorHeaps[idx].empty())
    {
        ID3D12DescriptorHeap* HeapPtr = this->m_AvailableDescriptorHeaps[idx].front();
        this->m_AvailableDescriptorHeaps[idx].pop();
        return HeapPtr;
    }
    else
    {
        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.Type = HeapType;
        HeapDesc.NumDescriptors = kNumDescriptorsPerHeap;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HeapDesc.NodeMask = 1;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> HeapPtr;
        ASSERT_SUCCEEDED(this->m_core.m_pDevice->CreateDescriptorHeap(&HeapDesc, MY_IID_PPV_ARGS(&HeapPtr)));
        m_DescriptorHeapPool[idx].emplace_back(HeapPtr);
        return HeapPtr.Get();
    }
}

void DynamicDescriptorHeapStatics::DiscardDescriptorHeaps( D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint64_t FenceValue, const std::vector<ID3D12DescriptorHeap*>& UsedHeaps )
{
    uint32_t idx = HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;
    std::lock_guard<std::mutex> LockGuard(m_Mutex);
    for (auto iter = UsedHeaps.begin(); iter != UsedHeaps.end(); ++iter)
        m_RetiredDescriptorHeaps[idx].push(std::make_pair(FenceValue, *iter));
}

void DynamicDescriptorHeap::RetireCurrentHeap( void )
{
    // Don't retire unused heaps.
    if (m_CurrentOffset == 0)
    {
        ASSERT(m_CurrentHeapPtr == nullptr);
        return;
    }

    ASSERT(m_CurrentHeapPtr != nullptr);
    m_RetiredHeaps.push_back(m_CurrentHeapPtr);
    m_CurrentHeapPtr = nullptr;
    m_CurrentOffset = 0;
}

void DynamicDescriptorHeap::RetireUsedHeaps( uint64_t fenceValue )
{
    this->m_dynamicDescriptorHeapStatics.DiscardDescriptorHeaps(m_DescriptorType, fenceValue, m_RetiredHeaps);
    m_RetiredHeaps.clear();
}

DynamicDescriptorHeap::DynamicDescriptorHeap(CommandContext& OwningContext, D3D12_DESCRIPTOR_HEAP_TYPE HeapType, DynamicDescriptorHeapStatics& dynamicDescriptorHeapStatics)
    : m_OwningContext(OwningContext), m_DescriptorType(HeapType), m_dynamicDescriptorHeapStatics(dynamicDescriptorHeapStatics)
{
    m_CurrentHeapPtr = nullptr;
    m_CurrentOffset = 0;
    m_DescriptorSize = this->m_dynamicDescriptorHeapStatics.m_core.m_pDevice->GetDescriptorHandleIncrementSize(HeapType);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap()
{
}

void DynamicDescriptorHeap::CleanupUsedHeaps( uint64_t fenceValue )
{
    RetireCurrentHeap();
    RetireUsedHeaps(fenceValue);
    m_GraphicsHandleCache.ClearCache();
    m_ComputeHandleCache.ClearCache();
}

inline ID3D12DescriptorHeap* DynamicDescriptorHeap::GetHeapPointer()
{
    if (m_CurrentHeapPtr == nullptr)
    {
        ASSERT(m_CurrentOffset == 0);
        m_CurrentHeapPtr = this->m_dynamicDescriptorHeapStatics.RequestDescriptorHeap(m_DescriptorType);
        m_FirstDescriptor = DescriptorHandle(
            m_CurrentHeapPtr->GetCPUDescriptorHandleForHeapStart(),
            m_CurrentHeapPtr->GetGPUDescriptorHandleForHeapStart());
    }

    return m_CurrentHeapPtr;
}

uint32_t DynamicDescriptorHeap::DescriptorHandleCache::ComputeStagedSize()
{
    // Sum the maximum assigned offsets of stale descriptor tables to determine total needed space.
    uint32_t NeededSpace = 0;
    uint32_t RootIndex;
    uint32_t StaleParams = m_StaleRootParamsBitMap;
    while (_BitScanForward((unsigned long*)&RootIndex, StaleParams))
    {
        StaleParams ^= (1 << RootIndex);

        uint32_t MaxSetHandle;
        ASSERT(TRUE == _BitScanReverse((unsigned long*)&MaxSetHandle, m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap),
            "Root entry marked as stale but has no stale descriptors");

        NeededSpace += MaxSetHandle + 1;
    }
    return NeededSpace;
}

void DynamicDescriptorHeap::DescriptorHandleCache::CopyAndBindStaleTables(ID3D12Device4* device,
    D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize,
    DescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList,
    void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::*SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
{
    uint32_t StaleParamCount = 0;
    uint32_t TableSize[DescriptorHandleCache::kMaxNumDescriptorTables];
    uint32_t RootIndices[DescriptorHandleCache::kMaxNumDescriptorTables];
    uint32_t NeededSpace = 0;
    uint32_t RootIndex;

    // Sum the maximum assigned offsets of stale descriptor tables to determine total needed space.
    uint32_t StaleParams = m_StaleRootParamsBitMap;
    while (_BitScanForward((unsigned long*)&RootIndex, StaleParams))
    {
        RootIndices[StaleParamCount] = RootIndex;
        StaleParams ^= (1 << RootIndex);

        uint32_t MaxSetHandle;
        ASSERT(TRUE == _BitScanReverse((unsigned long*)&MaxSetHandle, m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap),
            "Root entry marked as stale but has no stale descriptors");

        NeededSpace += MaxSetHandle + 1;
        TableSize[StaleParamCount] = MaxSetHandle + 1;

        ++StaleParamCount;
    }

    ASSERT(StaleParamCount <= DescriptorHandleCache::kMaxNumDescriptorTables,
        "We're only equipped to handle so many descriptor tables");

    m_StaleRootParamsBitMap = 0;

    static const uint32_t kMaxDescriptorsPerCopy = 16;
    UINT NumDestDescriptorRanges = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[kMaxDescriptorsPerCopy];
    UINT pDestDescriptorRangeSizes[kMaxDescriptorsPerCopy];

    UINT NumSrcDescriptorRanges = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE pSrcDescriptorRangeStarts[kMaxDescriptorsPerCopy];
    UINT pSrcDescriptorRangeSizes[kMaxDescriptorsPerCopy];

    for (uint32_t i = 0; i < StaleParamCount; ++i)
    {
        RootIndex = RootIndices[i];
        (CmdList->*SetFunc)(RootIndex, DestHandleStart.GetGpuHandle());

        DescriptorTableCache& RootDescTable = m_RootDescriptorTable[RootIndex];

        D3D12_CPU_DESCRIPTOR_HANDLE* SrcHandles = RootDescTable.TableStart;
        uint64_t SetHandles = (uint64_t)RootDescTable.AssignedHandlesBitMap;
        D3D12_CPU_DESCRIPTOR_HANDLE CurDest = DestHandleStart.GetCpuHandle();
        DestHandleStart += TableSize[i] * DescriptorSize;

        unsigned long SkipCount;
#ifdef _M_AMD64
        while (_BitScanForward64(&SkipCount, SetHandles))
#else
        auto _BitScanForward64 = [](unsigned long* _Index, unsigned __int64 _Mask)
        {
            if (!_BitScanForward(_Index, (uint32_t)_Mask) &&
                _BitScanForward(_Index, (uint32_t)(_Mask >> 32))) {
                _Index += 32;
            }
            return _Index != 0;
        };
        while (_BitScanForward64(&SkipCount, SetHandles))
#endif
        {
            // Skip over unset descriptor handles
            SetHandles >>= SkipCount;
            SrcHandles += SkipCount;
            CurDest.ptr += SkipCount * DescriptorSize;

            unsigned long DescriptorCount;
            _BitScanForward64(&DescriptorCount, ~SetHandles);
            SetHandles >>= DescriptorCount;

            // If we run out of temp room, copy what we've got so far
            if (NumSrcDescriptorRanges + DescriptorCount > kMaxDescriptorsPerCopy)
            {
                device->CopyDescriptors(
                    NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
                    NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
                    Type);

                NumSrcDescriptorRanges = 0;
                NumDestDescriptorRanges = 0;
            }

            // Setup destination range
            pDestDescriptorRangeStarts[NumDestDescriptorRanges] = CurDest;
            pDestDescriptorRangeSizes[NumDestDescriptorRanges] = DescriptorCount;
            ++NumDestDescriptorRanges;

            // Setup source ranges (one descriptor each because we don't assume they are contiguous)
            for (uint32_t j = 0; j < DescriptorCount; ++j)
            {
                pSrcDescriptorRangeStarts[NumSrcDescriptorRanges] = SrcHandles[j];
                pSrcDescriptorRangeSizes[NumSrcDescriptorRanges] = 1;
                ++NumSrcDescriptorRanges;
            }

            // Move the destination pointer forward by the number of descriptors we will copy
            SrcHandles += DescriptorCount;
            CurDest.ptr += DescriptorCount * DescriptorSize;
        }
    }

    device->CopyDescriptors(
        NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
        NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
        Type);
}
    
void DynamicDescriptorHeap::CopyAndBindStagedTables( 
    GraphicsCore &core,
    DescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CmdList,
    void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::*SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
{
    uint32_t NeededSize = HandleCache.ComputeStagedSize();
    if (!HasSpace(NeededSize))
    {
        RetireCurrentHeap();
        UnbindAllValid();
        NeededSize = HandleCache.ComputeStagedSize();
    }

    // This can trigger the creation of a new heap
    m_OwningContext.SetDescriptorHeap(m_DescriptorType, GetHeapPointer());
    HandleCache.CopyAndBindStaleTables(core.m_pDevice, m_DescriptorType, m_DescriptorSize, Allocate(NeededSize), CmdList, SetFunc);
}

void DynamicDescriptorHeap::UnbindAllValid( void )
{
    m_GraphicsHandleCache.UnbindAllValid();
    m_ComputeHandleCache.UnbindAllValid();
}

D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::UploadDirect( D3D12_CPU_DESCRIPTOR_HANDLE Handle )
{
    if (!HasSpace(1))
    {
        RetireCurrentHeap();
        UnbindAllValid();
    }

    m_OwningContext.SetDescriptorHeap(m_DescriptorType, GetHeapPointer());

    DescriptorHandle DestHandle = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
    m_CurrentOffset += 1;

    this->m_dynamicDescriptorHeapStatics.m_core.m_pDevice->CopyDescriptorsSimple(1, DestHandle.GetCpuHandle(), Handle, m_DescriptorType);

    return DestHandle.GetGpuHandle();
}

void DynamicDescriptorHeap::DescriptorHandleCache::UnbindAllValid()
{
    m_StaleRootParamsBitMap = 0;

    unsigned long TableParams = m_RootDescriptorTablesBitMap;
    unsigned long RootIndex;
    while (_BitScanForward(&RootIndex, TableParams))
    {
        TableParams ^= (1 << RootIndex);
        if (m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap != 0)
            m_StaleRootParamsBitMap |= (1 << RootIndex);
    }
}

void DynamicDescriptorHeap::DescriptorHandleCache::StageDescriptorHandles( UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[] )
{
    ASSERT(((1 << RootIndex) & m_RootDescriptorTablesBitMap) != 0, "Root parameter is not a CBV_SRV_UAV descriptor table");
    ASSERT(Offset + NumHandles <= m_RootDescriptorTable[RootIndex].TableSize);

    DescriptorTableCache& TableCache = m_RootDescriptorTable[RootIndex];
    D3D12_CPU_DESCRIPTOR_HANDLE* CopyDest = TableCache.TableStart + Offset;
    for (UINT i = 0; i < NumHandles; ++i)
        CopyDest[i] = Handles[i];
    TableCache.AssignedHandlesBitMap |= ((1 << NumHandles) - 1) << Offset;
    m_StaleRootParamsBitMap |= (1 << RootIndex);
}

void DynamicDescriptorHeap::DescriptorHandleCache::ParseRootSignature( D3D12_DESCRIPTOR_HEAP_TYPE Type, const RootSignature& RootSig )
{
    UINT CurrentOffset = 0;

    ASSERT(RootSig.m_NumParameters <= 16, "Maybe we need to support something greater");

    m_StaleRootParamsBitMap = 0;
    m_RootDescriptorTablesBitMap = (Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ?
        RootSig.m_SamplerTableBitMap : RootSig.m_DescriptorTableBitMap);

    unsigned long TableParams = m_RootDescriptorTablesBitMap;
    unsigned long RootIndex;
    while (_BitScanForward(&RootIndex, TableParams))
    {
        TableParams ^= (1 << RootIndex);

        UINT TableSize = RootSig.m_DescriptorTableSize[RootIndex];
        ASSERT(TableSize > 0);

        DescriptorTableCache& RootDescriptorTable = m_RootDescriptorTable[RootIndex];
        RootDescriptorTable.AssignedHandlesBitMap = 0;
        RootDescriptorTable.TableStart = m_HandleCache + CurrentOffset;
        RootDescriptorTable.TableSize = TableSize;

        CurrentOffset += TableSize;
    }

    m_MaxCachedDescriptors = CurrentOffset;

    ASSERT(m_MaxCachedDescriptors <= kMaxNumDescriptors, "Exceeded user-supplied maximum cache size");
}
