#pragma once

//#include <SDKDDKVer.h>

//#ifndef WIN32_LEAN_AND_MEAN
//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
//#endif

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <string>
#include <wrl.h>
#include <shellapi.h>

#include <unordered_map>
#include <vector>
#include <array>

#include <fbxsdk.h>
#include "DirectXTex.h"
#include "DDSTextureLoader12.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

template<typename T> using ptr = T*;

