
#define LUTWidth 1024
#define LUTHeight 1024
#define PI 3.14159265358
RWTexture2D<float4> g_LUTTexture : register(u0);

[numthreads(32, 32, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint2 texCoord = DTid.xy;
    uint checkerboardSize = 32; // Adjust this value to change the size of the checkerboard squares
    uint size = 1024;

    bool isBlack = ((texCoord.x / checkerboardSize) + (texCoord.y / checkerboardSize)) % 2 == 0;

    float4 color = isBlack ? float4(0.0f, 0.0f, 0.0f, 1.0f) : float4(0.5f, 0.5f, 1.0f, 1.0f);

    // color = float4(float(DTid.x) / size, float(DTid.y) / size, 0.0f, 1.0f);

    g_LUTTexture[DTid.xy] = color;
}

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

// Taken from https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/data/shaders/genbrdflut.frag
// Based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley(uint i, uint N) 
{
	// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	uint bits = (i << 16u) | (i >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return float2(float(i) /float(N), rdi);
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float G_Smith(float NoV, float NoL, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GGXL = NoL / (NoL * (1.0 - k) + k);
	float GGXV = NoV / (NoV * (1.0 - k) + k);
	return GGXL * GGXV;
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness) {
    float a2 = pow(roughness, 4.0);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}


// Based on Karis 2014
float3 importanceSampleGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness;
    // Sample in spherical coordinates
    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float SinTheta = sqrt(1.0 - CosTheta * CosTheta);
    // Construct tangent space vector
    float3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    
    // Tangent to world space
    float3 UpVector = abs(N.z) < 0.999 ? float3(0.,0.,1.0) : float3(1.0,0.,0.);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    return TangentX * H.x + TangentY * H.y + N * H.z;
}


// Karis 2014
float2 integrateBRDF(float roughness, float NoV)
{
	float3 V;
    V.x = sqrt(1.0 - NoV * NoV); // sin
    V.y = 0.0;
    V.z = NoV; // cos
    
    // N points straight upwards for this integration
    const float3 N = float3(0.0, 0.0, 1.0);
    
    float A = 0.0;
    float B = 0.0;
    const uint numSamples = 1024u;
    
    for (uint i = 0u; i < numSamples; i++) {
        float2 Xi = hammersley(i, numSamples);
        // Sample microfacet direction
        float3 H = importanceSampleGGX(Xi, roughness, N);
        
        // Get the light direction
        float3 L = 2.0 * dot(V, H) * H - V;
        
        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));
        
        if(NoL > 0.0) {
            float V_pdf = V_SmithGGXCorrelated(NoV, NoL, roughness) * VoH * NoL / NoH;
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * V_pdf;
            B += Fc * V_pdf;
        }
    }

    return 4.0 * float2(A, B) / float(numSamples);
}

[numthreads(32, 32, 1)]
void CSLut(uint3 DTid : SV_DispatchThreadID)
{
    // Compute the texture coordinates in the LUT
    float2 texCoord = (DTid.xy + 0.5f) / float2(LUTWidth, LUTHeight);

    float roughness = texCoord.x;
    float nDotV = texCoord.y;

    // Encode the roughness and metallic values in the RGB channels
    float2 brdf = integrateBRDF(roughness, nDotV);

    // Write the color to the LUT texture
    g_LUTTexture[DTid.xy] = float4(brdf.x, brdf.y, 0, 1);
}