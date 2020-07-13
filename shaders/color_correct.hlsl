Texture3D<unorm float4> colorLUT : register(t0);
RWTexture2D<unorm float4> corrected : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float4 currentColor = corrected.Load(uint2(DTid.x, DTid.y));
	float4 correctedColor = colorLUT[uint3(currentColor.rgb * 255)];
	corrected[uint2(DTid.x, DTid.y)] = correctedColor;
}