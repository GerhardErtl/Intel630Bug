#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#define NOMINMAX

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifdef _DEBUG
//#define new new( _CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#define MY_IID_PPV_ARGS IID_PPV_ARGS
#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <string>
#include <wrl.h>
#include <shellapi.h>

#include "math\VectorMath.h"
#include "Utility.h"

#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

#define Logging
#undef Logging

#define PrintStatisticsValue(messageBuffer, v) OutputDebugStringA ((std::string(#v) + std::string(": ") + Timers::StringWithSeperators(stats.v) + "\n").c_str()); \
        printf((std::string(#v) + std::string(": ") + Timers::StringWithSeperators(stats.v) + "\n").c_str()); \
        messageBuffer += ((std::string(#v) + std::string(": ") + Timers::StringWithSeperators(stats.v) + "\n").c_str());

#define PrintText(messageBuffer, v) OutputDebugStringA ((std::string(v) + "\n").c_str()); \
        printf((std::string(v) + "\n").c_str()); \
        messageBuffer += std::string(v) + "\n";