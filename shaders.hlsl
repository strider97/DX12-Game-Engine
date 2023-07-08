//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 PV;
    float4x4 LPV;
    float3 eye;
    float3 lightDir;
    float padding[25];
};

struct VSInput
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
    float3 worldPos : W_POSITION;
    float3 eye : EYE;
    float4 fragPosLightSpace : LIGHT_SPACE;
};

Texture2D albedoTexture : register(t0);
Texture2D shadowMapTexture : register(t1);
SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput vInput)
{
    PSInput vOut;

    vOut.position = mul(PV, float4(vInput.pos, 1));
    vOut.normal = vInput.normal;
    vOut.uv = vInput.uv;
    vOut.worldPos = vInput.pos;
    vOut.eye = eye;

    float depthOffset = 0.01f;
    float3 depthDir = normalize(vInput.normal * 0.6 + lightDir * 0.4);
    vOut.fragPosLightSpace = mul(LPV, float4(vInput.pos + depthDir * depthOffset, 1));

    return vOut;
}

PSInput ShadowVS(VSInput vInput)
{
    PSInput vOut;

    vOut.position = mul(LPV, float4(vInput.pos, 1));
    vOut.normal = vInput.normal;
    vOut.uv = vInput.uv;
    vOut.worldPos = vInput.pos;
    vOut.eye = eye;

    return vOut;
}

void ShadowPS(PSInput input) {

}

float convert_sRGB_FromLinear(float theLinearValue) {
    return theLinearValue <= 0.0031308f
        ? theLinearValue * 12.92f
        : pow(theLinearValue, 1.0f / 2.4f) * 1.055f - 0.055f;
}

float3 sRGB_FromLinear3(float3 linearValue) {
    return float3(
        convert_sRGB_FromLinear(linearValue.x),
        convert_sRGB_FromLinear(linearValue.y),
        convert_sRGB_FromLinear(linearValue.z)
    );
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
    return (currentDepth - 0.00001f) > closestDepth ? 0.0f : 1.0f;
}

// float4 PSMain(PSInput vsOut) : SV_TARGET
// {
//     float kd = 0.4;
//     float ks = 0.2;
//     float ka = 0.1;
//     float3 color = albedoTexture.Sample(g_sampler, float2(vsOut.uv.x, 1 - vsOut.uv.y)).rgb;
//     bool metal = false;
//     float lightIntensity = 10.4;
    
//     float3 l = normalize(float3(6, 9, 4));
//     float3 v = normalize(vsOut.eye - vsOut.worldPos);
//     float3 h = normalize(l + v);
    
//     float3 diff = kd * saturate(dot(l, vsOut.normal));
//     float3 spec = pow(saturate(dot(vsOut.normal, h)), 32.0f);
    
//     float shadowValue = getShadowValue(vsOut.fragPosLightSpace);
//     float3 radiance = color * shadowValue * (diff * shadowValue + ka) + 0 * spec * (metal ? (color * ks) : kd);
//     radiance *= lightIntensity;
//     return float4(color, 1);
// }

float4 PSSimpleAlbedo(PSInput vsOut) : SV_TARGET
{
    float kd = 0.4;
    float ks = 0.2;
    float ka = 0.1;
    float3 color = float3(255, 212, 128)/255;
    bool isMetal = false;
    float lightIntensity = 1.2f;
    // float3 texColor = sRGB_FromLinear3(albedoTexture.Sample(g_sampler, float2(vsOut.uv.x, 1 - vsOut.uv.y)).rgb);

    // return float4(
    //     float3(30 * linearizeDepth(vsOut.fragPosLightSpace.z/vsOut.fragPosLightSpace.w), 0, 0), 
    //     1.0f);
    return float4(float3(getShadowMultiplier(vsOut.fragPosLightSpace), 0, 0), 1);

    float3 l = lightDir;
    float3 v = normalize(vsOut.eye - vsOut.worldPos);
    float3 h = normalize(l + v);

    float shadowValue = getShadowMultiplier(vsOut.fragPosLightSpace);
    float3 diff = kd * shadowValue * saturate(dot(l, vsOut.normal));

    float3 radiance = lightIntensity * color * (diff + ka);
    radiance *= lightIntensity;

    return float4(radiance, 1);
}