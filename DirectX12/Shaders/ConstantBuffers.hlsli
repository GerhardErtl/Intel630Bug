#include "Constants.hlsli"

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
cbuffer cbPerFrame : register(b0)
{
#include "SharedConstantBuffer.hlsli"
}
