#define PI 3.14159265358

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
Texture2D shadowMapTexture : register(t7);

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

float3 sRGBToLinear(float3 srgbColor)
{
    return srgbColor <= 0.04045 ? srgbColor / 12.92 : pow((srgbColor + 0.055) / 1.055, 2.4);
}

// Convert linear color to sRGB color
float3 linearToSRGB(float3 linearColor)
{
    return linearColor <= 0.0031308 ? linearColor * 12.92 : 1.055 * pow(linearColor, 1.0 / 2.4) - 0.055;
}

// Convert sRGB color to linear color
float4 SrgbToLinear4(float4 srgbColor)
{
    return float4(
        srgbColor.xyz <= 0.04045 ? srgbColor.xyz / 12.92 : pow((srgbColor.xyz + 0.055) / 1.055, 2.4),
        srgbColor.w
    );
}

// Convert linear color to sRGB color
float4 LinearToSrgb4(float4 linearColor)
{
    return float4(
        linearColor.xyz <= 0.0031308 ? linearColor.xyz * 12.92 : 1.055 * pow(linearColor.xyz, 1.0 / 2.4) - 0.055,
        linearColor.w
    );
}

float3 linearToGamma(float3 color) {
    return pow(color, 2.2f);
}

float3 gammaToLinear(float3 color) {
    return pow(color, 0.454545f);
}

float linearizeDepth(float depth)
{
    float near_plane = 0.0f;
    float far_plane = 100.0f;
    float z = depth;
    return (2.0f * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

float getShadowMultiplier(float4 fragposLightSpace) {
    float3 projectionCoords = fragposLightSpace.xyz / fragposLightSpace.w;
    projectionCoords.xy = projectionCoords.xy * 0.5f + 0.5f;
    projectionCoords.y = 1 - projectionCoords.y;
    float closestDepth = shadowMapTexture.Sample(gSampler, projectionCoords.xy).r;
    float currentDepth = projectionCoords.z;
    return (currentDepth - 0.0001f) > closestDepth ? 0.0f : 1.0f;
}

uint getRoughnessLOD(float roughness) {
    if (roughness < 0.05f)
        return 0;
    else if (roughness < 0.125f)
        return 1;
    else if (roughness < 0.2f)
        return 2;
    else if (roughness < 0.3f)
        return 3;
    else if (roughness < 0.45f)
        return 4;
    else if (roughness < 0.6f)
        return 5;
    else if (roughness < 0.8f)
        return 6;
    return 7;
}

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

uint getYOffsetForPreFilterEnv(uint LOD) {
	switch (LOD)
    {
        case 0: return 0;
        case 1: return 1024;
        case 2: return 1536;
        case 3: return 1792;
        case 4: return 1920;
        case 5: return 1984;
        case 6: return 2016;
        case 7: return 2032;
        case 8: return 2040;
        case 9: return 2044;
        default: return 0;
    }
}

// ----------------------------------------------------------------------------
float2 directionToEquirectangularUV(float3 direction)
{
    float phi = atan2(direction.z, direction.x); // Range [-pi, pi]
    float theta = acos(direction.y); // Range [0, pi]

    // Convert longitude (phi) to UV.x in the range [0, 1]
    float u = (phi + PI) / (2.0f * PI);

    // Convert latitude (theta) to UV.y in the range [0, 1]
    float v = theta / PI;

    return float2(u, v);
}
// ----------------------------------------------------------------------------
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------

[numthreads(32, 32, 1)]
void LightingPass( uint3 DTid : SV_DispatchThreadID )
{	
	uint outputWidth, outputHeight;
	renderTarget.GetDimensions(outputWidth, outputHeight);
	if(DTid.x >= outputWidth || DTid.y >= outputHeight) {
		return;
	}

    float2 uv = (DTid.xy + 0.5f) / float2(outputWidth, outputHeight);
	float4 albedo4 = albedoMapTexture.SampleLevel(samplerPoint, uv, 0);
	float3 normals = normalsTexture.SampleLevel(samplerPoint, uv, 0).rgb;
    if(length(normals) < 0.1f) {
        renderTarget[DTid.xy] = albedo4;
        return;
    }

    normals = 2 * normals - 1.0f;
	float3 rmAO = roughnessMetallicAO.SampleLevel(samplerPoint, uv, 0).rgb;
	float depth = depthMap.SampleLevel(samplerPoint, uv, 0).r;
	float3 worldPos = calculateWorldPosition(depth, uv);
    float roughness = rmAO.r;
    float metallic = rmAO.g;
    float occlusion = rmAO.b;
    float lightIntensity = 1.f;

    float3 albedo = albedo4.rgb;

    float3 l = lightDir;
    float3 v = normalize(eye - worldPos);
    float3 h = normalize(l + v);
    float3 n = normalize(normals);
    float3 r = reflect(-v, n);

    float3 F0 = 0.04f; 
    F0 = lerp(F0, albedo, metallic);

    float NDF = DistributionGGX(n, h, roughness);   
    float G   = GeometrySmith(n, v, l, roughness);      
    float3 F  = fresnelSchlick(clamp(dot(h, v), 0.0, 1.0), F0);

    float3 kS = F;
    float3 kD = 1 - kS;
    kD *= 1.0f - metallic;
    
    float3 numerator    = NDF * G * F;
    float denominator = 4.0f * max(dot(n, v), 0.0f) * max(dot(n, l), 0.0f) + 0.0001f;
    float3 specular     = numerator / denominator;  
        
    // // add to outgoing radiance Lo
    float NdotL = max(dot(n, l), 0.0f);
    
    // renderTarget[DTid.xy] = float4(NdotL, NdotL, NdotL, 1);
    // return;

    uint roughnessLOD = getRoughnessLOD(roughness);
    float2 preFilterEnvMapUV = directionToEquirectangularUV(r);
    preFilterEnvMapUV.y /= 2.0f;
    if (roughnessLOD > 0) {
        float maxUVy = 2046.5/4096.0f;

        uint powerOf2 = computePowerOfTwo(roughnessLOD);
        preFilterEnvMapUV /= powerOf2;
        maxUVy /= powerOf2;

        float yOffset = (getYOffsetForPreFilterEnv(roughnessLOD) + roughnessLOD)/2048.0f;
        preFilterEnvMapUV.y += yOffset;
        maxUVy += yOffset;

        uint w = 2048 / computePowerOfTwo(roughnessLOD);
        float maxUVx = (w-1.5f)/2048.0f;
        preFilterEnvMapUV = float2(min(maxUVx, preFilterEnvMapUV.x), min(maxUVy, preFilterEnvMapUV.y));
    }
    float3 envMapLi = preFilterEnvMapTexture.SampleLevel(gSampler, float2(preFilterEnvMapUV.x, preFilterEnvMapUV.y), 0).rgb;
    float nDotV = max(dot(n, v), 0.0f);
    float2 envBRDF = checkerBoardTexture.SampleLevel(gSampler, float2(roughness, nDotV), 0).rg;
    specular = envMapLi * (F * envBRDF.x + envBRDF.y);

    float2 irradianceMapUV = directionToEquirectangularUV(n);
    float3 irradianceFromMap = skyboxIrradianceTexture.SampleLevel(gSampler, float2(irradianceMapUV.x, irradianceMapUV.y), 0).rgb;

    float3 ambient = kD * irradianceFromMap * albedo;
    float3 radiance = lightIntensity * (ambient + specular);
    radiance *= occlusion;
    radiance = linearToSRGB(radiance);

    renderTarget[DTid.xy] = float4(radiance, 1);
}