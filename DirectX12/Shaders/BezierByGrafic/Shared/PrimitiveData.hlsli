#ifdef __cplusplus
#define uintType UINT
#define matrixType Math::Matrix4
#define float4Type Math::Vector4
#define float3Type Math::Vector3
#define float2Type Math::Vector2
#define int2Type Math::IntVector2
#else
#define uintType uint
#define matrixType matrix
#define float4Type float4
#define float3Type float3
#define float2Type float2
#define int2Type int2
#endif

#ifdef __cplusplus
float unUsedFloats1[4];
float unUsedFloats2[4];
#else
float4 unUsedFloats1;
float4 unUsedFloats2;
#endif

float unUsedFloat;

uintType mustBe5;