Texture2D<unorm float4> filterFrom : register(t0);
RWTexture2D<unorm float4> filterTo : register(u0);

cbuffer DispatchDimensions : register(b0)
{
	uint dispatchX;
	uint dispatchY;
};

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID )
{
	float4 threadColor = float4(normalize(float3(DTid.x, DTid.y, DTid.z)), 1);

	uint pixelsInNeighborhood = 0;
	float4 neighborhood = float4(0, 0, 0, 0);
	for (uint i = 0; i < 8; ++i)
	{
		for (uint j = 0; j < 8; ++j)
		{
			uint2 lookup = uint2((Gid.x * 64) + (GTid.x * 8) + i, (Gid.y * 64) + (GTid.y * 8) + j);
			neighborhood += filterFrom[lookup];
			++pixelsInNeighborhood;
		}
	}

	neighborhood = neighborhood / pixelsInNeighborhood;

	uint2 writeAt = uint2(Gid.x * 8 + GTid.x, Gid.y * 8 + GTid.y);
	filterTo[writeAt] = neighborhood;
}