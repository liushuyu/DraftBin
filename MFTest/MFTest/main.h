#pragma once

#define WINVER _WIN32_WINNT_WIN7

#include <stdio.h>
#include <assert.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <comdef.h>

#include <iostream>
#include <string>

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

void ReportError(std::wstring msg, HRESULT hr) {
	if (SUCCEEDED(hr))
	{
		return;
	}
	LPWSTR err;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hr,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		(LPWSTR)&err,
		0,
		NULL);
	if (err != NULL)
	{
		std::wcout << msg << L": " << err << std::endl;
	}
}