#pragma once

#include "Engine/GpuBuffer.h"
#include "Engine/GraphicsCore.h"

using Microsoft::WRL::ComPtr;

class VertexBuffer {
public:

    VertexBuffer() = delete;
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&&) = delete;

    template<class T>
    VertexBuffer(GraphicsCore& core, std::wstring name, const std::vector<T>& vertexInput) :
        buffer(core)
    {
        if (core.m_pDevice == nullptr)
        {
            throw L"No Device";
        }

        const auto numberOfVertices = static_cast<int>(vertexInput.size());
        const UINT64 vertexBufferSize = numberOfVertices * sizeof(vertexInput[0]);

        D3D12_RESOURCE_ALLOCATION_INFO allocInfo;
        allocInfo.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        allocInfo.SizeInBytes = vertexBufferSize;

        auto vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(allocInfo, D3D12_RESOURCE_FLAG_NONE);
        D3D12_RESOURCE_ALLOCATION_INFO possibleAllocInfo = core.m_pDevice->GetResourceAllocationInfo(
            0, 1, &vertexBufferDesc);

        if (possibleAllocInfo.Alignment != D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
        {
            // If the alignment requested is not granted, then let D3D tell us
            // the alignment that needs to be used for these resources.
            vertexBufferDesc.Alignment = 0;
            possibleAllocInfo = core.m_pDevice->GetResourceAllocationInfo(0, 1, &vertexBufferDesc);
        }

        const int vbCount = 1;
        const UINT64 heapSize = vbCount * possibleAllocInfo.SizeInBytes;
        CD3DX12_HEAP_DESC heapDesc(heapSize, D3D12_HEAP_TYPE_DEFAULT, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);

        HRESULT hr = core.m_pDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap));
        if (hr != 0)
        {
            throw L"Create Heap failed";
        }

        this->buffer.CreatePlaced(name.c_str(), heap.Get(), 0, numberOfVertices, sizeof(vertexInput[0]), &vertexInput[0]);

        this->view = buffer.VertexBufferView();
    }

    ~VertexBuffer()
    {
        buffer.Destroy();
        heap = nullptr; // Only used when the resources being drawn are placed resources.
    }

    D3D12_VERTEX_BUFFER_VIEW& GetView()
    {
        return view;
    }

    INT64 GetNumElements() const
    {
        return view.SizeInBytes / view.StrideInBytes;
    }

    StructuredBuffer& GetInternalBuffer()
    {
        return this->buffer;
    }

private:
    D3D12_VERTEX_BUFFER_VIEW view;
    ComPtr<ID3D12Heap> heap;
    StructuredBuffer buffer;
};

