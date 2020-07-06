Texture2D<unorm float4> filterFrom : register(t0);
RWTexture2D<unorm float> luminanceTex : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float luminance = length(filterFrom[uint2(DTid.x, DTid.y)].xyz);
	uint2 writeAt = uint2(DTid.x, DTid.y);
	luminanceTex[writeAt] = luminance;
}