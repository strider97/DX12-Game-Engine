
RWTexture2D<float4> renderTarget : register(u0);
Texture2D albedoTexture : register(t0);
Texture2D normalsTexture : register(t1);
Texture2D roughnessMetallicAO : register(t2);
Texture2D depthMap : register(t3);
SamplerState g_sampler : register(s0);
SamplerState samplerPoint : register(s1);

[numthreads(32, 32, 1)]
void LightingPass( uint3 DTid : SV_DispatchThreadID )
{	
	uint outputWidth, outputHeight;
	renderTarget.GetDimensions(outputWidth, outputHeight);
    float2 uv = (DTid.xy + 0.5f) / float2(outputWidth, outputHeight);

	if(DTid.x >= outputWidth || DTid.y >= outputHeight) {
		return;
	}

	float4 albedo = albedoTexture.SampleLevel(samplerPoint, uv, 0);
	float3 normals = normalsTexture.SampleLevel(samplerPoint, uv, 0).rgb;
	float3 rmAO = roughnessMetallicAO.SampleLevel(samplerPoint, uv, 0).rgb;
	float depth = depthMap.SampleLevel(samplerPoint, uv, 0).r;

	renderTarget[DTid.xy] = float4(albedo.xyz + normals + rmAO, 1 + depth);

	// uint tileWidth = 32;
	// float3 color = ((DTid.x / tileWidth) % 2 == 0) ^ ((DTid.y / tileWidth) % 2 == 1) ? 1 : 0;
	// renderTarget[DTid.xy] = float4(color, 1.f);
}