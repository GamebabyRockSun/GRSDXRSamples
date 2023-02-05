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

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingHlslCompat.h"

RWTexture2D<float4>						g_RenderTarget  : register(u0);

RaytracingAccelerationStructure			g_asScene       : register(t0, space0);

ByteAddressBuffer						g_Indices       : register(t1, space0);
StructuredBuffer<ST_GRS_VERTEX>			g_Vertices      : register(t2, space0);

Texture2D<float4>						g_texture       : register(t3);
Texture2D<float3>						g_normalmap     : register(t4);

SamplerState							g_sampler       : register(s0);

ConstantBuffer<ST_SCENE_CONSANTBUFFER>	g_stSceneCB     : register(b0);
ConstantBuffer<ST_MODULE_CONSANTBUFFER> g_stModuleCB    : register(b1);



// Load three 16 bit indices from a byte addressed buffer.
uint3 Load3x16BitIndices(uint offsetBytes)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;    
    const uint2 four16BitIndices = g_Indices.Load2(dwordAlignedOffset);
 
    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

// Retrieve hit world m_vNor.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(float2 v2PixelSize,uint2 index, out float3 origin, out float3 direction)
{
	float2 v2Origin = float2(
		v2PixelSize.x * (index.x - 0.5f * (DispatchRaysDimensions().x - 1.0f))
		,- v2PixelSize.y * (index.y - 0.5f * (DispatchRaysDimensions().y - 1.0f)));

	float4 world = mul( g_stSceneCB.m_mxView,float4(v2Origin, 1.0f, 1.0f));
    world.xyz /= world.w;

    origin = g_stSceneCB.m_vCameraPos.xyz;
	direction = normalize(world.xyz - origin);
}

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 vNor)
{
    float3 pixelToLight = normalize(g_stSceneCB.m_vLightPos.xyz - hitPosition);

    // Diffuse contribution.
    float fNDotL = max(0.0f, dot(pixelToLight, vNor));

    return g_stModuleCB.m_vAlbedo * g_stSceneCB.m_vLightDiffuseColor * fNDotL;
}

[shader("raygeneration")]
void MyRaygenShader()
{
    float3 rayDir;
    float3 origin;
    
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    GenerateCameraRay(g_stSceneCB.m_v2PixelSize,DispatchRaysIndex().xy, origin, rayDir);

    // Trace the ray.
    // Set the ray's extents.
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = rayDir;
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    RayPayload payload = { float4(0, 0, 0, 0) };
    TraceRay(g_asScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    // Write the raytraced color to the output texture.
    g_RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();

    // Get the base index of the triangle's first 16 bit index.
    uint indexSizeInBytes = 2;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;

    // Load up 3 16 bit indices for the triangle.
    const uint3 indices = Load3x16BitIndices(baseIndex);

	float2 txCod = g_Vertices[indices[0]].m_vTex +
		attr.barycentrics.x * (g_Vertices[indices[1]].m_vTex - g_Vertices[indices[0]].m_vTex) +
		attr.barycentrics.y * (g_Vertices[indices[2]].m_vTex - g_Vertices[indices[0]].m_vTex);

	//直接从Normal Map中读取法线
    float3 triangleNormal = g_normalmap.SampleLevel(g_sampler, txCod.xy, 0).rgb;
	triangleNormal = -1.0f * (2.0f * triangleNormal - 1.0f);
	triangleNormal = mul(ObjectToWorld3x4(), float4(triangleNormal,0.0f));
	triangleNormal = normalize(triangleNormal);

    float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);

	float4 txColor = g_texture.SampleLevel(g_sampler, txCod.xy,0);

	payload.color = txColor * diffuseColor + g_stSceneCB.m_vLightAmbientColor;
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float4 background = float4(0.2f, 0.5f, 1.0f, 1.0f);
    payload.color = background;
}

#endif // RAYTRACING_HLSL