
#define PI 3.14159265358

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 PV;
    float4x4 LPV;
    float3 eye;
    float3 lightDir;
};

cbuffer MaterialBuffer : register(b1) 
{
    float4 baseColor;
    float roughness0;
    float metallic0;
    float3 emission0;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
    float4 tangent : TANGENT;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
    float3 worldPos : W_POSITION;
    float4 tangent : TAN;
    float3 biTangent : BITAN;
    float3 eye : EYE;
    float4 fragPosLightSpace : LIGHT_SPACE;
};

struct PSOutput
{
	float4 albedo : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 roughnessMetallicAO : SV_TARGET2;
};

Texture2D shadowMapTexture : register(t0);
Texture2D albedoTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D metallicRoughnessTexture : register(t3);
Texture2D occlusionTexture : register(t4);
Texture2D emissiveTexture : register(t5);
Texture2D checkerBoardTexture : register(t6);
Texture2D skyboxIrradianceTexture : register(t7);
Texture2D preFilterEnvMapTexture : register(t8);
SamplerState g_sampler : register(s0);
SamplerState samplerPreFilter : register(s1);

PSInput VSMain(VSInput vInput)
{
    PSInput vOut;

    vOut.position = mul(PV, float4(vInput.pos, 1));
    vOut.normal = vInput.normal;
    vOut.uv = vInput.uv;
    vOut.worldPos = vInput.pos;
    vOut.eye = eye;
    vOut.tangent = vInput.tangent;
    vOut.biTangent = cross(vInput.normal, vInput.tangent.xyz);// * vInput.tangent.z;

    float depthOffset = 0.02f;
    float3 depthDir = normalize(vInput.normal * 0.8 + lightDir * 0.2);
    vOut.fragPosLightSpace = mul(LPV, float4(vInput.pos + depthOffset * depthDir, 1));

    return vOut;
}

PSInput ShadowVS(VSInput vInput)
{
    PSInput vOut;
    vOut.position = mul(LPV, float4(vInput.pos, 1));
    return vOut;
}

void ShadowPS(PSInput input) {

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
    float closestDepth = shadowMapTexture.Sample(g_sampler, projectionCoords.xy).r;
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

float4 PSSimpleAlbedo(PSInput vsOut) : SV_TARGET
{
    float kd = 0.4;
    float ks = 0.4;
    float3 ka = 0.8;
    float3 color = baseColor;//float3(255, 212, 128)/255;
    bool isMetal = false;
    float lightIntensity = 1.f;
    float4 texColor = albedoTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y));
    float3 normals = normalTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).rgb;
    float metallic = metallic0 * metallicRoughnessTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).b;
    float roughness = roughness0 * metallicRoughnessTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).g;
    float occlusion = occlusionTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).r;
    float3 emission = emissiveTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).rgb;

    // color = linearToSRGB(color);
    texColor = SrgbToLinear4(texColor);
    float3 albedo = color * texColor.rgb;

    float alpha = min(baseColor.a, texColor.a);
    if(alpha == 0.0f)
        discard;

    // return float4(roughness, 0, 0, 1);
    // return float4(irradianceFromMap, alpha);

    float3 tangent = vsOut.tangent.xyz;
    normals = 2 * normals - 1;
    normals = vsOut.tangent.xyz * normals.x + vsOut.biTangent * normals.y + vsOut.normal * normals.z;
    // return float4(normals, 1);

    float3 l = lightDir;
    float3 v = normalize(vsOut.eye - vsOut.worldPos);
    float3 h = normalize(l + v);
    float3 n = normalize(normals);
    float3 r = reflect(-v, n);

    // return float4(normals, 1);

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
    
    // roughness *= roughness;
    // roughness = 0.3;
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
        // uint w = 2048 / computePowerOfTwo(roughnessLOD);
        preFilterEnvMapUV = float2(min(maxUVx, preFilterEnvMapUV.x), min(maxUVy, preFilterEnvMapUV.y));
        // return float4(preFilterEnvMapUV , 0, 1);
    }
    float3 envMapLi = preFilterEnvMapTexture.Sample(samplerPreFilter, float2(preFilterEnvMapUV.x, preFilterEnvMapUV.y)).rgb;
    // return float4(envMapLi, 1);
    float nDotV = max(dot(n, v), 0.0f);
    float2 envBRDF = checkerBoardTexture.Sample(g_sampler, float2(roughness, nDotV)).rg;
    specular = envMapLi * (F * envBRDF.x + envBRDF.y);

    float2 irradianceMapUV = directionToEquirectangularUV(n);
    float3 irradianceFromMap = skyboxIrradianceTexture.Sample(samplerPreFilter, float2(irradianceMapUV.x, irradianceMapUV.y)).rgb;

    // float shadowValue = getShadowMultiplier(vsOut.fragPosLightSpace);
    // float3 diff = kd * saturate(dot(l, vsOut.normal)) * albedo;
    // float3 spec = ks * pow(saturate(dot(vsOut.normal, h)), 90) ;
    float3 ambient = kD * irradianceFromMap * albedo;
    emission = emission0 * emission;

    // float3 radiance = lightIntensity * shadowValue * (diff + spec) + ambient;
//    float3 radiance =  (kD * albedo / PI + specular) * lightIntensity * NdotL + ambient + emission;
    float3 radiance = lightIntensity * (ambient + specular) + emission;
    radiance *= occlusion;

    // return float4(albedo, 1);

    radiance = linearToSRGB(radiance);
    return float4(radiance, alpha);
}

PSOutput GBufferRenderTargets(PSInput vsOut) : SV_Target {
    float3 color = baseColor;
    float4 texColor = albedoTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y));
    float3 normals = normalTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).rgb;
    float3 metallicRoughness = metallicRoughnessTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).rgb;
    float metallic = metallic0 * metallicRoughness.b;
    float roughness = roughness0 * metallicRoughness.g;
    float occlusion = occlusionTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).r;
    float3 emission = emissiveTexture.Sample(g_sampler, float2(vsOut.uv.x, vsOut.uv.y)).rgb;

    texColor = SrgbToLinear4(texColor);
    float3 albedo = color * texColor.rgb;
    float alpha = min(baseColor.a, texColor.a);
    
    float3 tangent = vsOut.tangent.xyz;
    normals = 2 * normals - 1;
    normals = vsOut.tangent.xyz * normals.x + vsOut.biTangent * normals.y + vsOut.normal * normals.z;

    PSOutput psOutput;

    psOutput.albedo = float4(albedo, alpha);
    psOutput.normal = float4(normals, 1);
    psOutput.roughnessMetallicAO = float4(roughness, metallic, occlusion, 1);

    return psOutput;
}