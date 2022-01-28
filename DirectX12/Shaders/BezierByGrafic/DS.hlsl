#include "Types.hlsli"

void CubicBezierBasis(float u, out float4 b, out float4 d)
{
    float t = u;          
    float s = 1.0 - u;    
    float a0 = s * s;     
    float a1 = 2 * s * t; 
    float a2 = t * t;     

    b.x = s * a0;         
    b.y = t * a0 + s * a1;
    b.z = t * a1 + s * a2;
    b.w = t * a2;         

    d.x = -a0;            
    d.y = a0 - a1;        
    d.z = a1 - a2;        
    d.w = a2;             
}

[domain("isoline")]
DS_TO_GS main(
    HS_CONSTANT_DATA_OUTPUT input, 
    OutputPatch<HS_TO_DS, 4> op, 
    float2 uv : SV_DomainLocation)
{
    DS_TO_GS output;

    float t = uv.x;

    float4 b;
    float4 d;
    CubicBezierBasis(t, b, d);

    output.position = 
        b.x * op[0].position +
        b.y * op[1].position +
        b.z * op[2].position +
        b.w * op[3].position;

    output.tangent = normalize(
            d.x * op[0].position.xy +
            d.y * op[1].position.xy +
            d.z * op[2].position.xy +
            d.w * op[3].position.xy);

    output.mustBe5 = input.mustBe5;

    return output;
}
