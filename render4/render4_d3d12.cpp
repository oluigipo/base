#include "common.h"
#include "api_os.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "api_os_win32.h"
#include "api_os_d3d12.h"
#include "api_render4.h"

struct R4_Context
{
	OS_D3D12Api2 api;
	ID3D12Device* device;
	ID3D12Device2* device2;
	IDXGIAdapter1* adapter1;
	IDXGIFactory4* factory4;

	HRESULT device_lost_reason;
};

static bool
CheckHr_(R4_Context* ctx, HRESULT hr)
{
	if (hr == DXGI_ERROR_DEVICE_HUNG ||
		hr == DXGI_ERROR_DEVICE_REMOVED ||
		hr == DXGI_ERROR_DEVICE_RESET ||
		hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
	{
		__atomic_store_n(&ctx->device_lost_reason, hr, __ATOMIC_RELEASE);
		OS_LogErr("ERROR: HR: %u\n", (unsigned int)hr);
		return false;
	}
	return true;
}

static D3D12_RESOURCE_STATES
D3d12StatesFromResourceStates_(uint32 flags)
{
	D3D12_RESOURCE_STATES result = (D3D12_RESOURCE_STATES)0;

	if (flags == R4_ResourceState_Common)
		return result;
	if (flags & R4_ResourceState_Present)
		result |= D3D12_RESOURCE_STATE_PRESENT;
	if (flags & (R4_ResourceState_VertexBuffer | R4_ResourceState_ConstantBuffer))
		result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	if (flags & R4_ResourceState_IndexBuffer)
		result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (flags & R4_ResourceState_RenderTarget)
		result |= D3D12_RESOURCE_STATE_RENDER_TARGET;
	if (flags & R4_ResourceState_UnorderedAccess)
		result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (flags & R4_ResourceState_DepthWrite)
		result |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (flags & R4_ResourceState_DepthRead)
		result |= D3D12_RESOURCE_STATE_DEPTH_READ;
	if (flags & R4_ResourceState_NonPixelShaderResource)
		result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (flags & R4_ResourceState_PixelShaderResource)
		result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (flags & R4_ResourceState_StreamOut)
		result |= D3D12_RESOURCE_STATE_STREAM_OUT;
	if (flags & R4_ResourceState_IndirectArgument)
		result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	if (flags & R4_ResourceState_Predication)
		result |= D3D12_RESOURCE_STATE_PREDICATION;
	if (flags & R4_ResourceState_CopyDest)
		result |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (flags & R4_ResourceState_CopySource)
		result |= D3D12_RESOURCE_STATE_COPY_SOURCE;

	return result;
}

static D3D12_COMMAND_LIST_TYPE
D3d12TypeFromCommandListKind_(R4_CommandListKind kind)
{
	D3D12_COMMAND_LIST_TYPE result = (D3D12_COMMAND_LIST_TYPE)-1;

	switch (kind)
	{
		case R4_CommandListKind_Graphics: result = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
		case R4_CommandListKind_Compute: result = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
		case R4_CommandListKind_Copy: result = D3D12_COMMAND_LIST_TYPE_COPY; break;
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

static D3D12_PRIMITIVE_TOPOLOGY_TYPE
D3d12TopologyTypeFromTopologyType_(R4_PrimitiveTopology topology)
{
	D3D12_PRIMITIVE_TOPOLOGY_TYPE result = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)0;

	switch (topology)
	{
		case R4_PrimitiveTopology_TriangleList:
		case R4_PrimitiveTopology_TriangleFan:
		case R4_PrimitiveTopology_TriangleStrip:
			result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			break;
		case R4_PrimitiveTopology_LineList:
		case R4_PrimitiveTopology_LineStrip:
			result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			break;
		case R4_PrimitiveTopology_PointList:
			result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			break;
		case R4_PrimitiveTopology_Patch:
			result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
			break;
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

static D3D12_RESOURCE_DESC
D3d12ResourceDescFromDesc_(R4_ResourceDesc const* desc)
{
	D3D12_RESOURCE_DESC result = {
		.Dimension =
			(desc->kind == R4_ResourceKind_Buffer) ? D3D12_RESOURCE_DIMENSION_BUFFER :
			(desc->kind == R4_ResourceKind_Texture1D) ? D3D12_RESOURCE_DIMENSION_TEXTURE1D :
			(desc->kind == R4_ResourceKind_Texture2D) ? D3D12_RESOURCE_DIMENSION_TEXTURE2D :
			(desc->kind == R4_ResourceKind_Texture3D) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D :
			D3D12_RESOURCE_DIMENSION_UNKNOWN,
		.Width = desc->width,
		.Height = desc->height ? (UINT)desc->height : 1,
		.DepthOrArraySize = desc->depth ? (UINT16)desc->depth : (UINT16)1,
		.MipLevels = 1,
		.Format = DxgiFormatFromFormat_(desc->format),
		.SampleDesc = {
			.Count = 1,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
	};
	return result;
}

// =============================================================================
// =============================================================================
API R4_Context*
R4_D3D12_MakeContext(Arena* arena, R4_ContextDesc* desc)
{
	Trace();
	HRESULT hr;
	R4_Context ctx = {};
	if (!OS_MakeD3D12Api2(&ctx.api, true))
		return NULL;

	{
		hr = ctx.api.create_dxgi_factory1(IID_PPV_ARGS(&ctx.factory4));
		if (FAILED(hr))
			goto lbl_error;
		
		for (UINT i = 0;; ++i)
		{
			IDXGIAdapter1* this_adapter;
			if (ctx.factory4->EnumAdapters1(i, &this_adapter) == DXGI_ERROR_NOT_FOUND)
				break;
			hr = ctx.api.d3d12_create_device(this_adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), NULL);
			if (SUCCEEDED(hr))
			{
				ctx.adapter1 = this_adapter;
				break;
			}
			this_adapter->Release();
		}
		if (!ctx.adapter1)
			goto lbl_error;

		hr = ctx.api.d3d12_create_device(ctx.adapter1, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&ctx.device));
		if (FAILED(hr))
			goto lbl_error;
		hr = ctx.device->QueryInterface(IID_PPV_ARGS(&ctx.device2));
		if (FAILED(hr))
			goto lbl_error;

		if (ctx.api.debug_layer_enabled)
		{
			ID3D12InfoQueue* queue;
			hr = ctx.device->QueryInterface(IID_PPV_ARGS(&queue));
			if (SUCCEEDED(hr))
			{
				if (Assert_IsDebuggerPresent_())
				{
					queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
					queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
					queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
				}
				queue->Release();
			}
		}

		for (intsize i = 0; i < ArrayLength(desc->swapchains); ++i)
		{
			if (!desc->swapchains[i].window)
				break;
			HWND hwnd = OS_W32_HwndFromWindow(*desc->swapchains[i].window);
			R4_Format format = desc->swapchains[i].format;
			uint32 buffer_count = desc->swapchains[i].buffer_count;
			R4_Swapchain* out_swapchain = desc->swapchains[i].out_swapchain;
			R4_Queue* out_queue = desc->swapchains[i].out_graphics_queue;

			D3D12_COMMAND_QUEUE_DESC desc = {
				.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
				.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			};
			hr = ctx.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&out_queue->d3d12_queue));
			if (FAILED(hr))
				goto lbl_error;
			out_queue->kind = R4_CommandListKind_Graphics;

			DXGI_SWAP_CHAIN_DESC1 swapchain_desc1 = {
				.Format = DxgiFormatFromFormat_(format),
				.SampleDesc = {
					.Count = 1,
				},
				.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
				.BufferCount = buffer_count,
				.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			};
			IDXGISwapChain1* swapchain1;
			hr = ctx.factory4->CreateSwapChainForHwnd(out_queue->d3d12_queue, hwnd, &swapchain_desc1, NULL, NULL, &swapchain1);
			if (FAILED(hr))
				goto lbl_error;
			hr = swapchain1->QueryInterface(IID_PPV_ARGS(&out_swapchain->d3d12_swapchain));
			if (FAILED(hr))
			{
				swapchain1->Release();
				goto lbl_error;
			}

			hr = ctx.factory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
			if (FAILED(hr))
			{
				ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
				String str = OS_W32_StringFromHr(hr, scratch.arena);
				OS_LogErr("[WARN] render4: MakeWindowAssociation failed (0x%x): %S\n", hr, str);
				ArenaRestore(scratch);
			}
		}

		for (intsize i = 0; i < ArrayLength(desc->queues); ++i)
		{
			if (!desc->queues[i].out_queue)
				break;

			D3D12_COMMAND_QUEUE_DESC queue_desc = {
				.Type = D3d12TypeFromCommandListKind_(desc->queues[i].kind),
				.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			};
			hr = ctx.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&desc->queues[i].out_queue->d3d12_queue));
			if (FAILED(hr))
				goto lbl_error;
		}

		return ArenaPushStructData(arena, R4_Context, &ctx);
	}

lbl_error:
	String str = StringPrintfLocal(256, "HR: %x", (uint32)hr);
	OS_MessageBox(Str("Error D3D12"), str);
	for (intsize i = 0; i < ArrayLength(desc->swapchains); ++i)
	{
		if (desc->swapchains[i].out_swapchain && desc->swapchains[i].out_swapchain->d3d12_swapchain)
			desc->swapchains[i].out_swapchain->d3d12_swapchain->Release();
		if (desc->swapchains[i].out_graphics_queue && desc->swapchains[i].out_graphics_queue->d3d12_queue)
			desc->swapchains[i].out_graphics_queue->d3d12_queue->Release();
	}
	for (intsize i = 0; i < ArrayLength(desc->queues); ++i)
	{
		if (desc->queues[i].out_queue && desc->queues[i].out_queue->d3d12_queue)
			desc->queues[i].out_queue->d3d12_queue->Release();
	}
	if (ctx.factory4)
		ctx.factory4->Release();
	if (ctx.adapter1)
		ctx.adapter1->Release();
	if (ctx.device)
		ctx.device->Release();
	if (ctx.device2)
		ctx.device->Release();
	return NULL;
}

API void
R4_DestroyContext(R4_Context *ctx)
{
	Trace();

	if (ctx->factory4)
		ctx->factory4->Release();
	if (ctx->adapter1)
		ctx->adapter1->Release();
	if (ctx->device)
		ctx->device->Release();
	if (ctx->device2)
		ctx->device->Release();
}

API R4_ContextInfo
R4_QueryInfo(R4_Context *ctx)
{
	Trace();
	R4_ContextInfo info = {
		.backend_api = StrInit("Direct3D 12"),
	};

	return info;
}

API bool
R4_DeviceLost(R4_Context* ctx, R4_DeviceLostInfo* out_info)
{
	Trace();
	HRESULT hr;
	R4_DeviceLostInfo info = {};

	hr = __atomic_load_n(&ctx->device_lost_reason, __ATOMIC_ACQUIRE);
	if (hr != 0)
	{
		info.api_error_code = hr;
		switch (hr)
		{
			default: info.reason = Str("unknown error"); break;
			case DXGI_ERROR_DEVICE_HUNG: info.reason = Str("device hung"); break;
			case DXGI_ERROR_DEVICE_REMOVED: info.reason = Str("device removed"); break;
			case DXGI_ERROR_DEVICE_RESET: info.reason = Str("device reset"); break;
			case DXGI_ERROR_DRIVER_INTERNAL_ERROR: info.reason = Str("driver internal error"); break;
		}
	}

	if (out_info)
		*out_info = info;
	return hr != 0;
}

API bool
R4_RecoverDevice(R4_Context* ctx)
{
	Trace();
	Unreachable(); // TODO
}

// =============================================================================
// =============================================================================
API R4_Fence
R4_MakeFence(R4_Context *ctx)
{
	Trace();
	HRESULT hr;
	ID3D12Fence* fence = NULL;
	HANDLE event = NULL;
	
	hr = ctx->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (!CheckHr_(ctx, hr))
		return {};
	event = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!event || event == INVALID_HANDLE_VALUE)
	{
		fence->Release();
		return {};
	}

	return {
		.d3d12_fence = fence,
		.d3d12_event = event,
	};
}

API R4_CommandAllocator
R4_MakeCommandAllocator(R4_Context* ctx, R4_Queue* queue)
{
	Trace();
	HRESULT hr;
	ID3D12CommandAllocator* allocator = NULL;
	D3D12_COMMAND_LIST_TYPE type = D3d12TypeFromCommandListKind_(queue->kind);

	hr = ctx->device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator));
	CheckHr_(ctx, hr);

	return {
		.d3d12_allocator = allocator,
	};
}

API R4_CommandList
R4_MakeCommandList(R4_Context* ctx, R4_CommandListKind list_kind, R4_CommandAllocator* command_allocator)
{
	Trace();
	HRESULT hr;
	ID3D12GraphicsCommandList* list = NULL;
	D3D12_COMMAND_LIST_TYPE type = D3d12TypeFromCommandListKind_(list_kind);

	hr = ctx->device->CreateCommandList(0, type, command_allocator->d3d12_allocator, NULL, IID_PPV_ARGS(&list));
	if (!CheckHr_(ctx, hr))
		return {};
	hr = list->Close();
	if (!CheckHr_(ctx, hr))
	{
		list->Release();
		return {};
	}

	return {
		.d3d12_list = list,
	};
}

API R4_Heap
R4_MakeHeap(R4_Context* ctx, R4_HeapDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID3D12Heap* heap = NULL;
	D3D12_HEAP_DESC heap_desc = {
		.SizeInBytes = AlignUp(desc->size, 64<<10),
		.Properties = {
			.Type =
				(desc->kind == R4_HeapKind_Default)  ? D3D12_HEAP_TYPE_DEFAULT  :
				(desc->kind == R4_HeapKind_Upload)   ? D3D12_HEAP_TYPE_UPLOAD   :
				(desc->kind == R4_HeapKind_Readback) ? D3D12_HEAP_TYPE_READBACK :
				(D3D12_HEAP_TYPE)0,
		},
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
	};
	hr = ctx->device->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap));
	if (!CheckHr_(ctx, hr))
		return {};
	return {
		.d3d12_heap = heap,
	};
}

API R4_DescriptorHeap
R4_MakeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeapDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID3D12DescriptorHeap* heap = NULL;
	D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (desc->flags & R4_DescriptorHeapFlag_ShaderVisible)
		flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
		.Type =
			(desc->type == R4_DescriptorHeapType_Rtv) ? D3D12_DESCRIPTOR_HEAP_TYPE_RTV :
			(desc->type == R4_DescriptorHeapType_Dsv) ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV :
			(desc->type == R4_DescriptorHeapType_CbvSrvUav) ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV :
			(desc->type == R4_DescriptorHeapType_Sampler) ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER :
			(D3D12_DESCRIPTOR_HEAP_TYPE)0,
		.Flags = flags,
		.NodeMask = 1,
	};
	hr = ctx->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap));
	if (!CheckHr_(ctx, hr))
		return {};
	return {
		.d3d12_heap = heap,
	};
}

API R4_Resource
R4_MakePlacedResource(R4_Context* ctx, R4_PlacedResourceDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID3D12Resource* resource = NULL;
	D3D12_RESOURCE_DESC resource_desc = D3d12ResourceDescFromDesc_(&desc->resource_desc);
	D3D12_CLEAR_VALUE clear_value = {
		.Format = DxgiFormatFromFormat_(desc->optimized_clear_value.format),
		.Color = {
			desc->optimized_clear_value.color[0],
			desc->optimized_clear_value.color[1],
			desc->optimized_clear_value.color[2],
			desc->optimized_clear_value.color[3],
		},
	};
	if (clear_value.Format && IsDepthStencilFormat_(clear_value.Format))
	{
		clear_value.DepthStencil = {
			.Depth = desc->optimized_clear_value.depth,
			.Stencil = desc->optimized_clear_value.stencil,
		};
	}
	hr = ctx->device->CreatePlacedResource(
		desc->heap->d3d12_heap,
		desc->heap_offset,
		&resource_desc,
		D3d12StatesFromResourceStates_(desc->initial_state),
		clear_value.Format ? &clear_value : NULL,
		IID_PPV_ARGS(&resource));
	if (!CheckHr_(ctx, hr))
		return {};
	return {
		.d3d12_resource = resource,
	};
}

API R4_RootSignature
R4_MakeRootSignature(R4_Context* ctx, R4_RootSignatureDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID3D12RootSignature* rootsig = NULL;
	ID3D10Blob* rootsig_blob = NULL;
	
	D3D12_ROOT_PARAMETER root_params[ArrayLength(desc->params)] = {};
	intsize root_params_count = 0;
	for (intsize i = 0; i < ArrayLength(desc->params); ++i)
	{
		R4_RootParameter const* param = &desc->params[i];
		if (!param->type)
			break;
		D3D12_SHADER_VISIBILITY visibility = D3d12VisibilityFromVisibility_(param->visibility);
		switch (param->type)
		{
			case R4_RootParameterType_Constants:
			{
				root_params[root_params_count++] = D3D12_ROOT_PARAMETER {
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
					.Constants = {
						.ShaderRegister = param->constants.offset,
						.RegisterSpace = 0,
						.Num32BitValues= param->constants.count,
					},
					.ShaderVisibility = visibility,
				};
			} break;
		}
	}

	D3D12_ROOT_SIGNATURE_DESC rootsig_desc = {
		.NumParameters = (UINT32)root_params_count,
		.pParameters = root_params,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};
	hr = ctx->api.d3d12_serialize_root_signature(&rootsig_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootsig_blob, NULL);
	if (!CheckHr_(ctx, hr))
		return {};
	
	hr = ctx->device->CreateRootSignature(0, rootsig_blob->GetBufferPointer(), rootsig_blob->GetBufferSize(), IID_PPV_ARGS(&rootsig));
	rootsig_blob->Release();
	if (!CheckHr_(ctx, hr))
		return {};

	return {
		.d3d12_rootsig = rootsig,
	};
}

API R4_Pipeline
R4_MakeGraphicsPipeline(R4_Context* ctx, R4_GraphicsPipelineDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID3D12PipelineState* pipeline = NULL;

	D3D12_INPUT_ELEMENT_DESC input_layout[ArrayLength(desc->input_layout)] = {};
	UINT input_layout_size = 0;
	for (intsize i = 0; i < ArrayLength(desc->input_layout); ++i)
	{
		R4_GraphicsPipelineInputLayout const* layout = &desc->input_layout[i];
		if (!layout->format)
			break;
		input_layout[i] = {
			.SemanticName = "VINPUT",
			.SemanticIndex = input_layout_size,
			.Format = DxgiFormatFromFormat_(layout->format),
			.InputSlot = layout->input_slot,
			.AlignedByteOffset = layout->byte_offset,
			.InputSlotClass = (layout->instance_step_rate != 0) ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = layout->instance_step_rate,
		};
		++input_layout_size;
	}

	D3D12_BLEND_DESC blend_desc = {
		.AlphaToCoverageEnable = desc->blend_enable_alpha_to_coverage,
		.IndependentBlendEnable = desc->blend_enable_independent_blend,
	};
	intsize rtcount = Min(ArrayLength(blend_desc.RenderTarget), ArrayLength(desc->blend_rendertargets));
	for (intsize i = 0; i < rtcount; ++i)
	{
		R4_GraphicsPipelineRenderTargetBlendDesc const* rt = &desc->blend_rendertargets[i];

		blend_desc.RenderTarget[i] = {
			.BlendEnable = rt->enable_blend,
			.LogicOpEnable = rt->enable_logic_op,
			.SrcBlend = D3d12BlendFromBlendFunc_(rt->src),
			.DestBlend = D3d12BlendFromBlendFunc_(rt->dst),
			.BlendOp = D3d12BlendOpFromBlendOp_(rt->op),
			.SrcBlendAlpha = D3d12BlendFromBlendFunc_(rt->src_alpha),
			.DestBlendAlpha = D3d12BlendFromBlendFunc_(rt->dst_alpha),
			.BlendOpAlpha = D3d12BlendOpFromBlendOp_(rt->op_alpha),
			.LogicOp = D3d12LogicOpFromLogicOp_(rt->logic_op),
			.RenderTargetWriteMask = rt->rendertarget_write_mask,
		};
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {
		.pRootSignature = desc->rootsig->d3d12_rootsig,
		.VS = { desc->vs_dxil.data, desc->vs_dxil.size },
		.PS = { desc->ps_dxil.data, desc->ps_dxil.size },
		.BlendState = blend_desc,
		.SampleMask = desc->sample_mask,
		.RasterizerState = {
			.FillMode = D3d12FillModeFromFillMode_(desc->rast_fill_mode),
			.CullMode = D3d12CullModeFromCullMode_(desc->rast_cull_mode),
			.FrontCounterClockwise = !desc->rast_cw_frontface,
			.DepthBias = desc->rast_depth_bias,
			.DepthBiasClamp = desc->rast_depth_bias_clamp,
			.SlopeScaledDepthBias = desc->rast_slope_scaled_depth_bias,
			.DepthClipEnable = desc->rast_enable_depth_clip,
			.MultisampleEnable = desc->rast_enable_multisample,
			.AntialiasedLineEnable = desc->rast_enable_antialiased_line,
			.ForcedSampleCount = desc->rast_forced_sample_count,
			.ConservativeRaster = desc->rast_enable_conservative_mode ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
		},
		.DepthStencilState = {
			.DepthEnable = desc->depth_enable,
			.DepthWriteMask = desc->depth_disable_writes ? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3d12CompFuncFromCompFunc_(desc->depth_comparison_func),
			.StencilEnable = desc->stencil_enable,
			.StencilReadMask = desc->stencil_read_mask,
			.StencilWriteMask = desc->stencil_write_mask,
			.FrontFace = {}, // TODO(ljre)
			.BackFace = {}, // TODO(ljre)
		},
		.InputLayout = {
			.pInputElementDescs = input_layout,
			.NumElements = input_layout_size,
		},
		.PrimitiveTopologyType = D3d12TopologyTypeFromTopologyType_(desc->primitive_topology),
		.NumRenderTargets = desc->rendertarget_count,
		.RTVFormats = {
			DxgiFormatFromFormat_(desc->rendertarget_formats[0]),
			DxgiFormatFromFormat_(desc->rendertarget_formats[1]),
			DxgiFormatFromFormat_(desc->rendertarget_formats[2]),
			DxgiFormatFromFormat_(desc->rendertarget_formats[3]),
			DxgiFormatFromFormat_(desc->rendertarget_formats[4]),
			DxgiFormatFromFormat_(desc->rendertarget_formats[5]),
			DxgiFormatFromFormat_(desc->rendertarget_formats[6]),
			DxgiFormatFromFormat_(desc->rendertarget_formats[7]),
		},
		.DSVFormat = DxgiFormatFromFormat_(desc->depthstencil_format),
		.SampleDesc = {
			.Count = desc->sample_count,
			.Quality = desc->sample_quality,
		},
		.NodeMask = desc->node_mask,
		.CachedPSO = {},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};

	hr = ctx->device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline));
	CheckHr_(ctx, hr);

	return {
		.d3d12_pso = pipeline,
	};
}

API void
R4_FreeFence(R4_Context* ctx, R4_Fence* fence)
{
	Trace();
	if (fence->d3d12_fence)
		fence->d3d12_fence->Release();
	if (fence->d3d12_event)
		CloseHandle(fence->d3d12_event);
	*fence = {};
}

API void
R4_FreeCommandAllocator(R4_Context* ctx, R4_CommandAllocator* allocator)
{
	Trace();
	if (allocator->d3d12_allocator)
		allocator->d3d12_allocator->Release();
	*allocator = {};
}

API void
R4_FreeCommandList(R4_Context* ctx, R4_CommandAllocator* allocator, R4_CommandList* cmdlist)
{
	Trace();
	if (cmdlist->d3d12_list)
		cmdlist->d3d12_list->Release();
	*cmdlist = {};
}

API void
R4_FreePipeline(R4_Context* ctx, R4_Pipeline* pipeline)
{
	Trace();
	if (pipeline->d3d12_pso)
		pipeline->d3d12_pso->Release();
	*pipeline = {};
}

API void
R4_FreeRootSignature(R4_Context* ctx, R4_RootSignature* rootsig)
{
	Trace();
	if (rootsig->d3d12_rootsig)
		rootsig->d3d12_rootsig->Release();
	*rootsig = {};
}

API void
R4_FreeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeap* heap)
{
	Trace();
	if (heap->d3d12_heap)
		heap->d3d12_heap->Release();
	*heap = {};
}

API void
R4_FreeHeap(R4_Context* ctx, R4_Heap* heap)
{
	Trace();
	if (heap->d3d12_heap)
		heap->d3d12_heap->Release();
	*heap = {};
}

API void
R4_FreeResource(R4_Context* ctx, R4_Resource* resource)
{
	Trace();
	if (resource->d3d12_resource)
		resource->d3d12_resource->Release();
	*resource = {};
}

// =============================================================================
// =============================================================================
API uint32
R4_GetDescriptorSize(R4_Context* ctx, R4_DescriptorHeapType type)
{
	Trace();
	D3D12_DESCRIPTOR_HEAP_TYPE heap_type = (D3D12_DESCRIPTOR_HEAP_TYPE)type;
	return ctx->device->GetDescriptorHandleIncrementSize(heap_type);
}

API uint32
R4_GetSwapchainBuffers(R4_Context* ctx, R4_Swapchain* swapchain, intsize image_count, R4_Resource* out_resources)
{
	Trace();
	HRESULT hr;
	uint32 count = 0;
	for (intsize i = 0; i < image_count; ++i)
	{
		ID3D12Resource* resource = NULL;
		hr = swapchain->d3d12_swapchain->GetBuffer(i, IID_PPV_ARGS(&resource));
		if (!CheckHr_(ctx, hr))
			return 0;
		out_resources[i] = {
			.d3d12_resource = resource,
		};
		count = i+1;
	}
	
	return count;
}

API R4_DescriptorHeap
R4_CreateRenderTargetViewsFromResources(R4_Context* ctx, R4_Format format, intsize resource_count, R4_Resource* resources, R4_RenderTargetView* out_rtvs)
{
	Trace();
	(void)format;
	if (resource_count <= 0)
		return {};
	HRESULT hr;
	ID3D12DescriptorHeap* heap = NULL;
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = (UINT)resource_count,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
	};
	hr = ctx->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap));
	if (!CheckHr_(ctx, hr))
		return {};
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	uint64 heap_start = heap->GetCPUDescriptorHandleForHeapStart().ptr;
	for (intsize i = 0; i < resource_count; ++i)
	{
		uint64 ptr = heap_start + i * increment;
		ctx->device->CreateRenderTargetView(resources[i].d3d12_resource, NULL, {ptr});
		out_rtvs[i] = {
			.d3d12_resource = resources[i].d3d12_resource,
			.d3d12_rtv_ptr = ptr,
		};
		out_rtvs[i].d3d12_resource->AddRef();
	}
	return {
		.d3d12_heap = heap,
	};
}

API R4_MemoryRequirements
R4_GetResourceMemoryRequirements(R4_Context* ctx, R4_ResourceDesc const* desc)
{
	Trace();
	D3D12_RESOURCE_DESC resource_desc = D3d12ResourceDescFromDesc_(desc);
	D3D12_RESOURCE_ALLOCATION_INFO allocation_info = ctx->device->GetResourceAllocationInfo(1, 1, &resource_desc);
	return {
		.size = allocation_info.SizeInBytes,
		.alignment = allocation_info.Alignment,
	};
}

API void
R4_MapResource(R4_Context* ctx, R4_Resource* resource, uint32 subresource, uint64 size, void** out_memory)
{
	Trace();
	HRESULT hr;
	hr = resource->d3d12_resource->Map(subresource, {}, out_memory);
	CheckHr_(ctx, hr);
}

API void
R4_UnmapResource(R4_Context* ctx, R4_Resource* resource, uint32 subresource)
{
	Trace();
	resource->d3d12_resource->Unmap(subresource, NULL);
}

// =============================================================================
// =============================================================================
API bool
R4_WaitFence(R4_Context* ctx, R4_Fence* fence, uint32 timeout_ms)
{
	Trace();
	HRESULT hr;
	if (timeout_ms == UINT32_MAX)
		timeout_ms = INFINITE;
	if (fence->d3d12_fence->GetCompletedValue() < fence->counter)
	{
		hr = fence->d3d12_fence->SetEventOnCompletion(fence->counter, fence->d3d12_event);
		if (!CheckHr_(ctx, hr))
			return false;
		DWORD r = WaitForSingleObject(fence->d3d12_event, timeout_ms);
		return r == WAIT_OBJECT_0;
	}
	return true;
}

API uint32
R4_GetCurrentBackBufferIndex(R4_Context* ctx, R4_Swapchain* swapchain)
{
	Trace();
	return swapchain->d3d12_swapchain->GetCurrentBackBufferIndex();
}

API void
R4_ResetCommandAllocator(R4_Context* ctx, R4_CommandAllocator* allocator)
{
	Trace();
	HRESULT hr;
	hr = allocator->d3d12_allocator->Reset();
	CheckHr_(ctx, hr);
}

API void
R4_BeginCommandList(R4_Context* ctx, R4_CommandList* cmdlist, R4_CommandAllocator* allocator)
{
	Trace();
	HRESULT hr;
	ID3D12PipelineState* pipeline = NULL;
	hr = cmdlist->d3d12_list->Reset(allocator->d3d12_allocator, pipeline);
	CheckHr_(ctx, hr);
}

API void
R4_EndCommandList(R4_Context* ctx, R4_CommandList* cmdlist)
{
	Trace();
	HRESULT hr;
	hr = cmdlist->d3d12_list->Close();
	CheckHr_(ctx, hr);
}

API void
R4_ExecuteCommandLists(R4_Context* ctx, R4_Queue* queue, R4_Fence* completion_fence, R4_Swapchain* swapchain_to_signal, intsize cmdlist_count, R4_CommandList* cmdlists[])
{
	Trace();
	SafeAssert(cmdlist_count <= UINT32_MAX);
	HRESULT hr;
	// NOTE(ljre): unused since the ID3D12CommandQueue is already 'attached' to the swapchain
	(void)swapchain_to_signal;

	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	ID3D12CommandList** lists = ArenaPushArray(scratch.arena, ID3D12CommandList*, cmdlist_count);
	SafeAssert(lists);
	for (intsize i = 0; i < cmdlist_count; ++i)
		lists[i] = cmdlists[i]->d3d12_list;
	queue->d3d12_queue->ExecuteCommandLists(cmdlist_count, lists);
	ArenaRestore(scratch);

	++completion_fence->counter;
	hr = queue->d3d12_queue->Signal(completion_fence->d3d12_fence, completion_fence->counter);
	if (!CheckHr_(ctx, hr))
		return;
}

API void
R4_Present(R4_Context *ctx, R4_Queue* queue, R4_Swapchain* swapchain, uint32 sync_interval)
{
	Trace();
	HRESULT hr;
	hr = swapchain->d3d12_swapchain->Present(sync_interval, 0);
	CheckHr_(ctx, hr);
}

API void
R4_CmdSetPrimitiveTopology(R4_Context* ctx, R4_CommandList* cmdlist, R4_PrimitiveTopology topology)
{
	Trace();
	cmdlist->d3d12_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

API void
R4_CmdBeginRenderpass(R4_Context* ctx, R4_CommandList* cmdlist, R4_BeginRenderpassDesc const* desc)
{
	Trace();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs_to_bind[8] = {};
	intsize rtvs_to_bind_count = 0;
	for (intsize i = 0; i < ArrayLength(desc->color_attachments); ++i)
	{
		R4_RenderpassAttachment const* attachment = &desc->color_attachments[i];
		if (!attachment->rendertarget)
			break;
		rtvs_to_bind[rtvs_to_bind_count++] = { attachment->rendertarget->d3d12_rtv_ptr };
	}
	cmdlist->d3d12_list->OMSetRenderTargets(rtvs_to_bind_count, rtvs_to_bind, FALSE, NULL);

	for (intsize i = 0; i < ArrayLength(desc->color_attachments); ++i)
	{
		R4_RenderpassAttachment const* attachment = &desc->color_attachments[i];
		if (!attachment->rendertarget)
			break;
		if (attachment->load == R4_AttachmentLoadOp_Clear)
			cmdlist->d3d12_list->ClearRenderTargetView({attachment->rendertarget->d3d12_rtv_ptr}, attachment->clear_color, 0, NULL);
	}
}

API void
R4_CmdEndRenderpass(R4_Context* ctx, R4_CommandList* cmdlist)
{
	Trace();
	// NOTE(ljre): Nothing to do.
}

API void
R4_CmdSetViewports(R4_Context* ctx, R4_CommandList* cmdlist, intsize viewport_count, R4_Viewport const* viewports)
{
	Trace();
	D3D12_VIEWPORT vports[8] = {};
	if (viewport_count > ArrayLength(vports))
		viewport_count = ArrayLength(vports);
	for (intsize i = 0; i < viewport_count; ++i)
	{
		vports[i] = {
			.TopLeftX = viewports[i].x,
			.TopLeftY = viewports[i].y,
			.Width = viewports[i].width,
			.Height = viewports[i].height,
			.MinDepth = viewports[i].min_depth,
			.MaxDepth = viewports[i].max_depth,
		};
	}
	cmdlist->d3d12_list->RSSetViewports(viewport_count, vports);
}

API void
R4_CmdSetScissors(R4_Context* ctx, R4_CommandList* cmdlist, intsize scissor_count, const R4_Rect* scissors)
{
	Trace();
	D3D12_RECT rects[8] = {};
	if (scissor_count > ArrayLength(rects))
		scissor_count = ArrayLength(rects);
	for (intsize i = 0; i < scissor_count; ++i)
	{
		rects[i] = {
			.left = scissors[i].x,
			.top = scissors[i].y,
			.right = scissors[i].x + scissors[i].width,
			.bottom = scissors[i].y + scissors[i].height,
		};
	}
	cmdlist->d3d12_list->RSSetScissorRects(scissor_count, rects);
}

API void
R4_CmdSetPipeline(R4_Context* ctx, R4_CommandList* cmdlist, R4_Pipeline* pipeline)
{
	Trace();
	cmdlist->d3d12_list->SetPipelineState(pipeline->d3d12_pso);
}

API void
R4_CmdSetRootSignature(R4_Context* ctx, R4_CommandList* cmdlist, R4_RootSignature* rootsig)
{
	Trace();
	cmdlist->d3d12_list->SetGraphicsRootSignature(rootsig->d3d12_rootsig);
}

API void
R4_CmdSetIndexBuffer(R4_Context* ctx, R4_CommandList* cmdlist, R4_Resource* buffer, uint64 offset, uint32 size, R4_Format format)
{
	Trace();
	D3D12_INDEX_BUFFER_VIEW view = {
		.BufferLocation = buffer->d3d12_resource->GetGPUVirtualAddress() + offset,
		.SizeInBytes = size,
		.Format = (format == R4_Format_R32_UInt) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
	};
	cmdlist->d3d12_list->IASetIndexBuffer(&view);
}

API void
R4_CmdSetVertexBuffers(R4_Context* ctx, R4_CommandList* cmdlist, uint32 first_slot, uint32 slot_count, R4_VertexBuffer const* buffers)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	D3D12_VERTEX_BUFFER_VIEW* views = ArenaPushArray(scratch.arena, D3D12_VERTEX_BUFFER_VIEW, slot_count);
	for (intsize i = 0; i < slot_count; ++i)
	{
		views[i] = {
			.BufferLocation = buffers[i].resource->d3d12_resource->GetGPUVirtualAddress() + buffers[i].offset,
			.SizeInBytes = buffers[i].size,
			.StrideInBytes = buffers[i].stride,
		};
	}
	cmdlist->d3d12_list->IASetVertexBuffers(first_slot, slot_count, views);
	ArenaRestore(scratch);
}

API void
R4_CmdDraw(R4_Context* ctx, R4_CommandList* cmdlist, uint32 start_vertex, uint32 vertex_count, uint32 start_instance, uint32 instance_count)
{
	Trace();
	cmdlist->d3d12_list->DrawInstanced(vertex_count, instance_count, start_vertex, start_instance);
}

API void
R4_CmdDrawIndexed(R4_Context* ctx, R4_CommandList* cmdlist, uint32 start_index, uint32 index_count, uint32 start_instance, uint32 instance_count, int32 base_vertex)
{
	cmdlist->d3d12_list->DrawIndexedInstanced(index_count, instance_count, start_index, base_vertex, start_instance);
}

API void
R4_CmdDispatch(R4_Context* ctx, R4_CommandList* cmdlist, uint32 x, uint32 y, uint32 z)
{
	Trace();
	cmdlist->d3d12_list->Dispatch(x, y, z);
}

API void
R4_CmdSetGraphicsRootConstantU32(R4_Context* ctx, R4_CommandList* cmdlist, uint32 slot, uint32 dest_offset, uint32 value)
{
	Trace();
	cmdlist->d3d12_list->SetGraphicsRoot32BitConstant(slot, value, dest_offset);
}

API void
R4_CmdSetGraphicsRootConstantsU32(R4_Context* ctx, R4_CommandList* cmdlist, R4_RootArgument const* arg)
{
	Trace();
	cmdlist->d3d12_list->SetGraphicsRoot32BitConstants(arg->param_index, arg->count, arg->u32_args, arg->dest_offset);
}

API void
R4_CmdCopyBuffer(R4_Context* ctx, R4_CommandList* cmdlist, R4_Resource* dest, uint64 dest_offset, R4_Resource* source, uint64 source_offset, uint64 size)
{
	Trace();
	cmdlist->d3d12_list->CopyBufferRegion(dest->d3d12_resource, dest_offset, source->d3d12_resource, source_offset, size);
}

API void
R4_CmdBarrier(R4_Context* ctx, R4_CommandList *cmdlist, intsize barrier_count, const R4_ResourceBarrier *barriers)
{
	Trace();
	if (!barrier_count)
		return;
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	D3D12_RESOURCE_BARRIER* all_barriers = ArenaPushArray(scratch.arena, D3D12_RESOURCE_BARRIER, barrier_count);
	for (intsize i = 0; i < barrier_count; ++i)
	{
		R4_ResourceBarrier const* barrier = &barriers[i];
		if (!barrier->type)
			break;

		switch (barrier->type)
		{
			case R4_BarrierType_Transition:
			{
				R4_TransitionBarrier const* transition = &barrier->transition;
				all_barriers[i] = D3D12_RESOURCE_BARRIER {
					.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
					.Transition = {
						.pResource = barrier->resource->d3d12_resource,
						.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
						.StateBefore = D3d12StatesFromResourceStates_(transition->from),
						.StateAfter = D3d12StatesFromResourceStates_(transition->to),
					},
				};
			} break;
		}
	}
	cmdlist->d3d12_list->ResourceBarrier(barrier_count, all_barriers);
	ArenaRestore(scratch);
}
