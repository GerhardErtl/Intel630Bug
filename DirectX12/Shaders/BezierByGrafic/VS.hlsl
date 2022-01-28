#include "Types.hlsli"

VS_TO_HS main(VS_INPUT Input)
{
    VS_TO_HS Output;

    Output.position.w = Input.position.w;
    Input.position.w = 1.0f;
    Output.position.xyz = mul(cViewProjection, Input.position).xyz;

    return Output;
}