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
    float3 eye;
    float padding[45];
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
};

Texture2D albedoTexture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput vInput)
{
    PSInput vOut;

    vOut.position = mul(PV, float4(vInput.pos, 1));
    vOut.normal = vInput.normal;
    vOut.uv = vInput.uv;
    vOut.worldPos = vInput.pos;
    vOut.eye = eye;

    return vOut;
}

float convert_sRGB_FromLinear(float theLinearValue) {
    return theLinearValue <= 0.0031308f
        ? theLinearValue * 12.92f
        : pow(theLinearValue, 1.0f / 2.4f) * 1.055f - 0.055f;
}

float4 PSMain(PSInput vsOut) : SV_TARGET
{
    float kd = 0.4;
    float ks = 0.2;
    float ka = 0.1;
    float3 color = albedoTexture.Sample(g_sampler, float2(vsOut.uv.x, 1 - vsOut.uv.y)).rgb;
    bool metal = false;
    float lightIntensity = 10.4;
    
    float3 l = normalize(float3(6, 9, 4));
    float3 v = normalize(vsOut.eye - vsOut.worldPos);
    float3 h = normalize(l + v);
    
    float3 diff = kd * saturate(dot(l, vsOut.normal));
    float3 spec = pow(saturate(dot(vsOut.normal, h)), 32.0f);
    
    float3 radiance = color * (diff + ka) + 0 * spec * (metal ? (color * ks) : kd);
    radiance *= lightIntensity;
    return float4(color, 1);
}
