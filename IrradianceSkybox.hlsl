
#define PI 3.14159265358

static const float TwoPI = 2 * PI;
static const uint NumSamples = 1024 * 8;
static const float InvNumSamples = 1.0f / NumSamples;
static const float Epsilon = 0.00001;

RWTexture2D<float4> irradianceMap : register(u0);
Texture2D skyboxTexture : register(t0);
SamplerState g_sampler : register(s0);

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

// Uniformly sample point on a hemisphere.
// Cosine-weighted sampling would be a better fit for Lambertian BRDF but since this
// compute shader runs only once as a pre-processing step performance is not *that* important.
// See: "Physically Based Rendering" 2nd ed., section 13.6.1.
float3 sampleHemisphere(float u1, float u2)
{
	const float u1p = sqrt(max(0.0, 1.0 - u1*u1));
	return float3(cos(TwoPI*u2) * u1p, sin(TwoPI*u2) * u1p, u1);
}

float3 tangentToWorld(const float3 v, const float3 N, const float3 S, const float3 T)
{
	return S * v.x + T * v.y + N * v.z;
}

float3 EquirectangularToDirection(float2 uv)
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

[numthreads(32, 32, 1)]
void CSIrradianceSkybox(uint3 DTid : SV_DispatchThreadID)
{   
	uint outputWidth, outputHeight;
	irradianceMap.GetDimensions(outputWidth, outputHeight);
    // Compute the texture coordinates in the LUT
    float2 uv_ = (DTid.xy + 0.5f) / float2(outputWidth, outputHeight);
    float3 N = getSamplingVector(uv_);
	
	float3 S, T;
	computeBasisVectors(N, S, T);

	// // Monte Carlo integration of hemispherical irradiance.
	// // As a small optimization this also includes Lambertian BRDF assuming perfectly white surface (albedo of 1.0)
	// // so we don't need to normalize in PBR fragment shader (so technically it encodes exitant radiance rather than irradiance).
	float3 irradiance = 0.0;
	for(uint i=0; i<NumSamples; ++i) {
		float2 u  = sampleHammersley(i);
		float3 Li = tangentToWorld(sampleHemisphere(u.x, u.y), N, S, T);
		float cosTheta = max(0.0, dot(Li, N));

		// PIs here cancel out because of division by pdf.
        
        float2 uv = directionToEquirectangularUV(Li);
        float3 radiance = min(24, skyboxTexture.SampleLevel(g_sampler, uv, 0).rgb);
		irradiance += 2.0 * radiance * cosTheta;
	}
	irradiance /= float(NumSamples);

	irradianceMap[DTid.xy] = float4(irradiance, 1.0);

}