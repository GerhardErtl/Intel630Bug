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

#include "PixelBuffer.h"
#include "Color.h"

class EsramAllocator;
class CommandContext;

class ColorBuffer : public PixelBuffer
{
public:
    ColorBuffer(GraphicsCore& core, DXGI_FORMAT format, Color ClearColor = Color(0.0f, 0.0f, 0.0f, 0.0f)  )
        : PixelBuffer(core, format), m_ClearColor(ClearColor), m_NumMipMaps(0), m_FragmentCount(1), m_SampleCount(1)
    {
        m_SRVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        m_RTVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        std::memset(m_UAVHandle, 0xFF, sizeof(m_UAVHandle));
    }

    // Create a color buffer from a swap chain buffer.  Unordered access is restricted.
    void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource );

    // Create a color buffer.  If an address is supplied, memory will not be allocated.
    // The vmem address allows you to alias buffers
    void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
                D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN, const void* InitialData = nullptr, bool createUavAlways = false);
    void CreateShared(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
                      D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

    // Get pre-created CPU-visible descriptor handles
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRVHandle; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const { return m_RTVHandle; }
    const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV(void) const { return m_UAVHandle[0]; }

    void SetClearColor( Color ClearColor ) { m_ClearColor = ClearColor; }

    void SetMsaaMode( uint32_t NumColorSamples, uint32_t NumCoverageSamples )
    {
        ASSERT(NumCoverageSamples >= NumColorSamples);
        this->m_FragmentCount = NumColorSamples;
        this->m_SampleCount = NumCoverageSamples;
    }

    Color GetClearColor(void) const { return m_ClearColor; }

    int GetHighestMipMapLevel() const
    {
        return static_cast<int>(this->m_NumMipMaps);
    }

    // This will work for all texture sizes, but it's recommended for speed and quality
    // that you use dimensions with powers of two (but not necessarily square.)  Pass
    // 0 for ArrayCount to reserve space for mips at creation time.
    void GenerateMipMaps(CommandContext& Context);

protected:

    D3D12_RESOURCE_FLAGS CombineResourceFlags( bool createUavAlways = false ) const
    {
        D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;

        if (m_FragmentCount == 1 && (m_Format != DXGI_FORMAT_B8G8R8A8_UNORM || createUavAlways))
            Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Flags;
    }

    // Compute the number of texture levels needed to reduce to 1x1.  This uses
    // _BitScanReverse to find the highest set bit.  Each dimension reduces by
    // half and truncates bits.  The dimension 256 (0x100) has 9 mip levels, same
    // as the dimension 511 (0x1FF).
    static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
    {
        uint32_t HighBit;
        _BitScanReverse((unsigned long*)&HighBit, Width | Height);
        return HighBit + 1;
    }

    void CreateDerivedViews(DXGI_FORMAT Format, uint32_t ArraySize, D3D12_RESOURCE_FLAGS flags, uint32_t NumMips = 1);

    Color m_ClearColor;
    D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_UAVHandle[12];
    uint32_t m_NumMipMaps; // number of texture sublevels
    uint32_t m_FragmentCount;
    uint32_t m_SampleCount;
};
