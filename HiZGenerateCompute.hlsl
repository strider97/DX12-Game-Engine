
cbuffer ConstantBuffer : register(b0)
{
    uint level;
};

RWTexture2D<float4> renderTarget : register(u0);
Texture2D depthMapTexture : register(t0);

SamplerState samplerPoint : register(s0);

uint computePowerOfTwo(int exponent)
{
    switch (exponent)
    {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        case 3: return 8;
        case 4: return 16;
        case 5: return 32;
        case 6: return 64;
        case 7: return 128;
        case 8: return 256;
        case 9: return 512;
        default: return 0; // For values outside the range [0, 9]
    }
}

float4 LinearToSrgb4(float4 linearColor)
{
    return float4(
        linearColor.xyz <= 0.0031308 ? linearColor.xyz * 12.92 : 1.055 * pow(linearColor.xyz, 1.0 / 2.4) - 0.055,
        linearColor.w
    );
}

float LinearToSrgb(float linearColor)
{
    return linearColor <= 0.0031308 ? linearColor * 12.92 : 1.055 * pow(linearColor, 1.0f / 2.4) - 0.055f;
}

float linearizeDepth(float depth)
{   
    float nearZ = 0.1f;
    float farZ = 100.0f;
    // Convert depth from [0, 1] to [-1, 1] (NDC space)
    float ndcDepth = depth * 2.0 - 1.0;
    // Linearize depth using the near and far planes
    float linearDepth = 2.0 * nearZ * farZ / (farZ + nearZ - ndcDepth * (farZ - nearZ));
    
    return linearDepth;
}

uint sumOfGPOfHalf(uint a, uint n) {
    return 2 * a * (1 - 1.f/computePowerOfTwo(n));
}

uint getYOffset(uint LOD, uint HEIGHT) {
    return sumOfGPOfHalf(HEIGHT, LOD);
}

[numthreads(32, 32, 1)]
void HiZDepthCompute( uint3 DTid : SV_DispatchThreadID )
{   
    uint outputWidth, outputHeight;
	renderTarget.GetDimensions(outputWidth, outputHeight);
	outputHeight /= 2;

	uint LOD = level;
	uint powerOf2 = computePowerOfTwo(LOD);
	uint width = outputWidth / powerOf2;
	uint height = outputHeight / powerOf2;
	uint Yoffset = getYOffset(LOD, outputHeight);
	uint YoffsetNext = getYOffset(LOD + 1, outputHeight);

	if(DTid.x >= width || DTid.y >= YoffsetNext) {
		return;
	}

    if(level == 0) {
        float Depth0 = depthMapTexture[DTid.xy];
        float Depth1 = depthMapTexture[DTid.xy + uint2(1, 0)];
        float Depth2 = depthMapTexture[DTid.xy + uint2(1, 1)];
        float Depth3 = depthMapTexture[ DTid.xy + uint2(0, 1)];

        Depth1 = Depth1 == 0 ? 1000 : Depth1;
        Depth2 = Depth2 == 0 ? 1000 : Depth2;
        Depth3 = Depth3 == 0 ? 1000 : Depth3;

        float depth = min(min(Depth0, Depth1), min(Depth2, Depth3));
        depth = linearizeDepth(depth);
        // depth = LinearToSrgb(depth);
        renderTarget[DTid.xy] = depth;
        return;
    }

    uint2 depthTexel = uint2(2*DTid.x, 2 * DTid.y + getYOffset(level - 1, outputHeight));

    float Depth0 = renderTarget[depthTexel];
    float Depth1 = renderTarget[depthTexel + uint2(1, 0)];
    float Depth2 = renderTarget[depthTexel + uint2(1, 1)];
    float Depth3 = renderTarget[depthTexel + uint2(0, 1)];
    float depth = min(min(Depth0, Depth1), min(Depth2, Depth3));

    uint2 texelLocation = uint2(DTid.x, DTid.y + Yoffset);
	renderTarget[texelLocation] = depth;
}