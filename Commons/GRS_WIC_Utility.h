#pragma once
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <wincodec.h> //for WIC
#include <d3d12.h> //for d3d12
#include <wrl.h> //���WTL֧�� ����ʹ��COM
#include <atlexcept.h>

#include "GRS_Def.h"
#include "GRS_Mem.h"

using namespace ATL;
using namespace Microsoft;
using namespace Microsoft::WRL;

struct WICTranslate
{
	GUID wic;
	DXGI_FORMAT format;
};

static WICTranslate g_WICFormats[] =
{//WIC��ʽ��DXGI���ظ�ʽ�Ķ�Ӧ���ñ��еĸ�ʽΪ��֧�ֵĸ�ʽ
	{ GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

	{ GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
	{ GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

	{ GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
	{ GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

	{ GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

	{ GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
	{ GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

	{ GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
	{ GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
	{ GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
	{ GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

	{ GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
};

// WIC ���ظ�ʽת����.
struct WICConvert
{
	GUID source;
	GUID target;
};

static WICConvert g_WICConvert[] =
{
	// Ŀ���ʽһ������ӽ��ı�֧�ֵĸ�ʽ
	{ GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
	{ GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
	//// 2021-06-12
	//{ GUID_WICPixelFormat8bppGray,				GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
	{ GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

	{ GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

	{ GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

	{ GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

	{ GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

	{ GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
};

bool GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{//���ȷ�����ݵ���ӽ���ʽ���ĸ�
	*pTargetFormat = *pSourceFormat;
	for (size_t i = 0; i < _countof(g_WICConvert); ++i)
	{
		if (InlineIsEqualGUID(g_WICConvert[i].source, *pSourceFormat))
		{
			*pTargetFormat = g_WICConvert[i].target;
			return true;
		}
	}
	return false;
}

DXGI_FORMAT GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat)
{//���ȷ�����ն�Ӧ��DXGI��ʽ����һ��
	for (size_t i = 0; i < _countof(g_WICFormats); ++i)
	{
		if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
		{
			return g_WICFormats[i].format;
		}
	}
	return DXGI_FORMAT_UNKNOWN;
}

// ��������
__inline BOOL WICLoadImageFromFile(LPCWSTR pszTextureFile
	, DXGI_FORMAT& emTextureFormat
	, UINT& nTextureW
	, UINT& nTextureH
	, UINT& nPicRowPitch
	, BYTE*& pbImageData
	, size_t& szBufferSize )
{
	BOOL bRet = TRUE;
	try
	{
		static ComPtr<IWICImagingFactory>	pIWICFactory;
		ComPtr<IWICBitmapDecoder>			pIWICDecoder;
		ComPtr<IWICBitmapFrameDecode>		pIWICFrame;
		ComPtr<IWICBitmapSource>			pIBMP;

		UINT								nBPP = 0;

		USES_CONVERSION;
		//ʹ�ô�COM��ʽ����WIC�೧����Ҳ�ǵ���WIC��һ��Ҫ��������
		GRS_THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory
			, nullptr
			, CLSCTX_INPROC_SERVER
			, IID_PPV_ARGS(&pIWICFactory)));

		WCHAR pszTextureFileName[MAX_PATH] = {};
		StringCchCopyW(pszTextureFileName, MAX_PATH, pszTextureFile);

		GRS_THROW_IF_FAILED(pIWICFactory->CreateDecoderFromFilename(
			pszTextureFileName,						// �ļ���
			NULL, 		// ��ָ����������ʹ��Ĭ��
			GENERIC_READ, // ����Ȩ��
			WICDecodeMetadataCacheOnDemand,			// ����Ҫ�ͻ������� 
			&pIWICDecoder // ����������
		));

		// ��ȡ��һ֡ͼƬ(��ΪGIF�ȸ�ʽ�ļ����ܻ��ж�֡ͼƬ�������ĸ�ʽһ��ֻ��һ֡ͼƬ)
		// ʵ�ʽ���������������λͼ��ʽ����
		GRS_THROW_IF_FAILED(pIWICDecoder->GetFrame(0, &pIWICFrame));

		WICPixelFormatGUID wpf = {};
		//��ȡWICͼƬ��ʽ
		GRS_THROW_IF_FAILED(pIWICFrame->GetPixelFormat(&wpf));
		// �����Ƿ����ֱ��ʹ��
		emTextureFormat = GetDXGIFormatFromPixelFormat( &wpf );
		GUID tgFormat = { wpf };
		if ( DXGI_FORMAT_UNKNOWN == emTextureFormat )
		{// ֱ�Ӳ�֧�ֵ�ͼƬ��ʽ ����һ�¼�����ת��
		 // һ����ʵ�ʵ����浱�ж����ṩ�����ʽת�����ߣ�
		 // ͼƬ����Ҫ��ǰת���ã����Բ�����ֲ�֧�ֵ�����
			//ͨ����һ��ת��֮���ȡDXGI�ĵȼ۸�ʽ
			if ( GetTargetPixelFormat( &wpf, &tgFormat ) )
			{
				emTextureFormat = GetDXGIFormatFromPixelFormat( &tgFormat );
				ComPtr<IWICFormatConverter> pIConverter;
				GRS_THROW_IF_FAILED( pIWICFactory->CreateFormatConverter( &pIConverter ) );

				//��ʼ��һ��ͼƬת������ʵ��Ҳ���ǽ�ͼƬ���ݽ����˸�ʽת��
				GRS_THROW_IF_FAILED( pIConverter->Initialize(
					pIWICFrame.Get(),                // ����ԭͼƬ����
					tgFormat,						 // ָ����ת����Ŀ���ʽ
					WICBitmapDitherTypeNone,         // ָ��λͼ�Ƿ��е�ɫ�壬�ִ��������λͼ�����õ�ɫ�壬����ΪNone
					NULL,                            // ָ����ɫ��ָ��
					0.f,                             // ָ��Alpha��ֵ
					WICBitmapPaletteTypeCustom       // ��ɫ�����ͣ�ʵ��û��ʹ�ã�����ָ��ΪCustom
				) );
				// ����QueryInterface������ö����λͼ����Դ�ӿ�
				GRS_THROW_IF_FAILED( pIConverter.As( &pIBMP ) );
			}
		}
		else
		{
			//��ȡλͼ����Դ�ӿ�
			GRS_THROW_IF_FAILED( pIWICFrame.As( &pIBMP ) );
		}		
		//���ͼƬ��С����λ�����أ�
		GRS_THROW_IF_FAILED(pIBMP->GetSize(&nTextureW, &nTextureH));

		//��ȡͼƬ���ص�λ��С��BPP��Bits Per Pixel����Ϣ�����Լ���ͼƬ�����ݵ���ʵ��С����λ���ֽڣ�
		ComPtr<IWICComponentInfo> pIWICmntinfo;
		GRS_THROW_IF_FAILED(pIWICFactory->CreateComponentInfo(tgFormat, pIWICmntinfo.GetAddressOf()));

		WICComponentType type;
		GRS_THROW_IF_FAILED(pIWICmntinfo->GetComponentType(&type));

		if (type != WICPixelFormat)
		{
			AtlThrow(S_FALSE);
		}

		ComPtr<IWICPixelFormatInfo> pIWICPixelinfo;
		GRS_THROW_IF_FAILED(pIWICmntinfo.As(&pIWICPixelinfo));

		// ���������ڿ��Եõ�BPP�ˣ���Ҳ���ҿ��ıȽ���Ѫ�ĵط���Ϊ��BPP��Ȼ������ô�໷��
		GRS_THROW_IF_FAILED(pIWICPixelinfo->GetBitsPerPixel(&nBPP));

		// ����ͼƬʵ�ʵ��д�С����λ���ֽڣ�������ʹ����һ����ȡ����������A+B-1��/B ��
		// ����������˵��΢���������,ϣ�����Ѿ���������ָ��
		nPicRowPitch = (UINT)GRS_UPPER_DIV(uint64_t(nTextureW) * uint64_t(nBPP), 8);

		size_t szImageBuffer = nPicRowPitch * nTextureH;

		//����ʵ��ͼƬ���ݴ洢���ڴ��С
		BYTE* pbPicData = (BYTE*)GRS_CALLOC(szImageBuffer);
		if (nullptr == pbPicData)
		{
			AtlThrowLastWin32();
		}

		//��ͼƬ�ж�ȡ������
		GRS_THROW_IF_FAILED(pIBMP->CopyPixels(nullptr
			, nPicRowPitch
			, static_cast<UINT>(szImageBuffer)
			, pbPicData) );

		pbImageData = pbPicData;
		szBufferSize = szImageBuffer;
	}
	catch (CAtlException& e)
	{//������COM�쳣
		e;
		bRet = FALSE;
	}
	catch (...)
	{
		bRet = FALSE;
	}

	return bRet;
}