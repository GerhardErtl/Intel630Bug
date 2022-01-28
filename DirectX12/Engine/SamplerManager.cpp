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
// Author(s):  James Stanard
//             Alex Nankervis
//

#include "pchDirectX.h"
#include "SamplerManager.h"
#include "GraphicsCore.h"
#include "Hash.h"
#include <map>

using namespace std;

D3D12_CPU_DESCRIPTOR_HANDLE SamplerDesc::CreateDescriptor(GraphicsCore& core)
{
    size_t hashValue = Utility::HashState(this);
    auto iter = core.m_SamplerCache.find(hashValue);
    if (iter != core.m_SamplerCache.end())
    {
        return iter->second;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Handle = core.AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    core.m_pDevice->CreateSampler(this, Handle);
    return Handle;
}
