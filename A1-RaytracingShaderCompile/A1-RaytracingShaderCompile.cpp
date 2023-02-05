#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <wrl.h>  //添加WTL支持 方便使用COM
#include <strsafe.h>
#include <atlbase.h>
#include <atlcoll.h> //for atl array
#include <atlconv.h> //for T2A
//#include <d3dcompiler.h>
#include "../RayTracingFallback/Libraries/D3D12RaytracingFallback/Include/dxcapi.h"
#include "../Commons/GRSMem.h"
#include "../Commons/GRSCOMException.h"
//#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft;
using namespace Microsoft::WRL;

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


int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));  //for WIC & COM
		GRS_INIT_OUTPUT();

		ComPtr<IDxcCompiler2> pICompiler2;

		GRS_THROW_IF_FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pICompiler2)));

	}
	catch (CGRSCOMException & e)
	{//发生了COM异常
		e.Error();
	}
	GRS_FREE_OUTPUT();

	return 0;
}
