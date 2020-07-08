RWTexture3D<unorm float4> colorLUT : register(u0);

[numthreads(8, 8, 8)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float3 thisColor = float3(DTid.x/255.0, DTid.y/255.0, DTid.z/255.0);
	float colorMagnitude = length(thisColor);
	// 0.1697 is approximately the magnitude of R8G8B8_Unorm(25, 25, 25), below which LEDs won't light up
	if (colorMagnitude <= 0.1697)
	{
		// 0.098 is approximately the normalized 8-bit color value of 25
		float3 minColor = float3(0.098, 0.098, 0.098);
		thisColor = max(thisColor, minColor);
	}

	uint3 writeAt = uint3(DTid.x, DTid.y, DTid.z);
	colorLUT[writeAt] = float4(thisColor, 1.0);
}