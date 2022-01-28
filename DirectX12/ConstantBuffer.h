#pragma once
#include <Math\Matrix4.h>

const int RootSignature_ConstantBuffer_Index = 0;
const int RootSignature_PrimitiveBuffer_Index = 1;

static const auto SizeX = 25.0f;
static const auto SizeY = 100.0f;

#pragma pack(push, 1)
struct ConstantBuffer
{
#include "DirectX12/Shaders/SharedConstantBuffer.hlsli"

    ConstantBuffer() :
       cViewProjection(Math::Matrix4(Math::kIdentity)),
       cTessellationFactor(20.0f)
    {
    }
};
#pragma pack(pop)
