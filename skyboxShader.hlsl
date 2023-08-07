#define PI 3.14159265358979

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 PV;
    float4x4 LPV;
    float3 eye;
    float3 lightDir;
};

struct VSInput
{
    float3 pos : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : W_POSITION;
    float3 dir : DIR;
};

Texture2D skyboxTexture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput vInput)
{
    PSInput vOut;
    float3 pos = vInput.pos;
    // pos *= 2.5;
    // pos += float3(0, 2, 0);

    float3x3 rotationAndScalingMatrix = (float3x3)PV;
    vOut.position = float4(mul(rotationAndScalingMatrix, pos), 1);
    vOut.position.w = vOut.position.z + 0.00001f;

    vOut.worldPos = vInput.pos;
    return vOut;
}

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

float3 sRGBToLinear(float3 srgbColor)
{
    return srgbColor <= 0.04045 ? srgbColor / 12.92 : pow((srgbColor + 0.055) / 1.055, 2.4);
}

// Convert linear color to sRGB color
float3 linearToSRGB(float3 linearColor)
{
    return linearColor <= 0.0031308 ? linearColor * 12.92 : 1.055 * pow(linearColor, 1.0 / 2.4) - 0.055;
}

float4 skyboxPS(PSInput input) : SV_TARGET {
    float3 dir = normalize(input.worldPos);
    float2 uv = directionToEquirectangularUV(dir);
    float3 color = skyboxTexture.Sample(g_sampler, float2(uv.x, uv.y)).rgb;

    color *= 0.6f;
    color = linearToSRGB(color);
    return float4(color, 1);
}

/*
    PSInput VSMain(VSInput vInput)
    {
        PSInput vOut;
        // vOut.position = float4(vInput.pos, 1);
        return vOut;
    }

    Texture2D shadowMapTexture : register(t0);

    float4 skyboxPS(PSInput input) {
        return float4(1.0f, 1, 1, 1.f);
    }
*/