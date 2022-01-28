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
#include "GpuTimeManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "CommandListManager.h"

struct ID3D12Device4;

class GpuTimeManager::Impl
{
public:
    ID3D12QueryHeap* m_pTimerQueryHeap;
    ID3D12Resource* m_pTimerReadBackBuffer;
    uint64_t* m_pTimeStampBuffer;
    uint64_t m_Fence;
    uint32_t m_MaxNumTimers;
    uint32_t m_NumTimers;
    uint64_t m_ValidTimeStart;
    uint64_t m_ValidTimeEnd;
    double m_GpuTickDelta;
    uint64_t m_GpuFrequency;

    // pipeline statistics
    ID3D12QueryHeap* m_pPipelineQueryHeap;
    ID3D12Resource* m_pPipelineQueryReadbackBuffer;
    D3D12_QUERY_DATA_PIPELINE_STATISTICS* m_pPipelineQueryDataBuffer;
    uint32_t m_MaxNumPipelineQueries;
    uint32_t m_NumPipelineQueries;

    GraphicsCore& m_core;

    Impl(GraphicsCore& core) : m_core(core)
    {
        m_pTimerQueryHeap = nullptr;
        m_pTimerReadBackBuffer = nullptr;
        m_pTimeStampBuffer = nullptr;
        m_Fence = 0;
        m_MaxNumTimers = 0;
        m_NumTimers = 0;
        m_ValidTimeStart = 0;
        m_ValidTimeEnd = 0;
        m_GpuTickDelta = 0.0;
        m_pPipelineQueryHeap = nullptr;
        m_pPipelineQueryReadbackBuffer = nullptr;
        m_pPipelineQueryDataBuffer = nullptr;
        m_MaxNumPipelineQueries = 0;
        m_NumPipelineQueries = 0;
    }
};

void GpuTimeManager::Initialize(GraphicsCore& core, uint32_t MaxNumTimers, uint32_t MaxNumPipelineQueries)
{
    this->pImpl = new Impl(core);

    this->pImpl->m_core.m_pCommandManager->GetCommandQueue()->GetTimestampFrequency(&this->pImpl->m_GpuFrequency);
    this->pImpl->m_GpuTickDelta = 1.0 / static_cast<double>(this->pImpl->m_GpuFrequency);

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask = 1;
    HeapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC TimerQueryBufferDesc;
    TimerQueryBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    TimerQueryBufferDesc.Alignment = 0;
    TimerQueryBufferDesc.Width = sizeof(uint64_t) * MaxNumTimers * 2;
    TimerQueryBufferDesc.Height = 1;
    TimerQueryBufferDesc.DepthOrArraySize = 1;
    TimerQueryBufferDesc.MipLevels = 1;
    TimerQueryBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    TimerQueryBufferDesc.SampleDesc.Count = 1;
    TimerQueryBufferDesc.SampleDesc.Quality = 0;
    TimerQueryBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    TimerQueryBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_RESOURCE_DESC PipelineQueryBufferDesc = TimerQueryBufferDesc;
    PipelineQueryBufferDesc.Width = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * MaxNumPipelineQueries;

    ASSERT_SUCCEEDED(pImpl->m_core.m_pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &TimerQueryBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, MY_IID_PPV_ARGS(&this->pImpl->m_pTimerReadBackBuffer)));
    this->pImpl->m_pTimerReadBackBuffer->SetName(L"GpuTimeStamp Buffer");

    D3D12_QUERY_HEAP_DESC QueryHeapDesc;
    QueryHeapDesc.Count = MaxNumTimers * 2;
    QueryHeapDesc.NodeMask = 1;
    QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    ASSERT_SUCCEEDED(pImpl->m_core.m_pDevice->CreateQueryHeap(&QueryHeapDesc, MY_IID_PPV_ARGS(&this->pImpl->m_pTimerQueryHeap)));
    this->pImpl->m_pTimerQueryHeap->SetName(L"GpuTimeStamp QueryHeap");

    ASSERT_SUCCEEDED(pImpl->m_core.m_pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &PipelineQueryBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, MY_IID_PPV_ARGS(&this->pImpl->m_pPipelineQueryReadbackBuffer)));
    this->pImpl->m_pPipelineQueryReadbackBuffer->SetName(L"Pipeline Query Buffer");

    D3D12_QUERY_HEAP_DESC PipelineQueryHeapDesc = {};
    PipelineQueryHeapDesc.Count = MaxNumPipelineQueries;
    //QueryHeapDesc.NodeMask = 0;
    PipelineQueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
    ASSERT_SUCCEEDED(pImpl->m_core.m_pDevice->CreateQueryHeap(&PipelineQueryHeapDesc, MY_IID_PPV_ARGS(&this->pImpl->m_pPipelineQueryHeap)));
    this->pImpl->m_pPipelineQueryHeap->SetName(L"Pipeline QueryHeap");

    this->pImpl->m_MaxNumTimers = (uint32_t)MaxNumTimers;
    this->pImpl->m_MaxNumPipelineQueries = (uint32_t)MaxNumPipelineQueries;
}

void GpuTimeManager::Shutdown()
{
    if (this->pImpl->m_pTimerReadBackBuffer != nullptr)
        this->pImpl->m_pTimerReadBackBuffer->Release();

    if (this->pImpl->m_pTimerQueryHeap != nullptr)
        this->pImpl->m_pTimerQueryHeap->Release();

    if (this->pImpl->m_pPipelineQueryReadbackBuffer != nullptr)
        this->pImpl->m_pPipelineQueryReadbackBuffer->Release();

    if (this->pImpl->m_pPipelineQueryHeap != nullptr)
        this->pImpl->m_pPipelineQueryHeap->Release();

    delete this->pImpl;
    this->pImpl = nullptr;
}

uint32_t GpuTimeManager::NewTimer(void)
{
    if (this->pImpl->m_NumTimers == this->pImpl->m_MaxNumTimers)
    {
        throw L"NewPipelineQuery max num timers";
    }

    return this->pImpl->m_NumTimers++;
}

uint32_t GpuTimeManager::NewPipelineQuery()
{
    if(this->pImpl->m_NumPipelineQueries == this->pImpl->m_MaxNumPipelineQueries)
    {
        throw L"NewPipelineQuery max num pipelines";
    }

    return this->pImpl->m_NumPipelineQueries++;
}

void GpuTimeManager::StartTimer(CommandContext& Context, uint32_t TimerIdx)
{
    Context.InsertTimeStamp(this->pImpl->m_pTimerQueryHeap, TimerIdx * 2);
}

void GpuTimeManager::StopTimer(CommandContext& Context, uint32_t TimerIdx)
{
    Context.InsertTimeStamp(this->pImpl->m_pTimerQueryHeap, TimerIdx * 2 + 1);
}

void GpuTimeManager::BeginPipelineQuery(CommandContext& Context, uint32_t QueryIdx) {
    Context.BeginPipelineQuery(this->pImpl->m_pPipelineQueryHeap, QueryIdx);
}

void GpuTimeManager::EndPipelineQuery(CommandContext& Context, uint32_t QueryIdx) {
    Context.EndPipelineQuery(this->pImpl->m_pPipelineQueryHeap, QueryIdx);
}

void GpuTimeManager::BeginReadBack(void)
{
    pImpl->m_core.m_pCommandManager->WaitForFence(this->pImpl->m_Fence);

    D3D12_RANGE Range;
    Range.Begin = 0;
    Range.End = (this->pImpl->m_NumTimers * 2) * sizeof(uint64_t);
    ASSERT_SUCCEEDED(this->pImpl->m_pTimerReadBackBuffer->Map(0, &Range, reinterpret_cast<void**>(&this->pImpl->m_pTimeStampBuffer)));

    D3D12_RANGE PipelineRange;
    PipelineRange.Begin = 0;
    PipelineRange.End = this->pImpl->m_NumPipelineQueries * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
    ASSERT_SUCCEEDED(this->pImpl->m_pPipelineQueryReadbackBuffer->Map(0, &PipelineRange, reinterpret_cast<void**>(&this->pImpl->m_pPipelineQueryDataBuffer)));

    this->pImpl->m_ValidTimeStart = this->pImpl->m_pTimeStampBuffer[0];
    this->pImpl->m_ValidTimeEnd = this->pImpl->m_pTimeStampBuffer[1];

    // On the first frame, with random values in the timestamp query heap, we can avoid a misstart.
    if (this->pImpl->m_ValidTimeEnd < this->pImpl->m_ValidTimeStart)
    {
        this->pImpl->m_ValidTimeStart = 0ull;
        this->pImpl->m_ValidTimeEnd = 0ull;
    }
}

void GpuTimeManager::EndReadBack(void)
{
    // Unmap with an empty range to indicate nothing was written by the CPU
    D3D12_RANGE EmptyRange = {};
    this->pImpl->m_pTimerReadBackBuffer->Unmap(0, &EmptyRange);
    this->pImpl->m_pPipelineQueryReadbackBuffer->Unmap(0, &EmptyRange);
    this->pImpl->m_pTimeStampBuffer = nullptr;
    this->pImpl->m_pPipelineQueryDataBuffer = nullptr;

    CommandContext& Context = CommandContext::Begin(L"EndReadBack", pImpl->m_core);
    Context.InsertTimeStamp(this->pImpl->m_pTimerQueryHeap, 1);
    Context.ResolveTimeStamps(this->pImpl->m_pTimerReadBackBuffer, this->pImpl->m_pTimerQueryHeap, this->pImpl->m_NumTimers * 2);
    Context.ResolvePipelineQueries(this->pImpl->m_pPipelineQueryReadbackBuffer, this->pImpl->m_pPipelineQueryHeap, this->pImpl->m_NumPipelineQueries);
    Context.InsertTimeStamp(this->pImpl->m_pTimerQueryHeap, 0);
    this->pImpl->m_Fence = Context.Finish();
}

double GpuTimeManager::GetTime(uint32_t TimerIdx)
{
    ASSERT(this->pImpl->m_pTimeStampBuffer != nullptr, "Time stamp readback buffer is not mapped");
    ASSERT(TimerIdx < this->pImpl->m_NumTimers, "Invalid GPU timer index");

    uint64_t TimeStamp1 = this->pImpl->m_pTimeStampBuffer[TimerIdx * 2];
    uint64_t TimeStamp2 = this->pImpl->m_pTimeStampBuffer[TimerIdx * 2 + 1];

    if (TimeStamp1 < this->pImpl->m_ValidTimeStart || TimeStamp2 > this->pImpl->m_ValidTimeEnd || TimeStamp2 <= TimeStamp1)
        return 0.0f;

    return static_cast<double>(this->pImpl->m_GpuTickDelta * (TimeStamp2 - TimeStamp1));
}

std::chrono::high_resolution_clock::duration GpuTimeManager::GetDuration(uint32_t TimerIdx)
{
    ASSERT(this->pImpl->m_pTimeStampBuffer != nullptr, "Time stamp readback buffer is not mapped");
    ASSERT(TimerIdx < this->pImpl->m_NumTimers, "Invalid GPU timer index");

    std::chrono::seconds sec(0);

    uint64_t TimeStamp1 = this->pImpl->m_pTimeStampBuffer[TimerIdx * 2];
    uint64_t TimeStamp2 = this->pImpl->m_pTimeStampBuffer[TimerIdx * 2 + 1];

    if (TimeStamp1 < this->pImpl->m_ValidTimeStart || TimeStamp2 > this->pImpl->m_ValidTimeEnd || TimeStamp2 <= TimeStamp1)
    {
        return sec;
    }

    double dt = static_cast<double>(TimeStamp2 - TimeStamp1);
    dt = dt * 1000000000.0 / this->pImpl->m_GpuFrequency;
    return std::chrono::nanoseconds(TimeStamp2 - TimeStamp1);
}

void GpuTimeManager::GetPipelineStatistics(uint32_t QueryIdx, D3D12_QUERY_DATA_PIPELINE_STATISTICS& stats) {
    ASSERT(this->pImpl->m_pPipelineQueryDataBuffer != nullptr, "Pipeline query readback buffer is not mapped");
    ASSERT(QueryIdx < this->pImpl->m_NumPipelineQueries, "Invalid pipeline query index");

    stats = this->pImpl->m_pPipelineQueryDataBuffer[QueryIdx];
}