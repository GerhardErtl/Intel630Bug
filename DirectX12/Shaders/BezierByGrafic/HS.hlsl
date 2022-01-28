#include "Types.hlsli"

HS_CONSTANT_DATA_OUTPUT BezierConstantHS(InputPatch<VS_TO_HS, 4> ip,
    uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT Output;

    Output.tesselationFactor[0] = 1.0f;
    Output.tesselationFactor[1] = cTessellationFactor;

    PrimitiveData primitiveData = perPrimitiveFlags[PatchID];
    Output.mustBe5 = 5;

    Output.unUsedFloat1 =  primitiveData.unUsedFloats1;
    Output.unUsedFloat2 =  primitiveData.unUsedFloats2;

    Output.unUsedFloat3.x = 0;
    Output.unUsedFloat3.y = 0;

    return Output;
}

[domain("isoline")]
[partitioning("integer")]
[outputtopology("line")]
[outputcontrolpoints(4)]
[patchconstantfunc("BezierConstantHS")]
HS_TO_DS main(InputPatch<VS_TO_HS, 4> p,
    uint i : SV_OutputControlPointID,
    uint PatchID : SV_PrimitiveID)
{
    HS_TO_DS output;

    output.position = p[i].position;

    return output;
}

