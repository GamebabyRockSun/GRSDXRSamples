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

#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;

// Shader will use byte encoding to access indices.
typedef UINT16 GRS_TYPE_INDEX;
#endif

struct ST_SCENE_CONSANTBUFFER
{
    XMMATRIX m_mxView;
    XMVECTOR m_vCameraPos;

    XMVECTOR m_vLightPos;
    XMVECTOR m_vLightAmbientColor;
    XMVECTOR m_vLightDiffuseColor;

	XMFLOAT2 m_v2PixelSize;
};

struct ST_MODULE_CONSANTBUFFER
{
    XMFLOAT4 m_vAlbedo;
};

// ¶¥µã½á¹¹
struct ST_GRS_VERTEX
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};


#endif // RAYTRACINGHLSLCOMPAT_H