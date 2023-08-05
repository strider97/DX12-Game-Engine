// Physically Based Rendering
// Copyright (c) 2017-2018 Michał Siejak

// Pre-filters environment cube map using GGX NDF importance sampling.
// Part of specular IBL split-sum approximation.

#define PI 3.14159265358
static const float TwoPI = 2 * PI;
static const float Epsilon = 0.00001;

cbuffer ConstantBuffer : register(b0)
{
    uint roughness_;
};

static const uint NumSamples = 1024;
static const float InvNumSamples = 1.0 / float(NumSamples);

Texture2D inputTexture : register(t0);
RWTexture2DArray<float4> outputTexture : register(u0);

SamplerState defaultSampler : register(s0);

// Compute Van der Corput radical inverse
// See: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float radicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Sample i-th point from Hammersley point set of NumSamples points total.
float2 sampleHammersley(uint i)
{
	return float2(i * InvNumSamples, radicalInverse_VdC(i));
}

// Importance sample GGX normal distribution function for a fixed roughness value.
// This returns normalized half-vector between Li & Lo.
// For derivation see: http://blog.tobias-franke.eu/2014/03/30/notes_on_importance_sampling.html
float3 sampleGGX(float u1, float u2, float roughness)
{
	float alpha = roughness * roughness;

	float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha*alpha - 1.0) * u2));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta); // Trig. identity
	float phi = TwoPI * u1;

	// Convert to Cartesian upon return.
	return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness)
{
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

float3 equirectangularToDirection(float2 uv)
{
    // Convert the UV coordinates to spherical coordinates
    float theta = uv.y * PI;  // Latitude (Elevation) [0, PI]
    float phi = uv.x * 2 * PI - PI; // Longitude (Azimuth) [-PI, PI]

    // Convert spherical coordinates to Cartesian coordinates
    float3 direction;
    direction.x = sin(theta) * cos(phi); // x = r * sin(theta) * cos(phi)
    direction.y = cos(theta);            // y = r * cos(theta)
    direction.z = sin(theta) * sin(phi); // z = r * sin(theta) * sin(phi)

    return normalize(direction);
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

float3 getSamplingVector(float2 uv)
{
	return equirectangularToDirection(uv);
}

// Compute orthonormal basis for converting from tanget/shading space to world space.
void computeBasisVectors(const float3 N, out float3 S, out float3 T)
{
	// Branchless select non-degenerate T.
	T = cross(N, float3(0.0, 1.0, 0.0));
	T = lerp(cross(N, float3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));

	T = normalize(T);
	S = normalize(cross(N, T));
}

// Convert point from tangent/shading space to world space.
float3 tangentToWorld(const float3 v, const float3 N, const float3 S, const float3 T)
{
	return S * v.x + T * v.y + N * v.z;
}

uint getYOffset(uint LOD) {
	switch (LOD)
    {
        case 0: return 0;
        case 1: return 1024;
        case 2: return 1024 + 512;
        case 3: return 1024 + 512 + 256;
        case 4: return 1024 + 512 + 256 + 128;
        case 5: return 1024 + 512 + 256 + 128 + 64;
        case 6: return 1024 + 512 + 256 + 128 + 64 + 32;
        case 7: return 1024 + 512 + 256 + 128 + 64 + 32 + 16;
        case 8: return 1024 + 512 + 256 + 128 + 64 + 32 + 16 + 8;
        case 9: return 1024 + 512 + 256 + 128 + 64 + 32 + 16 + 8 + 4;
        default: return 0;
    }
}

uint getLOD(float roughness) {
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

[numthreads(32, 32, 1)]
void CSPreFilterEnvMap(uint3 ThreadID : SV_DispatchThreadID)
{
	// Make sure we won't write past output when computing higher mipmap levels.
	uint outputWidth, outputHeight, outputDepth;
	outputTexture.GetDimensions(outputWidth, outputHeight, outputDepth);
	outputHeight /= 2;

	float roughness = roughness_ / 1000.0f;
	uint LOD = getLOD(roughness);
	uint powerOf2 = computePowerOfTwo(LOD);
	uint width = outputWidth / powerOf2;
	uint height = outputHeight / powerOf2;
	uint Yoffset = getYOffset(LOD) + LOD;

	if(ThreadID.x >= width + 4 || ThreadID.y >= height + 4) {
		return;
	}
	
	// Get input cubemap dimensions at zero mipmap level.
	float inputWidth, inputHeight, inputLevels;
	inputTexture.GetDimensions(0, inputWidth, inputHeight, inputLevels);

	// Solid angle associated with a single cubemap texel at zero mipmap level.
	// This will come in handy for importance sampling below.
	float wt = 4.0 * PI / (inputWidth * inputHeight);
	
	// Approximation: Assume zero viewing angle (isotropic reflections).
	float2 uv_ = (ThreadID.xy + 0.5f) / float2(width, height);
    float3 N = getSamplingVector(uv_);
	float3 Lo = N;
	
	float3 S, T;
	computeBasisVectors(N, S, T);

	float3 color = 0;
	float weight = 0;

	// Convolve environment map using GGX NDF importance sampling.
	// Weight by cosine term since Epic claims it generally improves quality.
	for(uint i=0; i<NumSamples; ++i) {
		float2 u = sampleHammersley(i);
		float3 Lh = tangentToWorld(sampleGGX(u.x, u.y, roughness), N, S, T);

		// Compute incident direction (Li) by reflecting viewing direction (Lo) around half-vector (Lh).
		float3 Li = 2.0 * dot(Lo, Lh) * Lh - Lo;

		float cosLi = dot(N, Li);
		if(cosLi > 0.0) {
			// Use Mipmap Filtered Importance Sampling to improve convergence.
			// See: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html, section 20.4

			float cosLh = max(dot(N, Lh), 0.0);

			// GGX normal distribution function (D term) probability density function.
			// Scaling by 1/4 is due to change of density in terms of Lh to Li (and since N=V, rest of the scaling factor cancels out).
			float pdf = ndfGGX(cosLh, roughness) * 0.25;

			// Solid angle associated with this sample.
			float ws = 1.0 / (NumSamples * pdf);

			// Mip level to sample from.
			// float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);
			float2 uv = directionToEquirectangularUV(Li);
			color  += inputTexture.SampleLevel(defaultSampler, uv, 0).rgb * cosLi;
			weight += cosLi;
		}
	}
	color /= weight;

	uint3 texelLocation = uint3(ThreadID.x, ThreadID.y + Yoffset, ThreadID.z);
	outputTexture[texelLocation] = float4(color, 1.0);
}