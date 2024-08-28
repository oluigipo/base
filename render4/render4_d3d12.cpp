#include "common.hpp"
#include "api.h"
#include "api_os.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "api_os_win32.h"
#include "render4_d3d12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

static_assert(sizeof(R4_D3D12_Queue)            <= sizeof(R4_Queue), "");
static_assert(sizeof(R4_D3D12_Buffer)           <= sizeof(R4_Buffer), "");
static_assert(sizeof(R4_D3D12_Image)            <= sizeof(R4_Image), "");
static_assert(sizeof(R4_D3D12_Heap)             <= sizeof(R4_Heap), "");
static_assert(sizeof(R4_D3D12_CommandAllocator) <= sizeof(R4_CommandAllocator), "");
static_assert(sizeof(R4_D3D12_CommandList)      <= sizeof(R4_CommandList), "");
static_assert(sizeof(R4_D3D12_Pipeline)         <= sizeof(R4_Pipeline), "");
static_assert(sizeof(R4_D3D12_PipelineLayout)   <= sizeof(R4_PipelineLayout), "");
static_assert(sizeof(R4_D3D12_DescriptorHeap)   <= sizeof(R4_DescriptorHeap), "");
static_assert(sizeof(R4_D3D12_DescriptorSet)    <= sizeof(R4_DescriptorSet), "");
static_assert(sizeof(R4_D3D12_BufferView)       <= sizeof(R4_BufferView), "");
static_assert(sizeof(R4_D3D12_ImageView)        <= sizeof(R4_ImageView), "");
static_assert(sizeof(R4_D3D12_Sampler)          <= sizeof(R4_Sampler), "");
static_assert(sizeof(R4_D3D12_RenderTargetView) <= sizeof(R4_RenderTargetView), "");
static_assert(sizeof(R4_D3D12_DepthStencilView) <= sizeof(R4_DepthStencilView), "");

struct ViewPool_
{
	Slice<int32> free_table;
	int32 free_count;
	int32 allocated_count;
};

struct R4_D3D12_Context
{
	Allocator allocator;
	struct ID3D12Device* device;
	struct ID3D12Device2* device2;
	struct IDXGIAdapter1* adapter1;
	struct IDXGIFactory4* factory4;
	struct IDXGISwapChain1* swapchain1;
	struct IDXGISwapChain3* swapchain3;
	struct ID3D12CommandQueue* direct_queue;
	struct ID3D12CommandQueue* compute_queue;
	struct ID3D12CommandQueue* copy_queue;
	R4_ContextInfo info;
	bool debug_layer_enabled;

	struct ID3D12Fence* fence;
	void* fence_event;
	uint64 fence_counter;

	struct ID3D12DescriptorHeap* rtv_heap;
	struct ID3D12DescriptorHeap* dsv_heap;
	struct ID3D12DescriptorHeap* sampler_heap;
	struct ID3D12DescriptorHeap* cbv_srv_uav_heap;

	ViewPool_ rtv_pool;
	ViewPool_ dsv_pool;
	ViewPool_ sampler_pool;
	ViewPool_ cbv_srv_uav_pool;
}
typedef R4_D3D12_Context;

struct R4_Context: public R4_D3D12_Context {};

static ViewPool_
InitViewPool_(int32 max_size, Allocator allocator, AllocatorError* err)
{
	Trace();
	return {
		.free_table = AllocatorNewSlice<int32>(&allocator, max_size, err),
		.free_count = 0,
		.allocated_count = 0,
	};
}

static int32
AllocateFromViewPool_(ViewPool_* pool)
{
	Trace();
	SafeAssert(pool->free_table);
	int32 result;
	if (pool->free_count > 0)
		result = pool->free_table[--pool->free_count];
	else if (pool->allocated_count < pool->free_table.count)
		result = pool->allocated_count++;
	else
	{
		SafeAssert("RAN OUT OF DESCRIPTORS!!!");
		Unreachable();
	}
	return result;
}

static void
FreeFromViewPool_(ViewPool_* pool, int32 index)
{
	Trace();
	SafeAssert(pool->free_table);
	pool->free_table[pool->free_count++] = index;
}

static void
FreeViewPool_(ViewPool_* pool, Allocator allocator, AllocatorError* err)
{
	Trace();
	if (pool->free_table)
		AllocatorDeleteSlice(&allocator, pool->free_table, err);
	*pool = {};
}

static bool
CheckHr_(HRESULT hr, R4_Result* r)
{
	if (SUCCEEDED(hr))
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}
		
	OS_LogErr("ERROR: HR: %u\n", (unsigned int)hr);
	if (r)
	{
		if (hr == DXGI_ERROR_DEVICE_HUNG ||
			hr == DXGI_ERROR_DEVICE_REMOVED ||
			hr == DXGI_ERROR_DEVICE_RESET ||
			hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
		{
			*r = R4_Result_DeviceLost;
		}
		else if (hr == E_OUTOFMEMORY)
			*r = R4_Result_DeviceOutOfMemory;
		else
			*r = R4_Result_Failure;
	}
	return false;
}

static bool
CheckAllocatorErr_(AllocatorError err, R4_Result* r)
{
	if (!err)
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}

	OS_LogErr("ERROR: ALLOCATOR: %u\n", (unsigned int)err);
	if (r)
	{
		if (err == AllocatorError_OutOfMemory)
			*r = R4_Result_AllocatorOutOfMemory;
		else
			*r = R4_Result_AllocatorError;
	}
	return false;
}

static D3D12_RESOURCE_STATES
D3d12StatesFromResourceStates_(R4_ResourceState state)
{
	D3D12_RESOURCE_STATES result = (D3D12_RESOURCE_STATES)0;

	switch (state)
	{
		default: Unreachable(); break;
		case R4_ResourceState_Null: result = D3D12_RESOURCE_STATE_COMMON; break;
		case R4_ResourceState_Common: result = D3D12_RESOURCE_STATE_COMMON; break;
		case R4_ResourceState_UniformBuffer:
		case R4_ResourceState_VertexBuffer: result = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; break;
		case R4_ResourceState_Present: result = D3D12_RESOURCE_STATE_PRESENT; break;
		case R4_ResourceState_IndexBuffer: result = D3D12_RESOURCE_STATE_INDEX_BUFFER; break;
		case R4_ResourceState_TransferSrc: result = D3D12_RESOURCE_STATE_COPY_SOURCE; break;
		case R4_ResourceState_TransferDst: result = D3D12_RESOURCE_STATE_COPY_DEST; break;
		case R4_ResourceState_SampledImage:
		case R4_ResourceState_ShaderStorage: result = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE; break;
		case R4_ResourceState_ShaderReadWrite: result = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; break;
		case R4_ResourceState_DepthStencil: result = D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ; break;
		case R4_ResourceState_RenderTarget: result = D3D12_RESOURCE_STATE_RENDER_TARGET; break;
	}

	return result;
}

static D3D12_COMMAND_LIST_TYPE
D3d12TypeFromCommandListKind_(R4_CommandListType kind)
{
	D3D12_COMMAND_LIST_TYPE result = (D3D12_COMMAND_LIST_TYPE)-1;

	switch (kind)
	{
		case R4_CommandListType_Graphics: result = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
		case R4_CommandListType_Compute: result = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
		case R4_CommandListType_Copy: result = D3D12_COMMAND_LIST_TYPE_COPY; break;
		default: Unreachable(); break;
	}

	return result;
}

static DXGI_FORMAT
DxgiFormatFromFormat_(R4_Format format)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

	switch (format)
	{
		case R4_Format_Null: result = DXGI_FORMAT_UNKNOWN; break;

		case R4_Format_R8_UNorm: result = DXGI_FORMAT_R8_UNORM; break;
		case R4_Format_R8G8_UNorm: result = DXGI_FORMAT_R8G8_UNORM; break;
		case R4_Format_R8G8B8A8_UNorm: result = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case R4_Format_R8G8B8A8_SRgb: result = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
		case R4_Format_R8_UInt: result = DXGI_FORMAT_R8_UINT; break;
		case R4_Format_R8G8_UInt: result = DXGI_FORMAT_R8G8_UINT; break;
		case R4_Format_R8G8B8A8_UInt: result = DXGI_FORMAT_R8G8B8A8_UINT; break;

		case R4_Format_R16_SNorm: result = DXGI_FORMAT_R16_SNORM; break;
		case R4_Format_R16G16_SNorm: result = DXGI_FORMAT_R16G16_SNORM; break;
		case R4_Format_R16G16B16A16_SNorm: result = DXGI_FORMAT_R16G16B16A16_SNORM; break;
		case R4_Format_R16_SInt: result = DXGI_FORMAT_R16_SINT; break;
		case R4_Format_R16G16_SInt: result = DXGI_FORMAT_R16G16_SINT; break;
		case R4_Format_R16G16B16A16_SInt: result = DXGI_FORMAT_R16G16B16A16_SINT; break;

		case R4_Format_R32_UInt: result = DXGI_FORMAT_R32_UINT; break;

		case R4_Format_R32_Float: result = DXGI_FORMAT_R32_FLOAT; break;
		case R4_Format_R32G32_Float: result = DXGI_FORMAT_R32G32_FLOAT; break;
		case R4_Format_R32G32B32_Float: result = DXGI_FORMAT_R32G32B32_FLOAT; break;
		case R4_Format_R32G32B32A32_Float: result = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
		case R4_Format_R16G16_Float: result = DXGI_FORMAT_R16G16_FLOAT; break;
		case R4_Format_R16G16B16A16_Float: result = DXGI_FORMAT_R16G16B16A16_FLOAT; break;

		case R4_Format_D16_UNorm: result = DXGI_FORMAT_D16_UNORM; break;
		case R4_Format_D24_UNorm_S8_UInt: result = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
	}

	return result;
}

static bool
IsDepthStencilFormat_(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return true;
		default:
			return false;
	}
}

static D3D12_BLEND
D3d12BlendFromBlendFunc_(R4_BlendFunc func)
{
	D3D12_BLEND result = (D3D12_BLEND)func;

	switch (func)
	{
		case R4_BlendFunc_Null: Unreachable(); break;
		case R4_BlendFunc_Zero: result = D3D12_BLEND_ZERO; break;
		case R4_BlendFunc_One: result = D3D12_BLEND_ONE; break;
		case R4_BlendFunc_SrcColor: result = D3D12_BLEND_SRC_COLOR; break;
		case R4_BlendFunc_InvSrcColor: result = D3D12_BLEND_INV_SRC_COLOR; break;
		case R4_BlendFunc_DstColor: result = D3D12_BLEND_DEST_COLOR; break;
		case R4_BlendFunc_InvDstColor: result = D3D12_BLEND_INV_DEST_COLOR; break;
		case R4_BlendFunc_SrcAlpha: result = D3D12_BLEND_SRC_ALPHA; break;
		case R4_BlendFunc_InvSrcAlpha: result = D3D12_BLEND_INV_SRC_ALPHA; break;
		case R4_BlendFunc_DstAlpha: result = D3D12_BLEND_DEST_ALPHA; break;
		case R4_BlendFunc_InvDstAlpha: result = D3D12_BLEND_INV_DEST_ALPHA; break;
		case R4_BlendFunc_SrcAlphaSat: result = D3D12_BLEND_SRC_ALPHA_SAT; break;
		case R4_BlendFunc_BlendFactor: result = D3D12_BLEND_BLEND_FACTOR; break;
		case R4_BlendFunc_InvBlendFactor: result = D3D12_BLEND_INV_BLEND_FACTOR; break;
		case R4_BlendFunc_Src1Color: result = D3D12_BLEND_SRC1_COLOR; break;
		case R4_BlendFunc_InvSrc1Color: result = D3D12_BLEND_INV_SRC1_COLOR; break;
		case R4_BlendFunc_Src1Alpha: result = D3D12_BLEND_SRC1_ALPHA; break;
		case R4_BlendFunc_InvSrc1Alpha: result = D3D12_BLEND_INV_SRC1_ALPHA; break;
		case R4_BlendFunc_AlphaFactor: result = D3D12_BLEND_ALPHA_FACTOR; break;
		case R4_BlendFunc_InvAlphaFactor: result = D3D12_BLEND_INV_ALPHA_FACTOR; break;
	}

	return result;
}

static D3D12_BLEND_OP
D3d12BlendOpFromBlendOp_(R4_BlendOp op)
{
	D3D12_BLEND_OP result = (D3D12_BLEND_OP)0;

	switch (op)
	{
		case R4_BlendOp_Add: result = D3D12_BLEND_OP_ADD; break;
		case R4_BlendOp_Subtract: result = D3D12_BLEND_OP_SUBTRACT; break;
		case R4_BlendOp_RevSubtract: result = D3D12_BLEND_OP_REV_SUBTRACT; break;
		case R4_BlendOp_Min: result = D3D12_BLEND_OP_MIN; break;
		case R4_BlendOp_Max: result = D3D12_BLEND_OP_MAX; break;
	}

	return result;
}

static D3D12_FILL_MODE
D3d12FillModeFromFillMode_(R4_FillMode mode)
{
	D3D12_FILL_MODE result = (D3D12_FILL_MODE)0;

	switch (mode)
	{
		case R4_FillMode_Solid: result = D3D12_FILL_MODE_SOLID; break;
		case R4_FillMode_Wireframe: result = D3D12_FILL_MODE_WIREFRAME; break;
	}

	return result;
}

static D3D12_LOGIC_OP
D3d12LogicOpFromLogicOp_(R4_LogicOp op)
{
	D3D12_LOGIC_OP result = (D3D12_LOGIC_OP)0;

	switch (op)
	{

	}

	return result;
}

static D3D12_CULL_MODE
D3d12CullModeFromCullMode_(R4_CullMode mode)
{
	D3D12_CULL_MODE result = (D3D12_CULL_MODE)0;

	switch (mode)
	{
		case R4_CullMode_None: result = D3D12_CULL_MODE_NONE; break;
		case R4_CullMode_Back: result = D3D12_CULL_MODE_BACK; break;
		case R4_CullMode_Front: result = D3D12_CULL_MODE_FRONT; break;
	}

	return result;
}

static D3D12_COMPARISON_FUNC
D3d12CompFuncFromCompFunc_(R4_ComparisonFunc func)
{
	D3D12_COMPARISON_FUNC result = (D3D12_COMPARISON_FUNC)func;

	switch (func)
	{

	}

	return result;
}

static D3D12_SHADER_VISIBILITY
D3d12VisibilityFromVisibility_(R4_ShaderVisibility visibility)
{
	D3D12_SHADER_VISIBILITY result = D3D12_SHADER_VISIBILITY_ALL;

	switch (visibility)
	{
		case R4_ShaderVisibility_All: break;
		case R4_ShaderVisibility_Vertex: result = D3D12_SHADER_VISIBILITY_VERTEX; break;
		case R4_ShaderVisibility_Pixel: result = D3D12_SHADER_VISIBILITY_PIXEL; break;
		case R4_ShaderVisibility_Mesh: result = D3D12_SHADER_VISIBILITY_MESH; break;
	}

	return result;
}

// =============================================================================
// =============================================================================
// Context creation
API R4_Context*
R4_MakeContext(Allocator allocator, R4_Result* r, R4_ContextDesc const* desc)
{
	Trace();
	AllocatorError err;
	HRESULT hr;
	R4_D3D12_Context* d3d12 = AllocatorNew<R4_D3D12_Context>(&allocator, &err);
	if (!CheckAllocatorErr_(err, r))
		return NULL;
	d3d12->allocator = allocator;

	{
		if (desc->debug_mode)
		{
			HMODULE dll_d3d12 = GetModuleHandleW(L"d3d12.dll");
			if (dll_d3d12)
			{
				PFN_D3D12_GET_DEBUG_INTERFACE proc = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(dll_d3d12, "D3D12GetDebugInterface");
				if (proc)
				{
					ID3D12Debug* debug;
					hr = proc(IID_PPV_ARGS(&debug));
					if (SUCCEEDED(hr))
					{
						debug->EnableDebugLayer();
						debug->Release();
						d3d12->debug_layer_enabled = true;
					}
				}
			}
		}

		hr = CreateDXGIFactory1(IID_PPV_ARGS(&d3d12->factory4));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		for (UINT i = 0;; ++i)
		{
			IDXGIAdapter1* curr_adapter;
			if (FAILED(d3d12->factory4->EnumAdapters1(i, &curr_adapter)))
				break;
			hr = D3D12CreateDevice(curr_adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), NULL);
			if (SUCCEEDED(hr))
			{
				d3d12->adapter1 = curr_adapter;
				break;
			}
			curr_adapter->Release();
		}
		if (!d3d12->adapter1)
			goto lbl_error;

		hr = D3D12CreateDevice(d3d12->adapter1, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12->device));
		if (!CheckHr_(hr, r))
			goto lbl_error;
		hr = d3d12->device->QueryInterface(IID_PPV_ARGS(&d3d12->device2));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		if (d3d12->debug_layer_enabled && IsDebuggerPresent())
		{
			ID3D12InfoQueue* queue;
			hr = d3d12->device->QueryInterface(IID_PPV_ARGS(&queue));
			if (SUCCEEDED(hr))
			{
				queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
				queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
				queue->Release();
			}
		}

		D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		};
		hr = d3d12->device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&d3d12->direct_queue));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		HWND hwnd = (HWND)desc->window_handle;
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
			.Format = DxgiFormatFromFormat_(desc->backbuffer_format),
			.SampleDesc = {
				.Count = 1,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		};
		hr = d3d12->factory4->CreateSwapChainForHwnd(d3d12->direct_queue, hwnd, &swapchain_desc, NULL, NULL, &d3d12->swapchain1);
		if (!CheckHr_(hr, r))
			goto lbl_error;
		hr = d3d12->swapchain1->QueryInterface(IID_PPV_ARGS(&d3d12->swapchain3));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		hr = d3d12->factory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
		if (FAILED(hr))
			OS_LogErr("WARNING: MakeWindowAssociation failed (0x%x)\n", (uint32)hr);

		*desc->out_graphics_queue = { .d3d12 = { d3d12->direct_queue } };
		if (desc->out_compute_queue)
		{
			D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {
				.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
			};
			hr = d3d12->device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&d3d12->compute_queue));
			if (!CheckHr_(hr, r))
				goto lbl_error;
			*desc->out_compute_queue = { .d3d12 = { d3d12->compute_queue } };
		}
		if (desc->out_copy_queue)
		{
			D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {
				.Type = D3D12_COMMAND_LIST_TYPE_COPY,
			};
			hr = d3d12->device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&d3d12->copy_queue));
			if (!CheckHr_(hr, r))
				goto lbl_error;
			*desc->out_copy_queue = { .d3d12 = { d3d12->copy_queue } };
		}

		int32 max_rtvs = desc->max_render_target_views;
		int32 max_dsvs = desc->max_depth_stencil_views;
		int32 max_samplers = desc->max_samplers;
		int32 max_image_views = desc->max_image_views;
		int32 max_buffer_views = desc->max_buffer_views;
		if (!max_rtvs)
			max_rtvs = 1024;
		if (!max_dsvs)
			max_dsvs = 1024;
		if (!max_samplers)
			max_samplers = 1024;
		if (!max_image_views)
			max_image_views = 1<<20;
		if (!max_buffer_views)
			max_buffer_views = 1<<20;

		D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = (UINT)max_rtvs,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&d3d12->rtv_heap));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			.NumDescriptors = (UINT)max_dsvs,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&d3d12->dsv_heap));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			.NumDescriptors = (UINT)max_samplers,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&sampler_heap_desc, IID_PPV_ARGS(&d3d12->sampler_heap));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		SafeAssert(max_image_views >= 0 && max_buffer_views >= 0);
		D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = (UINT)max_image_views + (UINT)max_buffer_views,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&cbv_srv_uav_heap_desc, IID_PPV_ARGS(&d3d12->cbv_srv_uav_heap));
		if (!CheckHr_(hr, r))
			goto lbl_error;

		d3d12->rtv_pool = InitViewPool_(max_rtvs, allocator, &err);
		if (!CheckAllocatorErr_(err, r))
			goto lbl_error;
		d3d12->dsv_pool = InitViewPool_(max_dsvs, allocator, &err);
		if (!CheckAllocatorErr_(err, r))
			goto lbl_error;
		d3d12->sampler_pool = InitViewPool_(max_samplers, allocator, &err);
		if (!CheckAllocatorErr_(err, r))
			goto lbl_error;
		d3d12->cbv_srv_uav_pool = InitViewPool_(max_image_views + max_buffer_views, allocator, &err);
		if (!CheckAllocatorErr_(err, r))
			goto lbl_error;
	}

	return (R4_Context*)d3d12;

lbl_error:
	R4_DestroyContext((R4_Context*)d3d12, NULL);
	return NULL;
}

API R4_ContextInfo
R4_GetContextInfo(R4_Context* ctx)
{
	Trace();
	return ctx->info;
}

API bool
R4_IsDeviceLost(R4_Context* ctx, R4_Result* r)
{
	Trace();
	// TODO(ljre)
	return false;
}

API void
R4_DestroyContext(R4_Context* ctx, R4_Result* r)
{
	Trace();
	AllocatorError err;

	FreeViewPool_(&ctx->cbv_srv_uav_pool, ctx->allocator, &err);
	FreeViewPool_(&ctx->sampler_pool, ctx->allocator, &err);
	FreeViewPool_(&ctx->dsv_pool, ctx->allocator, &err);
	FreeViewPool_(&ctx->rtv_pool, ctx->allocator, &err);
	if (ctx->dsv_heap)
		ctx->dsv_heap->Release();
	if (ctx->sampler_heap)
		ctx->sampler_heap->Release();
	if (ctx->cbv_srv_uav_heap)
		ctx->cbv_srv_uav_heap->Release();
	if (ctx->copy_queue)
		ctx->copy_queue->Release();
	if (ctx->compute_queue)
		ctx->compute_queue->Release();
	if (ctx->swapchain3)
		ctx->swapchain3->Release();
	if (ctx->swapchain1)
		ctx->swapchain1->Release();
	if (ctx->direct_queue)
		ctx->direct_queue->Release();
	if (ctx->device2)
		ctx->device2->Release();
	if (ctx->device)
		ctx->device->Release();
	if (ctx->adapter1)
		ctx->adapter1->Release();
	if (ctx->factory4)
		ctx->factory4->Release();

	if (ctx->allocator.proc)
	{
		Allocator allocator = ctx->allocator;
		AllocatorDelete(&allocator, ctx, &err);
		if (!CheckAllocatorErr_(err, r))
			return;
	}
}

API void
R4_PresentAndSync(R4_Context* ctx, R4_Result* r, int32 sync_interval)
{
	Trace();
	HRESULT hr = ctx->swapchain3->Present((UINT)sync_interval, 0);
	CheckHr_(hr, r);
}

API intz
R4_AcquireNextBackbuffer(R4_Context* ctx, R4_Result* r)
{
	Trace();
	HRESULT hr;
	intz backbuffer_index = ctx->swapchain3->GetCurrentBackBufferIndex();

	ctx->fence_counter += 1;
	hr = ctx->direct_queue->Signal(ctx->fence, ctx->fence_counter);
	if (!CheckHr_(hr, r))
		return 0;
	if (ctx->fence->GetCompletedValue() < ctx->fence_counter)
	{
		hr = ctx->fence->SetEventOnCompletion(ctx->fence_counter, ctx->fence_event);
		if (!CheckHr_(hr, r))
			return 0;
		if (WaitForSingleObject(ctx->fence_event, INFINITE) != WAIT_OBJECT_0)
			return 0;
	}

	return backbuffer_index;
}

API void
R4_ExecuteCommandLists(R4_Context* ctx, R4_Result* r, R4_Queue* queue, bool last_submission, intz cmdlist_count, R4_CommandList* const cmdlists[])
{
	Trace();
	AllocatorError err;

	Slice<ID3D12CommandList*> lists = AllocatorNewSlice<ID3D12CommandList*>(&ctx->allocator, cmdlist_count, &err);
	if (!CheckAllocatorErr_(err, r))
		return;
	for (intz i = 0; i < lists.count; ++i)
		lists[i] = cmdlists[i]->d3d12.list;
	queue->d3d12.queue->ExecuteCommandLists(lists.count, &lists[0]);
	AllocatorDeleteSlice(&ctx->allocator, lists, &err);
	if (!CheckAllocatorErr_(err, r))
		return;
}

// =============================================================================
// =============================================================================
// Heap management
API R4_Heap
R4_MakeHeap(R4_Context* ctx, R4_Result* r, int64 size, R4_HeapType type)
{
	Trace();
	SafeAssert(size >= 0);
	HRESULT hr;

	ID3D12Heap* heap = NULL;
	D3D12_HEAP_DESC heap_desc = {
		.SizeInBytes = AlignUp((UINT64)size, (64U<<10) - 1),
		.Properties = {
			.Type =
				(type == R4_HeapType_Default)  ? D3D12_HEAP_TYPE_DEFAULT  :
				(type == R4_HeapType_Upload)   ? D3D12_HEAP_TYPE_UPLOAD   :
				(type == R4_HeapType_Readback) ? D3D12_HEAP_TYPE_READBACK :
				(D3D12_HEAP_TYPE)0,
		},
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
	};
	hr = ctx->device->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap));
	if (!CheckHr_(hr, r))
		return {};
	return { .d3d12 = { heap } };
}

API void
R4_FreeHeap(R4_Context* ctx, R4_Heap* heap)
{
	Trace();
	auto* d3d12_heap = (R4_D3D12_Heap*)heap;
	d3d12_heap->heap->Release();
}

// =============================================================================
// =============================================================================
// Buffers & Images

// =============================================================================
// =============================================================================
// Buffer views & Image views

// =============================================================================
// =============================================================================
// Render target views & depth stencil views
API R4_RenderTargetView
R4_MakeRenderTargetView(R4_Context* ctx, R4_Result* r, R4_RenderTargetViewDesc const* desc)
{
	Trace();
	int32 index = AllocateFromViewPool_(&ctx->rtv_pool);
	SafeAssert(index >= 0);
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	uint64 heap_start = ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = heap_start + (uint64)index * increment;
	ctx->device->CreateRenderTargetView(desc->image->d3d12.resource, NULL, {ptr});
	return { .d3d12 = { ptr } };
}

API void
R4_FreeRenderTargetView(R4_Context* ctx, R4_RenderTargetView* render_target_view)
{
	Trace();
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	uint64 heap_start = ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = render_target_view->d3d12.rtv_ptr;
	SafeAssert(ptr >= heap_start);
	ptr -= heap_start;
	ptr /= increment;
	SafeAssert(ptr >= 0 && ptr <= INT32_MAX);
	FreeFromViewPool_(&ctx->rtv_pool, (int32)ptr);

	*render_target_view = {};
}

// =============================================================================
// =============================================================================
// Samplers

// =============================================================================
// =============================================================================
// Pipeline Layout
API R4_PipelineLayout
R4_MakePipelineLayout(R4_Context* ctx, R4_Result* r, R4_PipelineLayoutDesc const* desc)
{
	Trace();
	SafeAssert(desc->table_count >= 0 && desc->table_count <= 64);
	HRESULT hr;
	ID3D12RootSignature* rootsig = NULL;
	ID3D10Blob* rootsig_blob = NULL;

	D3D12_ROOT_PARAMETER root_params[ArrayLength(desc->push_constants) + 64] = {};
	intz root_params_count = 0;
	intz rest_table_count = 0;
	intz sampler_table_count = 0;
	intz const_count = 0;

	for (intz i = 0; i < desc->table_count; ++i)
	{
		bool has_sampler = false;
		bool has_other = false;
		for (intz j = 0; j < ArrayLength(desc->tables[i].ranges); ++j)
		{
			R4_PipelineLayoutDescriptorRange const* range_desc = &desc->tables[i].ranges[j];
			if (!range_desc->type)
				break;
			if (range_desc->type == R4_DescriptorType_Sampler)
				has_sampler = true;
			else
				has_other = true;
		}

		if (has_sampler)
			++sampler_table_count;
		if (has_other)
			++rest_table_count;
	}

	intz rest_table_i = 0;
	intz sampler_table_i = rest_table_count;
	D3D12_DESCRIPTOR_RANGE rest_ranges[64][ArrayLength(desc->tables[0].ranges)] = {};
	D3D12_DESCRIPTOR_RANGE sampler_ranges[64][ArrayLength(desc->tables[0].ranges)] = {};
	for (intz i = 0; i < desc->table_count; ++i)
	{
		D3D12_DESCRIPTOR_RANGE* table_rest_ranges = rest_ranges[i];
		D3D12_DESCRIPTOR_RANGE* table_sampler_ranges = sampler_ranges[i];
		intz rest_count = 0;
		intz sampler_count = 0;

		for (intz j = 0; j < ArrayLength(desc->tables[i].ranges); ++j)
		{
			R4_PipelineLayoutDescriptorRange const* range_desc = &desc->tables[i].ranges[j];
			if (!range_desc->type)
				break;
			D3D12_DESCRIPTOR_RANGE* out_range = NULL;
			if (range_desc->type == R4_DescriptorType_Sampler)
				out_range = &table_sampler_ranges[sampler_count++];
			else
				out_range = &table_rest_ranges[rest_count++];
			D3D12_DESCRIPTOR_RANGE_TYPE type = (D3D12_DESCRIPTOR_RANGE_TYPE)0;
			switch (range_desc->type)
			{
				case 0: Unreachable(); break;
				case R4_DescriptorType_Sampler: type = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
				case R4_DescriptorType_ImageShaderStorage:
				case R4_DescriptorType_BufferShaderStorage:
				case R4_DescriptorType_DynamicBufferShaderStorage:
				case R4_DescriptorType_ImageSampled: type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
				case R4_DescriptorType_UniformBuffer:
				case R4_DescriptorType_DynamicUniformBuffer: type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
				case R4_DescriptorType_ImageShaderReadWrite:
				case R4_DescriptorType_BufferShaderReadWrite:
				case R4_DescriptorType_DynamicBufferShaderReadWrite: type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
			}
			*out_range = {
				.RangeType = type,
				.NumDescriptors = (UINT)range_desc->count,
				.BaseShaderRegister = (UINT)range_desc->first_shader_slot,
				.RegisterSpace = 0,
				.OffsetInDescriptorsFromTableStart = (UINT)range_desc->offset_in_table,
			};
		}

		D3D12_SHADER_VISIBILITY visibility = D3d12VisibilityFromVisibility_(desc->tables[i].shader_visibility);
		if (rest_count)
			root_params[rest_table_i++] = {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable = {
					.NumDescriptorRanges = (UINT)rest_count,
					.pDescriptorRanges = table_rest_ranges,
				},
				.ShaderVisibility = visibility,
			};
		if (sampler_count)
			root_params[sampler_table_i++] = {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable = {
					.NumDescriptorRanges = (UINT)sampler_count,
					.pDescriptorRanges = table_rest_ranges,
				},
				.ShaderVisibility = visibility,
			};
	}

	D3D12_ROOT_SIGNATURE_DESC rootsig_desc = {
		.NumParameters = (UINT)root_params_count,
		.pParameters = root_params,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};
	hr = D3D12SerializeRootSignature(&rootsig_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootsig_blob, NULL);
	if (!CheckHr_(hr, r))
		return {};
	hr = ctx->device->CreateRootSignature(0, rootsig_blob->GetBufferPointer(), rootsig_blob->GetBufferSize(), IID_PPV_ARGS(&rootsig));
	rootsig_blob->Release();
	if (!CheckHr_(hr, r))
		return {};

	return {
		.d3d12 = {
			.rootsig = rootsig,
			.rest_table_count = (int32)rest_table_count,
			.sampler_table_count = (int32)sampler_table_count,
			.const_count = (int32)const_count,
		},
	};
}

API void
R4_FreePipelineLayout(R4_Context* ctx, R4_PipelineLayout* pipeline_layout)
{
	Trace();
	if (pipeline_layout->d3d12.rootsig)
		pipeline_layout->d3d12.rootsig->Release();
	*pipeline_layout = {};
}

// =============================================================================
// =============================================================================
// Pipeline

// =============================================================================
// =============================================================================
// Descriptor heap & descriptor sets

// =============================================================================
// =============================================================================
// Command allocator & command lists

// =============================================================================
// =============================================================================
// Command list recording

