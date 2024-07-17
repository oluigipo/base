#ifndef API_OS_D3D11_H
#define API_OS_D3D11_H

#include "common.h"
#include "api_os.h"

// NOTE(ljre):
//     Usage of this API will require:
//         - Windows 7 (with Platform Update) or later;

struct OS_D3D11Api typedef OS_D3D11Api;
struct OS_D3D11Api
{
	struct ID3D11Device* device;
	struct ID3D11Device1* device1;
	struct ID3D11DeviceContext* context;
	struct ID3D11DeviceContext1* context1;
	struct IDXGISwapChain* swapchain;
	struct IDXGISwapChain1* swapchain1;
	struct IDXGIFactory2* factory2;
	struct IDXGIAdapter* adapter;
	struct IDXGIDevice1* dxgi_device1;
	struct ID3D11RenderTargetView* target;
	struct ID3D11DepthStencilView* depth_stencil;
	
	void (*present)(OS_D3D11Api* d3d11_api);
	void (*resize_buffers)(OS_D3D11Api* d3d11_api);
};

struct OS_D3D11ApiDesc
{
	OS_Window window;
	bool use_igpu_if_possible : 1;
	bool hw_video_decoding : 1;
}
typedef OS_D3D11ApiDesc;

API bool OS_MakeD3D11Api(OS_D3D11Api* out_api, OS_D3D11ApiDesc const* args);
API bool OS_FreeD3D11Api(OS_D3D11Api* api);

#ifdef __cplusplus
static inline bool OS_MakeD3D11Api(OS_D3D11Api* out_api, OS_D3D11ApiDesc const& args) { return OS_MakeD3D11Api(out_api, &args); }
#endif

#endif //API_OS_D3D11_H
