#pragma once
#include <strsafe.h>
#include <wrl.h>
#include <atlconv.h>
#include <atltrace.h>
#include <atlexcept.h>

#include "GRS_WIC_Utility.h"

using namespace ATL;
using namespace Microsoft;
using namespace Microsoft::WRL;

// ��������
__inline BOOL LoadTextureFromMem( ID3D12GraphicsCommandList* pCMDList
    , const BYTE* pbImageData
    , const size_t& szImageBufferSize
    , const DXGI_FORMAT emTextureFormat
    , const UINT 		nTextureW
    , const UINT 		nTextureH
    , const UINT 		nPicRowPitch
    , ID3D12Resource*& pITextureUpload
    , ID3D12Resource*& pITexture )
{
    BOOL bRet = TRUE;
    try
    {
        ComPtr<ID3D12Device> pID3D12Device;
        ComPtr<ID3D12GraphicsCommandList> pICMDList( pCMDList );

        GRS_THROW_IF_FAILED( pICMDList->GetDevice( IID_PPV_ARGS( &pID3D12Device ) ) );

        D3D12_HEAP_PROPERTIES stTextureHeapProp = {};
        stTextureHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC	stTextureDesc = {};

        stTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        stTextureDesc.MipLevels = 1;
        stTextureDesc.Format = emTextureFormat;
        stTextureDesc.Width = nTextureW;
        stTextureDesc.Height = nTextureH;
        stTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        stTextureDesc.DepthOrArraySize = 1;
        stTextureDesc.SampleDesc.Count = 1;
        stTextureDesc.SampleDesc.Quality = 0;

        GRS_THROW_IF_FAILED( pID3D12Device->CreateCommittedResource(
            &stTextureHeapProp
            , D3D12_HEAP_FLAG_NONE
            , &stTextureDesc				//����ʹ��CD3DX12_RESOURCE_DESC::Tex2D���򻯽ṹ��ĳ�ʼ��
            , D3D12_RESOURCE_STATE_COPY_DEST
            , nullptr
            , IID_PPV_ARGS( &pITexture ) ) );

        //��ȡ��Ҫ���ϴ�����Դ����Ĵ�С������ߴ�ͨ������ʵ��ͼƬ�ĳߴ�
        D3D12_RESOURCE_DESC stDestDesc = pITexture->GetDesc();
        UINT64 n64UploadBufferSize = 0;
        pID3D12Device->GetCopyableFootprints( &stDestDesc, 0, 1, 0, nullptr, nullptr, nullptr, &n64UploadBufferSize );

        stTextureHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC stUploadTextureDesc = {};

        stUploadTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        stUploadTextureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        stUploadTextureDesc.Width = n64UploadBufferSize;
        stUploadTextureDesc.Height = 1;
        stUploadTextureDesc.DepthOrArraySize = 1;
        stUploadTextureDesc.MipLevels = 1;
        stUploadTextureDesc.Format = DXGI_FORMAT_UNKNOWN;
        stUploadTextureDesc.SampleDesc.Count = 1;
        stUploadTextureDesc.SampleDesc.Quality = 0;
        stUploadTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        stUploadTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        GRS_THROW_IF_FAILED( pID3D12Device->CreateCommittedResource(
            &stTextureHeapProp
            , D3D12_HEAP_FLAG_NONE
            , &stUploadTextureDesc
            , D3D12_RESOURCE_STATE_GENERIC_READ
            , nullptr
            , IID_PPV_ARGS( &pITextureUpload ) ) );

        //��ȡ���ϴ��ѿ����������ݵ�һЩ����ת���ߴ���Ϣ
        //���ڸ��ӵ�DDS�������Ƿǳ���Ҫ�Ĺ���
        UINT   nNumSubresources = 1u;  //����ֻ��һ��ͼƬ��������Դ����Ϊ1
        UINT   nTextureRowNum = 0u;
        UINT64 n64TextureRowSizes = 0u;
        UINT64 n64RequiredSize = 0u;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT	stTxtLayouts = {};

        pID3D12Device->GetCopyableFootprints( &stDestDesc
            , 0
            , nNumSubresources
            , 0
            , &stTxtLayouts
            , &nTextureRowNum
            , &n64TextureRowSizes
            , &n64RequiredSize );

        //��Ϊ�ϴ���ʵ�ʾ���CPU�������ݵ�GPU���н�
        //�������ǿ���ʹ����Ϥ��Map����������ӳ�䵽CPU�ڴ��ַ��
        //Ȼ�����ǰ��н����ݸ��Ƶ��ϴ�����
        //��Ҫע�����֮���԰��п�������ΪGPU��Դ���д�С
        //��ʵ��ͼƬ���д�С���в����,���ߵ��ڴ�߽����Ҫ���ǲ�һ����
        BYTE* pData = nullptr;
        GRS_THROW_IF_FAILED( pITextureUpload->Map( 0, NULL, reinterpret_cast<void**>( &pData ) ) );

        BYTE* pDestSlice = reinterpret_cast<BYTE*>( pData ) + stTxtLayouts.Offset;
        const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>( pbImageData );
        for ( UINT y = 0; y < nTextureRowNum; ++y )
        {
            memcpy( pDestSlice + static_cast<SIZE_T>( stTxtLayouts.Footprint.RowPitch ) * y
                , pSrcSlice + static_cast<SIZE_T>( nPicRowPitch ) * y
                , nPicRowPitch );
        }
        //ȡ��ӳ�� �����ױ��������ÿ֡�ı任��������ݣ�������������Unmap�ˣ�
        //������פ�ڴ�,������������ܣ���Ϊÿ��Map��Unmap�Ǻܺ�ʱ�Ĳ���
        //��Ϊ�������붼��64λϵͳ��Ӧ���ˣ���ַ�ռ����㹻�ģ�������ռ�ò���Ӱ��ʲô
        pITextureUpload->Unmap( 0, NULL );

        ////�ͷ�ͼƬ���ݣ���һ���ɾ��ĳ���Ա
        //GRS_SAFE_FREE((VOID*)pbImageData);

        D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
        stDstCopyLocation.pResource = pITexture;
        stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        stDstCopyLocation.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
        stSrcCopyLocation.pResource = pITextureUpload;
        stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        stSrcCopyLocation.PlacedFootprint = stTxtLayouts;

        pICMDList->CopyTextureRegion( &stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr );

        //����һ����Դ���ϣ�ͬ����ȷ�ϸ��Ʋ������
        //ֱ��ʹ�ýṹ��Ȼ����õ���ʽ
        D3D12_RESOURCE_BARRIER stResBar = {};
        stResBar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        stResBar.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        stResBar.Transition.pResource = pITexture;
        stResBar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        stResBar.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        stResBar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        pICMDList->ResourceBarrier( 1, &stResBar );
    }
    catch ( CAtlException& e )
    {//������COM�쳣
        e;
        bRet = FALSE;
    }
    catch ( ... )
    {
        bRet = FALSE;
    }

    return bRet;
}


// ��������
__inline BOOL LoadTextureFromFile(
    LPCWSTR pszTextureFile
    , ID3D12GraphicsCommandList* pCMDList
    , ID3D12Resource*& pITextureUpload
    , ID3D12Resource*& pITexture )
{
    BOOL bRet = TRUE;
    try
    {
        BYTE*       pbImageData = nullptr;
        size_t		szImageBufferSize = 0;
        DXGI_FORMAT emTextureFormat = DXGI_FORMAT_UNKNOWN;
        UINT 		nTextureW = 0;
        UINT 		nTextureH = 0;
        UINT 		nPicRowPitch = 0;

        bRet = WICLoadImageFromFile( pszTextureFile
            , emTextureFormat
            , nTextureW
            , nTextureH
            , nPicRowPitch
            , pbImageData
            , szImageBufferSize );

        if ( bRet )
        {
            bRet = LoadTextureFromMem( pCMDList
                , pbImageData
                , szImageBufferSize
                , emTextureFormat
                , nTextureW
                , nTextureH
                , nPicRowPitch
                , pITextureUpload
                , pITexture
            );

            GRS_SAFE_FREE( pbImageData );
        }
    }
    catch ( CAtlException& e )
    {//������COM�쳣
        e;
        bRet = FALSE;
    }
    catch ( ... )
    {
        bRet = FALSE;
    }
    return bRet;
}