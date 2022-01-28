#include "Types.hlsli"

[maxvertexcount(4)]
void main(line DS_TO_GS points[2], inout TriangleStream<GS_TO_PS> output)
{
	float4 p0 = points[0].position;
	float4 p1 = points[1].position;

	float tangent0xWithoutAspect = points[0].tangent.x;
	float tangent1xWithoutAspect = points[1].tangent.x;

	float4 normal1 = float4(-points[0].tangent.y, tangent0xWithoutAspect, 0, 0);
	float4 normal2 = float4(-points[1].tangent.y, tangent1xWithoutAspect, 0, 0);

	normal1 = normalize(normal1);
	normal2 = normalize(normal2);

	float4 normalWidth1 = 1 * scaleVector * 0.25 / 2.0f * normal1;
	float4 normalWidth2 = 1 * scaleVector * 0.25 / 2.0f * normal2;

	// scale to correct window aspect ratio
	GS_TO_PS o;

	o.position.w = 1;

	float4 color1 = float4(0.5, 0.5, 0.0, 1.0);
	float4 color2 = color1;

	color1.r = points[0].mustBe5 / 6.0f; // 5 / 6 => 0,83333
	color2.r = points[0].mustBe5 / 6.0f;

	float3 p0L = (p0 + 1.5f * normalWidth1).xyz;
	float3 p0R = (p0 - 1.5f * normalWidth1).xyz;

	float3 p1L = (p1 + 1.5f * normalWidth2).xyz;
	float3 p1R = (p1 - 1.5f * normalWidth2).xyz;

	float4 p0Lc = float4(p0L, 0.0f);
	float4 p0Rc = float4(p0R, 1.0f);

	float4 p1Lc = float4(p1L, 0.0f);
	float4 p1Rc = float4(p1R, 1.0f);

	o.colorAndBrightness = color1;
	o.position.xyz = p0Lc.xyz;
	output.Append(o);

	o.colorAndBrightness = color2;
	o.position.xyz = p1Lc.xyz;
	output.Append(o);

	o.colorAndBrightness = color1;
	o.position.xyz = p0Rc.xyz;
	output.Append(o);

	o.colorAndBrightness = color2;
	o.position.xyz = p1Rc.xyz;
	output.Append(o);

	output.RestartStrip();
}

