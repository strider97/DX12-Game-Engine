#define PI 3.14159265358
static const float TwoPI = 2 * PI;

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
Texture2D diffuseTexture : register(t7);

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
    float nearZ = 0.1f;
    float farZ = 100.0f;
    // Convert depth from [0, 1] to [-1, 1] (NDC space)
    float ndcDepth = depth * 2.0 - 1.0;
    // Linearize depth using the near and far planes
    float linearDepth = 2.0 * nearZ * farZ / (farZ + nearZ - ndcDepth * (farZ - nearZ));

    /*
        z_ndc = 2.0 * depth - 1.0;
        z_eye = 2.0 * n * f / (f + n - z_ndc * (f - n));
    */
    
    return abs(linearDepth);
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

float radicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float3 tangentToWorld(const float3 v, const float3 N, const float3 S, const float3 T)
{
	return S * v.x + T * v.y + N * v.z;
}

// Sample i-th point from Hammersley point set of NumSamples points total.
float2 sampleHammersley(uint i, float invNumSamples)
{
	return float2(i * invNumSamples, radicalInverse_VdC(i));
}

float3 sampleHemisphere(float2 u, float2 v)
{   
    float u1 = u.x;
    float u2 = u.y;
	const float u1p = sqrt(max(0.0, 1.0 - u1*u1));
	float3 r = float3(cos(TwoPI*u2) * u1p, sin(TwoPI*u2) * u1p, u1);
    r * v.x;
    return r;
}

void computeBasisVectors(const float3 N, out float3 S, out float3 T)
{
	// Branchless select non-degenerate T.
	T = cross(N, float3(0.0, 1.0, 0.0));
	T = lerp(cross(N, float3(1.0, 0.0, 0.0)), T, step(0.00001, dot(T, T)));

	T = normalize(T);
	S = normalize(cross(N, T));
}

float3 getNDCCoordinates(float3 pos) {
    float4 ndcPos = mul(PV, float4(pos, 1.f));
    float3 newPos = ndcPos.xyz / ndcPos.w;
    newPos.xy = (newPos.xy + 1)/2.0f;
    newPos.y = 1 - newPos.y;
    return newPos.xyz;
}

float3 evaluateSSR(float3 normal, float3 pos, float3 v) {
    uint MAX_ITER = 128;
    float stepSize = 0.1;
    float3 r = reflect(-v, normal);
    // if(dot(r, v) > 0) {
    //     return 0;
    // }
    float epsilon = 0.1f;

    float3 p0 = pos + normal * 0.001;
    float3 p1 = p0;
    for(int i=0; i<MAX_ITER; i++) {
        p1 = p0 + r * stepSize * (i + 1);
        float3 ndcP1 = getNDCCoordinates(p1);
        if(ndcP1.x < 0 || ndcP1.y < 0 || ndcP1.y > 1 || ndcP1.x > 1)
            return 0;
        float depthP1 = ndcP1.z;
        float2 uvP1 = ndcP1.xy;
        float actualDepth = depthMap.SampleLevel(samplerPoint, uvP1, 0).r;
        depthP1 = linearizeDepth(depthP1);
        actualDepth = linearizeDepth(actualDepth);
        
        if(actualDepth < depthP1 && (actualDepth + epsilon) > depthP1) {
            float3 Li = diffuseTexture.SampleLevel(samplerPoint, uvP1, 0).rgb;
            return Li;
        }
    }
    return 0;
}

[numthreads(32, 32, 1)]
void SSRPass( uint3 DTid : SV_DispatchThreadID )
{	
	uint outputWidth, outputHeight;
	renderTarget.GetDimensions(outputWidth, outputHeight);
	if(DTid.x >= outputWidth || DTid.y >= outputHeight) {
		return;
	}

    float2 uv = (DTid.xy + 0.5f) / float2(outputWidth, outputHeight);
	float3 diffuse = diffuseTexture.SampleLevel(samplerPoint, uv, 0).rgb;
	float3 normals = normalsTexture.SampleLevel(samplerPoint, uv, 0).rgb;
    if(length(normals) < 0.1f) {
        renderTarget[DTid.xy] = float4(diffuse, 1);
        return;
    }

    normals = 2 * normals - 1.0f;
	float3 rmAO = roughnessMetallicAO.SampleLevel(samplerPoint, uv, 0).rgb;
	float depth = depthMap.SampleLevel(samplerPoint, uv, 0).r;
	float3 worldPos = calculateWorldPosition(depth, uv);
    float roughness = rmAO.r;
    float metallic = rmAO.g;

    float3 v = normalize(eye - worldPos);
    float3 n = normalize(normals);
    float3 ssr = 0;
    if(roughness < 0.1f)
        ssr = evaluateSSR(n, worldPos, v);

    float3 radiance = ssr + diffuse;
	
    renderTarget[DTid.xy] = float4(radiance, 1);
}