#include "Types.hlsli"

PS_OUTPUT main(GS_TO_PS input)
{
	PS_OUTPUT o;
	o.Color = input.colorAndBrightness;
	return o;
 }
