Texture2D tx : register(t0);
SamplerState sampleLinear : register(s0);

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	//float4 color = tx.SampleLevel(sampleLinear, input.Tex, 20);
	float4 color = tx.Sample(sampleLinear, input.Tex);
	return color;
}