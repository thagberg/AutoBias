Texture2D<unorm float4> SrcTex : register(t0);
Texture3D<unorm float4> CorrectionTex : register(t1);
RWBuffer<unorm float4> LEDOut : register(u0);
SamplerState Bilinear : register(s0);

cbuffer Constants : register(b0)
{
	uint3 DispatchSize;
	float2 TexelSize;
}

groupshared float gs_R[64];
groupshared float gs_G[64];
groupshared float gs_B[64];

void StoreColor(uint index, float4 color)
{
	gs_R[index] = color.r;
	gs_G[index] = color.g;
	gs_B[index] = color.b;
}

void StoreColor3(uint index, float3 color)
{
	gs_R[index] = color.r;
	gs_G[index] = color.g;
	gs_B[index] = color.b;
}

float3 LoadColor(uint index)
{
	return float3(gs_R[index], gs_G[index], gs_B[index]);
}

[numthreads(8, 8, 1)]
void main( uint2 GroupID : SV_GroupID, uint2 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID )
{
	float2 UV = TexelSize * (DTid.xy + 0.5);
	float4 sampleColor = SrcTex.SampleLevel(Bilinear, UV, 0);

	StoreColor(GI, sampleColor);

	GroupMemoryBarrierWithGroupSync();

	// "first" thread on each "line" of 8 will average the color for that line
	if (GTid.x == 0)
	{
		float3 LineColor = float3(0, 0, 0);
		LineColor += LoadColor(GI);
		LineColor += LoadColor(GI + 1);
		LineColor += LoadColor(GI + 2);
		LineColor += LoadColor(GI + 3);
		LineColor += LoadColor(GI + 4);
		LineColor += LoadColor(GI + 5);
		LineColor += LoadColor(GI + 6);
		LineColor += LoadColor(GI + 7);
		LineColor *= 0.125;
		StoreColor3(GI, LineColor);
	}

	GroupMemoryBarrierWithGroupSync();

	// only 1 thread per group will actually write out to the LED texture
	if (GI == 0)
	{
		float3 FinalColor = float3(0, 0, 0);
		FinalColor += LoadColor(GI);
		FinalColor += LoadColor(GI + 8);
		FinalColor += LoadColor(GI + (8 * 2));
		FinalColor += LoadColor(GI + (8 * 3));
		FinalColor += LoadColor(GI + (8 * 4));
		FinalColor += LoadColor(GI + (8 * 5));
		FinalColor += LoadColor(GI + (8 * 6));
		FinalColor += LoadColor(GI + (8 * 7));
		FinalColor *= 0.125;

		float3 CorrectionUV = FinalColor.rgb;
		float4 CorrectedColor = CorrectionTex.SampleLevel(Bilinear, CorrectionUV, 0);

		uint writeIndex = GroupID.y * DispatchSize.x + GroupID.x;
		LEDOut[writeIndex] = float4(CorrectedColor.rgb, 1.0);
	}
}