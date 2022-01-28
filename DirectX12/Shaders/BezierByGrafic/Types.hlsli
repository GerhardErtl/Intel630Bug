#include "../ConstantBuffers.hlsli"

struct PrimitiveData
{
#include "Shared/PrimitiveData.hlsli"
};

StructuredBuffer<PrimitiveData> perPrimitiveFlags : register(t0);

struct VS_INPUT
{
    float4 position : SV_POSITION;
};

struct VS_TO_HS
{
    float4 position : SV_POSITION;
};

struct HS_CONSTANT_DATA_OUTPUT
{
    float4 unUsedFloat1        : F1;
    float4 unUsedFloat2        : F2;
    float2 unUsedFloat3        : F3;
    float tesselationFactor[2] : SV_TessFactor;
    uint mustBe5               : MUSTBE5;
};

struct HS_TO_DS
{
    float4 position : SV_POSITION;
};

struct DS_TO_GS
{
    float4 position : SV_POSITION; 
    float2 tangent : TANGENT;
    uint mustBe5 : MUSTBE5;
};

struct GS_TO_PS 
{
    linear float4 position             : SV_POSITION;
    linear float4 colorAndBrightness   : COLOR;
};

struct PS_OUTPUT
{
    float4 Color   : SV_Target0;
};