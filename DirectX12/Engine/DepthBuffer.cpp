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
#include "DepthBuffer.h"
#include "GraphicsCore.h"
#include "DescriptorHeap.h"

void DepthBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t Samples, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
{
    if (this->m_Format == DXGI_FORMAT_UNKNOWN)
    {
        throw  L"You can not create a ColorBuffer without a format, give me a valid format in the Constructor";
    }

    D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, 1, this->m_Format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ResourceDesc.SampleDesc.Count = Samples;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = this->m_Format;
    this->CreateTextureResource(Name, ResourceDesc, ClearValue, VidMemPtr);
    this->CreateDerivedViews();
}

void DepthBuffer::CreateDerivedViews()
{
    if (this->m_Format == DXGI_FORMAT_UNKNOWN)
    {
        throw  L"You can not create a ColorBuffer without a format, give me a valid format in the Constructor";
    }

    ID3D12Resource* Resource = m_pResource.Get();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format = GetDSVFormat(this->m_Format);
    if (Resource->GetDesc().SampleDesc.Count == 1)
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
    }
    else
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    }

    if (m_hDSV[0].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
    {
        m_hDSV[0] = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        m_hDSV[1] = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }

    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    this->m_core.m_pDevice->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[0]);

    dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    this->m_core.m_pDevice->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[1]);

    DXGI_FORMAT stencilReadFormat = GetStencilFormat(this->m_Format);
    if (stencilReadFormat != DXGI_FORMAT_UNKNOWN)
    {
        if (m_hDSV[2].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        {
            m_hDSV[2] = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            m_hDSV[3] = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        }

        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        this->m_core.m_pDevice->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[2]);

        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        this->m_core.m_pDevice->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV[3]);
    }
    else
    {
        m_hDSV[2] = m_hDSV[0];
        m_hDSV[3] = m_hDSV[1];
    }

    if (m_hDepthSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        m_hDepthSRV = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create the shader resource view
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = GetDepthFormat(this->m_Format);
    if (dsvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
    {
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = 1;
    }
    else
    {
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    }
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    this->m_core.m_pDevice->CreateShaderResourceView( Resource, &SRVDesc, m_hDepthSRV );

    if (stencilReadFormat != DXGI_FORMAT_UNKNOWN)
    {
        if (m_hStencilSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
            m_hStencilSRV = this->m_core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        SRVDesc.Format = stencilReadFormat;
        this->m_core.m_pDevice->CreateShaderResourceView( Resource, &SRVDesc, m_hStencilSRV );
    }
}