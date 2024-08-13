#include "common.h"
#include "api_os.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#define INITGUID
#include <dxgi1_2.h>
#include <d3d11_1.h>

#include <windowsx.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <mfapi.h>
#include <codecapi.h>

#include "third_party/cd2d.h"
#include "third_party/cdwrite.h"

#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplat.lib")

#include "api_os_d3d11.h"
#include "api_os_win32.h"
#include "api_render3.h"

struct R3_Context
{
	OS_D3D11Api api;
	IMFDXGIDeviceManager* mf_device_manager;
	IMFAttributes* mf_attribs;
	D3D_FEATURE_LEVEL feature_level;
	HRESULT hr_status;
	uint8 adapter_desc[256];
}
typedef R3_Context;

static bool
CheckHr_(R3_Context* ctx, HRESULT hr)
{
	if (SUCCEEDED(hr))
		return false;

	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		ctx->hr_status = hr;
		OS_LogErr("[ERROR] render3_d3d11: Device Lost!\n");
	}
	else
	{
		ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
		String err = OS_W32_StringFromHr(hr, scratch.arena);
		OS_LogErr("[ERROR] render3_d3d11: HR Failure: %S\n", err);
		ArenaRestore(scratch);
	}
	return true;
}

static DXGI_FORMAT
D3d11FormatToDxgi_(R3_Format format, uint32* out_pixel_size, uint32* out_block_size)
{
	DXGI_FORMAT result = 0;
	uint32 size = 0;
	uint32 block_size = 0;
	
	switch (format)
	{
		case R3_Format__Count:
		case R3_Format_Null: result = DXGI_FORMAT_UNKNOWN; break;
		
		case R3_Format_U8x1Norm:         result = DXGI_FORMAT_R8_UNORM;            size = 1; break;
		case R3_Format_U8x1Norm_ToAlpha: result = DXGI_FORMAT_A8_UNORM;            size = 1; break;
		case R3_Format_U8x2Norm:         result = DXGI_FORMAT_R8G8_UNORM;          size = 2; break;
		case R3_Format_U8x4Norm:         result = DXGI_FORMAT_R8G8B8A8_UNORM;      size = 4; break;
		case R3_Format_U8x4Norm_Srgb:    result = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; size = 4; break;
		case R3_Format_U8x4Norm_Bgrx:    result = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB; size = 4; break;
		case R3_Format_U8x4Norm_Bgra:    result = DXGI_FORMAT_B8G8R8A8_UNORM;      size = 4; break;
		case R3_Format_U8x1:             result = DXGI_FORMAT_R8_UINT;             size = 1; break;
		case R3_Format_U8x2:             result = DXGI_FORMAT_R8G8_UINT;           size = 2; break;
		case R3_Format_U8x4:             result = DXGI_FORMAT_R8G8B8A8_UINT;       size = 4; break;
		case R3_Format_I16x2Norm:        result = DXGI_FORMAT_R16G16_SNORM;        size = 4; break;
		case R3_Format_I16x4Norm:        result = DXGI_FORMAT_R16G16B16A16_SNORM;  size = 8; break;
		case R3_Format_I16x2:            result = DXGI_FORMAT_R16G16_SINT;         size = 4; break;
		case R3_Format_I16x4:            result = DXGI_FORMAT_R16G16B16A16_SINT;   size = 8; break;
		case R3_Format_U16x1Norm:        result = DXGI_FORMAT_R16_UNORM;           size = 2; break;
		case R3_Format_U16x2Norm:        result = DXGI_FORMAT_R16G16_UNORM;        size = 4; break;
		case R3_Format_U16x4Norm:        result = DXGI_FORMAT_R16G16B16A16_UNORM;  size = 8; break;
		case R3_Format_U16x1:            result = DXGI_FORMAT_R16_UINT;            size = 2; break;
		case R3_Format_U16x2:            result = DXGI_FORMAT_R16G16_UINT;         size = 4; break;
		case R3_Format_U16x4:            result = DXGI_FORMAT_R16G16B16A16_UINT;   size = 8; break;
		case R3_Format_U32x1:            result = DXGI_FORMAT_R32_UINT;            size = 4; break;
		case R3_Format_U32x2:            result = DXGI_FORMAT_R32G32_UINT;         size = 8; break;
		case R3_Format_U32x4:            result = DXGI_FORMAT_R32G32B32A32_UINT;   size = 16; break;
		
		case R3_Format_F16x2: result = DXGI_FORMAT_R16G16_FLOAT;       size = 4; break;
		case R3_Format_F16x4: result = DXGI_FORMAT_R16G16B16A16_FLOAT; size = 8; break;
		case R3_Format_F32x1: result = DXGI_FORMAT_R32_FLOAT;          size = 4; break;
		case R3_Format_F32x2: result = DXGI_FORMAT_R32G32_FLOAT;       size = 8; break;
		case R3_Format_F32x3: result = DXGI_FORMAT_R32G32B32_FLOAT;    size = 12; break;
		case R3_Format_F32x4: result = DXGI_FORMAT_R32G32B32A32_FLOAT; size = 16; break;
		
		case R3_Format_D16:   result = DXGI_FORMAT_D16_UNORM;         size = 2; break;
		case R3_Format_D24S8: result = DXGI_FORMAT_D24_UNORM_S8_UINT; size = 4; break;

		case R3_Format_BC1: result = DXGI_FORMAT_BC1_UNORM; block_size = 8; break;
		case R3_Format_BC2: result = DXGI_FORMAT_BC2_UNORM; block_size = 16; break;
		case R3_Format_BC3: result = DXGI_FORMAT_BC3_UNORM; block_size = 16; break;
		case R3_Format_BC4: result = DXGI_FORMAT_BC4_UNORM; block_size = 8; break;
		case R3_Format_BC5: result = DXGI_FORMAT_BC5_UNORM; block_size = 16; break;
		case R3_Format_BC6: result = DXGI_FORMAT_BC6H_UF16; block_size = 16; break;
		case R3_Format_BC7: result = DXGI_FORMAT_BC7_UNORM; block_size = 16; break;
	}
	
	if (out_pixel_size)
		*out_pixel_size = size;
	if (out_block_size)
		*out_block_size = block_size;
	return result;
}

static D3D11_USAGE
D3d11UsageToD3dUsage_(R3_Usage usage)
{
	switch (usage)
	{
		default: SafeAssert(!"value for R3_Usage not known");
		
		case R3_Usage_Immutable:    return D3D11_USAGE_IMMUTABLE;
		case R3_Usage_Dynamic:      return D3D11_USAGE_DYNAMIC;
		case R3_Usage_GpuReadWrite: return D3D11_USAGE_DEFAULT;
	}
	return 0;
}

//------------------------------------------------------------------------
API R3_Context*
R3_D3D11_MakeContext(Arena* output_arena, R3_ContextDesc const* desc)
{
	Trace();
	R3_Context* ctx = ArenaPushStruct(output_arena, R3_Context);
	if (!ctx)
		return NULL;

	bool ok = OS_MakeD3D11Api(&ctx->api, &(OS_D3D11ApiDesc) {
		.window = *desc->window,
	});
	if (!ok)
	{
		ArenaPop(output_arena, ctx);
		return NULL;
	}

	ctx->feature_level = ID3D11Device_GetFeatureLevel(ctx->api.device);

	return ctx;
}

API R3_ContextInfo
R3_QueryInfo(R3_Context* ctx)
{
	Trace();
	R3_ContextInfo info = {
		.backend_api = StrInit("Direct3D 11"),
		.driver_renderer = StrInit("Unknown"),
		.driver_adapter = StrInit("TODO"),
		.driver_version = StrInit("TODO"),
	};

	D3D_FEATURE_LEVEL feature_level = ctx->feature_level;
	DXGI_ADAPTER_DESC adapter_desc;
	if (SUCCEEDED(IDXGIAdapter_GetDesc(ctx->api.adapter, &adapter_desc)))
	{
		Arena arena = ArenaFromMemory(ctx->adapter_desc, sizeof(ctx->adapter_desc));
		info.driver_adapter = OS_W32_WideToString(adapter_desc.Description, &arena);
	}

	switch (feature_level)
	{
		default: break;
		//case D3D_FEATURE_LEVEL_12_2: info.driver_renderer = Str("Feature Level 12_2"); break;
		case D3D_FEATURE_LEVEL_12_1: info.driver_renderer = Str("Feature Level 12_1"); break;
		case D3D_FEATURE_LEVEL_12_0: info.driver_renderer = Str("Feature Level 12_0"); break;
		case D3D_FEATURE_LEVEL_11_1: info.driver_renderer = Str("Feature Level 11_1"); break;
		case D3D_FEATURE_LEVEL_11_0: info.driver_renderer = Str("Feature Level 11_0"); break;
		case D3D_FEATURE_LEVEL_10_1: info.driver_renderer = Str("Feature Level 10_1"); break;
		case D3D_FEATURE_LEVEL_10_0: info.driver_renderer = Str("Feature Level 10_0"); break;
		case D3D_FEATURE_LEVEL_9_3: info.driver_renderer = Str("Feature Level 9_3"); break;
		case D3D_FEATURE_LEVEL_9_2: info.driver_renderer = Str("Feature Level 9_2"); break;
		case D3D_FEATURE_LEVEL_9_1: info.driver_renderer = Str("Feature Level 9_1"); break;
	}

	if (feature_level >= D3D_FEATURE_LEVEL_9_1) // NOTE(ljre): This should always be true, really
	{
		info.has_base_vertex = true;
		info.max_texture_size = 2048;
		info.max_render_target_textures = 1;
		info.max_textures_per_drawcall = 8;
		info.supported_texture_formats[0] |= (1 << R3_Format_D16);
		info.supported_texture_formats[0] |= (1 << R3_Format_D24S8);
		info.supported_texture_formats[0] |= (1 << R3_Format_U8x1Norm);
		info.supported_texture_formats[0] |= (1 << R3_Format_U8x4Norm);
		info.supported_texture_formats[0] |= (1 << R3_Format_U8x4Norm_Srgb);
		info.supported_texture_formats[0] |= (1ULL << R3_Format_BC1);
		info.supported_texture_formats[0] |= (1ULL << R3_Format_BC2);
		info.supported_texture_formats[0] |= (1ULL << R3_Format_BC3);
		info.supported_render_target_formats[0] |= (1 << R3_Format_U8x4Norm);
		info.supported_render_target_formats[0] |= (1 << R3_Format_U8x4Norm_Srgb);
	}
	
	if (feature_level >= D3D_FEATURE_LEVEL_9_2)
	{
		info.has_32bit_index = true;
		info.has_separate_alpha_blend = true;
		info.supported_texture_formats[0] |= (1 << R3_Format_U8x1Norm_ToAlpha);
		info.supported_texture_formats[0] |= (1 << R3_Format_U16x1Norm);
		info.supported_texture_formats[0] |= (1 << R3_Format_U16x2Norm);
		info.supported_texture_formats[0] |= (1 << R3_Format_U16x4Norm);
		info.supported_texture_formats[0] |= (1 << R3_Format_F16x2);
		info.supported_texture_formats[0] |= (1 << R3_Format_F16x4);
		info.supported_render_target_formats[0] |= (1 << R3_Format_U16x4Norm);
		info.supported_render_target_formats[0] |= (1 << R3_Format_F16x4);
	}
	
	if (feature_level >= D3D_FEATURE_LEVEL_9_3)
	{
		info.max_texture_size = 4096;
		info.max_render_target_textures = 4;
		info.has_instancing = true;
		info.supported_texture_formats[0] |= (1 << R3_Format_F32x1);
		info.supported_texture_formats[0] |= (1 << R3_Format_F32x3);
		info.supported_texture_formats[0] |= (1 << R3_Format_F32x4);
		info.supported_render_target_formats[0] |= (1 << R3_Format_F32x4);
	}
	
	if (feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		info.max_texture_size = 8192;
		info.max_render_target_textures = 8;
		info.supported_texture_formats[0] |= (1 << R3_Format_U8x2Norm);
		info.supported_texture_formats[0] |= (1 << R3_Format_I16x4Norm);
		info.supported_texture_formats[0] |= (1 << R3_Format_I16x4);
		info.supported_texture_formats[0] |= (1 << R3_Format_U16x2);
		info.supported_texture_formats[0] |= (1 << R3_Format_U16x4);
		info.supported_texture_formats[0] |= (1 << R3_Format_U32x2);
		info.supported_texture_formats[0] |= (1 << R3_Format_U32x4);
		info.supported_texture_formats[0] |= (1 << R3_Format_F32x2);
		info.supported_texture_formats[0] |= (1ull << R3_Format_BC4);
		info.supported_texture_formats[0] |= (1ull << R3_Format_BC5);
	}
	
	if (feature_level >= D3D_FEATURE_LEVEL_10_1)
	{
	}
	
	if (feature_level >= D3D_FEATURE_LEVEL_11_0)
	{
		info.max_texture_size = 16384;
		info.max_dispatch_x = 65535u;
		info.max_dispatch_y = 65535u;
		info.max_dispatch_z = 65535u;
		info.has_compute_pipeline = true;
		info.supported_texture_formats[0] |= (1ull << R3_Format_BC6);
		info.supported_texture_formats[0] |= (1ull << R3_Format_BC7);
	}
	
	if (feature_level >= D3D_FEATURE_LEVEL_11_1)
	{
	}

	//------------------------------------------------------------------------
	// Check optional features
	if (!info.has_compute_pipeline)
	{
		D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options = { 0 };
		if (SUCCEEDED(ID3D11Device_CheckFeatureSupport(
			ctx->api.device, D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &options, sizeof(options))))
		{
			info.has_compute_pipeline = options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x;
			info.max_dispatch_x = 65535u;
			info.max_dispatch_y = 65535u;
			info.max_dispatch_z = 65535u;
		}
	}
	
	return info;
}

API void
R3_Present(R3_Context* ctx)
{
	Trace();
	ctx->api.present(&ctx->api);
}

API void
R3_ResizeBuffers(R3_Context* ctx)
{
	Trace();
	ctx->api.resize_buffers(&ctx->api);
}

API bool
R3_IsDeviceLost(R3_Context* ctx)
{
	Trace();
	HRESULT hr = ctx->hr_status;
	return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

API void
R3_FreeContext(R3_Context* ctx)
{
	Trace();
	OS_FreeD3D11Api(&ctx->api);
}

//------------------------------------------------------------------------
API R3_Texture
R3_MakeTexture(R3_Context* ctx, R3_TextureDesc const* desc)
{
	Trace();
	R3_Texture out = {};
	HRESULT hr;

	D3D11_USAGE usage = D3d11UsageToD3dUsage_(desc->usage);
	uint32 pixel_size;
	DXGI_FORMAT format = D3d11FormatToDxgi_(desc->format, &pixel_size, NULL);
	DXGI_FORMAT format_srv, format_uav;
	switch (format)
	{
		case DXGI_FORMAT_D16_UNORM:
			format_srv = format_uav = DXGI_FORMAT_R16_UNORM;
			format = DXGI_FORMAT_R16_TYPELESS;
			break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			format_srv = format_uav = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			format = DXGI_FORMAT_R24G8_TYPELESS;
			break;
		default:
			format_srv = format_uav = format;
			break;
	}

	UINT bind_flags = 0;
	UINT misc_flags = 0;
	if (desc->binding_flags & R3_BindingFlag_VertexBuffer)
		SafeAssert(false);
	if (desc->binding_flags & R3_BindingFlag_IndexBuffer)
		SafeAssert(false);
	if (desc->binding_flags & R3_BindingFlag_UniformBuffer)
		SafeAssert(false);
	if (desc->binding_flags & R3_BindingFlag_StructuredBuffer)
		SafeAssert(false);
	if (desc->binding_flags & R3_BindingFlag_ShaderResource)
		bind_flags |= D3D11_BIND_SHADER_RESOURCE;
	if (desc->binding_flags & R3_BindingFlag_UnorderedAccess)
		bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
	if (desc->binding_flags & R3_BindingFlag_RenderTarget)
		bind_flags |= D3D11_BIND_RENDER_TARGET;
	if (desc->binding_flags & R3_BindingFlag_DepthStencil)
		bind_flags |= D3D11_BIND_DEPTH_STENCIL;
	if (desc->binding_flags & R3_BindingFlag_Indirect)
		SafeAssert(false);

	UINT miplevels = 1;
	if (desc->mipmap_count)
	{
		if (desc->mipmap_count == -1)
			miplevels = 0;
		else
			miplevels = desc->mipmap_count;
	}

	D3D11_TEXTURE2D_DESC texture2d_desc = {
		.Width = desc->width,
		.Height = desc->height,
		.MipLevels = miplevels,
		.ArraySize = desc->depth ? desc->depth : 1,
		.Format = format,
		.SampleDesc = {
			.Count = 1,
		},
		.Usage = usage,
		.BindFlags = bind_flags,
		.CPUAccessFlags = (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : 0,
		.MiscFlags = misc_flags,
	};

	D3D11_SUBRESOURCE_DATA* initial = NULL;
	if (desc->initial_data)
		initial = &(D3D11_SUBRESOURCE_DATA) {
			.pSysMem = desc->initial_data,
			.SysMemPitch = desc->width * pixel_size,
			.SysMemSlicePitch = desc->width * desc->height * pixel_size,
		};

	hr = ID3D11Device_CreateTexture2D(ctx->api.device, &texture2d_desc, initial, &out.d3d11_tex2d);
	CheckHr_(ctx, hr);
	if (bind_flags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
			.Format = format_srv,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = -1,
			},
		};
		hr = ID3D11Device_CreateShaderResourceView(ctx->api.device, (ID3D11Resource*)out.d3d11_tex2d, &srv_desc, &out.d3d11_srv);
		CheckHr_(ctx, hr);
	}
	if (bind_flags & D3D11_BIND_UNORDERED_ACCESS)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
			.Format = format_uav,
			.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MipSlice = 0,
			},
		};
		hr = ID3D11Device_CreateUnorderedAccessView(ctx->api.device, (ID3D11Resource*)out.d3d11_tex2d, &uav_desc, &out.d3d11_uav);
		CheckHr_(ctx, hr);
	}

	out.width = desc->width;
	out.height = desc->height;
	out.depth = desc->depth;
	out.format = desc->format;

	return out;
}

API R3_Buffer
R3_MakeBuffer(R3_Context* ctx, R3_BufferDesc const* desc)
{
	Trace();
	R3_Buffer out = {};
	HRESULT hr;

	D3D11_USAGE usage = D3d11UsageToD3dUsage_(desc->usage);
	UINT bind_flags = 0;
	UINT misc_flags = 0;

	if (desc->binding_flags & R3_BindingFlag_VertexBuffer)
		bind_flags |= D3D11_BIND_VERTEX_BUFFER;
	if (desc->binding_flags & R3_BindingFlag_IndexBuffer)
		bind_flags |= D3D11_BIND_INDEX_BUFFER;
	if (desc->binding_flags & R3_BindingFlag_UniformBuffer)
		bind_flags |= D3D11_BIND_CONSTANT_BUFFER;
	if (desc->binding_flags & R3_BindingFlag_StructuredBuffer)
		misc_flags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	if (desc->binding_flags & R3_BindingFlag_ShaderResource)
		bind_flags |= D3D11_BIND_SHADER_RESOURCE;
	if (desc->binding_flags & R3_BindingFlag_UnorderedAccess)
		bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
	if (desc->binding_flags & R3_BindingFlag_RenderTarget)
		SafeAssert(false);
	if (desc->binding_flags & R3_BindingFlag_DepthStencil)
		SafeAssert(false);
	if (desc->binding_flags & R3_BindingFlag_Indirect)
		misc_flags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

	D3D11_BUFFER_DESC buffer_desc = {
		.Usage = usage,
		.ByteWidth = desc->size,
		.BindFlags = bind_flags,
		.MiscFlags = misc_flags,
		.StructureByteStride = desc->struct_size,
		.CPUAccessFlags = (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : 0,
	};

	D3D11_SUBRESOURCE_DATA* initial = NULL;
	if (desc->initial_data)
		initial = &(D3D11_SUBRESOURCE_DATA) {
			.pSysMem = desc->initial_data,
		};

	hr = ID3D11Device_CreateBuffer(ctx->api.device, &buffer_desc, initial, &out.d3d11_buffer);
	CheckHr_(ctx, hr);
	if (bind_flags & D3D11_BIND_SHADER_RESOURCE)
	{
		SafeAssert(desc->struct_size != 0 && desc->size % desc->struct_size == 0);
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
			.Buffer = {
				.FirstElement = 0,
				.NumElements = desc->size / desc->struct_size,
			},
		};
		hr = ID3D11Device_CreateShaderResourceView(ctx->api.device, (ID3D11Resource*)out.d3d11_buffer, &srv_desc, &out.d3d11_srv);
		CheckHr_(ctx, hr);
	}
	if (bind_flags & D3D11_BIND_UNORDERED_ACCESS)
	{
		SafeAssert(desc->struct_size != 0 && desc->size % desc->struct_size == 0);
		const D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
			.Buffer = {
				.FirstElement = 0,
				.NumElements = desc->size / desc->struct_size,
				.Flags = 0,
			},
		};
		hr = ID3D11Device_CreateUnorderedAccessView(ctx->api.device, (ID3D11Resource*)out.d3d11_buffer, &uav_desc, &out.d3d11_uav);
		CheckHr_(ctx, hr);
	}

	return out;
}

API R3_RenderTarget
R3_MakeRenderTarget(R3_Context* ctx, R3_RenderTargetDesc const* desc)
{
	Trace();
	R3_RenderTarget out = {};
	HRESULT hr;

	for (intsize i = 0; i < ArrayLength(out.d3d11_rtvs); ++i)
	{
		R3_Texture* texture = desc->color_textures[i];
		if (!texture)
			break;

		DXGI_FORMAT format = D3d11FormatToDxgi_(texture->format, NULL, NULL);
		ID3D11Resource* resource = (ID3D11Resource*)texture->d3d11_tex2d;
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {
			.Format = format,
			.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MipSlice = 0,
			},
		};

		hr = ID3D11Device_CreateRenderTargetView(ctx->api.device, resource, &rtv_desc, &out.d3d11_rtvs[i]);
		CheckHr_(ctx, hr);
	}

	if (desc->depth_stencil_texture)
	{
		R3_Texture* texture = desc->depth_stencil_texture;
		ID3D11Resource* resource = (ID3D11Resource*)texture->d3d11_tex2d;
		DXGI_FORMAT format = D3d11FormatToDxgi_(texture->format, NULL, NULL);
		D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
			.Format = format,
			.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
			.Flags = 0,
			.Texture2D = {
				.MipSlice = 0,
			},
		};

		hr = ID3D11Device_CreateDepthStencilView(ctx->api.device, resource, &dsv_desc, &out.d3d11_dsv);
		CheckHr_(ctx, hr);
	}

	return out;
}

API R3_Pipeline
R3_MakePipeline(R3_Context* ctx, R3_PipelineDesc const* desc)
{
	Trace();
	R3_Pipeline out = {};
	HRESULT hr;

	//------------------------------------------------------------------------
	Buffer vs;
	Buffer ps;
	if (desc->dx50.vs.size && ctx->feature_level >= D3D_FEATURE_LEVEL_11_0)
	{
		vs = desc->dx50.vs;
		ps = desc->dx50.ps;
	}
	else if (desc->dx40.vs.size && ctx->feature_level >= D3D_FEATURE_LEVEL_10_0)
	{
		vs = desc->dx40.vs;
		ps = desc->dx40.ps;
	}
	else if (desc->dx40_93.vs.size && ctx->feature_level >= D3D_FEATURE_LEVEL_9_3)
	{
		vs = desc->dx40_93.vs;
		ps = desc->dx40_93.ps;
	}
	else
	{
		vs = desc->dx40_91.vs;
		ps = desc->dx40_91.ps;
	}
	
	SafeAssert(vs.size && ps.size);

	hr = ID3D11Device_CreateVertexShader(ctx->api.device, vs.data, vs.size, NULL, &out.d3d11_vs);
	CheckHr_(ctx, hr);
	hr = ID3D11Device_CreatePixelShader(ctx->api.device, ps.data, ps.size, NULL, &out.d3d11_ps);
	CheckHr_(ctx, hr);

	//------------------------------------------------------------------------
	intsize layout_size = 0;
	D3D11_INPUT_ELEMENT_DESC layout_desc[ArrayLength(desc->input_layout)] = {};
	for (intsize i = 0; i < ArrayLength(layout_desc); ++i, ++layout_size)
	{
		if (!desc->input_layout[i].format)
			break;

		R3_LayoutDesc layout = desc->input_layout[i];
		layout_desc[i] = (D3D11_INPUT_ELEMENT_DESC) {
			.SemanticName = "VINPUT",
			.SemanticIndex = (UINT)i,
			.Format = D3d11FormatToDxgi_(layout.format, NULL, NULL),
			.InputSlot = layout.buffer_slot,
			.AlignedByteOffset = layout.offset,
			.InputSlotClass = (layout.divisor == 0) ? D3D11_INPUT_PER_VERTEX_DATA : D3D11_INPUT_PER_INSTANCE_DATA,
			.InstanceDataStepRate = layout.divisor,
		};
	}

	static D3D11_BLEND const functable[] = {
		[R3_BlendFunc_Zero]        = D3D11_BLEND_ZERO,
		[R3_BlendFunc_One]         = D3D11_BLEND_ONE,
		[R3_BlendFunc_SrcColor]    = D3D11_BLEND_SRC_COLOR,
		[R3_BlendFunc_InvSrcColor] = D3D11_BLEND_INV_SRC_COLOR,
		[R3_BlendFunc_DstColor]    = D3D11_BLEND_DEST_COLOR,
		[R3_BlendFunc_InvDstColor] = D3D11_BLEND_INV_DEST_COLOR,
		[R3_BlendFunc_SrcAlpha]    = D3D11_BLEND_SRC_ALPHA,
		[R3_BlendFunc_InvSrcAlpha] = D3D11_BLEND_INV_SRC_ALPHA,
		[R3_BlendFunc_DstAlpha]    = D3D11_BLEND_DEST_ALPHA,
		[R3_BlendFunc_InvDstAlpha] = D3D11_BLEND_INV_DEST_ALPHA,
	};
	
	static D3D11_BLEND_OP const optable[] = {
		[R3_BlendOp_Add]      = D3D11_BLEND_OP_ADD,
		[R3_BlendOp_Subtract] = D3D11_BLEND_OP_SUBTRACT,
	};
	
	D3D11_BLEND_DESC blend_desc = {};
	for (intsize i = 0; i < ArrayLength(desc->rendertargets); ++i)
	{
		if (!desc->rendertargets[i].enable_blend)
			continue;

		R3_BlendFunc src = desc->rendertargets[i].src;
		R3_BlendFunc dst = desc->rendertargets[i].dst;
		R3_BlendFunc src_alpha = desc->rendertargets[i].src_alpha;
		R3_BlendFunc dst_alpha = desc->rendertargets[i].dst_alpha;
		R3_BlendOp op = desc->rendertargets[i].op;
		R3_BlendOp op_alpha = desc->rendertargets[i].op_alpha;

		SafeAssert(src       >= 0 && src       < ArrayLength(functable));
		SafeAssert(dst       >= 0 && dst       < ArrayLength(functable));
		SafeAssert(src_alpha >= 0 && src_alpha < ArrayLength(functable));
		SafeAssert(dst_alpha >= 0 && dst_alpha < ArrayLength(functable));
		SafeAssert(op       >= 0 && op       < ArrayLength(optable));
		SafeAssert(op_alpha >= 0 && op_alpha < ArrayLength(optable));

		blend_desc.RenderTarget[i] = (D3D11_RENDER_TARGET_BLEND_DESC) {
			.BlendEnable = true,
			.SrcBlend = src ? functable[src] : D3D11_BLEND_ONE,
			.DestBlend = dst ? functable[dst] : D3D11_BLEND_ZERO,
			.BlendOp = op ? optable[op] : D3D11_BLEND_OP_ADD,
			.SrcBlendAlpha = src_alpha ? functable[src_alpha] : D3D11_BLEND_ONE,
			.DestBlendAlpha = dst_alpha ? functable[dst_alpha] : D3D11_BLEND_ZERO,
			.BlendOpAlpha = op_alpha ? optable[op_alpha] : D3D11_BLEND_OP_ADD,
			.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
		};
	}

	static D3D11_FILL_MODE const filltable[] = {
		[R3_FillMode_Solid]     = D3D11_FILL_SOLID,
		[R3_FillMode_Wireframe] = D3D11_FILL_WIREFRAME,
	};
	
	static D3D11_CULL_MODE const culltable[] = {
		[R3_CullMode_None]  = D3D11_CULL_NONE,
		[R3_CullMode_Front] = D3D11_CULL_FRONT,
		[R3_CullMode_Back]  = D3D11_CULL_BACK,
	};

	SafeAssert(desc->fill_mode >= 0 && desc->fill_mode < ArrayLength(filltable));
	SafeAssert(desc->cull_mode >= 0 && desc->cull_mode < ArrayLength(culltable));
	
	D3D11_RASTERIZER_DESC rasterizer_desc = {
		.FillMode = filltable[desc->fill_mode],
		.CullMode = culltable[desc->cull_mode],
		.FrontCounterClockwise = !desc->flag_cw_frontface,
		.DepthBias = 0,
		.SlopeScaledDepthBias = 0.0f,
		.DepthBiasClamp = 0.0f,
		.DepthClipEnable = true,
		.ScissorEnable = false, //desc->flag_scissor,
		.MultisampleEnable = false,
		.AntialiasedLineEnable = false,
	};
	
	D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {
		.DepthEnable = desc->flag_depth_test,
		.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
		.DepthFunc = D3D11_COMPARISON_LESS,
	};

	if (layout_size > 0)
	{
		hr = ID3D11Device_CreateInputLayout(ctx->api.device, layout_desc, layout_size, vs.data, vs.size, &out.d3d11_input_layout);
		CheckHr_(ctx, hr);
	}
	hr = ID3D11Device_CreateBlendState(ctx->api.device, &blend_desc, &out.d3d11_blend);
	CheckHr_(ctx, hr);
	hr = ID3D11Device_CreateRasterizerState(ctx->api.device, &rasterizer_desc, &out.d3d11_rasterizer);
	CheckHr_(ctx, hr);
	hr = ID3D11Device_CreateDepthStencilState(ctx->api.device, &depth_stencil_desc, &out.d3d11_depth_stencil);
	CheckHr_(ctx, hr);

	return out;
}

API R3_ComputePipeline
R3_MakeComputePipeline(R3_Context* ctx, R3_ComputePipelineDesc const* desc)
{
	Trace();
	R3_ComputePipeline out = {};
	HRESULT hr;

	Buffer cs = desc->dx40;
	if (desc->dx50.size && ctx->feature_level >= D3D_FEATURE_LEVEL_11_0)
		cs = desc->dx50;

	hr = ID3D11Device_CreateComputeShader(ctx->api.device, cs.data, cs.size, NULL, &out.d3d11_cs);
	CheckHr_(ctx, hr);

	return out;
}

API R3_Sampler
R3_MakeSampler(R3_Context* ctx, R3_SamplerDesc const* desc)
{
	Trace();
	R3_Sampler out = {};
	HRESULT hr;

	static D3D11_FILTER const filtertable[] = {
		[R3_TextureFiltering_Null] = 0,
		[R3_TextureFiltering_Nearest] = D3D11_FILTER_MIN_MAG_MIP_POINT,
		[R3_TextureFiltering_Linear] = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		[R3_TextureFiltering_Anisotropic] = D3D11_FILTER_ANISOTROPIC,
	};
	SafeAssert(desc->filtering < ArrayLength(filtertable));

	D3D11_SAMPLER_DESC sampler_desc = {
		.Filter = filtertable[desc->filtering],
		.AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
		.MinLOD = -FLT_MAX,
		.MaxLOD = FLT_MAX,
		.MaxAnisotropy = desc->anisotropy,
		.ComparisonFunc = D3D11_COMPARISON_NEVER,
	};
	hr = ID3D11Device_CreateSamplerState(ctx->api.device, &sampler_desc, &out.d3d11_sampler);
	CheckHr_(ctx, hr);

	return out;
}

API void
R3_FreeTexture(R3_Context* ctx, R3_Texture* texture)
{
	Trace();

	if (texture->d3d11_srv)
		ID3D11ShaderResourceView_Release(texture->d3d11_srv);
	if (texture->d3d11_uav)
		ID3D11ShaderResourceView_Release(texture->d3d11_uav);
	if (texture->d3d11_tex2d)
		ID3D11Texture2D_Release(texture->d3d11_tex2d);
	if (texture->d3d11_tex3d)
		ID3D11Texture3D_Release(texture->d3d11_tex3d);

	*texture = (R3_Texture) {};
}

API void
R3_FreeBuffer(R3_Context* ctx, R3_Buffer* buffer)
{
	Trace();

	if (buffer->d3d11_srv)
		ID3D11ShaderResourceView_Release(buffer->d3d11_srv);
	if (buffer->d3d11_uav)
		ID3D11ShaderResourceView_Release(buffer->d3d11_uav);
	if (buffer->d3d11_buffer)
		ID3D11Buffer_Release(buffer->d3d11_buffer);

	*buffer = (R3_Buffer) {};
}

API void
R3_FreeRenderTarget(R3_Context* ctx, R3_RenderTarget* rendertarget)
{
	Trace();

	for (intsize i = 0; i < ArrayLength(rendertarget->d3d11_rtvs); ++i)
	{
		if (rendertarget->d3d11_rtvs[i])
			ID3D11RenderTargetView_Release(rendertarget->d3d11_rtvs[i]);
	}
	if (rendertarget->d3d11_dsv)
		ID3D11DepthStencilView_Release(rendertarget->d3d11_dsv);

	*rendertarget = (R3_RenderTarget) {};
}

API void
R3_FreePipeline(R3_Context* ctx, R3_Pipeline* pipeline)
{
	Trace();

	if (pipeline->d3d11_blend)
		ID3D11BlendState_Release(pipeline->d3d11_blend);
	if (pipeline->d3d11_rasterizer)
		ID3D11RasterizerState_Release(pipeline->d3d11_rasterizer);
	if (pipeline->d3d11_depth_stencil)
		ID3D11DepthStencilState_Release(pipeline->d3d11_depth_stencil);
	if (pipeline->d3d11_input_layout)
		ID3D11InputLayout_Release(pipeline->d3d11_input_layout);
	if (pipeline->d3d11_vs)
		ID3D11VertexShader_Release(pipeline->d3d11_vs);
	if (pipeline->d3d11_ps)
		ID3D11PixelShader_Release(pipeline->d3d11_ps);

	*pipeline = (R3_Pipeline) {};
}

API void
R3_FreeComputePipeline(R3_Context* ctx, R3_ComputePipeline* pipeline)
{
	Trace();

	if (pipeline->d3d11_cs)
		ID3D11ComputeShader_Release(pipeline->d3d11_cs);

	*pipeline = (R3_ComputePipeline) {};
}

API void
R3_FreeSampler(R3_Context* ctx, R3_Sampler* sampler)
{
	Trace();

	if (sampler->d3d11_sampler)
		ID3D11SamplerState_Release(sampler->d3d11_sampler);

	*sampler = (R3_Sampler) {};
}

API void
R3_UpdateBuffer(R3_Context* ctx, R3_Buffer* buffer, void const* memory, uint32 size)
{
	Trace();
	HRESULT hr;

	D3D11_BUFFER_DESC desc;
	ID3D11Buffer_GetDesc(buffer->d3d11_buffer, &desc);
	SafeAssert(size <= desc.ByteWidth);

	D3D11_MAPPED_SUBRESOURCE map;
	hr = ID3D11DeviceContext_Map(ctx->api.context, (ID3D11Resource*)buffer->d3d11_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (CheckHr_(ctx, hr))
		return;
	MemoryCopy(map.pData, memory, size);
	ID3D11DeviceContext_Unmap(ctx->api.context, (ID3D11Resource*)buffer->d3d11_buffer, 0);
}

API void
R3_UpdateTexture(R3_Context* ctx, R3_Texture* texture, void const* memory, uint32 size, uint32 slice)
{
	Trace();

	D3D11_TEXTURE2D_DESC desc;
	ID3D11Texture2D_GetDesc(texture->d3d11_tex2d, &desc);
	uint32 pixel_size;
	D3d11FormatToDxgi_(texture->format, &pixel_size, NULL);
	SafeAssert(desc.Width  == texture->width);
	SafeAssert(desc.Height == texture->height);
	SafeAssert(texture->width * texture->height * pixel_size == size);

	uint32 row = texture->width * pixel_size;
	uint32 depth = texture->width * texture->height * pixel_size;
	ID3D11DeviceContext_UpdateSubresource(ctx->api.context, (ID3D11Resource*)texture->d3d11_tex2d, slice, NULL, memory, row, depth);
}

API void
R3_SetViewports(R3_Context* ctx, intsize count, R3_Viewport viewports[])
{
	Trace();
	Assert((uintsize)count <= 8);

	D3D11_VIEWPORT d3d11_viewports[8] = {};
	for (intsize i = 0; i < count; ++i)
	{
		d3d11_viewports[i] = (D3D11_VIEWPORT) {
			.TopLeftX = viewports[i].x,
			.TopLeftY = viewports[i].y,
			.Width = viewports[i].width,
			.Height = viewports[i].height,
			.MinDepth = viewports[i].min_depth,
			.MaxDepth = viewports[i].max_depth,
		};
	}

	ID3D11DeviceContext_RSSetViewports(ctx->api.context, count, d3d11_viewports);
}

API void
R3_SetPipeline(R3_Context* ctx, R3_Pipeline* pipeline)
{
	Trace();

	ID3D11DeviceContext_OMSetBlendState(ctx->api.context, pipeline->d3d11_blend, NULL, 0xFFFFFFFF);
	ID3D11DeviceContext_RSSetState(ctx->api.context, pipeline->d3d11_rasterizer);
	ID3D11DeviceContext_OMSetDepthStencilState(ctx->api.context, pipeline->d3d11_depth_stencil, 1);
	ID3D11DeviceContext_IASetInputLayout(ctx->api.context, pipeline->d3d11_input_layout);
	ID3D11DeviceContext_VSSetShader(ctx->api.context, pipeline->d3d11_vs, NULL, 0);
	ID3D11DeviceContext_PSSetShader(ctx->api.context, pipeline->d3d11_ps, NULL, 0);
}

API void
R3_SetRenderTarget(R3_Context* ctx, R3_RenderTarget* rendertarget)
{
	Trace();

	intsize color_count = 1;
	ID3D11RenderTargetView* rtvs[8] = { ctx->api.target };
	ID3D11DepthStencilView* dsv = ctx->api.depth_stencil;
	if (rendertarget)
	{
		color_count = 0;
		for (intsize i = 0; i < ArrayLength(rendertarget->d3d11_rtvs); ++i)
		{
			if (!rendertarget->d3d11_rtvs[i])
				break;
			rtvs[i] = rendertarget->d3d11_rtvs[i];
			color_count = i+1;
		}
		dsv = rendertarget->d3d11_dsv;
	}

	ID3D11DeviceContext_OMSetRenderTargets(ctx->api.context, color_count, rtvs, dsv);
}

API void
R3_SetVertexInputs(R3_Context *ctx, const R3_VertexInputs* desc)
{
	Trace();
	
	ID3D11Buffer* vbuffers[ArrayLength(desc->vbuffers)] = {};
	UINT offsets[ArrayLength(vbuffers)] = {};
	UINT strides[ArrayLength(vbuffers)] = {};
	for (intsize i = 0; i < ArrayLength(vbuffers); ++i)
	{
		if (desc->vbuffers[i].buffer)
		{
			vbuffers[i] = desc->vbuffers[i].buffer->d3d11_buffer;
			offsets[i] = desc->vbuffers[i].offset;
			strides[i] = desc->vbuffers[i].stride;
		}
	}
	ID3D11DeviceContext_IASetVertexBuffers(ctx->api.context, 0, ArrayLength(vbuffers), vbuffers, strides, offsets);

	DXGI_FORMAT format = D3d11FormatToDxgi_(desc->index_format, NULL, NULL);
	ID3D11Buffer* ibuffer = NULL;
	if (desc->ibuffer)
		ibuffer = desc->ibuffer->d3d11_buffer;
	ID3D11DeviceContext_IASetIndexBuffer(ctx->api.context, ibuffer, format, 0);
}

API void
R3_SetUniformBuffers(R3_Context* ctx, intsize count, R3_UniformBuffer buffers[])
{
	Trace();
	Assert((uintsize)count <= 16);

	ID3D11Buffer* cbuffers[8] = {};
	uint32 cbuffers_offsets[8] = {};
	uint32 cbuffers_sizes[8] = {};
	for (intsize i = 0; i < count; ++i)
	{
		cbuffers[i] = buffers[i].buffer->d3d11_buffer;
		cbuffers_offsets[i] = buffers[i].offset / 16;
		if (buffers[i].size)
			cbuffers_sizes[i] = buffers[i].size / 16;
		else
			cbuffers_sizes[i] = 4096;
	}

	ID3D11DeviceContext1_VSSetConstantBuffers1(ctx->api.context1, 0, ArrayLength(cbuffers), cbuffers, cbuffers_offsets, cbuffers_sizes);
	ID3D11DeviceContext1_PSSetConstantBuffers1(ctx->api.context1, 0, ArrayLength(cbuffers), cbuffers, cbuffers_offsets, cbuffers_sizes);
}

API void
R3_SetResourceViews(R3_Context* ctx, intsize count, R3_ResourceView views[])
{
	Trace();
	Assert((uintsize)count <= 16);

	ID3D11ShaderResourceView* srvs[16] = {};
	for (intsize i = 0; i < count; ++i)
	{
		if (views[i].buffer)
			srvs[i] = views[i].buffer->d3d11_srv;
		else if (views[i].texture)
			srvs[i] = views[i].texture->d3d11_srv;
	}

	ID3D11DeviceContext_VSSetShaderResources(ctx->api.context, 0, ArrayLength(srvs), srvs);
	ID3D11DeviceContext_PSSetShaderResources(ctx->api.context, 0, ArrayLength(srvs), srvs);
}

API void
R3_SetSamplers(R3_Context* ctx, intsize count, R3_Sampler* samplers[])
{
	Trace();
	Assert((uintsize)count <= 16);

	ID3D11SamplerState* states[16] = {};
	for (intsize i = 0; i < count; ++i)
		states[i] = samplers[i]->d3d11_sampler;

	ID3D11DeviceContext_PSSetSamplers(ctx->api.context, 0, ArrayLength(states), states);
}

API void
R3_SetPrimitiveType(R3_Context* ctx, R3_PrimitiveType type)
{
	Trace();

	static D3D11_PRIMITIVE_TOPOLOGY const topologytable[] = {
		[R3_PrimitiveType_TriangleList] = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		[R3_PrimitiveType_TriangleStrip] = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
		[R3_PrimitiveType_LineList] = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
		[R3_PrimitiveType_LineStrip] = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
		[R3_PrimitiveType_PointList] = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
	};
	SafeAssert(type <= ArrayLength(topologytable));

	ID3D11DeviceContext_IASetPrimitiveTopology(ctx->api.context, topologytable[type]);
}

API void
R3_Clear(R3_Context* ctx, R3_ClearDesc const* desc)
{
	Trace();

	ID3D11RenderTargetView* rtvs[8] = {};
	ID3D11DepthStencilView* dsv = NULL;
	ID3D11DeviceContext_OMGetRenderTargets(ctx->api.context, 8, rtvs, &dsv);

	if (desc->flag_color)
	{
		for (intsize i = 0; i < ArrayLength(rtvs); ++i)
		{
			if (rtvs[i])
				ID3D11DeviceContext_ClearRenderTargetView(ctx->api.context, rtvs[i], desc->color);
		}
	}

	if (dsv)
	{
		UINT flags = 0;
		if (desc->flag_depth)
			flags |= D3D11_CLEAR_DEPTH;
		if (desc->flag_stencil)
			flags |= D3D11_CLEAR_STENCIL;

		if (flags)
			ID3D11DeviceContext_ClearDepthStencilView(ctx->api.context, dsv, flags, desc->depth, desc->stencil);
	}
}

API void
R3_Draw(R3_Context* ctx, uint32 start_vertex, uint32 vertex_count, uint32 start_instance, uint32 instance_count)
{
	Trace();
	if (instance_count)
		ID3D11DeviceContext_DrawInstanced(ctx->api.context, vertex_count, instance_count, start_vertex, start_instance);
	else
		ID3D11DeviceContext_Draw(ctx->api.context, vertex_count, start_vertex);
}

API void
R3_DrawIndexed(R3_Context* ctx, uint32 start_index, uint32 index_count, uint32 start_instance, uint32 instance_count, int32 base_vertex)
{
	Trace();
	if (instance_count)
		ID3D11DeviceContext_DrawIndexedInstanced(ctx->api.context, index_count, instance_count, start_index, base_vertex, start_instance);
	else
		ID3D11DeviceContext_DrawIndexed(ctx->api.context, index_count, start_index, base_vertex);
}

API void
R3_SetComputePipeline(R3_Context* ctx, R3_ComputePipeline* pipeline)
{
	Trace();
	ID3D11DeviceContext_CSSetShader(ctx->api.context, pipeline->d3d11_cs, NULL, 0);
}

API void
R3_SetComputeUniformBuffers(R3_Context* ctx, intsize count, R3_UniformBuffer buffers[])
{
	Trace();
	Assert((uintsize)count <= 8);

	ID3D11Buffer* cbuffers[8] = {};
	uint32 cbuffers_offsets[8] = {};
	uint32 cbuffers_sizes[8] = {};
	for (intsize i = 0; i < count; ++i)
	{
		cbuffers[i] = buffers[i].buffer->d3d11_buffer;
		cbuffers_offsets[i] = buffers[i].offset / 16;
		if (buffers[i].size)
			cbuffers_sizes[i] = buffers[i].size / 16;
		else
			cbuffers_sizes[i] = 4096;
	}

	ID3D11DeviceContext1_CSSetConstantBuffers1(ctx->api.context1, 0, ArrayLength(cbuffers), cbuffers, cbuffers_offsets, cbuffers_sizes);
}

API void
R3_SetComputeResourceViews(R3_Context* ctx, intsize count, R3_ResourceView views[])
{
	Trace();
	Assert((uintsize)count <= 16);

	ID3D11ShaderResourceView* srvs[16] = {};
	for (intsize i = 0; i < count; ++i)
	{
		if (views[i].buffer)
			srvs[i] = views[i].buffer->d3d11_srv;
		else if (views[i].texture)
			srvs[i] = views[i].texture->d3d11_srv;
	}

	ID3D11DeviceContext_CSSetShaderResources(ctx->api.context, 0, ArrayLength(srvs), srvs);
}

API void
R3_SetComputeUnorderedViews(R3_Context* ctx, intsize count, R3_UnorderedView views[])
{
	Trace();
	Assert((uintsize)count <= 16);

	ID3D11UnorderedAccessView* uavs[16] = {};
	for (intsize i = 0; i < count; ++i)
	{
		if (views[i].buffer)
			uavs[i] = views[i].buffer->d3d11_uav;
		else if (views[i].texture)
			uavs[i] = views[i].texture->d3d11_uav;
	}

	ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx->api.context, 0, ArrayLength(uavs), uavs, NULL);
}

API void
R3_Dispatch(R3_Context* ctx, uint32 x, uint32 y, uint32 z)
{
	Trace();
	ID3D11DeviceContext_Dispatch(ctx->api.context, x, y, z);
}

//------------------------------------------------------------------------
// TODO(ljre): Proper error handling
API R3_VideoDecoder
R3_MakeVideoDecoder(R3_Context* ctx, R3_VideoDecoderDesc const* desc)
{
	Trace();
	R3_VideoDecoder out = {};
	HRESULT hr;

	ID3D10Multithread* multithread;
	hr = ID3D11Device_QueryInterface(ctx->api.device, &IID_ID3D10Multithread, (void**)&multithread);
	if (CheckHr_(ctx, hr))
		return out;
	hr = ID3D10Multithread_SetMultithreadProtected(multithread, TRUE);
	if (CheckHr_(ctx, hr))
		return out;
	ID3D10Multithread_Release(multithread);

	if (!ctx->mf_device_manager)
	{
		UINT token;
		hr = MFCreateDXGIDeviceManager(&token, &ctx->mf_device_manager);
		if (CheckHr_(ctx, hr))
			return out;
		hr = IMFDXGIDeviceManager_ResetDevice(ctx->mf_device_manager, (IUnknown*)ctx->api.device, token);
		if (CheckHr_(ctx, hr))
			return out;
	}

	if (!ctx->mf_attribs)
	{
		hr = MFCreateAttributes(&ctx->mf_attribs, 4);
		if (CheckHr_(ctx, hr))
			return out;
		hr = IMFAttributes_SetUINT32(ctx->mf_attribs, &MF_LOW_LATENCY, TRUE);
		if (CheckHr_(ctx, hr))
			return out;
		hr = IMFAttributes_SetUINT32(ctx->mf_attribs, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		if (CheckHr_(ctx, hr))
			return out;
		hr = IMFAttributes_SetUnknown(ctx->mf_attribs, &MF_SOURCE_READER_D3D_MANAGER, (IUnknown*)ctx->mf_device_manager);
		if (CheckHr_(ctx, hr))
			return out;
		hr = IMFAttributes_SetUINT32(ctx->mf_attribs, &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
		if (CheckHr_(ctx, hr))
			return out;
	}

	IMFSourceReader* reader;
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	LPWSTR url = OS_W32_StringToWide(desc->filepath, scratch.arena);
	hr = MFCreateSourceReaderFromURL(url, ctx->mf_attribs, &reader);
	ArenaRestore(scratch);
	if (CheckHr_(ctx, hr))
		return out;
		
	PROPVARIANT duration_prop;
	hr = IMFSourceReader_GetPresentationAttribute(reader, MF_SOURCE_READER_MEDIASOURCE, &MF_PD_DURATION, &duration_prop);
	if (CheckHr_(ctx, hr))
		return out;
	hr = IMFSourceReader_SetStreamSelection(reader, MF_SOURCE_READER_ALL_STREAMS, FALSE);
	if (CheckHr_(ctx, hr))
		return out;
	hr = IMFSourceReader_SetStreamSelection(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
	if (CheckHr_(ctx, hr))
		return out;

	IMFMediaType* type;
	hr = IMFSourceReader_GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &type);
	if (CheckHr_(ctx, hr))
		return out;
	UINT64 framerate;
	hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_RATE, &framerate);
	if (CheckHr_(ctx, hr))
		return out;
	UINT64 framesize;
	hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &framesize);
	if (CheckHr_(ctx, hr))
		return out;
	UINT64 pixel_aspect;
	hr = IMFMediaType_GetUINT64(type, &MF_MT_PIXEL_ASPECT_RATIO, &pixel_aspect);
	if (CheckHr_(ctx, hr))
		return out;
	LONGLONG duration;
	hr = PropVariantToInt64(&duration_prop, &duration);
	if (CheckHr_(ctx, hr))
		return out;

	uint32 width = framesize >> 32;
	uint32 height = framesize & UINT32_MAX;
	uint32 framerate_num = framerate >> 32;
	uint32 framerate_den = framerate & UINT32_MAX;
	int64 frame_count = MFllMulDiv(duration, framerate_num, framerate_den * 10000000LL, 0);

	UINT32 nominal_range = 0;
	UINT32 chroma_sitting = 0;
	UINT32 yuv_matrix = 0;
	UINT32 transfer_function = 0;
	hr = IMFMediaType_GetUINT32(type, &MF_MT_VIDEO_NOMINAL_RANGE, &nominal_range);
	hr = IMFMediaType_GetUINT32(type, &MF_MT_VIDEO_CHROMA_SITING, &chroma_sitting);
	hr = IMFMediaType_GetUINT32(type, &MF_MT_YUV_MATRIX, &yuv_matrix);
	hr = IMFMediaType_GetUINT32(type, &MF_MT_TRANSFER_FUNCTION, &transfer_function);

	GUID subtype;
	hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype);
	if (CheckHr_(ctx, hr))
		return out;
	UINT32 profile = 0;
	hr = IMFMediaType_GetUINT32(type, &MF_MT_VIDEO_PROFILE, &profile);
	if (CheckHr_(ctx, hr))
		return out;

	uint32 max_profile = UINT32_MAX;
	if (IsEqualGUID(&subtype, &MFVideoFormat_H264))
		max_profile = eAVEncH264VProfile_High;
	if (IsEqualGUID(&subtype, &MFVideoFormat_HEVC))
		max_profile = eAVEncH265VProfile_Main_420_8;
	if (IsEqualGUID(&subtype, &MFVideoFormat_VP90))
		max_profile = eAVEncVP9VProfile_420_8;
	if (IsEqualGUID(&subtype, &MFVideoFormat_AV1))
		max_profile = eAVEncAV1VProfile_Main_420_8;
	if (profile > max_profile)
		return out;
		
	hr = IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
	if (CheckHr_(ctx, hr))
		return out;
	hr = IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type);
	if (CheckHr_(ctx, hr))
		return out;
	IMFMediaType_Release(type);

	out.d3d11_mfsource = reader;
	out.width = width;
	out.height = height;
	out.fps_num = framerate_num;
	out.fps_den = framerate_den;
	out.frame_count = frame_count;
	return out;
}

API void
R3_FreeVideoDecoder(R3_Context* ctx, R3_VideoDecoder* video)
{
	Trace();

	if (video->d3d11_mfsource)
		IMFSourceReader_Release((IMFSourceReader*)video->d3d11_mfsource);

	*video = (R3_VideoDecoder) {};
}

API void
R3_SetDecoderPosition(R3_Context* ctx, R3_VideoDecoder* video, uint64 frame_index)
{

}

// NOTE(ljre): 'output_texture' should be either a valid texture or be zero-initialized. If it's a valid
//             texture, it'll be free'd with R3_FreeTexture() before the new contents are stored.
//             You can keep passing the same struct over and over again to iterate through the video's frames.
//
//             'out_timestamp' will contain the frame timestamp in the video. This timestamp is 0 for the first
//             frame and will advance roughly 'OS_TickRate() * fps_den / fps_num' ticks per frame.
//
//             Returns true if we're not at the end of the video yet.
API bool
R3_DecodeFrame(R3_Context* ctx, R3_VideoDecoder* video, R3_Texture* output_texture, uint64* out_timestamp)
{
	Trace();
	IMFSourceReader* reader = video->d3d11_mfsource;
	HRESULT hr;
	
	DWORD flags;
	LONGLONG timestamp;
	IMFSample* sample;
	hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &timestamp, &sample);
	if (CheckHr_(ctx, hr))
		return false;

	if (flags & MF_SOURCE_READERF_ERROR)
		return false;
	if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		return false;
	if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
		return false;
	if (flags & MF_SOURCE_READERF_STREAMTICK)
		return true;
	
	R3_FreeTexture(ctx, output_texture);
	// LONG pitch = 0;
	IMFMediaBuffer* buffer;
	hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer);
	if (CheckHr_(ctx, hr))
		return false;
	IMFDXGIBuffer* dxgibuffer;
	hr = IMFMediaBuffer_QueryInterface(buffer, &IID_IMFDXGIBuffer, (void**)&dxgibuffer);
	if (CheckHr_(ctx, hr))
		return false;
	ID3D11Texture2D* texture;
	hr = IMFDXGIBuffer_GetResource(dxgibuffer, &IID_ID3D11Texture2D, (void**)&texture);
	if (CheckHr_(ctx, hr))
		return false;
	UINT subresource;
	hr = IMFDXGIBuffer_GetSubresourceIndex(dxgibuffer, &subresource);
	if (CheckHr_(ctx, hr))
		return false;
		
	D3D11_TEXTURE2D_DESC texdesc;
	ID3D11Texture2D_GetDesc(texture, &texdesc);
	D3D11_SHADER_RESOURCE_VIEW_DESC viewdesc = {
		.Format = DXGI_FORMAT_B8G8R8X8_UNORM,
		.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY,
		.Texture2DArray = {
			.MostDetailedMip = 0,
			.MipLevels = -1,
			.FirstArraySlice = subresource,
			.ArraySize = 1,
		},
	};
	ID3D11ShaderResourceView* srv;
	hr = ID3D11Device_CreateShaderResourceView(ctx->api.device, (ID3D11Resource*)texture, &viewdesc, &srv);
	if (CheckHr_(ctx, hr))
		return false;
		
	output_texture->format = R3_Format_U8x4Norm_Bgrx;
	output_texture->depth = 1;
	output_texture->width = texdesc.Width;
	output_texture->height = texdesc.Height;
	output_texture->d3d11_srv = srv;
	output_texture->d3d11_tex2d = texture;
	*out_timestamp = (uint64)timestamp;

	IMFDXGIBuffer_Release(dxgibuffer);
	IMFMediaBuffer_Release(buffer);
	IMFSample_Release(sample);
	return true;
}

// =============================================================================
// =============================================================================
// Font drawing
API uint32
R3_TextRequiredBindingFlags(void)
{
	Trace();
	return R3_BindingFlag_RenderTarget;
}

API R3_Usage
R3_TextRequiredUsage(void)
{
	Trace();
	return R3_Usage_GpuReadWrite;
}

API R3_TextContext
R3_DWRITE_MakeTextContext(R3_Context* ctx, R3_TextContextDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID2D1Factory1* d2d_factory = NULL;
	ID2D1RenderTarget* rendertarget = NULL;
	ID2D1SolidColorBrush* brush = NULL;
	IDWriteFactory* dw_factory = NULL;

	D2D1_FACTORY_OPTIONS factory_desc = {
		.debugLevel = D2D1_DEBUG_LEVEL_ERROR,
	};
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, &factory_desc, (void**)&d2d_factory);
	SafeAssert(SUCCEEDED(hr));

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (void**)&dw_factory);
	SafeAssert(SUCCEEDED(hr));

	IDXGISurface* glyphs_texture_surface = NULL;
	hr = ID3D11Texture2D_QueryInterface(desc->rendertarget->d3d11_tex2d, &IID_IDXGISurface, (void**)&glyphs_texture_surface);
	SafeAssert(SUCCEEDED(hr));

	D2D1_RENDER_TARGET_PROPERTIES rtprops = {
		.type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
		.pixelFormat = {
			.format = D3d11FormatToDxgi_(desc->rendertarget->format, NULL, NULL),
			.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED,
		},
		.usage = D2D1_RENDER_TARGET_USAGE_NONE,
		.minLevel = D2D1_FEATURE_LEVEL_DEFAULT,
		.dpiX = 0.0f,
		.dpiY = 0.0f,
	};
	hr = ID2D1Factory1_CreateDxgiSurfaceRenderTarget(d2d_factory, glyphs_texture_surface, &rtprops, &rendertarget);
	SafeAssert(SUCCEEDED(hr));
	IDXGISurface_Release(glyphs_texture_surface);

	hr = ID2D1RenderTarget_CreateSolidColorBrush(rendertarget, &(D2D1_COLOR_F) {1.0f, 1.0f, 1.0f, 1.0f}, NULL, &brush);
	SafeAssert(SUCCEEDED(hr));

	return (R3_TextContext) {
		.d2d_brush = brush,
		.d2d_factory = d2d_factory,
		.dw_factory = dw_factory,
		.d2d_rendertarget = rendertarget,
		.rt_width = desc->rendertarget->width,
		.rt_height = desc->rendertarget->height,
	};
}

API R3_Font
R3_MakeFont(R3_Context* ctx, R3_TextContext* text_ctx, R3_FontDesc const* desc)
{
	Trace();
	HRESULT hr;
	IDWriteTextFormat* text_format = NULL;
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));

	LPWSTR wname = OS_W32_StringToWide(desc->name, scratch.arena);
	DWRITE_FONT_WEIGHT weight;
	DWRITE_FONT_STYLE style;
	DWRITE_FONT_STRETCH stretch;
	switch (desc->weight)
	{
		default: Unreachable(); break;
		case R3_FontWeight_Regular: weight = DWRITE_FONT_WEIGHT_REGULAR; break;
	}
	switch (desc->style)
	{
		default: Unreachable(); break;
		case R3_FontStyle_Normal: style = DWRITE_FONT_STYLE_NORMAL; break;
	}
	switch (desc->stretch)
	{
		default: Unreachable(); break;
		case R3_FontStretch_Normal: stretch = DWRITE_FONT_STRETCH_NORMAL; break;
	}

	hr = IDWriteFactory_CreateTextFormat(text_ctx->dw_factory, wname, NULL, weight, style, stretch, desc->pt, L"en-us", &text_format);
	SafeAssert(SUCCEEDED(hr));
	hr = IDWriteTextFormat_SetTextAlignment(text_format, DWRITE_TEXT_ALIGNMENT_LEADING);
	SafeAssert(SUCCEEDED(hr));
	hr = IDWriteTextFormat_SetParagraphAlignment(text_format, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	SafeAssert(SUCCEEDED(hr));

	ArenaRestore(scratch);
	return (R3_Font) {
		.dw_text_format = text_format,
	};
}

API void
R3_TextBegin(R3_Context* ctx, R3_TextContext* text_ctx)
{
	Trace();
	ID2D1RenderTarget_BeginDraw(text_ctx->d2d_rendertarget);
}

API void
R3_TextDrawGlyphs(R3_Context* ctx, R3_TextContext* text_ctx, String str, R3_Font* font, float32 const transform[2][3])
{
	Trace();
	D2D1_MATRIX_3X2_F d2d_transform = {
		._11 = 1.0f,
		._22 = 1.0f,
	};
	if (transform)
	{
		d2d_transform.m[0][0] = transform[0][0];
		d2d_transform.m[1][0] = transform[0][1];
		d2d_transform.m[2][0] = transform[0][2];
		d2d_transform.m[0][1] = transform[1][0];
		d2d_transform.m[1][1] = transform[1][1];
		d2d_transform.m[2][1] = transform[1][2];
	}
	ID2D1RenderTarget_SetTransform(text_ctx->d2d_rendertarget, &d2d_transform);
	ID2D1RenderTarget_SetTextAntialiasMode(text_ctx->d2d_rendertarget, D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	ID2D1RenderTarget_Clear(text_ctx->d2d_rendertarget, &(D2D1_COLOR_F) {});

	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	LPWSTR wstr = OS_W32_StringToWide(str, scratch.arena);
	UINT32 wstr_size = 0;
	for (LPWSTR it = wstr; *it; ++it)
		++wstr_size;
	D2D1_RECT_F layout_rect = {
		.bottom = text_ctx->rt_height,
		.right = text_ctx->rt_width,
	};
	ID2D1RenderTarget_DrawText(text_ctx->d2d_rendertarget, wstr, wstr_size, font->dw_text_format, &layout_rect, (ID2D1Brush*)text_ctx->d2d_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP|D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, DWRITE_MEASURING_MODE_NATURAL);
	ArenaRestore(scratch);
}

API void
R3_TextEnd(R3_Context* ctx, R3_TextContext* text_ctx)
{
	Trace();
	HRESULT hr;
	hr = ID2D1RenderTarget_EndDraw(text_ctx->d2d_rendertarget, NULL, NULL);
	SafeAssert(SUCCEEDED(hr));
}
