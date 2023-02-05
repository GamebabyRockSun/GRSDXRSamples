#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <wrl.h>  //���WTL֧�� ����ʹ��COM
#include <strsafe.h>
#include <atlbase.h>
#include <atlcoll.h> //for atl array
#include <atlconv.h> //for T2A
#include <fstream>  //for ifstream
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3d12.h>//for d3d12
#include <d3d12shader.h>
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "..\Commons\GRSWICHelper.cpp"
#include "..\Commons\GRSMem.h"
#include "..\Commons\GRSCOMException.h"
#include "..\Commons\DDSTextureLoader12.h"
#include "Shader\RayTracingHlslCompat.h" //shader �� C++������ʹ����ͬ��ͷ�ļ����峣���ṹ�� �Լ�����ṹ���

#include "../RayTracingFallback/Libraries/D3D12RaytracingFallback/Include/d3dx12.h"
#include "../RayTracingFallback/Libraries/D3D12RaytracingFallback/Include/d3d12_1.h"

#if defined(_DEBUG)
#include "Debug/x64/CompiledShaders/Raytracing.hlsl.h"
#else
#include "Release/x64/CompiledShaders/Raytracing.hlsl.h"
#endif

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

//------------------------------------------------------------------------------------------------------------
//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
//------------------------------------------------------------------------------------------------------------

#define GRS_WND_CLASS_NAME _T("Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 RayTracing Base Sample")

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

#define GRS_UPPER_SIZEOFUINT32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) { HRESULT _hr = (hr); if (FAILED(_hr)){ throw CGRSCOMException(_hr); } }
#define GRS_THROW_IF_FALSE(b) {  if (!(b)){ throw CGRSCOMException(0xF0000001); } }

//����̨������������
#if defined(_DEBUG)
//ʹ�ÿ���̨���������Ϣ��������̵߳���
#define GRS_INIT_OUTPUT() 	if (!::AllocConsole()){throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));}
//#define GRS_FREE_OUTPUT()	::_tsystem(_T("PAUSE"));\
//							::FreeConsole();
#define GRS_FREE_OUTPUT()	::FreeConsole();
#define GRS_USEPRINTF() TCHAR pBuf[1024] = {};TCHAR pszOutput[1024] = {};
#define GRS_PRINTF(...) \
    StringCchPrintf(pBuf,1024,__VA_ARGS__);\
	StringCchPrintf(pszOutput,1024,_T("��ID=0x%04x����%s"),::GetCurrentThreadId(),pBuf);\
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),pszOutput,lstrlen(pszOutput),NULL,NULL);
#else
#define GRS_INIT_OUTPUT()
#define GRS_FREE_OUTPUT()
#define GRS_USEPRINTF()
#define GRS_PRINTF(...)
#endif
//------------------------------------------------------------------------------------------------------------
// Ϊ�˵��Լ�����������������ͺ궨�壬Ϊÿ���ӿڶ����������ƣ�����鿴�������
#if defined(_DEBUG)
inline void GRS_SetD3D12DebugName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}

inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		pObject->SetName(_DebugName);
	}
}
#else

inline void GRS_SetD3D12DebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_D3D12_DEBUGNAME(x)						GRS_SetD3D12DebugName(x, L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED(x, n)			GRS_SetD3D12DebugNameIndexed(x[n], L#x, n)

#define GRS_SET_D3D12_DEBUGNAME_COMPTR(x)				GRS_SetD3D12DebugName(x.Get(), L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(x, n)	GRS_SetD3D12DebugNameIndexed(x[n].Get(), L#x, n)

#if defined(_DEBUG)
inline void GRS_SetDXGIDebugName(IDXGIObject* pObject, LPCWSTR name)
{
	size_t szLen = 0;
	StringCchLengthW(name, 50, &szLen);
	pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen - 1), name);
}

inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject* pObject, LPCWSTR name, UINT index)
{
	size_t szLen = 0;
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		StringCchLengthW(_DebugName, _countof(_DebugName), &szLen);
		pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen), _DebugName);
	}
}
#else

inline void GRS_SetDXGIDebugName(IDXGIObject*, LPCWSTR)
{
}
inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_DXGI_DEBUGNAME(x)						GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)				GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)
//------------------------------------------------------------------------------------------------------------

//ȫ�ֹ�Դ��Ϣ����
XMFLOAT4 g_v4LightPosition = XMFLOAT4(0.0f, 1.8f, -3.0f, 0.0f);
XMFLOAT4 g_v4LightAmbientColor = XMFLOAT4(0.01f, 0.01f, 0.01f, 1.0f);
XMFLOAT4 g_v4LightDiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

//ȫ���������Ϣ����
XMVECTOR g_vEye = { 0.0f,0.0f,-50.0f,0.0f };
XMVECTOR g_vLookAt = { 0.0f,-1.0f,0.0f };
XMVECTOR g_vUp = { 0.0f,1.0f,0.0f,0.0f };

//��Ļ���ص�ߴ�
float g_fPixelSize = 0.0030f;
float g_fPixelSizeDelta = 0.0001f;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, GRS_TYPE_INDEX*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	int											iWidth = 1024;
	int											iHeight = 768;

	HWND										hWnd = nullptr;
	MSG											stMsg = {};
	TCHAR										pszAppPath[MAX_PATH] = {};

	const UINT									c_nFrameBackBufCount = 3u;
	UINT										nCurFrameIndex = 0;
	UINT										nDXGIFactoryFlags = 0U;
	DXGI_FORMAT									emfmtBackBuffer = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT									fmtDepthStencil = DXGI_FORMAT_D24_UNORM_S8_UINT;
	const float									c_faClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };
	CD3DX12_VIEWPORT							stViewPort(0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight));
	CD3DX12_RECT								stScissorRect(0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight));

	ComPtr<IDXGIFactory5>						pIDXGIFactory5;
	ComPtr<IDXGIFactory6>						pIDXGIFactory6;
	ComPtr<IDXGIAdapter1>						pIDXGIAdapter1;
	ComPtr<ID3D12Device4>						pID3D12Device4;

	ComPtr<ID3D12CommandQueue>					pICMDQueue;
	ComPtr<ID3D12CommandAllocator>				pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>			pICMDList;
	ComPtr<ID3D12Fence1>						pIFence1;

	ComPtr<ID3D12DescriptorHeap>				pIRTVHeap;   //Render Target View

	ComPtr<IDXGISwapChain1>						pISwapChain1;
	ComPtr<IDXGISwapChain3>						pISwapChain3;

	ComPtr<ID3D12Resource>						pIRenderTargetBufs[c_nFrameBackBufCount];

	//=====================================================================================================================
	//DXR �ӿ�
	ComPtr<ID3D12Device5>						pID3D12DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4>			pIDXRCmdList;
	ComPtr<ID3D12StateObjectPrototype>			pIDXRPSO;

	ComPtr<ID3D12Resource>						pIDXRUAVBufs;
	ComPtr<ID3D12DescriptorHeap>				pIDXRUAVHeap;		//UAV Output��VB�� IB�� Scene CB�� Module CB

	//��ǩ�� DXR����Ҫ������ǩ����һ����ȫ�ֵĸ�ǩ������һ�����Ǿֲ��ĸ�ǩ��
	ComPtr<ID3D12RootSignature>					pIRSGlobal;
	ComPtr<ID3D12RootSignature>					pIRSLocal;

	ComPtr<ID3DBlob>							pIDXRHLSLCode; //Ray Tracing Shader Code

	ComPtr<ID3D12Resource>						pIUAVBottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource>						pIUAVTopLevelAccelerationStructure;
	ComPtr<ID3D12Resource>						pIUAVScratchResource;
	ComPtr<ID3D12Resource>						pIUploadBufInstanceDescs;

	ComPtr<ID3D12Heap1>							pIHeapShaderTable;
	ComPtr<ID3D12Resource>						pIRESMissShaderTable;
	ComPtr<ID3D12Resource>						pIRESHitGroupShaderTable;
	ComPtr<ID3D12Resource>						pIRESRayGenShaderTable;

	const wchar_t*								c_pszHitGroupName = L"MyHitGroup";
	const wchar_t*								c_pszRaygenShaderName = L"MyRaygenShader";
	const wchar_t*								c_pszClosestHitShaderName = L"MyClosestHitShader";
	const wchar_t*								c_pszMissShaderName = L"MyMissShader";
	//=====================================================================================================================

	ComPtr<ID3D12Resource>						pIVBBufs;			//ST_GRS_VERTEX Buffer
	ComPtr<ID3D12Resource>						pIIBBufs;			//GRS_TYPE_INDEX Buffer
	ComPtr<ID3D12Resource>						pITexture;			//Earth Texture
	ComPtr<ID3D12Resource>						pITextureUpload;	//Earth Texture Upload
	ComPtr<ID3D12Resource>						pINormalMap;		//Normal Map Texture
	ComPtr<ID3D12Resource>						pINormalMapUpload;	//Normal Map Texture Upload
	ComPtr<ID3D12DescriptorHeap>				pISampleHeap;		//Sample Heap

	ComPtr<ID3D12Resource>						pICBFrameConstant;

	UINT										nRTVDescriptorSize = 0;
	UINT										nSRVDescriptorSize = 0;

	const UINT									c_nDSHIndxUAVOutput = 0;
	const UINT									c_nDSHIndxIBView = 1;
	const UINT									c_nDSHIndxVBView = 2;
	const UINT									c_nDSHIndxCBScene = 3;
	const UINT									c_nDSHIndxCBModule = 4;
	const UINT									c_nDSHIndxASBottom1 = 5;
	const UINT									c_nDSHIndxASBottom2 = 6;
	const UINT									c_nDSNIndxTexture = 7;
	const UINT									c_nDSNIndxNormal = 8;
	UINT										nMaxDSCnt = 9;

	const UINT									c_nMaxSampleCnt = 6;

	ST_MODULE_CONSANTBUFFER						stCBModule = { XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) };
	ST_SCENE_CONSANTBUFFER*						pstCBScene = nullptr;

	D3D_FEATURE_LEVEL							emMinFeature = D3D_FEATURE_LEVEL_12_1;
	HANDLE										hEventFence1 = nullptr;
	UINT64										n64FenceValue = 0i64;
	CAtlArray<HANDLE>							arWaitHandles;

	BOOL										bISDXRSupport = FALSE;

	ST_GRS_VERTEX*								pstVertices = nullptr;
	GRS_TYPE_INDEX*								pnIndices = nullptr;
	UINT										nVertexCnt = 0;
	UINT										nIndexCnt = 0;
	D3D12_RAYTRACING_INSTANCE_DESC*				pstInstanceDesc = nullptr;

	GRS_USEPRINTF();
	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));  //for WIC & COM
		GRS_INIT_OUTPUT();

		// �õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
		{
			UINT nBytes = GetCurrentDirectory(MAX_PATH, pszAppPath);
			if (MAX_PATH == nBytes)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			WCHAR* lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{
				*(lastSlash + 1) = _T('\0');
			}
		}

		// ע�Ტ��������
		{
			WNDCLASSEX wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_GLOBALCLASS;
			wcex.lpfnWndProc = WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//��ֹ���ĵı����ػ�
			wcex.lpszClassName = GRS_WND_CLASS_NAME;

			::RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWidth, iHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// ���㴰�ھ��е���Ļ����
			INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
			INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

			hWnd = CreateWindowW(
				GRS_WND_CLASS_NAME
				, GRS_WND_TITLE
				, dwWndStyle
				, posX
				, posY
				, rtWnd.right - rtWnd.left
				, rtWnd.bottom - rtWnd.top
				, nullptr
				, nullptr
				, hInstance
				, nullptr);

			if (!hWnd)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		//����ʾ��ϵͳ�ĵ���֧��
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// �򿪸��ӵĵ���֧��
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		//����DXGI Factory����
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags
				, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(
				hWnd
				, DXGI_MWA_NO_ALT_ENTER));

			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		//ö����ʾ��������ѡ���Կ�����ΪҪDXR ����ѡ��������ǿ�ģ�
		{//ע�� ���������½ӿ�IDXGIFactory6���·���EnumAdapterByGpuPreference
			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(
				0
				, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
				, IID_PPV_ARGS(&pIDXGIAdapter1)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIAdapter1);
		}

		//=====================================================================================================================
		// �ӱ�����ʼ����׷��Ⱦ���ֵ�ʾ��������ʹ��Fallback��ʽ�����Ӳ����֧��DXR����ô��ֱ���˳�
		{
			// ����һ���豸���Կ�DX12�в���
			HRESULT hr2 = D3D12CreateDevice(pIDXGIAdapter1.Get()
				, emMinFeature
				, IID_PPV_ARGS(&pID3D12Device4));

			if (!SUCCEEDED(hr2))
			{
				::MessageBox(hWnd
					, _T("�ǳ���Ǹ��֪ͨ����\r\n��ϵͳ����NB���Կ�Ҳ����֧��DX12������û���������У�\r\n�����˳���")
					, GRS_WND_TITLE
					, MB_OK | MB_ICONINFORMATION);

				return -1;
			}

			DXGI_ADAPTER_DESC1 stAdapterDesc = {};
			pIDXGIAdapter1->GetDesc1(&stAdapterDesc);

			D3D12_FEATURE_DATA_D3D12_OPTIONS5 stFeatureSupportData = {};
			HRESULT hr = pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5
				, &stFeatureSupportData
				, sizeof(stFeatureSupportData));

			//���Ӳ���Ƿ���ֱ��֧��DXR
			bISDXRSupport = SUCCEEDED(hr)
				&& (stFeatureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);

			if (bISDXRSupport)
			{
				GRS_PRINTF(_T("��ϲ�������Կ���%s��ֱ��֧��DXR��\n"), stAdapterDesc.Description);
			}
			else
			{
				::MessageBox(hWnd
					, _T("�ǳ���Ǹ��֪ͨ����\r\n��ϵͳ����NB���Կ���֧��Ӳ��DXR������û���������С�\r\n�����˳���")
					, GRS_WND_TITLE
					, MB_OK | MB_ICONINFORMATION);

				return -1;
			}
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12Device4);

			//����ʹ�õ��豸������ʾ�����ڱ�����
			TCHAR pszWndTitle[MAX_PATH] = {};
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle, MAX_PATH, _T("%s( Device: %s )"), pszWndTitle, stAdapterDesc.Description);
			::SetWindowText(hWnd, pszWndTitle);

		}
		//=====================================================================================================================

		//����������С�����������������б�
		{
			D3D12_COMMAND_QUEUE_DESC stCMDQueueDesc = {};
			stCMDQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			stCMDQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(
				&stCMDQueueDesc
				, IID_PPV_ARGS(&pICMDQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDQueue);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAlloc);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(
				0
				, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAlloc.Get()
				, nullptr
				, IID_PPV_ARGS(&pICMDList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDList);
		}

		//=====================================================================================================================
		//����DXR�豸���Լ�DXR�����б�
		{// DirectX Raytracing
			GRS_THROW_IF_FAILED(pID3D12Device4->QueryInterface(IID_PPV_ARGS(&pID3D12DXRDevice)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12DXRDevice);
			GRS_THROW_IF_FAILED(pICMDList->QueryInterface(IID_PPV_ARGS(&pIDXRCmdList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRCmdList);
		}
		//=====================================================================================================================

		//����Fence
		{
			// ����һ��ͬ�����󡪡�Χ�������ڵȴ���Ⱦ��ɣ���Ϊ����Draw Call���첽����
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence1)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIFence1);

			// ����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
			hEventFence1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (nullptr == hEventFence1)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			arWaitHandles.Add(hEventFence1);
		}

		//����RTV
		{
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = c_nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVHeap);

			nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		//����������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = c_nFrameBackBufCount;
			stSwapChainDesc.Width = iWidth;
			stSwapChainDesc.Height = iHeight;
			stSwapChainDesc.Format = emfmtBackBuffer;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory6->CreateSwapChainForHwnd(
				pICMDQueue.Get(),
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			nCurFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < c_nFrameBackBufCount; i++)
			{//���ѭ����©����������ʵ�����Ǹ�����ı���
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i
					, IID_PPV_ARGS(&pIRenderTargetBufs[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIRenderTargetBufs, i);
				pID3D12Device4->CreateRenderTargetView(pIRenderTargetBufs[i].Get()
					, nullptr
					, stRTVHandle);
				stRTVHandle.Offset(1, nRTVDescriptorSize);
			}
		}

		//������Ⱦ��������Ҫ�ĸ�����������
		{
			D3D12_DESCRIPTOR_HEAP_DESC stDXRDescriptorHeapDesc = {};
			// ���9��������:
			// 2 - �������������� SRVs
			// 1 - ��׷��Ⱦ������� SRV
			// 2 - ���ٽṹ�Ļ��� UAVs
			// 2 - ���ٽṹ�����ݻ��� UAVs ��Ҫ����fallback��
			// 2 - �����Normal Map��������
			stDXRDescriptorHeapDesc.NumDescriptors = nMaxDSCnt;
			stDXRDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stDXRDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stDXRDescriptorHeapDesc
				, IID_PPV_ARGS(&pIDXRUAVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRUAVHeap);

			nSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			//������������������
			D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
			stSamplerHeapDesc.NumDescriptors = c_nMaxSampleCnt;
			stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(
				&stSamplerHeapDesc
				, IID_PPV_ARGS(&pISampleHeap)));

		}

		//=====================================================================================================================
		//����RayTracing Render Target UAV
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Tex2D(emfmtBackBuffer
					, iWidth
					, iHeight
					, 1, 1, 1, 0
					, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
				, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				, nullptr
				, IID_PPV_ARGS(&pIDXRUAVBufs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRUAVBufs);

			//UAV Heap�ĵ�һ�������� �ͷ�UAV Ҳ���� DXR��Output
			CD3DX12_CPU_DESCRIPTOR_HANDLE stUAVDescriptorHandle(
				pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
				, c_nDSHIndxUAVOutput
				, nSRVDescriptorSize);
			D3D12_UNORDERED_ACCESS_VIEW_DESC stUAVDesc = {};
			stUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			pID3D12Device4->CreateUnorderedAccessView(pIDXRUAVBufs.Get()
				, nullptr
				, &stUAVDesc
				, stUAVDescriptorHandle);
		}

		//������ǩ�� ע��DXR����������ǩ����һ����ȫ�֣�Global����ǩ����һ���Ǳ��أ�Local����ǩ��
		{
			CD3DX12_DESCRIPTOR_RANGE stRanges[4] = {}; // Perfomance TIP: Order from most frequent to least frequent.
			stRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
			stRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.
			stRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);  // 2 Texture and Normal Texture.
			stRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER stGlobalRootParams[6];
			stGlobalRootParams[0].InitAsDescriptorTable(1, &stRanges[0]);
			stGlobalRootParams[1].InitAsShaderResourceView(0);
			stGlobalRootParams[2].InitAsConstantBufferView(0);
			stGlobalRootParams[3].InitAsDescriptorTable(1, &stRanges[1]);
			stGlobalRootParams[4].InitAsDescriptorTable(1, &stRanges[2]);
			stGlobalRootParams[5].InitAsDescriptorTable(1, &stRanges[3]);

			CD3DX12_ROOT_SIGNATURE_DESC stGlobalRootSignatureDesc(ARRAYSIZE(stGlobalRootParams), stGlobalRootParams);

			CD3DX12_ROOT_PARAMETER stLocalRootParams[1] = {};
			stLocalRootParams[0].InitAsConstants(GRS_UPPER_SIZEOFUINT32(ST_MODULE_CONSANTBUFFER), 1);
			CD3DX12_ROOT_SIGNATURE_DESC stLocalRootSignatureDesc(ARRAYSIZE(stLocalRootParams), stLocalRootParams);
			stLocalRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

			ComPtr<ID3DBlob> pIRSBlob;
			ComPtr<ID3DBlob> pIRSErrMsg;

			HRESULT hrRet = D3D12SerializeRootSignature(&stGlobalRootSignatureDesc
				, D3D_ROOT_SIGNATURE_VERSION_1
				, &pIRSBlob
				, &pIRSErrMsg);
			if (FAILED(hrRet))
			{
				if (pIRSErrMsg)
				{
					GRS_PRINTF(_T("�����ǩ������%s\n")
						, static_cast<wchar_t*>(pIRSErrMsg->GetBufferPointer()));
				}
				GRS_THROW_IF_FAILED(hrRet);
			}

			GRS_THROW_IF_FAILED(pID3D12DXRDevice->CreateRootSignature(1
				, pIRSBlob->GetBufferPointer()
				, pIRSBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSGlobal)));

			pIRSBlob.Reset();
			pIRSErrMsg.Reset();

			hrRet = D3D12SerializeRootSignature(&stLocalRootSignatureDesc
				, D3D_ROOT_SIGNATURE_VERSION_1
				, &pIRSBlob
				, &pIRSErrMsg);

			if (FAILED(hrRet))
			{
				if (pIRSErrMsg)
				{
					GRS_PRINTF(_T("�����ǩ������%s\n")
						, static_cast<wchar_t*>(pIRSErrMsg->GetBufferPointer()));
				}
				GRS_THROW_IF_FAILED(hrRet);
			}

			GRS_THROW_IF_FAILED(pID3D12DXRDevice->CreateRootSignature(1
				, pIRSBlob->GetBufferPointer()
				, pIRSBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSLocal)));


			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSGlobal);
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSLocal);
		}

		//����RayTracing����״̬����
		{
			CAtlArray<D3D12_STATE_SUBOBJECT> arSubObjects;

			// Global Root Signature
			D3D12_STATE_SUBOBJECT stSubObjGlobalRS = {};
			stSubObjGlobalRS.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
			stSubObjGlobalRS.pDesc = pIRSGlobal.GetAddressOf();
		    
			arSubObjects.Add(stSubObjGlobalRS);

			// Raytracing Pipeline Config (��Ҫ�趨�ݹ���ȣ�����ֻ��������/һ�ι��ߣ��趨Ϊ1)
			D3D12_RAYTRACING_PIPELINE_CONFIG stPipelineCfg;
			stPipelineCfg.MaxTraceRecursionDepth = 1;

			D3D12_STATE_SUBOBJECT stSubObjPipelineCfg = {};
			stSubObjPipelineCfg.pDesc = &stPipelineCfg;
			stSubObjPipelineCfg.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
			
			arSubObjects.Add(stSubObjPipelineCfg);
			
			// DXIL Library 
			D3D12_EXPORT_DESC	 stRayGenExportDesc[3] = {};
			D3D12_DXIL_LIBRARY_DESC stRayGenDxilLibDesc = {};

			stRayGenExportDesc[0].Name = c_pszRaygenShaderName;
			stRayGenExportDesc[0].Flags = D3D12_EXPORT_FLAG_NONE;
			stRayGenExportDesc[1].Name = c_pszClosestHitShaderName;
			stRayGenExportDesc[1].Flags = D3D12_EXPORT_FLAG_NONE;
			stRayGenExportDesc[2].Name = c_pszMissShaderName;
			stRayGenExportDesc[2].Flags = D3D12_EXPORT_FLAG_NONE;

			stRayGenDxilLibDesc.NumExports = _countof(stRayGenExportDesc);
			stRayGenDxilLibDesc.pExports = stRayGenExportDesc;
			stRayGenDxilLibDesc.DXILLibrary.pShaderBytecode = g_pRaytracing;
			stRayGenDxilLibDesc.DXILLibrary.BytecodeLength = ARRAYSIZE(g_pRaytracing);

			D3D12_STATE_SUBOBJECT stSubObjDXILLib = {};
			stSubObjDXILLib.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
			stSubObjDXILLib.pDesc = &stRayGenDxilLibDesc;

			arSubObjects.Add(stSubObjDXILLib);

			// Hit Shader Table Group 
			D3D12_HIT_GROUP_DESC stHitGroupDesc = {};
			stHitGroupDesc.ClosestHitShaderImport = c_pszClosestHitShaderName;
			stHitGroupDesc.HitGroupExport = c_pszHitGroupName;
			D3D12_STATE_SUBOBJECT hitGroupSubobject = {};
			hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
			hitGroupSubobject.pDesc = &stHitGroupDesc;
			arSubObjects.Add(hitGroupSubobject);

			// Raytracing Shader Config (��Ҫ�趨��������ṹ���ֽڴ�С��TraceRay�����ֽڴ�С)
			D3D12_RAYTRACING_SHADER_CONFIG stShaderCfg = {};
			stShaderCfg.MaxAttributeSizeInBytes = sizeof(XMFLOAT2);
			stShaderCfg.MaxPayloadSizeInBytes = sizeof(XMFLOAT4);

			D3D12_STATE_SUBOBJECT stShaderCfgStateObject = {};
			stShaderCfgStateObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
			stShaderCfgStateObject.pDesc = &stShaderCfg;
			
			arSubObjects.Add(stShaderCfgStateObject);

			// Local Root Signature
			D3D12_STATE_SUBOBJECT stSubObjLocalRS = {};
			stSubObjLocalRS.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			stSubObjLocalRS.pDesc = pIRSLocal.GetAddressOf();
			arSubObjects.Add(stSubObjLocalRS);
		
			// Local Root Signature to Exports Association
			// (��Ҫ�趨�ֲ���ǩ��������Shader������Ĺ�ϵ,������Shader������ɷ��ʵ�Shaderȫ�ֱ���)
			D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION stObj2ExportsAssoc = {};
			stObj2ExportsAssoc.pSubobjectToAssociate = &stSubObjLocalRS;
			stObj2ExportsAssoc.NumExports = 1;
			stObj2ExportsAssoc.pExports = &c_pszHitGroupName;

			D3D12_STATE_SUBOBJECT stSubObjAssoc = {};
			stSubObjAssoc.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
			stSubObjAssoc.pDesc = &stObj2ExportsAssoc;

			arSubObjects.Add(stSubObjAssoc);
			
			// ������State Object�Ľṹ�� ������Raytracing PSO
			D3D12_STATE_OBJECT_DESC stRaytracingPSOdesc = {};
			stRaytracingPSOdesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			stRaytracingPSOdesc.NumSubobjects = (UINT)arSubObjects.GetCount();
			stRaytracingPSOdesc.pSubobjects = arSubObjects.GetData();
			
			// ��������״̬����
			GRS_THROW_IF_FAILED(pID3D12DXRDevice->CreateStateObject(
				&stRaytracingPSOdesc
				, IID_PPV_ARGS(&pIDXRPSO)));
		}

		//����Shader Table
		{
			void* pRayGenShaderIdentifier;
			void* pMissShaderIdentifier;
			void* pHitGroupShaderIdentifier;

			// Get shader identifiers.
			UINT nShaderIdentifierSize = 0;

			ComPtr<ID3D12StateObjectPropertiesPrototype> pIDXRStateObjectProperties;
			GRS_THROW_IF_FAILED(pIDXRPSO.As(&pIDXRStateObjectProperties));

			pRayGenShaderIdentifier = pIDXRStateObjectProperties->GetShaderIdentifier(c_pszRaygenShaderName);
			pMissShaderIdentifier = pIDXRStateObjectProperties->GetShaderIdentifier(c_pszMissShaderName);
			pHitGroupShaderIdentifier = pIDXRStateObjectProperties->GetShaderIdentifier(c_pszHitGroupName);
			nShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

			D3D12_HEAP_DESC stUploadHeapDesc = {  };
			UINT64 n64HeapSize = 1 * 1024 * 1024;		//����1M�Ķ� �����㹻������Shader Table����
			UINT64 n64HeapOffset = 0;					//���ϵ�ƫ��
			UINT64 n64AllocSize = 0;
			UINT8* pBufs = nullptr;
			D3D12_RANGE stReadRange = { 0, 0 };

			stUploadHeapDesc.SizeInBytes = GRS_UPPER(n64HeapSize
				, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);//64K�߽�����С
			//ע���ϴ��ѿ϶���Buffer���ͣ����Բ�ָ�����뷽ʽ����Ĭ����64k�߽����
			stUploadHeapDesc.Alignment = 0;
			stUploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;		//�ϴ�������
			stUploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			stUploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			//�ϴ��Ѿ��ǻ��壬���԰ڷ���������
			stUploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

			//�������ڻ���Shader Table��Heap������ʹ�õ����Զ����ϴ���
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateHeap(&stUploadHeapDesc
				, IID_PPV_ARGS(&pIHeapShaderTable)));

			//ע�����ߴ������32�ֽ��϶��룬����DXR��ʽ���лᱨ��
			UINT64 nAlignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			UINT64 nSizeAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

			// Ray gen shader table
			{
				UINT nNumShaderRecords = 1;
				UINT nShaderRecordSize = nShaderIdentifierSize;

				n64AllocSize = nNumShaderRecords * nShaderRecordSize;
				n64AllocSize = GRS_UPPER(n64AllocSize, nSizeAlignment);

				GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
					pIHeapShaderTable.Get()
					, n64HeapOffset
					, &CD3DX12_RESOURCE_DESC::Buffer(n64AllocSize)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&pIRESRayGenShaderTable)
				));
				pIRESRayGenShaderTable->SetName(L"RayGenShaderTable");

				GRS_THROW_IF_FAILED(pIRESRayGenShaderTable->Map(
					0
					, &stReadRange
					, reinterpret_cast<void**>(&pBufs)));

				memcpy(pBufs
					, pRayGenShaderIdentifier
					, nShaderIdentifierSize);

				pIRESRayGenShaderTable->Unmap(0, nullptr);
			}

			n64HeapOffset += GRS_UPPER(n64AllocSize, nAlignment); //����64k�߽����׼����һ������
			GRS_THROW_IF_FALSE(n64HeapOffset < n64HeapSize);

			// Miss shader table
			{
				UINT nNumShaderRecords = 1;
				UINT nShaderRecordSize = nShaderIdentifierSize;
				n64AllocSize = nNumShaderRecords * nShaderRecordSize;
				n64AllocSize = GRS_UPPER(n64AllocSize, nSizeAlignment);

				GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
					pIHeapShaderTable.Get()
					, n64HeapOffset
					, &CD3DX12_RESOURCE_DESC::Buffer(n64AllocSize)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&pIRESMissShaderTable)
				));
				pIRESMissShaderTable->SetName(L"MissShaderTable");
				pBufs = nullptr;

				GRS_THROW_IF_FAILED(pIRESMissShaderTable->Map(
					0
					, &stReadRange
					, reinterpret_cast<void**>(&pBufs)));

				memcpy(pBufs
					, pMissShaderIdentifier
					, nShaderIdentifierSize);

				pIRESMissShaderTable->Unmap(0, nullptr);
			}

			//D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
			n64HeapOffset += GRS_UPPER(n64AllocSize, nAlignment); //����64k�߽����׼����һ������
			GRS_THROW_IF_FALSE(n64HeapOffset < n64HeapSize);

			// Hit group shader table
			{
				UINT nNumShaderRecords = 1;
				UINT nShaderRecordSize = nShaderIdentifierSize + sizeof(stCBModule);

				n64AllocSize = nNumShaderRecords * nShaderRecordSize;
				n64AllocSize = GRS_UPPER(n64AllocSize, nSizeAlignment);

				GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
					pIHeapShaderTable.Get()
					, n64HeapOffset
					, &CD3DX12_RESOURCE_DESC::Buffer(n64AllocSize)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&pIRESHitGroupShaderTable)
				));
				pIRESHitGroupShaderTable->SetName(L"HitGroupShaderTable");
				pBufs = nullptr;

				GRS_THROW_IF_FAILED(pIRESHitGroupShaderTable->Map(
					0
					, &stReadRange
					, reinterpret_cast<void**>(&pBufs)));

				//����Shader Identifier
				memcpy(pBufs
					, pHitGroupShaderIdentifier
					, nShaderIdentifierSize);

				pBufs = static_cast<BYTE*>(pBufs) + nShaderIdentifierSize;

				//���ƾֲ��Ĳ�����Ҳ����Local Root Signature��ʶ�ľֲ�����
				memcpy(pBufs, &stCBModule, sizeof(stCBModule));

				pIRESHitGroupShaderTable->Unmap(0, nullptr);
			}
		}
		//=====================================================================================================================

		//������������
		{{
				size_t szCBBuf = GRS_UPPER(sizeof(ST_SCENE_CONSANTBUFFER)
					, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				//szCBBuf *= c_nFrameBackBufCount;

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(szCBBuf),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&pICBFrameConstant)));

				CD3DX12_RANGE readRange(0, 0);
				GRS_THROW_IF_FAILED(pICBFrameConstant->Map(0
					, nullptr
					, reinterpret_cast<void**>(&pstCBScene)));
		}}

		//�������� �� ��������
		{
			TCHAR pszTexture[MAX_PATH] = {};
			StringCchPrintf(pszTexture, MAX_PATH, _T("%sEarthResource\\Earth4kTexture_4K.dds"), pszAppPath);

			std::unique_ptr<uint8_t[]>			pbDDSData;
			std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
			DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bool								bIsCube = false;

			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
				pID3D12Device4.Get()
				, pszTexture
				, pITexture.GetAddressOf()
				, pbDDSData
				, stArSubResources
				, SIZE_MAX
				, &emAlphaMode
				, &bIsCube));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITexture);

			UINT64 n64szUpSphere = GetRequiredIntermediateSize(
				pITexture.Get()
				, 0
				, static_cast<UINT>(stArSubResources.size()));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pITextureUpload)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITextureUpload);

			//ִ������Copy�����������ϴ���Ĭ�϶���
			UpdateSubresources(pICMDList.Get()
				, pITexture.Get()
				, pITextureUpload.Get()
				, 0
				, 0
				, static_cast<UINT>(stArSubResources.size())
				, stArSubResources.data());

			//ͬ��
			pICMDList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pITexture.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			//���ط�������
			StringCchPrintf(pszTexture, MAX_PATH, _T("%sEarthResource\\Earth4kNormal_4K.dds"), pszAppPath);

			pbDDSData.reset();
			stArSubResources.empty();
			emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bIsCube = false;

			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
				pID3D12Device4.Get()
				, pszTexture
				, pINormalMap.GetAddressOf()
				, pbDDSData
				, stArSubResources
				, SIZE_MAX
				, &emAlphaMode
				, &bIsCube));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pINormalMap);

			n64szUpSphere = GetRequiredIntermediateSize(
				pINormalMap.Get()
				, 0
				, static_cast<UINT>(stArSubResources.size()));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pINormalMapUpload)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pINormalMapUpload);

			//ִ������Copy�����������ϴ���Ĭ�϶���
			UpdateSubresources(pICMDList.Get()
				, pINormalMap.Get()
				, pINormalMapUpload.Get()
				, 0
				, 0
				, static_cast<UINT>(stArSubResources.size())
				, stArSubResources.data());

			//ͬ��
			pICMDList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pINormalMap.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));



			//����������
			D3D12_RESOURCE_DESC stTXDesc = pITexture->GetDesc();
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = stTXDesc.Format;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandleTexture(
				pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
				, c_nDSNIndxTexture
				, nSRVDescriptorSize);

			pID3D12Device4->CreateShaderResourceView(
				pITexture.Get()
				, &stSRVDesc
				, stSrvHandleTexture);

			//�������������������
			stTXDesc = pINormalMap->GetDesc();
			stSRVDesc.Format = stTXDesc.Format;
			CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandleNormalMap(
				pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
				, c_nDSNIndxNormal
				, nSRVDescriptorSize);
			pID3D12Device4->CreateShaderResourceView(
				pINormalMap.Get()
				, &stSRVDesc
				, stSrvHandleNormalMap);


			//����������
			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			pID3D12Device4->CreateSampler(&stSamplerDesc
				, pISampleHeap->GetCPUDescriptorHandleForHeapStart());

		}

		//����ģ������
		{{

		
			CHAR pszMeshFileName[MAX_PATH] = {};
			USES_CONVERSION;
			StringCchPrintfA(pszMeshFileName, MAX_PATH, "%s\\Mesh\\sphere.txt", T2A(pszAppPath));

			LoadMeshVertex(pszMeshFileName, nVertexCnt, pstVertices, pnIndices);
			nIndexCnt = nVertexCnt;

			//*******************************************************************************************
			//nIndexCnt = nVertexCnt = 3;

			//pstVertices = (ST_GRS_VERTEX*)GRS_CALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX));
			//pnIndices = (GRS_TYPE_INDEX*)GRS_CALLOC(nIndexCnt * sizeof(GRS_TYPE_INDEX));

			//pnIndices[0] = 0;
			//pnIndices[1] = 1;
			//pnIndices[2] = 2;

			//float depthValue = 1.0;
			//float offset = 0.7f;

			//pstVertices[0].m_vPos = XMFLOAT4(0, -offset, depthValue, 1.0f);
			//pstVertices[1].m_vPos = XMFLOAT4(-offset, offset, depthValue, 1.0f);
			//pstVertices[2].m_vPos = XMFLOAT4(offset, offset, depthValue, 1.0f);

			//pstVertices[0].m_vNor = XMFLOAT3(-1.0f, 0.0f, -1.0f);
			//pstVertices[1].m_vNor = XMFLOAT3(0.0f, -1.0f, -1.0f);
			//pstVertices[2].m_vNor = XMFLOAT3(0.0f, 0.0f, -1.0f);

			//pstVertices[0].m_vTex = XMFLOAT2(1.0f, 1.0f);
			//pstVertices[1].m_vTex = XMFLOAT2(1.0f, 1.0f);
			//pstVertices[2].m_vTex = XMFLOAT2(1.0f, 1.0f);
			//*******************************************************************************************

			//���� ST_GRS_VERTEX Buffer ��ʹ��Upload��ʽ��
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nVertexCnt * sizeof(ST_GRS_VERTEX))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVBBufs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVBBufs);

			//ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
			UINT8* pVertexDataBegin = nullptr;
			CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

			GRS_THROW_IF_FAILED(pIVBBufs->Map(0
				, &stReadRange
				, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstVertices, nVertexCnt * sizeof(ST_GRS_VERTEX));
			pIVBBufs->Unmap(0, nullptr);

			//���� GRS_TYPE_INDEX Buffer ��ʹ��Upload��ʽ��
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nIndexCnt * sizeof(GRS_TYPE_INDEX))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIBBufs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIBBufs);

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIBBufs->Map(0
				, &stReadRange
				, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pnIndices, nIndexCnt * sizeof(GRS_TYPE_INDEX));
			pIIBBufs->Unmap(0, nullptr);

			GRS_SAFE_FREE(pstVertices);
			GRS_SAFE_FREE(pnIndices);

			// GRS_TYPE_INDEX SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			stSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			stSRVDesc.Buffer.NumElements = (nIndexCnt * sizeof(GRS_TYPE_INDEX)) / 4;// GRS_UPPER_DIV((nIndexCnt * sizeof(GRS_TYPE_INDEX)), 4);
			stSRVDesc.Buffer.StructureByteStride = 0;

			pID3D12Device4->CreateShaderResourceView(pIIBBufs.Get()
				, &stSRVDesc
				, CD3DX12_CPU_DESCRIPTOR_HANDLE(pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
					, c_nDSHIndxIBView
					, nSRVDescriptorSize));

			// Vertex SRV
			stSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			stSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			stSRVDesc.Buffer.NumElements = nVertexCnt;
			stSRVDesc.Buffer.StructureByteStride = sizeof(ST_GRS_VERTEX);

			pID3D12Device4->CreateShaderResourceView(pIVBBufs.Get()
				, &stSRVDesc
				, CD3DX12_CPU_DESCRIPTOR_HANDLE(pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
					, c_nDSHIndxVBView
					, nSRVDescriptorSize));

		}}


		//����ģ�͵ļ��ٽṹ��
		{{
			D3D12_RAYTRACING_GEOMETRY_DESC stModuleGeometryDesc = {};
			stModuleGeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			stModuleGeometryDesc.Triangles.IndexBuffer = pIIBBufs->GetGPUVirtualAddress();
			stModuleGeometryDesc.Triangles.IndexCount = static_cast<UINT>(pIIBBufs->GetDesc().Width) / sizeof(GRS_TYPE_INDEX);
			stModuleGeometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
			stModuleGeometryDesc.Triangles.Transform3x4 = 0;
			stModuleGeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			stModuleGeometryDesc.Triangles.VertexCount = static_cast<UINT>(pIVBBufs->GetDesc().Width) / sizeof(ST_GRS_VERTEX);
			stModuleGeometryDesc.Triangles.VertexBuffer.StartAddress = pIVBBufs->GetGPUVirtualAddress();
			stModuleGeometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(ST_GRS_VERTEX);

			// Mark the geometry as opaque. 
			// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
			// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
			stModuleGeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

			// Get required sizes for an acceleration structure.
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS emBuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC stBottomLevelBuildDesc = {};
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& stBottomLevelInputs = stBottomLevelBuildDesc.Inputs;
			stBottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			stBottomLevelInputs.Flags = emBuildFlags;
			stBottomLevelInputs.NumDescs = 1;
			stBottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			stBottomLevelInputs.pGeometryDescs = &stModuleGeometryDesc;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC stTopLevelBuildDesc = {};
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& stTopLevelInputs = stTopLevelBuildDesc.Inputs;
			stTopLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			stTopLevelInputs.Flags = emBuildFlags;
			stTopLevelInputs.NumDescs = 1;
			stTopLevelInputs.pGeometryDescs = nullptr;
			stTopLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO stTopLevelPrebuildInfo = {};
			pID3D12DXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&stTopLevelInputs
				, &stTopLevelPrebuildInfo);

			GRS_THROW_IF_FALSE(stTopLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO stBottomLevelPrebuildInfo = {};
			pID3D12DXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&stBottomLevelInputs
				, &stBottomLevelPrebuildInfo);

			GRS_THROW_IF_FALSE(stBottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(
					max(stTopLevelPrebuildInfo.ScratchDataSizeInBytes
						, stBottomLevelPrebuildInfo.ScratchDataSizeInBytes)
					, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&pIUAVScratchResource)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUAVScratchResource);

			D3D12_RESOURCE_STATES emInitialResourceState;
			emInitialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(stBottomLevelPrebuildInfo.ResultDataMaxSizeInBytes
					, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				emInitialResourceState,
				nullptr,
				IID_PPV_ARGS(&pIUAVBottomLevelAccelerationStructure)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUAVBottomLevelAccelerationStructure);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(stTopLevelPrebuildInfo.ResultDataMaxSizeInBytes
					, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				emInitialResourceState,
				nullptr,
				IID_PPV_ARGS(&pIUAVTopLevelAccelerationStructure)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUAVTopLevelAccelerationStructure);


			D3D12_RAYTRACING_INSTANCE_DESC stInstanceDesc = {};
			float fScales = 20.0f;
			XMMATRIX mxScale = XMMatrixScaling(fScales, fScales, fScales);
			XMMATRIX mxRotat = XMMatrixRotationY( 3.0f * XM_2PI / 8.0f );
			XMMATRIX mxTrans = XMMatrixTranslation(0.0f, 0.0f, 10.0f);
			mxTrans = XMMatrixMultiply(XMMatrixMultiply(mxScale,mxRotat),mxTrans);
			memcpy(stInstanceDesc.Transform, &mxTrans, 3 * 4 * sizeof(float));

			stInstanceDesc.InstanceMask = 1;
			stInstanceDesc.AccelerationStructure
				= pIUAVBottomLevelAccelerationStructure->GetGPUVirtualAddress();

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(sizeof(stInstanceDesc)),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIUploadBufInstanceDescs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUploadBufInstanceDescs);

			pIUploadBufInstanceDescs->Map(0, nullptr, (void**)&pstInstanceDesc);
			memcpy(pstInstanceDesc, &stInstanceDesc, sizeof(stInstanceDesc));
			pIUploadBufInstanceDescs->Unmap(0, nullptr);

			// Bottom Level Acceleration Structure desc
			stBottomLevelBuildDesc.ScratchAccelerationStructureData = pIUAVScratchResource->GetGPUVirtualAddress();
			stBottomLevelBuildDesc.DestAccelerationStructureData = pIUAVBottomLevelAccelerationStructure->GetGPUVirtualAddress();

			// Top Level Acceleration Structure desc
			stTopLevelBuildDesc.DestAccelerationStructureData = pIUAVTopLevelAccelerationStructure->GetGPUVirtualAddress();
			stTopLevelBuildDesc.ScratchAccelerationStructureData = pIUAVScratchResource->GetGPUVirtualAddress();
			stTopLevelBuildDesc.Inputs.InstanceDescs = pIUploadBufInstanceDescs->GetGPUVirtualAddress();


			// Build acceleration structure.
			pIDXRCmdList->BuildRaytracingAccelerationStructure(&stBottomLevelBuildDesc, 0, nullptr);
			pICMDList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::UAV(pIUAVBottomLevelAccelerationStructure.Get()));
			pIDXRCmdList->BuildRaytracingAccelerationStructure(&stTopLevelBuildDesc, 0, nullptr);

		}}


		//ִ�������б������Դ�ϴ��Դ�
		{{
			GRS_THROW_IF_FAILED(pICMDList->Close());
			ID3D12CommandList* pIarCMDList[] = { pICMDList.Get() };
			pICMDQueue->ExecuteCommandLists(ARRAYSIZE(pIarCMDList), pIarCMDList);

			const UINT64 n64CurFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence1.Get(), n64CurFenceValue));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(pIFence1->SetEventOnCompletion(n64CurFenceValue, hEventFence1));
		}}


		float fPhysicsAspetRatio = 0.0f; //��������ߴ��ݺ��
		{
			int nScreenPhysicsWidth = 0;
			int nScreenPhysicsHeight = 0;	// ����ߴ�	
			long nScreenPixelWidth = 0;
			long nScreenPixelHeight = 0;	
			long nDisplayFrequency = 0; 	

			// ��ȡ����ߴ�	
			HDC hdcScreen = ::GetDC(NULL);
			nScreenPhysicsWidth = ::GetDeviceCaps(hdcScreen, HORZSIZE);
			nScreenPhysicsHeight = ::GetDeviceCaps(hdcScreen, VERTSIZE);
			::ReleaseDC(NULL, hdcScreen);

			DEVMODE stDevMode = {};
			stDevMode.dmSize = sizeof(DEVMODE);
			::EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &stDevMode);
			nScreenPixelWidth = stDevMode.dmPelsWidth;
			nScreenPixelHeight = stDevMode.dmPelsHeight;
			nDisplayFrequency = stDevMode.dmDisplayFrequency;

			GRS_PRINTF(_T("��Ļ����ߴ� : ��: %d mm, ��: %dmm.\n")
				, nScreenPhysicsWidth
				, nScreenPhysicsHeight);
			GRS_PRINTF(_T("��Ļ�ֱ��� : ��: %d px, ��: %d px.\n")
				, nScreenPixelWidth
				, nScreenPixelHeight);
			GRS_PRINTF(_T("��Ļˢ��Ƶ�� : %d Hz.\n")
				, nDisplayFrequency);

			GRS_PRINTF(_T("��Ļ��������ߴ� : ��: %f mm, ��: %fmm.\n")
				, (float)nScreenPhysicsWidth / nScreenPixelWidth
				, (float)nScreenPhysicsHeight / nScreenPixelHeight);

			g_fPixelSize = (float)nScreenPhysicsWidth / nScreenPixelWidth;
			g_fPixelSizeDelta = g_fPixelSize * 0.1f;

			fPhysicsAspetRatio 
				= ((float)nScreenPhysicsWidth / nScreenPixelWidth ) 
				 / ((float)nScreenPhysicsHeight / nScreenPixelHeight);

			pstCBScene->m_v2PixelSize = XMFLOAT2(
				g_fPixelSize 
				, g_fPixelSize * fPhysicsAspetRatio );
		}

		//��Ϣѭ������Ⱦ���壩
		{
			//��������ʾ��ˢ�´��ڣ����������д��ڳ�ʱ�䲻��Ӧ������
			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);
			
			XMMATRIX mxView = XMMatrixLookAtLH(g_vEye, g_vLookAt, g_vUp);
	
			// ��ʼ��Ϣѭ�����������в�����Ⱦ
			DWORD dwRet = 0;
			BOOL bExit = FALSE;

			while (!bExit)
			{
				dwRet = ::MsgWaitForMultipleObjects(
					static_cast<DWORD>(arWaitHandles.GetCount())
					, arWaitHandles.GetData(), FALSE, INFINITE, QS_ALLINPUT);

				switch (dwRet - WAIT_OBJECT_0)
				{
				case 0:
				{//hEventFence1 ���ź�״̬:һ֡��Ⱦ������������һ֡ 
					//---------------------------------------------------------------------------------------------
					// �����³������൱��OnUpdate()
					pstCBScene->m_mxView = XMMatrixLookAtLH(g_vEye, g_vLookAt, g_vUp);
					pstCBScene->m_vCameraPos = g_vEye;

					pstCBScene->m_vLightPos = XMLoadFloat4(&g_v4LightPosition);
					pstCBScene->m_vLightAmbientColor = XMLoadFloat4(&g_v4LightAmbientColor);
					pstCBScene->m_vLightDiffuseColor = XMLoadFloat4(&g_v4LightDiffuseColor);

					//������Ļ���سߴ�
					pstCBScene->m_v2PixelSize = XMFLOAT2(g_fPixelSize, fPhysicsAspetRatio * g_fPixelSize);

					//---------------------------------------------------------------------------------------------

					//����ֱ�������൱��OnRender()
					//---------------------------------------------------------------------------------------------
					//�����������Resetһ��
					GRS_THROW_IF_FAILED(pICMDAlloc->Reset());
					//Reset�����б�������ָ�������������PSO����
					GRS_THROW_IF_FAILED(pICMDList->Reset(pICMDAlloc.Get(), nullptr));
					//��ȡ�µĺ󻺳���ţ���ΪPresent�������ʱ�󻺳����ž͸�����
					nCurFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
					//---------------------------------------------------------------------------------------------
					// �����Ǵ�ͳ��׷��Ⱦ��Ҫ��׼�����裬���ڴ���׷��Ⱦ�Ļ����Բ�Ҫ�ˣ�
					// ���ǲ���Ҫ��Ⱦ���������ĺ󻺳����ˣ�����ֱ�ӽ���׷���Ļ��������ĸ��Ƶ��󻺳������

					//pICMDList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					//		pIRenderTargetBufs[nCurFrameIndex].Get()
					//		, D3D12_RESOURCE_STATE_PRESENT
					//		, D3D12_RESOURCE_STATE_RENDER_TARGET
					//	));

					////ƫ��������ָ�뵽ָ��֡������ͼλ�ã���Щ������ɿ����ɲ���
					//CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart(), nCurFrameIndex, nRTVDescriptorSize);
					////������ȾĿ��
					//pICMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, nullptr);

					//pICMDList->RSSetViewports(1, &stViewPort);
					//pICMDList->RSSetScissorRects(1, &stScissorRect);

					//pICMDList->ClearRenderTargetView(stRTVHandle, c_faClearColor, 0, nullptr);
					//---------------------------------------------------------------------------------------------
					//��ʼ��Ⱦ
					D3D12_DISPATCH_RAYS_DESC stDispatchRayDesc = {};
					stDispatchRayDesc.HitGroupTable.StartAddress
						= pIRESHitGroupShaderTable->GetGPUVirtualAddress();
					stDispatchRayDesc.HitGroupTable.SizeInBytes
						= pIRESHitGroupShaderTable->GetDesc().Width;
					stDispatchRayDesc.HitGroupTable.StrideInBytes
						= stDispatchRayDesc.HitGroupTable.SizeInBytes;

					stDispatchRayDesc.MissShaderTable.StartAddress
						= pIRESMissShaderTable->GetGPUVirtualAddress();
					stDispatchRayDesc.MissShaderTable.SizeInBytes
						= pIRESMissShaderTable->GetDesc().Width;
					stDispatchRayDesc.MissShaderTable.StrideInBytes
						= stDispatchRayDesc.MissShaderTable.SizeInBytes;

					stDispatchRayDesc.RayGenerationShaderRecord.StartAddress
						= pIRESRayGenShaderTable->GetGPUVirtualAddress();
					stDispatchRayDesc.RayGenerationShaderRecord.SizeInBytes
						= pIRESRayGenShaderTable->GetDesc().Width;
					stDispatchRayDesc.Width = iWidth;
					stDispatchRayDesc.Height = iHeight;
					stDispatchRayDesc.Depth = 1;

					pICMDList->SetComputeRootSignature(pIRSGlobal.Get());
					pIDXRCmdList->SetPipelineState1(pIDXRPSO.Get());

					ID3D12DescriptorHeap* ppDescriptorHeaps[] = { pIDXRUAVHeap.Get(),pISampleHeap.Get() };
					pIDXRCmdList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);

					CD3DX12_GPU_DESCRIPTOR_HANDLE objUAVHandle(
						pIDXRUAVHeap->GetGPUDescriptorHandleForHeapStart()
						, c_nDSHIndxUAVOutput
						, nSRVDescriptorSize);

					pICMDList->SetComputeRootDescriptorTable(
						0
						, objUAVHandle);

					pICMDList->SetComputeRootShaderResourceView(
						1
						, pIUAVTopLevelAccelerationStructure->GetGPUVirtualAddress());

					pICMDList->SetComputeRootConstantBufferView(
						2
						, pICBFrameConstant->GetGPUVirtualAddress());

					// ����Index��Vertex���������Ѿ��
					CD3DX12_GPU_DESCRIPTOR_HANDLE objIBHandle(
						pIDXRUAVHeap->GetGPUDescriptorHandleForHeapStart()
						, c_nDSHIndxIBView
						, nSRVDescriptorSize);
					pICMDList->SetComputeRootDescriptorTable(
						3
						, objIBHandle);

					//��������������������Ѿ��
					CD3DX12_GPU_DESCRIPTOR_HANDLE objTxtureHandle(
						pIDXRUAVHeap->GetGPUDescriptorHandleForHeapStart()
						, c_nDSNIndxTexture
						, nSRVDescriptorSize);
					pICMDList->SetComputeRootDescriptorTable(
						4
						, objTxtureHandle);

					pICMDList->SetComputeRootDescriptorTable(
						5
						, pISampleHeap->GetGPUDescriptorHandleForHeapStart()
					);

					pIDXRCmdList->DispatchRays(&stDispatchRayDesc);
					//---------------------------------------------------------------------------------------------

					D3D12_RESOURCE_BARRIER preCopyBarriers[2];
					preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
						pIRenderTargetBufs[nCurFrameIndex].Get()
						, D3D12_RESOURCE_STATE_PRESENT //D3D12_RESOURCE_STATE_RENDER_TARGET
						, D3D12_RESOURCE_STATE_COPY_DEST);
					preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
						pIDXRUAVBufs.Get()
						, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
						, D3D12_RESOURCE_STATE_COPY_SOURCE);

					pICMDList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

					pICMDList->CopyResource(
						pIRenderTargetBufs[nCurFrameIndex].Get()
						, pIDXRUAVBufs.Get());

					D3D12_RESOURCE_BARRIER postCopyBarriers[2];
					postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
						pIRenderTargetBufs[nCurFrameIndex].Get()
						, D3D12_RESOURCE_STATE_COPY_DEST
						, D3D12_RESOURCE_STATE_PRESENT);
					postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
						pIDXRUAVBufs.Get()
						, D3D12_RESOURCE_STATE_COPY_SOURCE
						, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					pICMDList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

					//�ر������б�����ȥִ����
					GRS_THROW_IF_FAILED(pICMDList->Close());

					//ִ�������б�
					ID3D12CommandList* ppCommandLists[] = { pICMDList.Get() };
					pICMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

					//�ύ����
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					//---------------------------------------------------------------------------------------------
					//��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
					const UINT64 n64CurFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence1.Get(), n64CurFenceValue));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(pIFence1->SetEventOnCompletion(n64CurFenceValue, hEventFence1));
				}
				break;
				case 1:
				{//������Ϣ
					while (::PeekMessage(&stMsg, NULL, 0, 0, PM_REMOVE))
					{
						if (WM_QUIT != stMsg.message)
						{
							::TranslateMessage(&stMsg);
							::DispatchMessage(&stMsg);
						}
						else
						{
							bExit = TRUE;
						}
					}
				}
				break;
				case WAIT_TIMEOUT:
				{//��ʱ����

				}
				break;
				default:
					break;
				}
			}
		}
	}
	catch (CGRSCOMException & e)
	{//������COM�쳣
		e.Error();
	}
	GRS_FREE_OUTPUT();

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	float fDelta = 0.1f;
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	{//���������任��Դλ��
		USHORT n16KeyCode = (wParam & 0xFF);
		if(VK_UP == n16KeyCode)
		{
			g_v4LightPosition.y += fDelta;
		}
		if (VK_DOWN == n16KeyCode)
		{
			g_v4LightPosition.y -= fDelta;
		}
		if (VK_LEFT == n16KeyCode)
		{
			g_v4LightPosition.x -= fDelta;
		}
		if ( VK_RIGHT == n16KeyCode )
		{
			g_v4LightPosition.x += fDelta;
		}
		if (VK_ADD == n16KeyCode)
		{
			g_fPixelSize += g_fPixelSizeDelta;
		}
		if (VK_SUBTRACT == n16KeyCode)
		{
			g_fPixelSize -= g_fPixelSizeDelta;
			if (g_fPixelSize <= g_fPixelSizeDelta)
			{
				g_fPixelSize = g_fPixelSizeDelta;
			}
		}

		float fDelta = 0.1f;
		if ('w' == n16KeyCode || 'W' == n16KeyCode)
		{
			g_vEye = XMVectorSetZ(g_vEye, XMVectorGetZ(g_vEye) + 10.0f * fDelta);
		}

		if ('s' == n16KeyCode || 'S' == n16KeyCode)
		{
			g_vEye = XMVectorSetZ(g_vEye, XMVectorGetZ(g_vEye) - 10.0f * fDelta);
		}

		if ('d' == n16KeyCode || 'D' == n16KeyCode)
		{
			g_vEye = XMVectorSetX(g_vEye, XMVectorGetX(g_vEye) + fDelta);
		}

		if ('a' == n16KeyCode || 'A' == n16KeyCode)
		{
			g_vEye = XMVectorSetX(g_vEye, XMVectorGetX(g_vEye) - fDelta);
		}

		if ('q' == n16KeyCode || 'Q' == n16KeyCode)
		{
			g_vEye = XMVectorSetY(g_vEye, XMVectorGetY(g_vEye) + fDelta);
		}

		if ('e' == n16KeyCode || 'E' == n16KeyCode)
		{
			g_vEye = XMVectorSetY(g_vEye, XMVectorGetY(g_vEye) - fDelta);
		}


	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, GRS_TYPE_INDEX*& ppIndices)
{
	ifstream fin;
	char input;
	BOOL bRet = TRUE;
	try
	{
		fin.open(pszMeshFileName);
		if (fin.fail())
		{
			throw CGRSCOMException(E_FAIL);
		}
		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin >> nVertexCnt;

		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin.get(input);
		fin.get(input);

		ppVertex = (ST_GRS_VERTEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX));
		ppIndices = (GRS_TYPE_INDEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(GRS_TYPE_INDEX));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_vPos.x >> ppVertex[i].m_vPos.y >> ppVertex[i].m_vPos.z;
			ppVertex[i].m_vPos.w = 1.0f;
			fin >> ppVertex[i].m_vTex.x >> ppVertex[i].m_vTex.y;
			fin >> ppVertex[i].m_vNor.x >> ppVertex[i].m_vNor.y >> ppVertex[i].m_vNor.z;

			ppIndices[i] = static_cast<GRS_TYPE_INDEX>(i);
		}
	}
	catch (CGRSCOMException & e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}
