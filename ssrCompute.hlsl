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
Texture2D noiseTexture : register(t8);
Texture2D depthMapHiZ : register(t9);

SamplerState samplerLinear : register(s0);
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
    
    return linearDepth;
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

uint sumOfGPOfHalf(uint a, uint n) {
    return 2 * a * (1 - 1.f/computePowerOfTwo(n));
}

uint getYOffset(uint LOD, uint HEIGHT) {
    return sumOfGPOfHalf(HEIGHT, LOD);
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

float3 sampleToWorld(in float phi, in float cos_theta, in float sin_theta, in float3 N)
{
    float3 H;

    H.x = sin_theta * cos(phi);
    H.y = sin_theta * sin(phi);
    H.z = cos_theta;

    float3 up_vec = abs(N.z) < 0.999 ? float3(0,0,1) : float3(1,0,0);
    float3 tangent_x = normalize(cross(up_vec, N));
    float3 tangent_y = cross(N, tangent_x);

    return tangent_x * H.x + tangent_y * H.y + N * H.z;
}

// Importance sample a GGX specular function
float3 importanceSampleGGX(in float2 xi, in float a, in float3 N)
{
    float phi = 2 * PI * xi.x;
    float cos_theta = sqrt((1 - xi.y)/(1 + (a*a - 1) * xi.y));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);

    return sampleToWorld(phi, cos_theta, sin_theta, N);
}

float3 sampleGGXVisible(float3 N, float roughness, float2 rand) {
    float a2 = roughness * roughness;
    float cosTheta = sqrt((1.0 - rand.x) / (1.0 + (a2 - 1.0) * rand.x));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = 2.0 * PI * rand.y;

    // Calculate the sampled normal in tangent space
    float3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    // Transform the sampled normal from tangent space to world space
    float3 UpVector = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 TangentX = normalize(cross(UpVector, N));
    float3 TangentY = cross(N, TangentX);
    return normalize(TangentX * H.x + TangentY * H.y + N * H.z);
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

float3 getReflectionDirInTS(float3 p, float3 r) {
    float3 pTemp = p + 5 * r;
    float3 pTempTS = getNDCCoordinates(pTemp);
    float3 pTS = getNDCCoordinates(p);
    return normalize(pTempTS - pTS);
}

float3 binarySearchDepth(float3 minBounds, float3 maxBounds, float depth) {
    uint BIN_SEARCH_ITER = 12;
    for(int i=0; i< BIN_SEARCH_ITER; i++) {
        float3 mid = (minBounds + maxBounds) / 2;
        float midDepth = depthMap.SampleLevel(samplerPoint, mid.xy, 0).r;
        if(mid.z >= midDepth)
            maxBounds = mid;
        else
            minBounds = mid;

    }
    return (minBounds + maxBounds) / 2;
}

// Function to find the intersection point of a ray with a square
float findRaySquareIntersectionDist(float3 outSamplePosInTS, float3 outReflDirInTS) {
    // float tx0 = (0 - origin.x)/direction.x;
    // float tx1 = (1 - origin.x)/direction.x;
    // float ty1 = (0 - origin.y)/direction.y;
    // float ty0 = (1 - origin.y)/direction.y;
    // float tz0 = -(0 - origin.z)/direction.z;
    // float tz1 = -(1 - origin.z)/direction.z;

    // float tx = tx0 > 0 ? tx0 : tx1;
    // float ty = ty0 > 0 ? ty0 : ty1;
    // float tz = tz0 > 0 ? tz0 : tz1;

    // float3 px = origin + direction * tx;
    // float3 py = origin + direction * ty;
    // float3 pz = origin + direction * tz;

    // float distX = length(origin - px);
    // float distY = length(origin - py);
    // float distZ = length(origin - pz);

    // return min(distX, min(distY, distZ));
    float outMaxDistance;
    outMaxDistance = outReflDirInTS.x>=0 ? (1 - outSamplePosInTS.x)/outReflDirInTS.x  : -outSamplePosInTS.x/outReflDirInTS.x;
    outMaxDistance = min(outMaxDistance, outReflDirInTS.y<0 ? (-outSamplePosInTS.y/outReflDirInTS.y)  : ((1-outSamplePosInTS.y)/outReflDirInTS.y));
    outMaxDistance = min(outMaxDistance, outReflDirInTS.z<0 ? (-outSamplePosInTS.z/outReflDirInTS.z) : ((1-outSamplePosInTS.z)/outReflDirInTS.z));
    return outMaxDistance;
}

uint2 getTexelForUVandLOD(float2 uv, uint LOD, uint2 textureDim) {
    uint Yoffset = getYOffset(LOD, textureDim.y);
    uint YoffsetNext = getYOffset(LOD + 1, textureDim.y);
    float pow2 = computePowerOfTwo(LOD);
    uint2 lodTextureDim = uint2(textureDim.x/pow2, YoffsetNext - Yoffset);
    uint2 texelLocation = lodTextureDim * uv - 0.5f;
    texelLocation += uint2(0, Yoffset);
    return texelLocation;
}

float3 evaluateSSR(float3 normal, float3 pos, float3 v, float2 currentUV, uint2 screenDim, uint2 depthTextureDim) {
    uint MAX_ITER = 512;
    float WORLD_BOUNDS = 20;
    float stepSize = 1.f/MAX_ITER;
    float3 r = reflect(-v, normal);
    float3 reflectionDirTS = getReflectionDirInTS(pos, r);
    float MAX_THICKNESS = 0.3f;
    // return reflectionDirTS;

    float3 p0 = getNDCCoordinates(pos + r * 0.001f);
    float3 p1 = p0;
    float maxLengthRayIntersection = findRaySquareIntersectionDist(p0, reflectionDirTS);
    float3 reflectionEndPointTS = p0 + maxLengthRayIntersection * reflectionDirTS;
    float3 dp = reflectionEndPointTS - p0;
    
    int2 p0ScreenPos = p0.xy * screenDim;
    int2 endScreenPos = reflectionEndPointTS.xy * screenDim;
    int2 pixelDirection = endScreenPos - p0ScreenPos;
    uint maxPixelsCovered = max(abs(pixelDirection.x), abs(pixelDirection.y));
    dp *= 2.f/maxPixelsCovered;

    // return abs(reflectionEndPointTS);

    // dp = normalize(dp);

    for(int i=0; i<MAX_ITER; i++) {
        p1 = p0 + dp;
        float3 ndcP1 = p1;// getNDCCoordinates(p1);
        // if(abs(p1.x) > WORLD_BOUNDS || abs(p1.y) > WORLD_BOUNDS || abs(p1.z) > WORLD_BOUNDS )
        //     return 0;
        if(ndcP1.x < 0 || ndcP1.y < 0 || ndcP1.y > 1 || ndcP1.x > 1 )
            return 0;
        float2 uvP1 = ndcP1.xy;
        float depthP1 = abs(ndcP1.z);
        float depthP1Linear = linearizeDepth(depthP1);
        if(depthP1Linear > 0.6f * 100.f)
            return 0;
        uint2 depthTexel = getTexelForUVandLOD(uvP1, 0, depthTextureDim);
        float actualDepth = depthMapHiZ[depthTexel].r;
        // float actualDepth = depthMap.SampleLevel(samplerPoint, uvP1, 0).r;
        if(actualDepth == 1.f) {
            p0 = p1;
            continue;
        }
        float3 normalAtP1 = normalsTexture.SampleLevel(samplerPoint, uvP1, 0).rgb;
        normalAtP1 = 2 * normalAtP1 - 1.0f;
        if(dot(r, normalAtP1) >= (-0.087155742f)) {
            p0 = p1;
            continue;
        }
        float actualDepthLinear = linearizeDepth(actualDepth);
        float thickness = depthP1Linear - actualDepthLinear;
        if(thickness >= 0 && thickness < MAX_THICKNESS) {
            // return 1;
            // float2 uvSSR = binarySearchDepth(p0, p1, actualDepth).xy;
            // uvSSR = currentUV.x > 0.5f ? uvP1 : uvSSR;
            // return float3(uvSSR, 0);
            float3 Li = diffuseTexture.SampleLevel(samplerPoint, uvP1, 0).rgb;
            return Li;
        }
        p0 = p1;
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
    uint depthTextureWidth, depthTextureHeight;
	depthMapHiZ.GetDimensions(depthTextureWidth, depthTextureHeight);
    uint2 depthTextureDim = uint2(depthTextureWidth, depthTextureHeight/2);
	float depth = depthMap.SampleLevel(samplerPoint, uv, 0).r;
    // renderTarget[DTid.xy] = float4(linearToSRGB(depth/20), 1);
    // return;

	float3 worldPos = calculateWorldPosition(depth, uv);
    float roughness = rmAO.r;
    float metallic = rmAO.g;

    float3 v = normalize(eye - worldPos);
    float3 n = normalize(normals);

    // float2 xi = sampleHammersley(DTid.y * outputWidth + DTid.x, 1.f/(outputWidth * outputHeight));
    float2 xi = noiseTexture.SampleLevel(samplerLinear, uv, 0).rg;
    float2 xi1 = noiseTexture.SampleLevel(samplerLinear, 1-uv, 0).rg;
    float2 xi2 = 1-xi;
    float2 xi3 = xi.yx;
    float2 xi4 = 1 - xi.yx;
    xi = float2(xi.r, xi1.r);
    // float3 h = importanceSampleGGX(xi, roughness * roughness, n);
    float3 h = sampleGGXVisible(n, roughness * roughness, xi);
    // float3 h2 = importanceSampleGGX(xi2, roughness * roughness, n);
    // float3 h3 = importanceSampleGGX(xi3, roughness * roughness, n);
    // float3 h4 = importanceSampleGGX(xi4, roughness * roughness, n);
    

    // float3 ssr = 0;
    float3 specular = 0;
    if(roughness < 2.9f && dot(n, float3(0, 1, 0)) > 0.1f) {
        float3 ssr = evaluateSSR(n, worldPos, v, uv, uint2(outputWidth, outputHeight), depthTextureDim);
        // float3 ssr2 = evaluateSSR(h2, worldPos, v, uv, uint2(outputWidth, outputHeight));
        // float3 ssr3 = evaluateSSR(h3, worldPos, v, uv, uint2(outputWidth, outputHeight));
        // float3 ssr4 = evaluateSSR(h4, worldPos, v, uv, uint2(outputWidth, outputHeight));
        // specular = (ssr + ssr2 + ssr3 + ssr4) / 4;
        specular = max(0, ssr);
    }

    float3 radiance = specular * 0.8f + 0.2f *diffuse;
    radiance = linearToSRGB(radiance);
	
    renderTarget[DTid.xy] = float4(radiance, 1);
}