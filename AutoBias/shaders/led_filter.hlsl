Texture2D<unorm float4> filterFrom : register(t0);
RWBuffer<unorm float4> ledBuffer : register(u0);

cbuffer ConsumeDimensions : register(b0)
{
	uint consumeX;
	uint consumeY;
};

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
	float4 ledColor = float4(0, 0, 0, 0);

	uint pixelsInNeighborhood = 0;
	float4 neighborhood = float4(0, 0, 0, 0);
	for (uint i = 0; i < consumeX; ++i)
	{
		for (uint j = 0; j < consumeY; ++j)
		{
			uint2 lookup = uint2((GTid.x * consumeX) + j, (GTid.y * consumeY) + i);
			neighborhood += filterFrom[lookup];
			++pixelsInNeighborhood;
		}
	}

	ledColor = neighborhood / pixelsInNeighborhood;

	uint2 writeAt = uint2();
}