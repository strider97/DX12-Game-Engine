
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 PV;
    float4x4 LPV;
    float4x4 invPV;
    float3 eye;
    float3 lightDir;
};

RWTexture2D<float4> renderTarget : register(u0);

Texture2D albedoMapTexture : register(t0);
Texture2D normalsTexture : register(t1);
Texture2D roughnessMetallicAO : register(t2);
Texture2D depthMap : register(t3);
Texture2D checkerBoardTexture : register(t4);
Texture2D skyboxIrradianceTexture : register(t5);
Texture2D preFilterEnvMapTexture : register(t6);

SamplerState gSampler : register(s0);
SamplerState samplerPoint : register(s1);

float3 calculateWorldPosition(float z, float2 uv)
{
    float x = uv.x * 2 - 1;
    float y = (1-uv.y) * 2 - 1;
    float4 projectedPos = float4(x, y, z, 1.0);
    float4 viewPosVS = mul(invPV, projectedPos);
    return viewPosVS.xyz/viewPosVS.w;
}

[numthreads(32, 32, 1)]
void LightingPass( uint3 DTid : SV_DispatchThreadID )
{	
	uint outputWidth, outputHeight;
	renderTarget.GetDimensions(outputWidth, outputHeight);
	if(DTid.x >= outputWidth || DTid.y >= outputHeight) {
		return;
	}

    float2 uv = (DTid.xy + 0.5f) / float2(outputWidth, outputHeight);
	float4 albedo = albedoMapTexture.SampleLevel(samplerPoint, uv, 0);
	float4 brdfIntegral = checkerBoardTexture.SampleLevel(samplerPoint, uv, 0);
	float4 irradiance = skyboxIrradianceTexture.SampleLevel(samplerPoint, uv, 0);
	float4 preFilter = preFilterEnvMapTexture.SampleLevel(samplerPoint, uv, 0);
	float3 normals = normalsTexture.SampleLevel(samplerPoint, uv, 0).rgb;
    if(length(normals) < 0.1f) {
        renderTarget[DTid.xy] = albedo;
        return;
    }
    normals = 2 * normals - 1.0f;
	float3 rmAO = roughnessMetallicAO.SampleLevel(samplerPoint, uv, 0).rgb;
	float depth = depthMap.SampleLevel(samplerPoint, uv, 0).r;
	float3 worldPos = calculateWorldPosition(depth, uv);

    renderTarget[DTid.xy] = float4((brdfIntegral + irradiance + preFilter).rgb, 1);

	// uint tileWidth = 32;
	// float3 color = ((DTid.x / tileWidth) % 2 == 0) ^ ((DTid.y / tileWidth) % 2 == 1) ? 1 : 0;
	// renderTarget[DTid.xy] = float4(color, 1.f);
}