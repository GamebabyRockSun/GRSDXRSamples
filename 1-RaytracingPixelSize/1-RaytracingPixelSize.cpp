#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <wrl.h>  //添加WTL支持 方便使用COM
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
#include "Shader\RayTracingHlslCompat.h" //shader 和 C++代码中使用相同的头文件定义常量结构体 以及顶点结构体等

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

//新定义的宏用于上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

#define GRS_UPPER_SIZEOFUINT32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) { HRESULT _hr = (hr); if (FAILED(_hr)){ throw CGRSCOMException(_hr); } }
#define GRS_THROW_IF_FALSE(b) {  if (!(b)){ throw CGRSCOMException(0xF0000001); } }

//控制台输出，方便调试
#if defined(_DEBUG)
//使用控制台输出调试信息，方便多线程调试
#define GRS_INIT_OUTPUT() 	if (!::AllocConsole()){throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));}
//#define GRS_FREE_OUTPUT()	::_tsystem(_T("PAUSE"));\
//							::FreeConsole();
#define GRS_FREE_OUTPUT()	::FreeConsole();
#define GRS_USEPRINTF() TCHAR pBuf[1024] = {};TCHAR pszOutput[1024] = {};
#define GRS_PRINTF(...) \
    StringCchPrintf(pBuf,1024,__VA_ARGS__);\
	StringCchPrintf(pszOutput,1024,_T("【ID=0x%04x】：%s"),::GetCurrentThreadId(),pBuf);\
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),pszOutput,lstrlen(pszOutput),NULL,NULL);
#else
#define GRS_INIT_OUTPUT()
#define GRS_FREE_OUTPUT()
#define GRS_USEPRINTF()
#define GRS_PRINTF(...)
#endif
//------------------------------------------------------------------------------------------------------------
// 为了调试加入下面的内联函数和宏定义，为每个接口对象设置名称，方便查看调试输出
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

//全局光源信息变量
XMFLOAT4 g_v4LightPosition = XMFLOAT4(0.0f, 1.8f, -3.0f, 0.0f);
XMFLOAT4 g_v4LightAmbientColor = XMFLOAT4(0.01f, 0.01f, 0.01f, 1.0f);
XMFLOAT4 g_v4LightDiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

//全局摄像机信息变量
XMVECTOR g_vEye = { 0.0f,0.0f,-50.0f,0.0f };
XMVECTOR g_vLookAt = { 0.0f,-1.0f,0.0f };
XMVECTOR g_vUp = { 0.0f,1.0f,0.0f,0.0f };

//屏幕像素点尺寸
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
	//DXR 接口
	ComPtr<ID3D12Device5>						pID3D12DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4>			pIDXRCmdList;
	ComPtr<ID3D12StateObjectPrototype>			pIDXRPSO;

	ComPtr<ID3D12Resource>						pIDXRUAVBufs;
	ComPtr<ID3D12DescriptorHeap>				pIDXRUAVHeap;		//UAV Output、VB、 IB、 Scene CB、 Module CB

	//根签名 DXR中需要两个根签名，一个是全局的根签名，另一个就是局部的根签名
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

		// 得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
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

		// 注册并创建窗口
		{
			WNDCLASSEX wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_GLOBALCLASS;
			wcex.lpfnWndProc = WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//防止无聊的背景重绘
			wcex.lpszClassName = GRS_WND_CLASS_NAME;

			::RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWidth, iHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// 计算窗口居中的屏幕坐标
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

		//打开显示子系统的调试支持
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// 打开附加的调试支持
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		//创建DXGI Factory对象
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags
				, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(
				hWnd
				, DXGI_MWA_NO_ALT_ENTER));

			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		//枚举显示适配器（选择显卡，因为要DXR 所以选择性能最强的）
		{//注意 这里用了新接口IDXGIFactory6的新方法EnumAdapterByGpuPreference
			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(
				0
				, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
				, IID_PPV_ARGS(&pIDXGIAdapter1)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIAdapter1);
		}

		//=====================================================================================================================
		// 从本例开始，光追渲染部分的示例将不再使用Fallback方式，如果硬件不支持DXR，那么就直接退出
		{
			// 创建一个设备试试看DX12行不行
			HRESULT hr2 = D3D12CreateDevice(pIDXGIAdapter1.Get()
				, emMinFeature
				, IID_PPV_ARGS(&pID3D12Device4));

			if (!SUCCEEDED(hr2))
			{
				::MessageBox(hWnd
					, _T("非常抱歉的通知您，\r\n您系统中最NB的显卡也不能支持DX12，例子没法继续运行！\r\n程序将退出！")
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

			//检测硬件是否是直接支持DXR
			bISDXRSupport = SUCCEEDED(hr)
				&& (stFeatureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);

			if (bISDXRSupport)
			{
				GRS_PRINTF(_T("恭喜！您的显卡“%s”直接支持DXR！\n"), stAdapterDesc.Description);
			}
			else
			{
				::MessageBox(hWnd
					, _T("非常抱歉的通知您，\r\n您系统中最NB的显卡不支持硬件DXR，例子没法继续运行。\r\n程序将退出！")
					, GRS_WND_TITLE
					, MB_OK | MB_ICONINFORMATION);

				return -1;
			}
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12Device4);

			//将被使用的设备名称显示到窗口标题里
			TCHAR pszWndTitle[MAX_PATH] = {};
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle, MAX_PATH, _T("%s( Device: %s )"), pszWndTitle, stAdapterDesc.Description);
			::SetWindowText(hWnd, pszWndTitle);

		}
		//=====================================================================================================================

		//创建命令队列、命令分配器、命令列表
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
		//创建DXR设备，以及DXR命令列表
		{// DirectX Raytracing
			GRS_THROW_IF_FAILED(pID3D12Device4->QueryInterface(IID_PPV_ARGS(&pID3D12DXRDevice)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12DXRDevice);
			GRS_THROW_IF_FAILED(pICMDList->QueryInterface(IID_PPV_ARGS(&pIDXRCmdList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRCmdList);
		}
		//=====================================================================================================================

		//创建Fence
		{
			// 创建一个同步对象——围栏，用于等待渲染完成，因为现在Draw Call是异步的了
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence1)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIFence1);

			// 创建一个Event同步对象，用于等待围栏事件通知
			hEventFence1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (nullptr == hEventFence1)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			arWaitHandles.Add(hEventFence1);
		}

		//创建RTV
		{
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = c_nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVHeap);

			nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		//创建交换链
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

			//注意此处使用了高版本的SwapChain接口的函数
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			nCurFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < c_nFrameBackBufCount; i++)
			{//这个循环暴漏了描述符堆实际上是个数组的本质
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i
					, IID_PPV_ARGS(&pIRenderTargetBufs[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIRenderTargetBufs, i);
				pID3D12Device4->CreateRenderTargetView(pIRenderTargetBufs[i].Get()
					, nullptr
					, stRTVHandle);
				stRTVHandle.Offset(1, nRTVDescriptorSize);
			}
		}

		//创建渲染过程中需要的各种描述符堆
		{
			D3D12_DESCRIPTOR_HEAP_DESC stDXRDescriptorHeapDesc = {};
			// 存放9个描述符:
			// 2 - 顶点与索引缓冲 SRVs
			// 1 - 光追渲染输出纹理 SRV
			// 2 - 加速结构的缓冲 UAVs
			// 2 - 加速结构体数据缓冲 UAVs 主要用于fallback层
			// 2 - 纹理和Normal Map的描述符
			stDXRDescriptorHeapDesc.NumDescriptors = nMaxDSCnt;
			stDXRDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stDXRDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stDXRDescriptorHeapDesc
				, IID_PPV_ARGS(&pIDXRUAVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRUAVHeap);

			nSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			//创建采样器描述符堆
			D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
			stSamplerHeapDesc.NumDescriptors = c_nMaxSampleCnt;
			stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(
				&stSamplerHeapDesc
				, IID_PPV_ARGS(&pISampleHeap)));

		}

		//=====================================================================================================================
		//创建RayTracing Render Target UAV
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

			//UAV Heap的第一个描述符 就放UAV 也就是 DXR的Output
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

		//创建根签名 注意DXR中有两个根签名，一个是全局（Global）根签名另一个是本地（Local）根签名
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
					GRS_PRINTF(_T("编译根签名出错：%s\n")
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
					GRS_PRINTF(_T("编译根签名出错：%s\n")
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

		//创建RayTracing管线状态对象
		{
			CAtlArray<D3D12_STATE_SUBOBJECT> arSubObjects;

			// Global Root Signature
			D3D12_STATE_SUBOBJECT stSubObjGlobalRS = {};
			stSubObjGlobalRS.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
			stSubObjGlobalRS.pDesc = pIRSGlobal.GetAddressOf();
		    
			arSubObjects.Add(stSubObjGlobalRS);

			// Raytracing Pipeline Config (主要设定递归深度，这里只有主光线/一次光线，设定为1)
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

			// Raytracing Shader Config (主要设定质心坐标结构体字节大小、TraceRay负载字节大小)
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
			// (主要设定局部根签名与命中Shader函数组的关系,即命中Shader函数组可访问的Shader全局变量)
			D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION stObj2ExportsAssoc = {};
			stObj2ExportsAssoc.pSubobjectToAssociate = &stSubObjLocalRS;
			stObj2ExportsAssoc.NumExports = 1;
			stObj2ExportsAssoc.pExports = &c_pszHitGroupName;

			D3D12_STATE_SUBOBJECT stSubObjAssoc = {};
			stSubObjAssoc.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
			stSubObjAssoc.pDesc = &stObj2ExportsAssoc;

			arSubObjects.Add(stSubObjAssoc);
			
			// 最后填充State Object的结构体 并创建Raytracing PSO
			D3D12_STATE_OBJECT_DESC stRaytracingPSOdesc = {};
			stRaytracingPSOdesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			stRaytracingPSOdesc.NumSubobjects = (UINT)arSubObjects.GetCount();
			stRaytracingPSOdesc.pSubobjects = arSubObjects.GetData();
			
			// 创建管线状态对象
			GRS_THROW_IF_FAILED(pID3D12DXRDevice->CreateStateObject(
				&stRaytracingPSOdesc
				, IID_PPV_ARGS(&pIDXRPSO)));
		}

		//创建Shader Table
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
			UINT64 n64HeapSize = 1 * 1024 * 1024;		//分配1M的堆 这里足够放三个Shader Table即可
			UINT64 n64HeapOffset = 0;					//堆上的偏移
			UINT64 n64AllocSize = 0;
			UINT8* pBufs = nullptr;
			D3D12_RANGE stReadRange = { 0, 0 };

			stUploadHeapDesc.SizeInBytes = GRS_UPPER(n64HeapSize
				, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);//64K边界对齐大小
			//注意上传堆肯定是Buffer类型，可以不指定对齐方式，其默认是64k边界对齐
			stUploadHeapDesc.Alignment = 0;
			stUploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;		//上传堆类型
			stUploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			stUploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			//上传堆就是缓冲，可以摆放任意数据
			stUploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

			//创建用于缓冲Shader Table的Heap，这里使用的是自定义上传堆
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateHeap(&stUploadHeapDesc
				, IID_PPV_ARGS(&pIHeapShaderTable)));

			//注意分配尺寸对齐是32字节上对齐，否则纯DXR方式运行会报错
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

			n64HeapOffset += GRS_UPPER(n64AllocSize, nAlignment); //向上64k边界对齐准备下一个分配
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
			n64HeapOffset += GRS_UPPER(n64AllocSize, nAlignment); //向上64k边界对齐准备下一个分配
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

				//复制Shader Identifier
				memcpy(pBufs
					, pHitGroupShaderIdentifier
					, nShaderIdentifierSize);

				pBufs = static_cast<BYTE*>(pBufs) + nShaderIdentifierSize;

				//复制局部的参数，也就是Local Root Signature标识的局部参数
				memcpy(pBufs, &stCBModule, sizeof(stCBModule));

				pIRESHitGroupShaderTable->Unmap(0, nullptr);
			}
		}
		//=====================================================================================================================

		//创建常量缓冲
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

		//加载纹理 和 法线纹理
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

			//执行两个Copy动作将纹理上传到默认堆中
			UpdateSubresources(pICMDList.Get()
				, pITexture.Get()
				, pITextureUpload.Get()
				, 0
				, 0
				, static_cast<UINT>(stArSubResources.size())
				, stArSubResources.data());

			//同步
			pICMDList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pITexture.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			//加载法线纹理
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

			//执行两个Copy动作将纹理上传到默认堆中
			UpdateSubresources(pICMDList.Get()
				, pINormalMap.Get()
				, pINormalMapUpload.Get()
				, 0
				, 0
				, static_cast<UINT>(stArSubResources.size())
				, stArSubResources.data());

			//同步
			pICMDList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pINormalMap.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));



			//创建描述符
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

			//创建噪声纹理的描述符
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


			//创建采样器
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

		//加载模型数据
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

			//创建 ST_GRS_VERTEX Buffer 仅使用Upload隐式堆
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nVertexCnt * sizeof(ST_GRS_VERTEX))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVBBufs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVBBufs);

			//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
			UINT8* pVertexDataBegin = nullptr;
			CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

			GRS_THROW_IF_FAILED(pIVBBufs->Map(0
				, &stReadRange
				, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstVertices, nVertexCnt * sizeof(ST_GRS_VERTEX));
			pIVBBufs->Unmap(0, nullptr);

			//创建 GRS_TYPE_INDEX Buffer 仅使用Upload隐式堆
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


		//创建模型的加速结构体
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


		//执行命令列表，完成资源上传显存
		{{
			GRS_THROW_IF_FAILED(pICMDList->Close());
			ID3D12CommandList* pIarCMDList[] = { pICMDList.Get() };
			pICMDQueue->ExecuteCommandLists(ARRAYSIZE(pIarCMDList), pIarCMDList);

			const UINT64 n64CurFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence1.Get(), n64CurFenceValue));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(pIFence1->SetEventOnCompletion(n64CurFenceValue, hEventFence1));
		}}


		float fPhysicsAspetRatio = 0.0f; //像素物理尺寸纵横比
		{
			int nScreenPhysicsWidth = 0;
			int nScreenPhysicsHeight = 0;	// 物理尺寸	
			long nScreenPixelWidth = 0;
			long nScreenPixelHeight = 0;	
			long nDisplayFrequency = 0; 	

			// 获取物理尺寸	
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

			GRS_PRINTF(_T("屏幕物理尺寸 : 宽: %d mm, 高: %dmm.\n")
				, nScreenPhysicsWidth
				, nScreenPhysicsHeight);
			GRS_PRINTF(_T("屏幕分辨率 : 宽: %d px, 高: %d px.\n")
				, nScreenPixelWidth
				, nScreenPixelHeight);
			GRS_PRINTF(_T("屏幕刷新频率 : %d Hz.\n")
				, nDisplayFrequency);

			GRS_PRINTF(_T("屏幕像素物理尺寸 : 宽: %f mm, 高: %fmm.\n")
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

		//消息循环（渲染主体）
		{
			//在这里显示和刷新窗口，这样不会有窗口长时间不响应的问题
			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);
			
			XMMATRIX mxView = XMMatrixLookAtLH(g_vEye, g_vLookAt, g_vUp);
	
			// 开始消息循环，并在其中不断渲染
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
				{//hEventFence1 有信号状态:一帧渲染结束，继续下一帧 
					//---------------------------------------------------------------------------------------------
					// 更新下常量，相当于OnUpdate()
					pstCBScene->m_mxView = XMMatrixLookAtLH(g_vEye, g_vLookAt, g_vUp);
					pstCBScene->m_vCameraPos = g_vEye;

					pstCBScene->m_vLightPos = XMLoadFloat4(&g_v4LightPosition);
					pstCBScene->m_vLightAmbientColor = XMLoadFloat4(&g_v4LightAmbientColor);
					pstCBScene->m_vLightDiffuseColor = XMLoadFloat4(&g_v4LightDiffuseColor);

					//设置屏幕像素尺寸
					pstCBScene->m_v2PixelSize = XMFLOAT2(g_fPixelSize, fPhysicsAspetRatio * g_fPixelSize);

					//---------------------------------------------------------------------------------------------

					//以下直到结束相当于OnRender()
					//---------------------------------------------------------------------------------------------
					//命令分配器先Reset一下
					GRS_THROW_IF_FAILED(pICMDAlloc->Reset());
					//Reset命令列表，并重新指定命令分配器和PSO对象
					GRS_THROW_IF_FAILED(pICMDList->Reset(pICMDAlloc.Get(), nullptr));
					//获取新的后缓冲序号，因为Present真正完成时后缓冲的序号就更新了
					nCurFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
					//---------------------------------------------------------------------------------------------
					// 以下是传统光追渲染需要的准备步骤，现在纯光追渲染的话可以不要了，
					// 我们不需要渲染到交换链的后缓冲区了，而是直接将光追出的画面整个的复制到后缓冲就行了

					//pICMDList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					//		pIRenderTargetBufs[nCurFrameIndex].Get()
					//		, D3D12_RESOURCE_STATE_PRESENT
					//		, D3D12_RESOURCE_STATE_RENDER_TARGET
					//	));

					////偏移描述符指针到指定帧缓冲视图位置，这些操作变成可做可不做
					//CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart(), nCurFrameIndex, nRTVDescriptorSize);
					////设置渲染目标
					//pICMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, nullptr);

					//pICMDList->RSSetViewports(1, &stViewPort);
					//pICMDList->RSSetScissorRects(1, &stScissorRect);

					//pICMDList->ClearRenderTargetView(stRTVHandle, c_faClearColor, 0, nullptr);
					//---------------------------------------------------------------------------------------------
					//开始渲染
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

					// 设置Index和Vertex的描述符堆句柄
					CD3DX12_GPU_DESCRIPTOR_HANDLE objIBHandle(
						pIDXRUAVHeap->GetGPUDescriptorHandleForHeapStart()
						, c_nDSHIndxIBView
						, nSRVDescriptorSize);
					pICMDList->SetComputeRootDescriptorTable(
						3
						, objIBHandle);

					//设置两个纹理的描述符堆句柄
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

					//关闭命令列表，可以去执行了
					GRS_THROW_IF_FAILED(pICMDList->Close());

					//执行命令列表
					ID3D12CommandList* ppCommandLists[] = { pICMDList.Get() };
					pICMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

					//提交画面
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					//---------------------------------------------------------------------------------------------
					//开始同步GPU与CPU的执行，先记录围栏标记值
					const UINT64 n64CurFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence1.Get(), n64CurFenceValue));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(pIFence1->SetEventOnCompletion(n64CurFenceValue, hEventFence1));
				}
				break;
				case 1:
				{//处理消息
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
				{//超时处理

				}
				break;
				default:
					break;
				}
			}
		}
	}
	catch (CGRSCOMException & e)
	{//发生了COM异常
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
	{//按动按键变换光源位置
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
