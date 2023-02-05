#pragma once
#include <atlexcept.h>

#define GRS_STRING2(x) #x
#define GRS_STRING(x) GRS_STRING2(x)

//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#ifndef GRS_UPPER
#define GRS_UPPER(A,B) ((size_t)(((A)+((B)-1))&~((B) - 1)))
#endif

//��ȡ������
#ifndef GRS_UPPER_DIV
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
#endif

#ifndef GRS_THROW_IF_FAILED
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ ATLTRACE("Error: 0x%08x\n",_hr); AtlThrow(_hr); }}
#endif

#define GRS_SLEEP(dwMilliseconds)  ::WaitForSingleObject(::GetCurrentThread(),dwMilliseconds)