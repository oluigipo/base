/*
*   Implementation of api_os*.h, C++ translation unit.
*   Needed since DirectWrite headers are C++-exclusive.
*/

#include "common.h"
#include "api_os.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define INITGUID
#include <dwrite.h>
#include <d2d1.h>
#include <dxgi1_4.h>
#include <d3d12.h>

#include "api_os_d3d12.h"
#include "os_win32.h"

typedef HRESULT WINAPI D3D12CreateDeviceProc(IUnknown* pAdapter, int MinimumFeatureLevel, REFIID riid, void** ppDevice);
typedef HRESULT WINAPI CreateDXGIFactory1Proc(REFIID riid, void** ppFactory);

#if 0
#include "api_os_dwrite.h"
#pragma comment(lib, "Dwrite.lib")
#pragma comment(lib, "D2d1.lib")

API bool
OS_MakeDWriteApi(OS_DWriteApi* out_api, OS_DWriteApiDesc const* desc)
{
	Trace();
	HRESULT hr;
	IDWriteFactory* dwrite_factory = NULL;
	ID2D1Factory* d2d1_factory = NULL;
	
	D2D1_FACTORY_OPTIONS options = {};
	
	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite_factory);
	if (FAILED(hr))
		goto failure;
	
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &options, (void**)&d2d1_factory);
	if (FAILED(hr))
		goto failure;
	
	out_api->dwrite_factory = dwrite_factory;
	out_api->d2d1_factory = d2d1_factory;
	return true;
	
	failure:
	{
		if (dwrite_factory)
			dwrite_factory->Release();
		if (d2d1_factory)
			d2d1_factory->Release();
		return false;
	}
}
#endif

API bool
OS_MakeD3D12Api(OS_D3D12Api* out_api, OS_D3D12ApiDesc const* desc)
{
	Trace();
	
	WindowData_* window_data = (WindowData_*)desc->window.ptr;
	if (!window_data)
		return false;
	
	RTL_OSVERSIONINFOW version_info = { sizeof(RTL_OSVERSIONINFOW) };
	RtlGetVersion(&version_info);
	
	if (version_info.dwMajorVersion < 10)
	{
		OS_LogErr("[ERROR] win32: at %s: Windows version is not 10 or above\n", __func__);
		return false;
	}
	
	HRESULT hr;
	HMODULE dll_d3d12 = LoadLibraryW(L"d3d12.dll");
	HMODULE dll_dxgi = LoadLibraryW(L"dxgi.dll");
	SafeAssert(dll_d3d12 && dll_dxgi); // NOTE(ljre): These dlls are guaranteed to be on win10
	
	D3D12CreateDeviceProc* d3d12_create_device = (D3D12CreateDeviceProc*)GetProcAddress(dll_d3d12, "D3D12CreateDevice");
	CreateDXGIFactory1Proc* create_dxgi_factory1 = (CreateDXGIFactory1Proc*)GetProcAddress(dll_dxgi, "CreateDXGIFactory1");
	SafeAssert(d3d12_create_device && create_dxgi_factory1);
	
	if (desc->enable_debug_layer)
	{
		PFN_D3D12_GET_DEBUG_INTERFACE proc = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(dll_d3d12, "D3D12GetDebugInterface");
		if (proc)
		{
			ID3D12Debug* debug;
			hr = proc(IID_ID3D12Debug, (void**)&debug);
			if (SUCCEEDED(hr))
			{
				debug->EnableDebugLayer();
				debug->Release();
			}
		}
	}
	
	ID3D12Device* device = NULL;
	ID3D12Device2* device2 = NULL;
	ID3D12Fence* fence = NULL;
	ID3D12CommandQueue* cmdqueue = NULL;
	IDXGIAdapter1* adapter1 = NULL;
	IDXGIFactory4* factory4 = NULL;
	IDXGISwapChain3* swapchain3 = NULL;
	HANDLE fence_event = NULL;
	D3D_FEATURE_LEVEL minimum_ft = (D3D_FEATURE_LEVEL)desc->minimum_feature_level;
	if (!minimum_ft)
		minimum_ft = D3D_FEATURE_LEVEL_11_0;
	UINT buffer_count = Clamp((UINT)desc->render_target_count, 2, 4);
	IDXGISwapChain1* swapchain1 = NULL;
	HWND hwnd = window_data->handle;
	
	hr = create_dxgi_factory1(IID_IDXGIFactory4, (void**)&factory4);
	if (FAILED(hr))
	{
		OS_LogErr("[ERROR] win32: at %s: CreateDXGIFactory1(IID_IDXGIFactory4) failed with 0x%x\n", __func__, hr);
		goto failure;
	}
	
	for (UINT i = 0;; ++i)
	{
		IDXGIAdapter1* this_adapter;
		if (factory4->EnumAdapters1(i, &this_adapter) == DXGI_ERROR_NOT_FOUND)
			break;
		hr = d3d12_create_device((IUnknown*)this_adapter, minimum_ft, IID_ID3D12Device, NULL);
		if (SUCCEEDED(hr))
		{
			adapter1 = this_adapter;
			break;
		}
		this_adapter->Release();
	}
	if (!adapter1)
	{
		OS_LogErr("[ERROR] win32: at %s: failed to enum and choose an IDXGIAdapter1\n", __func__);
		goto failure;
	}
	
	hr = d3d12_create_device((IUnknown*)adapter1, minimum_ft, IID_ID3D12Device, (void**)&device);
	if (FAILED(hr))
	{
		OS_LogErr("[ERROR] win32: at %s: D3D12CreateDevice failed with 0x%x\n", __func__, hr);
		goto failure;
	}
	(void) device->QueryInterface(IID_ID3D12Device2, (void**)&device2); // It's ok if this fails
	
	if (desc->enable_debug_layer)
	{
		ID3D12InfoQueue* info_queue;
		hr = device->QueryInterface(IID_ID3D12InfoQueue, (void**)&info_queue);
		if (SUCCEEDED(hr))
		{
			(void) info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			(void) info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			(void) info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			info_queue->Release();
		}
	}
	
	// cmdqueue
	{
		D3D12_COMMAND_QUEUE_DESC cmdqueue_desc = {};
		cmdqueue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		cmdqueue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		hr = device->CreateCommandQueue(&cmdqueue_desc, IID_ID3D12CommandQueue, (void**)&cmdqueue);
		if (FAILED(hr))
		{
			OS_LogErr("[ERROR] win32: at %s: ID3D12Device::CreateCommandQueue failed with 0x%x\n", __func__, hr);
			goto failure;
		}
	}
	
	// swapchain3
	{
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc1 = {};
		swapchain_desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_desc1.SampleDesc.Count = 1;
		swapchain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchain_desc1.BufferCount = buffer_count;
		swapchain_desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		hr = factory4->CreateSwapChainForHwnd((IUnknown*)cmdqueue, window_data->handle, &swapchain_desc1, NULL, NULL, &swapchain1);
		if (FAILED(hr))
		{
			OS_LogErr("[ERROR] win32: at %s: IDXGIFactory4::CreateSwapChainForHwnd failed with 0x%x\n", __func__, hr);
			goto failure;
		}
		hr = swapchain1->QueryInterface(IID_IDXGISwapChain3, (void**)&swapchain3);
		if (FAILED(hr))
		{
			OS_LogErr("[ERROR] win32: at %s: IDXGISwapChain1::QueryInterface(IID_IDXGISwapChain3) failed with 0x%x\n", __func__, hr);
			goto failure;
		}
		swapchain1->Release();
		swapchain1 = NULL;
	}
	
	hr = factory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
	if (FAILED(hr))
		OS_LogErr("[WARN] win32: at %s: IDXGIFactory4::MakeWindowAssociation failed with 0x%x\n", __func__, hr);
	
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)&fence);
	if (FAILED(hr))
	{
		OS_LogErr("[ERROR] win32: at %s: ID3D12Device::CreateFence failed with 0x%x\n", __func__, hr);
		goto failure;
	}
	
	fence_event = CreateEventW(NULL, false, false, NULL);
	if (!fence_event || fence_event == INVALID_HANDLE_VALUE)
	{
		OS_LogErr("[ERROR] win32: at %s: CreateEventW failed with 0x%x\n", __func__, GetLastError());
		goto failure;
	}
	
	out_api->device = device;
	out_api->device2 = device2;
	out_api->cmdqueue = cmdqueue;
	out_api->fence = fence;
	out_api->adapter1 = adapter1;
	out_api->factory4 = factory4;
	out_api->swapchain3 = swapchain3;
	out_api->fence_event = fence_event;
	out_api->render_target_count = (int32)buffer_count;
	return true;
	
	failure:
	{
		if (device)
			device->Release();
		if (device2)
			device2->Release();
		if (cmdqueue)
			cmdqueue->Release();
		if (adapter1)
			adapter1->Release();
		if (factory4)
			factory4->Release();
		if (swapchain3)
			swapchain3->Release();
		if (swapchain1)
			swapchain1->Release();
		if (fence)
			fence->Release();
		if (fence_event)
			CloseHandle(fence_event);
		return false;
	}
}

API bool
OS_FreeD3D12Api(OS_D3D12Api* api)
{
	if (api->device)
		api->device->Release();
	if (api->device2)
		api->device2->Release();
	if (api->cmdqueue)
		api->cmdqueue->Release();
	if (api->fence)
		api->fence->Release();
	if (api->adapter1)
		api->adapter1->Release();
	if (api->factory4)
		api->factory4->Release();
	if (api->swapchain3)
		api->swapchain3->Release();
	if (api->fence_event)
		CloseHandle(api->fence_event);
	
	MemoryZero(api, sizeof(OS_D3D12Api));
	return true;
}

API bool
OS_MakeD3D12Api2(OS_D3D12Api2* out_api, bool enable_debug_layer)
{
	Trace();
	RTL_OSVERSIONINFOW version_info = { sizeof(RTL_OSVERSIONINFOW) };
	RtlGetVersion(&version_info);
	
	if (version_info.dwMajorVersion < 10)
	{
		OS_MessageBox(Str("Error"), Str("Not windows 10"));
		return false;
	}

	HMODULE dll_d3d12 = LoadLibraryW(L"d3d12.dll");
	HMODULE dll_dxgi = LoadLibraryW(L"dxgi.dll");

	if (!dll_d3d12 || !dll_dxgi)
	{
		if (dll_d3d12)
			FreeLibrary(dll_d3d12);
		if (dll_dxgi)
			FreeLibrary(dll_dxgi);
		OS_MessageBox(Str("Error"), Str("no d3d12.dll or dxgi.dll?"));
		return false;
	}

	D3D12CreateDeviceProc* d3d12_create_device = (D3D12CreateDeviceProc*)GetProcAddress(dll_d3d12, "D3D12CreateDevice");
	CreateDXGIFactory1Proc* create_dxgi_factory1 = (CreateDXGIFactory1Proc*)GetProcAddress(dll_dxgi, "CreateDXGIFactory1");
	PFN_D3D12_SERIALIZE_ROOT_SIGNATURE d3d12_serialize_root_signature = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(dll_d3d12, "D3D12SerializeRootSignature");
	if (!d3d12_create_device || !create_dxgi_factory1 || !d3d12_serialize_root_signature)
	{
		if (dll_d3d12)
			FreeLibrary(dll_d3d12);
		if (dll_dxgi)
			FreeLibrary(dll_dxgi);
		OS_MessageBox(Str("Error"), Str("missing required functions:\n    D3D12CreateDevice\n    CreateDXGIFactory1\n    D3D12SerializeRootSignature"));
		return false;
	}

	bool debug_layer_enabled = false;
	if (enable_debug_layer)
	{
		PFN_D3D12_GET_DEBUG_INTERFACE proc = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(dll_d3d12, "D3D12GetDebugInterface");
		if (proc)
		{
			ID3D12Debug* debug;
			HRESULT hr = proc(IID_ID3D12Debug, (void**)&debug);
			if (SUCCEEDED(hr))
			{
				debug->EnableDebugLayer();
				debug->Release();
				debug_layer_enabled = true;
			}
		}
	}

	out_api->d3d12_create_device = d3d12_create_device;
	out_api->create_dxgi_factory1 = create_dxgi_factory1;
	out_api->d3d12_serialize_root_signature = d3d12_serialize_root_signature;
	out_api->debug_layer_enabled = debug_layer_enabled;
	return true;
}

API bool
OS_FreeD3D12Api2(OS_D3D12Api2* out_api)
{
	Trace();

	// TODO(ljre): Maybe force unload the d3d12 and dxgi modules? not sure, might break some stuff.
	MemoryZero(out_api, sizeof(*out_api));
	return true;
}