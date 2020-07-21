// roughly based on https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli

RWTexture2D<float4> OutMip1 : register(u0);
RWTexture2D<float4> OutMip2 : register(u1);
RWTexture2D<float4> OutMip3 : register(u2);
RWTexture2D<float4> OutMip4 : register(u3);
Texture2D<float4> SrcMip : register(t0);
SamplerState Bilinear : register(s0);

cbuffer Constants : register(b0)
{
	uint SrcMipLevel;
	uint NumMipLevels;
	float2 TexelSize;
	uint SrcDimensions;
}

groupshared float gs_R[64];
groupshared float gs_G[64];
groupshared float gs_B[64];
groupshared float gs_A[64];

void StoreColor(uint index, float4 color)
{
	gs_R[index] = color.r;
	gs_G[index] = color.g;
	gs_B[index] = color.b;
	gs_A[index] = color.a;
}

float4 LoadColor(uint index)
{
	return float4(gs_R[index], gs_G[index], gs_B[index], gs_A[index]);
}

[numthreads(8, 8, 1)]
void main( uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID )
{
	float4 Src1;

	switch (SrcDimensions)
	{
	case 0: // X and Y even
	{
		// Only need  one bilinear sample. Offset by 0.5 to put
		// the sample location directly at the corner of 4 samples
		// for equal bilinear weights
		float2 UV = TexelSize * (DTid.xy + 0.5);
		Src1 = SrcMip.SampleLevel(Bilinear, UV, SrcMipLevel);
	}
	break;
	case 1: // X odd
	{
		// 2 samples on X, 1 sample on Y
		// Offset X by an additional 0.25 in both directions to cover
		// additional adjacent texels
		float2 UV1 = TexelSize * (DTid.xy + float2(0.25, 0.5));
		float2 offset = TexelSize * float2(0.5, 0.0);
		// Average the two samples
		Src1 = 0.5 * (SrcMip.SampleLevel(Bilinear, UV1, SrcMipLevel) +
			SrcMip.SampleLevel(Bilinear, UV1 + offset, SrcMipLevel));
	}
	break;
	case 2: // Y odd
	{
		// 1 sample on X, 2 samples on Y
		// Logic is the same as the previous case, but flipped axes
		float2 UV1 = TexelSize * (DTid.xy + float2(0.5, 0.25));
		float2 offset = TexelSize * float2(0.0, 0.5);
		Src1 = 0.5 * (SrcMip.SampleLevel(Bilinear, UV1, SrcMipLevel) +
			SrcMip.SampleLevel(Bilinear, UV1 + offset, SrcMipLevel));
	}
	break;
	case 3: // X and Y odd
	{
		// 2 samples in both directions
		float2 UV1 = TexelSize * (DTid.xy + float2(0.25, 0.25));
		float2 offset = TexelSize * 0.5;
		Src1 = SrcMip.SampleLevel(Bilinear, UV1, SrcMipLevel);
		Src1 += SrcMip.SampleLevel(Bilinear, UV1 + float2(offset.x, 0.0), SrcMipLevel);
		Src1 += SrcMip.SampleLevel(Bilinear, UV1 + float2(0.0, offset.y), SrcMipLevel);
		Src1 += SrcMip.SampleLevel(Bilinear, UV1 + offset, SrcMipLevel);
		Src1 *= 0.25;
	}
	break;
	}

	// Write the color out to the first mip UAV
	OutMip1[DTid.xy] = Src1;

	if (NumMipLevels == 1)
	{
		return;
	}

	StoreColor(GI, Src1);

	GroupMemoryBarrierWithGroupSync();

	// only need 1/4 of the threads for the second mip level (even group threads only)
	// GI.x and GI.y must both be even
	if ((GI & 0x9) == 0)
	{
		float4 Src2 = LoadColor(GI + 0x01);
		float4 Src3 = LoadColor(GI + 0x08);
		float4 Src4 = LoadColor(GI + 0x09);
		Src1 = 0.25 * (Src1 + Src2 + Src3 + Src4);

		OutMip2[DTid.xy / 2] = Src1;
		StoreColor(GI, Src1);
	}

	if (NumMipLevels == 2)
	{
		return;
	}

	GroupMemoryBarrierWithGroupSync();

	// only need 1/4 again. GI.x and GI.y must be multiples of 4
	if ((GI & 0x1B) == 0)
	{
		float4 Src2 = LoadColor(GI + 0x02);
		float4 Src3 = LoadColor(GI + 0x10);
		float4 Src4 = LoadColor(GI + 0x12);
		Src1 = 0.25 * (Src1 + Src2 + Src3 + Src4);

		OutMip3[DTid.xy / 4] = Src1;
		StoreColor(GI, Src1);
	}

	if (NumMipLevels == 3)
	{
		return;
	}

	GroupMemoryBarrierWithGroupSync();

	// only 1 thread for the final mip
	if (GI == 0)
	{
		float4 Src2 = LoadColor(GI + 0x04);
		float4 Src3 = LoadColor(GI + 0x20);
		float4 Src4 = LoadColor(GI + 0x24);
		Src1 = 0.25 * (Src1 + Src2 + Src3 + Src4);

		OutMip4[DTid.xy / 8] = Src1;
	}

}