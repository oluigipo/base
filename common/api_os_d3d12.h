#ifndef API_OS_D3D12_H
#define API_OS_D3D12_H

// NOTE(ljre):
//     Usage of this API will require:
//         - Windows 10 or later;

struct OS_D3D12Api
{
	struct ID3D12Device* device;
	struct ID3D12Device2* device2;
	struct ID3D12CommandQueue* cmdqueue;
	struct ID3D12Fence* fence;
	struct IDXGIAdapter1* adapter1;
	struct IDXGIFactory4* factory4;
	struct IDXGISwapChain3* swapchain3;
	void* fence_event; // HANDLE
	int32 render_target_count;
}
typedef OS_D3D12Api;

struct OS_D3D12ApiDesc
{
	OS_Window window;
	
	int32 minimum_feature_level;
	int32 render_target_count;
	
	bool enable_debug_layer : 1;
}
typedef OS_D3D12ApiDesc;

API bool OS_MakeD3D12Api(OS_D3D12Api* out_api, OS_D3D12ApiDesc const* desc);
API bool OS_FreeD3D12Api(OS_D3D12Api* api);

#ifdef __cplusplus
static inline bool OS_MakeD3D12Api(OS_D3D12Api* out_api, OS_D3D12ApiDesc const& desc) { return OS_MakeD3D12Api(out_api, &desc); }
#endif

struct _GUID;
struct IUnknown;
struct D3D12_ROOT_SIGNATURE_DESC typedef D3D12_ROOT_SIGNATURE_DESC;
struct ID3D10Blob typedef ID3D10Blob;
enum D3D_ROOT_SIGNATURE_VERSION typedef D3D_ROOT_SIGNATURE_VERSION;

struct OS_D3D12Api2
{
	long /*HRESULT*/ (__stdcall* d3d12_create_device)(
		struct IUnknown* pAdapter,
		int /*D3D_FEATURE_LEVEL*/ MinimumFeatureLevel,
		#ifdef __cplusplus
		struct _GUID const& riid,
		#else
		struct _GUID const* riid,
		#endif
		void** ppDevice);
	long /*HRESULT*/ (__stdcall* create_dxgi_factory1)(
		#ifdef __cplusplus
		struct _GUID const& riid,
		#else
		struct _GUID const* riid,
		#endif
		void** ppFactory);
	long /*HRESULT*/ (__stdcall* d3d12_serialize_root_signature)(
		D3D12_ROOT_SIGNATURE_DESC const* pRootSignature,
		D3D_ROOT_SIGNATURE_VERSION Version,
		ID3D10Blob** ppBlob,
		ID3D10Blob** ppErrorBlob);
	bool debug_layer_enabled;
}
typedef OS_D3D12Api2;

API bool OS_MakeD3D12Api2(OS_D3D12Api2* out_api, bool enable_debug_layer);
API bool OS_FreeD3D12Api2(OS_D3D12Api2* api);

#endif //API_OS_D3D12_H
