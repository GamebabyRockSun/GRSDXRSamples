#pragma once

#include "GRSMem.h"
#include "GRSCOMException.h"

#ifndef GRS_SAFE_CLOSEFILE
//安全关闭一个文件句柄
#define GRS_SAFE_CLOSEFILE(h) if(INVALID_HANDLE_VALUE != (h)){CloseHandle(h);(h)=INVALID_HANDLE_VALUE;}
#endif // !GRS_SAFE_CLOSEFILE

#ifdef _DEBUG
#define GRS_ASSERT(s) if(!(s)) { ::DebugBreak(); }
#else
#define GRS_ASSERT(s) 
#endif

#ifndef GRS_INLINE
#define GRS_INLINE __inline
#endif


class CGRSD3DInclude : public ID3DInclude
{
protected:
	TCHAR m_pszLocalDir[MAX_PATH];
	TCHAR m_pszSystemDir[MAX_PATH];
public:
	CGRSD3DInclude()
	{//默认情况下系统目录和本地目录都设置为应用当前目录
		ZeroMemory(m_pszLocalDir,MAX_PATH * sizeof(TCHAR));
		ZeroMemory(m_pszSystemDir,MAX_PATH * sizeof(TCHAR));
		GetCurrentDirectory(MAX_PATH,m_pszLocalDir);
		GetCurrentDirectory(MAX_PATH,m_pszSystemDir);
	}

	CGRSD3DInclude(LPCTSTR pszLocalDir, LPCTSTR pszSystemDir) 
	{ 
		ZeroMemory(m_pszLocalDir,MAX_PATH * sizeof(TCHAR));
		ZeroMemory(m_pszSystemDir,MAX_PATH * sizeof(TCHAR));
		SetLocalDir(pszLocalDir);
		SetSystemDir(pszSystemDir);
	}
	virtual ~CGRSD3DInclude()
	{

	}
public:
	VOID SetLocalDir(LPCTSTR pszLocalDir)
	{
		StringCchCopy(m_pszLocalDir,MAX_PATH,pszLocalDir);
	}
	VOID SetSystemDir(LPCTSTR pszSystemDir)
	{
		StringCchCopy(m_pszSystemDir,MAX_PATH,pszSystemDir);
	}
public:
	STDMETHODIMP Open(D3D_INCLUDE_TYPE IncludeType,LPCSTR pFileName,LPCVOID pParentData,LPCVOID *ppData,UINT *pBytes);
	STDMETHODIMP Close(LPCVOID pData);
};

GRS_INLINE STDMETHODIMP CGRSD3DInclude::Open(D3D_INCLUDE_TYPE IncludeType,LPCSTR pFileName,LPCVOID pParentData,LPCVOID *ppData,UINT *pBytes)
{
	GRS_ASSERT(NULL != ppData);
	GRS_ASSERT(NULL != pBytes);

	HRESULT hRet = S_OK;
	HANDLE	hFile = INVALID_HANDLE_VALUE;
	VOID* pFileData = NULL;

	try
	{
		*ppData = NULL;
		*pBytes = 0;

		USES_CONVERSION;
		TCHAR pszIncludeFile[MAX_PATH] = {};

		switch(IncludeType)
		{
		case D3D_INCLUDE_LOCAL: 
			{// #include "FILE"
				StringCchPrintf(pszIncludeFile,MAX_PATH,_T("%s\\%s"),m_pszLocalDir,A2T(pFileName));
			}
			break;
		case D3D_INCLUDE_SYSTEM: 
			{// #include <FILE>
				StringCchPrintf(pszIncludeFile,MAX_PATH,_T("%s\\%s"),m_pszSystemDir,A2T(pFileName));
			}
			break;
		default:
			{
				GRS_ASSERT(FALSE);
			}
			break;	
		}
		
		hFile = CreateFile(pszIncludeFile,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
		if(INVALID_HANDLE_VALUE == hFile)
		{
			throw CGRSCOMException(GetLastError());
		}
		
		DWORD dwFileSize = GetFileSize(hFile,NULL);//Include文件一般不会超过4G大小,所以......
		pFileData = GRS_CALLOC(dwFileSize);
		if(!ReadFile(hFile,pFileData,dwFileSize,(LPDWORD)pBytes,NULL))
		{
			throw CGRSCOMException(GetLastError());
		}
		*ppData = pFileData;
	}
	catch(CGRSCOMException& e)
	{
		e;
		GRS_SAFE_FREE(pFileData);
		hRet = E_FAIL;
	}

	GRS_SAFE_CLOSEFILE(hFile);
	
	return hRet;
}

GRS_INLINE STDMETHODIMP CGRSD3DInclude::Close(LPCVOID pData)
{	
	if(NULL != pData)
	{
		GRS_FREE((LPVOID)pData);
	}
	return S_OK;
}

////--------------------------------------------------------------------------------------
//// Use this until D3DX11 comes online and we get some compilation helpers
////--------------------------------------------------------------------------------------
//static const unsigned int MAX_INCLUDES = 9;
//struct sInclude
//{
//	HANDLE         hFile;
//	HANDLE         hFileMap;
//	LARGE_INTEGER  FileSize;
//	void* pMapData;
//};
//
//class CIncludeHandler : public ID3DInclude
//{
//private:
//	struct sInclude   m_includeFiles[MAX_INCLUDES];
//	unsigned int      m_nIncludes;
//
//public:
//	CIncludeHandler()
//	{
//		// array initialization
//		for (unsigned int i = 0; i < MAX_INCLUDES; i++)
//		{
//			m_includeFiles[i].hFile = INVALID_HANDLE_VALUE;
//			m_includeFiles[i].hFileMap = INVALID_HANDLE_VALUE;
//			m_includeFiles[i].pMapData = NULL;
//		}
//		m_nIncludes = 0;
//	}
//	~CIncludeHandler()
//	{
//		for (unsigned int i = 0; i < m_nIncludes; i++)
//		{
//			UnmapViewOfFile(m_includeFiles[i].pMapData);
//
//			if (m_includeFiles[i].hFileMap != INVALID_HANDLE_VALUE)
//				CloseHandle(m_includeFiles[i].hFileMap);
//
//			if (m_includeFiles[i].hFile != INVALID_HANDLE_VALUE)
//				CloseHandle(m_includeFiles[i].hFile);
//		}
//
//		m_nIncludes = 0;
//	}
//
//	STDMETHOD(Open(
//		D3D_INCLUDE_TYPE IncludeType,
//		LPCSTR pFileName,
//		LPCVOID pParentData,
//		LPCVOID* ppData,
//		UINT* pBytes
//	))
//	{
//		unsigned int   incIndex = m_nIncludes + 1;
//
//		// Make sure we have enough room for this include file
//		if (incIndex >= MAX_INCLUDES)
//			return E_FAIL;
//
//		// try to open the file
//		m_includeFiles[incIndex].hFile = CreateFileA(pFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
//			FILE_FLAG_SEQUENTIAL_SCAN, NULL);
//		if (INVALID_HANDLE_VALUE == m_includeFiles[incIndex].hFile)
//		{
//			return E_FAIL;
//		}
//
//		// Get the file size
//		GetFileSizeEx(m_includeFiles[incIndex].hFile, &m_includeFiles[incIndex].FileSize);
//
//		// Use Memory Mapped File I/O for the header data
//		m_includeFiles[incIndex].hFileMap = CreateFileMappingA(m_includeFiles[incIndex].hFile, NULL, PAGE_READONLY, m_includeFiles[incIndex].FileSize.HighPart, m_includeFiles[incIndex].FileSize.LowPart, pFileName);
//		if (m_includeFiles[incIndex].hFileMap == NULL)
//		{
//			if (m_includeFiles[incIndex].hFile != INVALID_HANDLE_VALUE)
//				CloseHandle(m_includeFiles[incIndex].hFile);
//			return E_FAIL;
//		}
//
//		// Create Map view
//		*ppData = MapViewOfFile(m_includeFiles[incIndex].hFileMap, FILE_MAP_READ, 0, 0, 0);
//		*pBytes = m_includeFiles[incIndex].FileSize.LowPart;
//
//		// Success - Increment the include file count
//		m_nIncludes = incIndex;
//
//		return S_OK;
//	}
//
//	STDMETHOD(Close(LPCVOID pData))
//	{
//		// Defer Closure until the container destructor 
//		return S_OK;
//	}
//};
//
//HRESULT CompileShaderFromFile(WCHAR* szFileName, DWORD flags, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
//{
//	HRESULT hr = S_OK;
//
//	// find the file
//	WCHAR str[MAX_PATH];
//	WCHAR workingPath[MAX_PATH], filePath[MAX_PATH];
//	WCHAR* strLastSlash = NULL;
//	bool  resetCurrentDir = false;
//
//	// Get the current working directory so we can restore it later
//	UINT nBytes = GetCurrentDirectory(MAX_PATH, workingPath);
//	if (nBytes == MAX_PATH)
//	{
//		return E_FAIL;
//	}
//
//	// Check we can find the file first
//	V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, szFileName));
//
//	// Check if the file is in the current working directory
//	wcscpy_s(filePath, MAX_PATH, str);
//
//	strLastSlash = wcsrchr(filePath, TEXT('\\'));
//	if (strLastSlash)
//	{
//		// Chop the exe name from the exe path
//		*strLastSlash = 0;
//
//		SetCurrentDirectory(filePath);
//		resetCurrentDir = true;
//	}
//
//	// open the file
//	HANDLE hFile = CreateFile(str, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
//		FILE_FLAG_SEQUENTIAL_SCAN, NULL);
//	if (INVALID_HANDLE_VALUE == hFile)
//		return E_FAIL;
//
//	// Get the file size
//	LARGE_INTEGER FileSize;
//	GetFileSizeEx(hFile, &FileSize);
//
//	// create enough space for the file data
//	BYTE* pFileData = new BYTE[FileSize.LowPart];
//	if (!pFileData)
//		return E_OUTOFMEMORY;
//
//	// read the data in
//	DWORD BytesRead;
//	if (!ReadFile(hFile, pFileData, FileSize.LowPart, &BytesRead, NULL))
//		return E_FAIL;
//
//	CloseHandle(hFile);
//
//	// Create an Include handler instance
//	CIncludeHandler* pIncludeHandler = new CIncludeHandler;
//
//	// Compile the shader using optional defines and an include handler for header processing
//	ID3DBlob* pErrorBlob;
//	hr = D3DCompile(pFileData, FileSize.LowPart, "none", NULL, static_cast<ID3DInclude*> (pIncludeHandler),
//		szEntryPoint, szShaderModel, flags, D3DCOMPILE_EFFECT_ALLOW_SLOW_OPS, ppBlobOut, &pErrorBlob);
//
//	delete pIncludeHandler;
//	delete[]pFileData;
//
//	// Restore the current working directory if we need to 
//	if (resetCurrentDir)
//	{
//		SetCurrentDirectory(workingPath);
//	}
//
//
//	if (FAILED(hr))
//	{
//		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
//		SAFE_RELEASE(pErrorBlob);
//		return hr;
//	}
//	SAFE_RELEASE(pErrorBlob);
//
//	return hr;
//}