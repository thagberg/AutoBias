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
}