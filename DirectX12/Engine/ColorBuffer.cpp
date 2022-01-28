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
#include "ColorBuffer.h"
#include "GraphicsCore.h"
#include "CommandContext.h"

void ColorBuffer::CreateDerivedViews(DXGI_FORMAT Format, uint32_t ArraySize, D3D12_RESOURCE_FLAGS flags, uint32_t NumMips)
{
    ASSERT(ArraySize == 1 || NumMips == 1, "We don't support auto-mips on texture arrays");

    this->m_NumMipMaps = NumMips - 1;

    D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

    RTVDesc.Format = Format;
    UAVDesc.Format = GetUAVFormat(Format);
    SRVDesc.Format = Format;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (ArraySize > 1)
    {
        RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        RTVDesc.Texture2DArray.MipSlice = 0;
        RTVDesc.Texture2DArray.FirstArraySlice = 0;
        RTVDesc.Texture2DArray.ArraySize = static_cast<UINT>(ArraySize);

        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        UAVDesc.Texture2DArray.MipSlice = 0;
        UAVDesc.Texture2DArray.FirstArraySlice = 0;
        UAVDesc.Texture2DArray.ArraySize = static_cast<UINT>(ArraySize);

        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        SRVDesc.Texture2DArray.MipLevels = NumMips;
        SRVDesc.Texture2DArray.MostDetailedMip = 0;
        SRVDesc.Texture2DArray.FirstArraySlice = 0;
        SRVDesc.Texture2DArray.ArraySize = static_cast<UINT>(ArraySize);
    }
    else if (this->m_FragmentCount > 1)
    {
        RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    }
    else 
    {
        RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        RTVDesc.Texture2D.MipSlice = 0;

        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        UAVDesc.Texture2D.MipSlice = 0;

        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = NumMips;
        SRVDesc.Texture2D.MostDetailedMip = 0;
    }

    if (this->m_SRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
    {
        m_RTVHandle = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_SRVHandle = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    ID3D12Resource* Resource = this->m_pResource.Get();

    // Create the render target view
    this->m_core.m_pDevice->CreateRenderTargetView(Resource, &RTVDesc, this->m_RTVHandle);

    // Create the shader resource view
    this->m_core.m_pDevice->CreateShaderResourceView(Resource, &SRVDesc, this->m_SRVHandle);

    if (this->m_FragmentCount > 1 || ((flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0))
    {
        return;
    }

    // Create the UAVs for each mip level (RWTexture2D)
    for (uint32_t i = 0; i < NumMips; ++i)
    {
        if (this->m_UAVHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        {
            this->m_UAVHandle[i] = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        this->m_core.m_pDevice->CreateUnorderedAccessView(Resource, nullptr, &UAVDesc, this->m_UAVHandle[i]);

        UAVDesc.Texture2D.MipSlice++;
    }
}

void ColorBuffer::CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource )
{
    this->AssociateWithResource(Name, BaseResource, D3D12_RESOURCE_STATE_PRESENT);

    //m_UAVHandle[0] = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    //Graphics::g_Device->CreateUnorderedAccessView(m_pResource.Get(), nullptr, nullptr, m_UAVHandle[0]);

    this->m_RTVHandle = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    this->m_core.m_pDevice->CreateRenderTargetView(this->m_pResource.Get(), nullptr, this->m_RTVHandle);
}

void ColorBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, D3D12_GPU_VIRTUAL_ADDRESS VidMem, const void* InitialData, bool createUavAlways)
{
    NumMips = (NumMips == 0 ? ComputeNumMips(Width, Height) : NumMips);

    NumMips = std::min(NumMips, static_cast<uint32_t>(std::size(this->m_UAVHandle)));

    if(this->m_Format == DXGI_FORMAT_UNKNOWN)
    {
        throw  L"You can not create a ColorBuffer without a format, give me a valid format in the Constructor";
    }

    D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags(createUavAlways);

    D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, NumMips, this->m_Format, Flags);

    ResourceDesc.SampleDesc.Count = m_FragmentCount;
    ResourceDesc.SampleDesc.Quality = 0;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = this->m_Format;
    ClearValue.Color[0] = m_ClearColor.R();
    ClearValue.Color[1] = m_ClearColor.G();
    ClearValue.Color[2] = m_ClearColor.B();
    ClearValue.Color[3] = m_ClearColor.A();

    this->CreateTextureResource(Name, ResourceDesc, ClearValue, VidMem);

    if (InitialData != nullptr)
    {
        D3D12_SUBRESOURCE_DATA texResource;
        texResource.pData = InitialData;
        texResource.RowPitch = Width * BytesPerPixel(this->m_Format);
        texResource.SlicePitch = texResource.RowPitch * Height;

        CommandContext::InitializeTexture(this->m_core, *this, 1, &texResource);
    }

    this->CreateDerivedViews(this->m_Format, 1, Flags, NumMips);
}

void ColorBuffer::CreateShared(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, D3D12_GPU_VIRTUAL_ADDRESS VidMem)
{
    NumMips = (NumMips == 0 ? ComputeNumMips(Width, Height) : NumMips);

    NumMips = std::min(NumMips, static_cast<uint32_t>(std::size(this->m_UAVHandle)));

    if (this->m_Format == DXGI_FORMAT_UNKNOWN)
    {
        throw  L"You can not create a ColorBuffer without a format, give me a valid format in the Constructor";
    }

    D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
    // Muck: Restore the original format and flags
    //Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; //  | D3D12_RESOURCE_FLAG_ALLOW_SHADER_RESOURCE;
    Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
    
    D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, NumMips, this->m_Format, Flags);

    ResourceDesc.SampleDesc.Count = m_FragmentCount;
    ResourceDesc.SampleDesc.Quality = 0;

    // Muck: Restore the original format and flags
    ResourceDesc.Format = this->m_Format;
    

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = this->m_Format;
    ClearValue.Color[0] = m_ClearColor.R();
    ClearValue.Color[1] = m_ClearColor.G();
    ClearValue.Color[2] = m_ClearColor.B();
    ClearValue.Color[3] = m_ClearColor.A();

    this->CreateSharedTextureResource(Name, ResourceDesc, ClearValue, VidMem);
    this->CreateDerivedViews(this->m_Format, 1, Flags, NumMips);
}

void ColorBuffer::GenerateMipMaps(CommandContext& BaseContext)
{
    if (m_NumMipMaps == 0)
        return;

    ComputeContext& Context = BaseContext.GetComputeContext();

    Context.SetRootSignature(this->m_core.m_GenerateMipsRS);

    Context.TransitionResource(*this, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.SetDynamicDescriptor(1, 0, this->m_SRVHandle);

    for (uint32_t TopMip = 0; TopMip < this->m_NumMipMaps; )
    {
        const uint32_t SrcWidth = this->m_Width >> TopMip;
        const uint32_t SrcHeight = this->m_Height >> TopMip;
        uint32_t DstWidth = SrcWidth >> 1;
        uint32_t DstHeight = SrcHeight >> 1;

        // Determine if the first downsample is more than 2:1.  This happens whenever
        // the source width or height is odd.
        const uint32_t NonPowerOfTwo = (SrcWidth & 1) | (SrcHeight & 1) << 1;
        if (this->m_Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
            Context.SetPipelineState(this->m_core.m_pGenerateMipsGammaPSOs[NonPowerOfTwo]);
        else
            Context.SetPipelineState(this->m_core.m_pGenerateMipsLinearPSOs[NonPowerOfTwo]);

        // We can downsample up to four times, but if the ratio between levels is not
        // exactly 2:1, we have to shift our blend weights, which gets complicated or
        // expensive.  Maybe we can update the code later to compute sample weights for
        // each successive downsample.  We use _BitScanForward to count number of zeros
        // in the low bits.  Zeros indicate we can divide by two without truncating.
        uint32_t AdditionalMips;
        _BitScanForward((unsigned long*)&AdditionalMips,
            (DstWidth == 1 ? DstHeight : DstWidth) | (DstHeight == 1 ? DstWidth : DstHeight));
        uint32_t NumMips = 1 + (AdditionalMips > 3 ? 3 : AdditionalMips);
        if (TopMip + NumMips > this->m_NumMipMaps)
        {
            NumMips = this->m_NumMipMaps - TopMip;
        }

        // These are clamped to 1 after computing additional mips because clamped
        // dimensions should not limit us from downsampling multiple times.  (E.g.
        // 16x1 -> 8x1 -> 4x1 -> 2x1 -> 1x1.)
        if (DstWidth == 0)
        {
            DstWidth = 1;
        }
        if (DstHeight == 0)
        {
            DstHeight = 1;
        }

        Context.SetConstants(0, TopMip, NumMips, 1.0f / DstWidth, 1.0f / DstHeight);
        Context.SetDynamicDescriptors(2, 0, NumMips, this->m_UAVHandle + TopMip + 1);
        Context.Dispatch2D(DstWidth, DstHeight);

        Context.InsertUAVBarrier(*this);

        TopMip += NumMips;
    }

    Context.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
