#include "common.hpp"
#include "api.h"
#include "api_os.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "api_os_win32.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

static_assert(sizeof(void*) >= sizeof(uint64), "must");

#include "render4_handlepool.h"

struct ViewPool_
{
	Slice<int32> free_table;
	int32 free_count;
	int32 allocated_count;
};

struct R4_Context
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
	uint64 resc_descriptor_size;
	uint64 sampler_descriptor_size;
	R4_ContextInfo info;
	bool debug_layer_enabled;

	R4_OnErrorCallback* on_error;
	void* user_data;

	struct ID3D12Fence* fence;
	void* fence_event;
	uint64 fence_counter;

	struct ID3D12DescriptorHeap* rtv_heap;
	struct ID3D12DescriptorHeap* dsv_heap;
	struct ID3D12DescriptorHeap* sampler_heap;
	struct ID3D12DescriptorHeap* cbv_srv_uav_heap;

	ViewPool_ sampler_pool;
	ViewPool_ cbv_srv_uav_pool;

	CppHandlePool_<R4_D3D12_Queue> hpool_queue;
	CppHandlePool_<R4_D3D12_Buffer> hpool_buffer;
	CppHandlePool_<R4_D3D12_Image> hpool_image;
	CppHandlePool_<R4_D3D12_Heap> hpool_heap;
	CppHandlePool_<R4_D3D12_CommandAllocator> hpool_cmdalloc;
	CppHandlePool_<R4_D3D12_CommandList> hpool_cmdlist;
	CppHandlePool_<R4_D3D12_Pipeline> hpool_pipeline;
	CppHandlePool_<R4_D3D12_PipelineLayout> hpool_pipelayout;
	CppHandlePool_<R4_D3D12_BindLayout> hpool_bindlayout;
	CppHandlePool_<R4_D3D12_DescriptorHeap> hpool_descheap;
	CppHandlePool_<R4_D3D12_DescriptorSet> hpool_descset;
	CppHandlePool_<R4_D3D12_BufferView> hpool_bufferview;
	CppHandlePool_<R4_D3D12_ImageView> hpool_imageview;
	CppHandlePool_<R4_D3D12_Sampler> hpool_sampler;
	CppHandlePool_<R4_D3D12_RenderTargetView> hpool_rtv;
	CppHandlePool_<R4_D3D12_DepthStencilView> hpool_dsv;
}
typedef R4_Context;

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
CheckHr_(R4_Context* ctx, HRESULT hr, R4_Result* r)
{
	if (SUCCEEDED(hr))
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}
		
	Log(LOG_ERROR, "ERROR: HR: %u\n", (unsigned int)hr);
	R4_Result result;
	if (hr == DXGI_ERROR_DEVICE_HUNG ||
		hr == DXGI_ERROR_DEVICE_REMOVED ||
		hr == DXGI_ERROR_DEVICE_RESET ||
		hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
	{
		result = R4_Result_DeviceLost;
	}
	else if (hr == E_OUTOFMEMORY)
		result = R4_Result_DeviceOutOfMemory;
	else
		result = R4_Result_Failure;
	if (IsDebuggerPresent())
		Debugbreak();
	if (r)
		*r = result;
	if (ctx->on_error)
		ctx->on_error(ctx->user_data, result, r);
	return false;
}

static bool
CheckAllocatorErr_(R4_Context* ctx, AllocatorError err, R4_Result* r)
{
	if (!err)
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}

	Log(LOG_ERROR, "ERROR: ALLOCATOR: %u\n", (unsigned int)err);
	R4_Result result;
	if (err == AllocatorError_OutOfMemory)
		result = R4_Result_AllocatorOutOfMemory;
	else
		result = R4_Result_AllocatorError;
	if (r)
		*r = result;
	if (ctx->on_error)
		ctx->on_error(ctx->user_data, result, r);
	return false;
}

static bool
CheckHandle_(R4_Context* ctx, void* handle, R4_Result* r)
{
	if (handle)
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}

	Log(LOG_ERROR, "ERROR: could not allocate handle\n");
	R4_Result result;
	if (r)
		result = R4_Result_OutOfHandles;
	if (r)
		*r = result;
	if (ctx->on_error)
		ctx->on_error(ctx->user_data, result, r);
	return false;
}

static bool
CheckHandleCreation_(R4_Context* ctx, void* handle, R4_Result* r)
{
	if (handle)
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}

	Log(LOG_ERROR, "ERROR: could not allocate handle\n");
	if (r)
		*r = R4_Result_OutOfHandles;
	if (ctx->on_error)
		ctx->on_error(ctx->user_data, R4_Result_OutOfHandles, r);
	return false;
}

static bool
CheckHandleFetch_(R4_Context* ctx, void* impl, R4_Result* r)
{
	if (impl)
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}

	Log(LOG_ERROR, "ERROR: could not fetch handle\n");
	if (r)
		*r = R4_Result_InvalidHandle;
	if (ctx->on_error)
		ctx->on_error(ctx->user_data, R4_Result_InvalidHandle, r);
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
		case R4_ResourceState_Preinitialized: result = D3D12_RESOURCE_STATE_GENERIC_READ; break;
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
		default: Unreachable(); break;
		case R4_LogicOp_Null: break;
	}

	return result;
}

static D3D12_CULL_MODE
D3d12CullModeFromCullMode_(R4_CullMode mode)
{
	D3D12_CULL_MODE result = (D3D12_CULL_MODE)0;

	switch (mode)
	{
		default: Unreachable(); break;
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
		default: Unreachable(); break;
		case R4_ComparisonFunc_Null: break;
		case R4_ComparisonFunc_Never: result = D3D12_COMPARISON_FUNC_NEVER; break;
		case R4_ComparisonFunc_Always: result = D3D12_COMPARISON_FUNC_ALWAYS; break;
		case R4_ComparisonFunc_Equal: result = D3D12_COMPARISON_FUNC_EQUAL; break;
		case R4_ComparisonFunc_NotEqual: result = D3D12_COMPARISON_FUNC_NOT_EQUAL; break;
		case R4_ComparisonFunc_Less: result = D3D12_COMPARISON_FUNC_LESS; break;
		case R4_ComparisonFunc_LessEqual: result = D3D12_COMPARISON_FUNC_LESS_EQUAL; break;
		case R4_ComparisonFunc_Greater: result = D3D12_COMPARISON_FUNC_GREATER; break;
		case R4_ComparisonFunc_GreaterEqual: result = D3D12_COMPARISON_FUNC_GREATER_EQUAL; break;
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

static D3D12_PRIMITIVE_TOPOLOGY
D3d12TopologyFromPrimitiveType_(R4_PrimitiveType type)
{
	D3D12_PRIMITIVE_TOPOLOGY result = (D3D12_PRIMITIVE_TOPOLOGY)0;

	switch (type)
	{
		default: Unreachable(); break;
		case R4_PrimitiveType_PointList: result = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case R4_PrimitiveType_LineList: result = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case R4_PrimitiveType_LineStrip: result = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
		case R4_PrimitiveType_TriangleList: result = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case R4_PrimitiveType_TriangleStrip: result = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
		case R4_PrimitiveType_TriangleFan: result = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ; break;
		case R4_PrimitiveType_Patch: /* TODO(ljre) */ break;
	}

	return result;
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE
D3d12TopologyTypeFromPrimitiveType_(R4_PrimitiveType type)
{
	D3D12_PRIMITIVE_TOPOLOGY_TYPE result = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)0;

	switch (type)
	{
		default: Unreachable(); break;
		case R4_PrimitiveType_PointList: result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
		case R4_PrimitiveType_LineList:
		case R4_PrimitiveType_LineStrip: result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
		case R4_PrimitiveType_TriangleList:
		case R4_PrimitiveType_TriangleStrip:
		case R4_PrimitiveType_TriangleFan: result = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
		case R4_PrimitiveType_Patch: /* TODO(ljre) */ break;
	}

	return result;
}

static D3D12_FILTER
D3d12FilterFromFilters_(R4_Filter min, R4_Filter mag, R4_Filter mip)
{
	if (min == R4_Filter_Anisotropic || mag == R4_Filter_Anisotropic || mip == R4_Filter_Anisotropic)
		return D3D12_FILTER_ANISOTROPIC;

	static D3D12_FILTER const nearest_min_mag[] = {
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
	};
	static D3D12_FILTER const nearest_min_linear_mag[] = {
		D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
		D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR,
	};
	static D3D12_FILTER const linear_min_nearest_mag[] = {
		D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,
		D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	};
	static D3D12_FILTER const linear_min_mag[] = {
		D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	};

	D3D12_FILTER result = (D3D12_FILTER)0;
	if (min == R4_Filter_Nearest && mag == R4_Filter_Nearest)
		result = nearest_min_mag[mip];
	else if (min == R4_Filter_Nearest && mag == R4_Filter_Linear)
		result = nearest_min_linear_mag[mip];
	else if (min == R4_Filter_Linear && mag == R4_Filter_Nearest)
		result = linear_min_nearest_mag[mip];
	else if (min == R4_Filter_Linear && mag == R4_Filter_Linear)
		result = linear_min_mag[mip];
	else
		Unreachable();
	return result;
}

static D3D12_TEXTURE_ADDRESS_MODE
D3d12AddressModeFromAddressMode_(R4_AddressMode mode)
{
	D3D12_TEXTURE_ADDRESS_MODE result = (D3D12_TEXTURE_ADDRESS_MODE)0;

	switch (mode)
	{
		default: Unreachable(); break;
		case R4_AddressMode_Wrap: result = D3D12_TEXTURE_ADDRESS_MODE_WRAP; break;
		case R4_AddressMode_Clamp: result = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; break;
		case R4_AddressMode_Mirror: result = D3D12_TEXTURE_ADDRESS_MODE_MIRROR; break;
		case R4_AddressMode_MirrorOnce: result = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE; break;
		case R4_AddressMode_Border: result = D3D12_TEXTURE_ADDRESS_MODE_BORDER; break;
	}

	return result;
}

static D3D12_COMMAND_LIST_TYPE
D3d12CommandListTypeFromType_(R4_CommandListType type)
{
	D3D12_COMMAND_LIST_TYPE result = (D3D12_COMMAND_LIST_TYPE)0;

	switch (type)
	{
		default: Unreachable(); break;
		case R4_CommandListType_Graphics: result = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
		case R4_CommandListType_Compute: result = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
		case R4_CommandListType_Copy: result = D3D12_COMMAND_LIST_TYPE_COPY; break;
	}

	return result;
}

static int64
RowPitchFromWidthAndFormat_(int64 width, R4_Format format)
{
	int64 block_size = 0;
	int64 block_period = 1;

	switch (format)
	{
		case R4_Format_Null: block_size = 0; break;

		case R4_Format_R8_UNorm: block_size = 1; break;
		case R4_Format_R8G8_UNorm: block_size = 2; break;
		case R4_Format_R8G8B8A8_UNorm: block_size = 4; break;
		case R4_Format_R8G8B8A8_SRgb: block_size = 4; break;
		case R4_Format_R8_UInt: block_size = 1; break;
		case R4_Format_R8G8_UInt: block_size = 2; break;
		case R4_Format_R8G8B8A8_UInt: block_size = 4; break;

		case R4_Format_R16_SNorm: block_size = 2; break;
		case R4_Format_R16G16_SNorm: block_size = 4; break;
		case R4_Format_R16G16B16A16_SNorm: block_size = 8; break;
		case R4_Format_R16_SInt: block_size = 2; break;
		case R4_Format_R16G16_SInt: block_size = 4; break;
		case R4_Format_R16G16B16A16_SInt: block_size = 8; break;

		case R4_Format_R32_UInt: block_size = 4; break;

		case R4_Format_R32_Float: block_size = 4; break;
		case R4_Format_R32G32_Float: block_size = 8; break;
		case R4_Format_R32G32B32_Float: block_size = 12; break;
		case R4_Format_R32G32B32A32_Float: block_size = 16; break;
		case R4_Format_R16G16_Float: block_size = 4; break;
		case R4_Format_R16G16B16A16_Float: block_size = 8; break;

		case R4_Format_D16_UNorm: block_size = 2; break;
		case R4_Format_D24_UNorm_S8_UInt: block_size = 4; break;
	}

	return (width + block_period-1) / block_period * block_size;
}

static ID3D12Resource*
D3d12ResourceFromResource_(R4_Context* ctx, R4_Resource resource)
{
	ID3D12Resource* result = NULL;

	if (resource.buffer)
	{
		R4_D3D12_Buffer* impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, resource.buffer, NULL);
		if (impl)
			result = impl->resource;
	}
	else if (resource.image)
	{
		R4_D3D12_Image* impl = FetchFromCppHandlePool_(&ctx->hpool_image, resource.image, NULL);
		if (impl)
			result = impl->resource;
	}
	
	return result;
}

static D3D12_RESOURCE_DESC
D3d12ResourceDescFromImageDesc_(R4_ImageDesc const* desc)
{
	D3D12_RESOURCE_FLAGS flags = (D3D12_RESOURCE_FLAGS)0;
	if (desc->usage_flags & R4_ResourceUsageFlag_ShaderReadWrite)
		flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (desc->usage_flags & R4_ResourceUsageFlag_RenderTarget)
		flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (desc->usage_flags & R4_ResourceUsageFlag_DepthStencil)
		flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	// if (desc->usage & (R4_ResourceUsageFlag_ShaderStorage | R4_ResourceUsageFlag_SampledImage))
		// flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	SafeAssert(desc->width >= 0);
	SafeAssert(desc->height >= 0 && desc->height <= UINT32_MAX);
	SafeAssert(desc->depth >= 0 && desc->depth <= UINT16_MAX);
	SafeAssert(desc->mip_levels >= 0 && desc->mip_levels <= UINT16_MAX);
	D3D12_RESOURCE_DESC result = {
		.Dimension =
			(desc->dimension == R4_ImageDimension_Texture1D) ? D3D12_RESOURCE_DIMENSION_TEXTURE1D :
			(desc->dimension == R4_ImageDimension_Texture2D) ? D3D12_RESOURCE_DIMENSION_TEXTURE2D :
			(desc->dimension == R4_ImageDimension_Texture3D) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D :
			D3D12_RESOURCE_DIMENSION_UNKNOWN,
		.Width = (UINT64)desc->width,
		.Height = desc->height ? (UINT)desc->height : 1,
		.DepthOrArraySize = (UINT16)(desc->depth ? desc->depth : 1),
		.MipLevels = (UINT16)(desc->mip_levels ? desc->mip_levels : 1),
		.Format = DxgiFormatFromFormat_(desc->format),
		.SampleDesc = {
			.Count = 1,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = flags,
	};
	return result;
}

static D3D12_RESOURCE_DESC
D3d12ResourceDescFromBufferDesc_(R4_BufferDesc const* desc)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	if (desc->usage_flags & R4_ResourceUsageFlag_ShaderReadWrite)
		flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (desc->usage_flags & R4_ResourceUsageFlag_ShaderStorage)
		flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	SafeAssert(desc->size >= 0);
	D3D12_RESOURCE_DESC result = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = (UINT64)desc->size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = {
			.Count = 1,
		},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = flags,
	};
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
	R4_Context* d3d12 = AllocatorNew<R4_Context>(&allocator, &err);
	if (err || !d3d12)
	{
		if (r)
			*r = R4_Result_AllocatorOutOfMemory;
		return NULL;
	}
	d3d12->allocator = allocator;
	d3d12->on_error = desc->on_error;
	d3d12->user_data = desc->user_data;

	{
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

		d3d12->hpool_heap = InitCppHandlePool_<R4_D3D12_Heap>(1024, allocator, &err);
		if (!CheckAllocatorErr_(d3d12, err, r))
			goto lbl_error;
		d3d12->hpool_rtv = InitCppHandlePool_<R4_D3D12_RenderTargetView>(max_rtvs, allocator, &err);
		if (!CheckAllocatorErr_(d3d12, err, r))
			goto lbl_error;
		d3d12->hpool_dsv = InitCppHandlePool_<R4_D3D12_DepthStencilView>(max_dsvs, allocator, &err);
		if (!CheckAllocatorErr_(d3d12, err, r))
			goto lbl_error;
		d3d12->hpool_buffer = InitCppHandlePool_<R4_D3D12_Buffer>(1024*32, allocator, &err);
		if (!CheckAllocatorErr_(d3d12, err, r))
			goto lbl_error;
		d3d12->hpool_image = InitCppHandlePool_<R4_D3D12_Image>(1024*32, allocator, &err);
		if (!CheckAllocatorErr_(d3d12, err, r))
			goto lbl_error;

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
		if (!CheckHr_(d3d12, hr, r))
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
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;
		hr = d3d12->device->QueryInterface(IID_PPV_ARGS(&d3d12->device2));
		if (!CheckHr_(d3d12, hr, r))
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
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;

		HWND hwnd = OS_W32_HwndFromWindow(*desc->os_window);
		SafeAssert(desc->backbuffer_count >= 0 && desc->backbuffer_count <= 8);
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
			.Format = DxgiFormatFromFormat_(desc->backbuffer_format),
			.SampleDesc = {
				.Count = 1,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = (UINT)desc->backbuffer_count,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		};
		hr = d3d12->factory4->CreateSwapChainForHwnd(d3d12->direct_queue, hwnd, &swapchain_desc, NULL, NULL, &d3d12->swapchain1);
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;
		hr = d3d12->swapchain1->QueryInterface(IID_PPV_ARGS(&d3d12->swapchain3));
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;

		hr = d3d12->factory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
		if (FAILED(hr))
			OS_LogErr("WARNING: MakeWindowAssociation failed (0x%x)\n", (uint32)hr);

		for (intz i = 0; i < desc->backbuffer_count; ++i)
		{
			ID3D12Resource* backbuffer = NULL;
			hr = d3d12->swapchain3->GetBuffer(i, IID_PPV_ARGS(&backbuffer));
			if (!CheckHr_(d3d12, hr, r))
				goto lbl_error;
			R4_D3D12_Image* impl = AllocateFromCppHandlePool_(&d3d12->hpool_image, &desc->out_backbuffer_images[i], NULL);
			if (!CheckHandleCreation_(d3d12, impl, r))
				goto lbl_error;
			impl->resource = backbuffer;
		}

		hr = d3d12->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12->fence));
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;
		d3d12->fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
		if (!d3d12->fence_event)
		{
			CheckHr_(d3d12, E_OUTOFMEMORY, r);
			goto lbl_error;
		}

		*desc->out_graphics_queue = { .d3d12 = { d3d12->direct_queue, D3D12_COMMAND_LIST_TYPE_DIRECT } };
		if (desc->out_compute_queue)
		{
			D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {
				.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
			};
			hr = d3d12->device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&d3d12->compute_queue));
			if (!CheckHr_(d3d12, hr, r))
				goto lbl_error;
			*desc->out_compute_queue = { .d3d12 = { d3d12->compute_queue, D3D12_COMMAND_LIST_TYPE_COMPUTE } };
		}
		if (desc->out_copy_queue)
		{
			D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {
				.Type = D3D12_COMMAND_LIST_TYPE_COPY,
			};
			hr = d3d12->device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&d3d12->copy_queue));
			if (!CheckHr_(d3d12, hr, r))
				goto lbl_error;
			*desc->out_copy_queue = { .d3d12 = { d3d12->copy_queue, D3D12_COMMAND_LIST_TYPE_COPY } };
		}

		d3d12->resc_descriptor_size = d3d12->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		d3d12->sampler_descriptor_size = d3d12->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = (UINT)max_rtvs,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&d3d12->rtv_heap));
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;

		D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			.NumDescriptors = (UINT)max_dsvs,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&d3d12->dsv_heap));
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;

		D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			.NumDescriptors = (UINT)max_samplers,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&sampler_heap_desc, IID_PPV_ARGS(&d3d12->sampler_heap));
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;

		SafeAssert(max_image_views >= 0 && max_buffer_views >= 0);
		D3D12_DESCRIPTOR_HEAP_DESC cbv_srv_uav_heap_desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = (UINT)max_image_views + (UINT)max_buffer_views,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};
		hr = d3d12->device->CreateDescriptorHeap(&cbv_srv_uav_heap_desc, IID_PPV_ARGS(&d3d12->cbv_srv_uav_heap));
		if (!CheckHr_(d3d12, hr, r))
			goto lbl_error;

		d3d12->sampler_pool = InitViewPool_(max_samplers, allocator, &err);
		if (!CheckAllocatorErr_(d3d12, err, r))
			goto lbl_error;
		d3d12->cbv_srv_uav_pool = InitViewPool_(max_image_views + max_buffer_views, allocator, &err);
		if (!CheckAllocatorErr_(d3d12, err, r))
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

	for (intz i = 0; i < ctx->hpool_image.allocated_count; ++i)
	{
		R4_D3D12_Image* impl = FetchByIndexFromCppHandlePool_(&ctx->hpool_image, i);
		if (impl && impl->resource)
			impl->resource->Release();
	}
	for (intz i = 0; i < ctx->hpool_buffer.allocated_count; ++i)
	{
		R4_D3D12_Buffer* impl = FetchByIndexFromCppHandlePool_(&ctx->hpool_buffer, i);
		if (impl && impl->resource)
			impl->resource->Release();
	}
	for (intz i = 0; i < ctx->hpool_heap.allocated_count; ++i)
	{
		R4_D3D12_Heap* impl = FetchByIndexFromCppHandlePool_(&ctx->hpool_heap, i);
		if (impl && impl->heap)
			impl->heap->Release();
	}
	FreeViewPool_(&ctx->cbv_srv_uav_pool, ctx->allocator, &err);
	FreeViewPool_(&ctx->sampler_pool, ctx->allocator, &err);
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
		if (err)
		{
			if (r)
				*r = R4_Result_AllocatorError;
			return;
		}
	}
}

API void
R4_PresentAndSync(R4_Context* ctx, R4_Result* r, int32 sync_interval)
{
	Trace();
	HRESULT hr = ctx->swapchain3->Present((UINT)sync_interval, 0);
	CheckHr_(ctx, hr, r);
}

API intz
R4_AcquireNextBackbuffer(R4_Context* ctx, R4_Result* r)
{
	Trace();
	HRESULT hr;
	intz backbuffer_index = ctx->swapchain3->GetCurrentBackBufferIndex();

	ctx->fence_counter += 1;
	hr = ctx->direct_queue->Signal(ctx->fence, ctx->fence_counter);
	if (!CheckHr_(ctx, hr, r))
		return 0;
	if (ctx->fence->GetCompletedValue() < ctx->fence_counter)
	{
		hr = ctx->fence->SetEventOnCompletion(ctx->fence_counter, ctx->fence_event);
		if (!CheckHr_(ctx, hr, r))
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
	if (!CheckAllocatorErr_(ctx, err, r))
		return;
	for (intz i = 0; i < lists.count; ++i)
		lists[i] = cmdlists[i]->d3d12.list;
	queue->d3d12.queue->ExecuteCommandLists(lists.count, &lists[0]);
	AllocatorDeleteSlice(&ctx->allocator, lists, &err);
	if (!CheckAllocatorErr_(ctx, err, r))
		return;
}

API void
R4_WaitRemainingWorkOnQueue(R4_Context* ctx, R4_Result* r, R4_Queue* queue)
{
	Trace();
	HRESULT hr;

	HANDLE tmp_event = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!tmp_event)
	{
		CheckHr_(ctx, E_OUTOFMEMORY, r);
		return;
	}
	ID3D12Fence* tmp_fence;
	hr = ctx->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&tmp_fence));
	if (!CheckHr_(ctx, hr, r))
	{
		CloseHandle(tmp_event);
		return;
	}

	hr = queue->d3d12.queue->Signal(tmp_fence, 1);
	if (!CheckHr_(ctx, hr, r))
	{
		tmp_fence->Release();
		CloseHandle(tmp_event);
		return;
	}

	if (tmp_fence->GetCompletedValue() < 1)
	{
		hr = tmp_fence->SetEventOnCompletion(1, tmp_event);
		if (!CheckHr_(ctx, hr, r))
		{
			tmp_fence->Release();
			CloseHandle(tmp_event);
			return;
		}
		DWORD wait_result = WaitForSingleObject(tmp_event, INFINITE);
		if (wait_result != WAIT_OBJECT_0)
			CheckHr_(ctx, ERROR_WAIT_1, r);
	}
	tmp_fence->Release();
	CloseHandle(tmp_event);
}

// =============================================================================
// =============================================================================
// Heap management
API R4_Heap
R4_MakeHeap(R4_Context* ctx, R4_Result* r, R4_HeapType type, int64 size)
{
	Trace();
	SafeAssert(size >= 0);
	HRESULT hr;
	R4_Heap handle = NULL;
	R4_D3D12_Heap* impl = AllocateFromCppHandlePool_(&ctx->hpool_heap, &handle, NULL);
	if (!CheckHandleCreation_(ctx, handle, r))
		return NULL;

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
	if (!CheckHr_(ctx, hr, r))
	{
		FreeFromHandlePool_(&ctx->hpool_queue, handle);
		return NULL;
	}

	impl->heap = heap;
	return handle;
}

API void
R4_FreeHeap(R4_Context* ctx, R4_Heap heap)
{
	Trace();
	R4_D3D12_Heap* heap_impl = FetchFromCppHandlePool_(&ctx->hpool_heap, heap, NULL);
	if (heap_impl)
	{
		heap_impl->heap->Release();
		FreeFromHandlePool_(&ctx->hpool_heap, heap);
	}
}

// =============================================================================
// =============================================================================
// Buffers & Images
API R4_Buffer
R4_MakeUploadBuffer(R4_Context* ctx, R4_Result* r, R4_UploadBufferDesc const* desc)
{
	Trace();
	SafeAssert(desc->heap_offset >= 0);
	HRESULT hr;
	R4_Buffer handle = NULL;
	R4_D3D12_Buffer* impl = AllocateFromCppHandlePool_(&ctx->hpool_buffer, &handle, NULL);

	R4_D3D12_Heap* heap_impl = FetchFromCppHandlePool_(&ctx->hpool_heap, desc->heap, NULL);
	if (!CheckHandle_(ctx, heap_impl, r))
	{
		FreeFromHandlePool_(&ctx->hpool_buffer, handle);
		return NULL;
	}

	D3D12_RESOURCE_DESC resource_desc = D3d12ResourceDescFromBufferDesc_(&desc->buffer_desc);
	hr = ctx->device->CreatePlacedResource(
		heap_impl->heap,
		(UINT64)desc->heap_offset,
		&resource_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&impl->resource));
	if (!CheckHr_(ctx, hr, r))
	{
		FreeFromHandlePool_(&ctx->hpool_buffer, handle);
		return {};
	}

	return handle;
}

API R4_Buffer
R4_MakePlacedBuffer(R4_Context* ctx, R4_Result* r, R4_PlacedBufferDesc const* desc)
{
	Trace();
	SafeAssert(desc->heap_offset >= 0);
	HRESULT hr;
	R4_Buffer handle = NULL;
	R4_D3D12_Buffer* impl = AllocateFromCppHandlePool_(&ctx->hpool_buffer, &handle, NULL);

	R4_D3D12_Heap* heap_impl = FetchFromCppHandlePool_(&ctx->hpool_heap, desc->heap, NULL);
	if (!CheckHandle_(ctx, heap_impl, r))
	{
		FreeFromHandlePool_(&ctx->hpool_buffer, handle);
		return NULL;
	}

	D3D12_RESOURCE_DESC resource_desc = D3d12ResourceDescFromBufferDesc_(&desc->buffer_desc);
	hr = ctx->device->CreatePlacedResource(
		heap_impl->heap,
		(UINT64)desc->heap_offset,
		&resource_desc,
		D3d12StatesFromResourceStates_(desc->initial_state),
		NULL,
		IID_PPV_ARGS(&impl->resource));
	if (!CheckHr_(ctx, hr, r))
	{
		FreeFromHandlePool_(&ctx->hpool_buffer, handle);
		return NULL;
	}
	return handle;
}

API void
R4_FreeBuffer(R4_Context* ctx, R4_Buffer buffer)
{
	Trace();
	R4_D3D12_Buffer* impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, buffer, NULL);
	if (impl)
	{
		impl->resource->Release();
		FreeFromHandlePool_(&ctx->hpool_buffer, buffer);
	}
}

API R4_Image
R4_MakePlacedImage(R4_Context* ctx, R4_Result* r, R4_PlacedImageDesc const* desc)
{
	Trace();
	SafeAssert(desc->heap_offset >= 0);
	HRESULT hr;
	R4_Image handle = NULL;
	R4_D3D12_Image* impl = AllocateFromCppHandlePool_(&ctx->hpool_image, &handle, NULL);
	if (!CheckHandleCreation_(ctx, handle, r))
		return NULL;

	R4_D3D12_Heap* heap_impl = FetchFromCppHandlePool_(&ctx->hpool_heap, desc->heap, NULL);
	if (!CheckHandleFetch_(ctx, heap_impl, r))
	{
		FreeFromHandlePool_(&ctx->hpool_image, handle);
		return NULL;
	}
	
	D3D12_RESOURCE_DESC resource_desc = D3d12ResourceDescFromImageDesc_(&desc->image_desc);
	D3D12_CLEAR_VALUE clear_value = {
		.Format = DxgiFormatFromFormat_(desc->clear_format),
		.Color = {
			desc->clear_color[0],
			desc->clear_color[1],
			desc->clear_color[2],
			desc->clear_color[3],
		},
	};
	if (clear_value.Format && IsDepthStencilFormat_(clear_value.Format))
	{
		clear_value.DepthStencil = {
			.Depth = desc->clear_depth,
			.Stencil = (UINT8)desc->clear_stencil,
		};
	}
	hr = ctx->device->CreatePlacedResource(
		heap_impl->heap,
		(UINT64)desc->heap_offset,
		&resource_desc,
		D3d12StatesFromResourceStates_(desc->initial_state),
		clear_value.Format ? &clear_value : NULL,
		IID_PPV_ARGS(&impl->resource));
	if (!CheckHr_(ctx, hr, r))
	{
		FreeFromHandlePool_(&ctx->hpool_image, handle);
		return NULL;
	}
	return handle;
}

API void
R4_FreeImage(R4_Context* ctx, R4_Image image)
{
	Trace();
	R4_D3D12_Image* impl = FetchFromCppHandlePool_(&ctx->hpool_image, image, NULL);
	if (impl)
	{
		impl->resource->Release();
		FreeFromHandlePool_(&ctx->hpool_image, image);
	}
}

API R4_MemoryRequirements
R4_GetMemoryRequirementsFromBufferDesc(R4_Context *ctx, R4_BufferDesc const* desc)
{
	Trace();
	D3D12_RESOURCE_DESC resource_desc = D3d12ResourceDescFromBufferDesc_(desc);
	D3D12_RESOURCE_ALLOCATION_INFO allocation_info = ctx->device->GetResourceAllocationInfo(1, 1, &resource_desc);
	SafeAssert(allocation_info.Alignment <= INT64_MAX);
	SafeAssert(allocation_info.SizeInBytes <= INT64_MAX);
	return {
		.size = (int64)allocation_info.SizeInBytes,
		.alignment = (int64)allocation_info.Alignment,
	};
}

API R4_MemoryRequirements R4_GetMemoryRequirementsFromBuffer(R4_Context* ctx, R4_Buffer* buffer);

API R4_MemoryRequirements
R4_GetMemoryRequirementsFromImageDesc(R4_Context *ctx, R4_ImageDesc const* desc)
{
	Trace();
	D3D12_RESOURCE_DESC resource_desc = D3d12ResourceDescFromImageDesc_(desc);
	D3D12_RESOURCE_ALLOCATION_INFO allocation_info = ctx->device->GetResourceAllocationInfo(1, 1, &resource_desc);
	SafeAssert(allocation_info.Alignment <= INT64_MAX);
	SafeAssert(allocation_info.SizeInBytes <= INT64_MAX);
	return {
		.size = (int64)allocation_info.SizeInBytes,
		.alignment = (int64)allocation_info.Alignment,
	};
}

API R4_MemoryRequirements R4_GetMemoryRequirementsFromImage(R4_Context *ctx, R4_Image* image);

API void
R4_MapResource(R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource, intz size, void** out_memory)
{
	Trace();
	SafeAssert(subresource >= 0);
	HRESULT hr;
	ID3D12Resource* resc = D3d12ResourceFromResource_(ctx, resource);
	SafeAssert(resc);
	hr = resc->Map((UINT)subresource, NULL, out_memory);
	if (!CheckHr_(ctx, hr, r))
		return;
}

API void
R4_UnmapResource(R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource)
{
	Trace();
	SafeAssert(subresource >= 0);
	ID3D12Resource* resc = D3d12ResourceFromResource_(ctx, resource);
	SafeAssert(resc);
	resc->Unmap((UINT)subresource, NULL);
	CheckHr_(ctx, ERROR_SUCCESS, r);
}

// =============================================================================
// =============================================================================
// Buffer views & Image views
API R4_ImageView
R4_MakeImageView(R4_Context* ctx, R4_Result* r, R4_ImageViewDesc const* desc)
{
	Trace();
	R4_D3D12_Image* image_impl = FetchFromCppHandlePool_(&ctx->hpool_image, desc->image, NULL);
	if (!CheckHandleFetch_(ctx, desc->image, r))
		return {};

	intz index = AllocateFromViewPool_(&ctx->cbv_srv_uav_pool);
	SafeAssert(index >= 0);
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uint64 heap_start = ctx->cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = heap_start + (uint64)index * increment;

	if (desc->type == R4_DescriptorType_ImageSampled ||
		desc->type == R4_DescriptorType_ImageShaderStorage)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
			.Format = DxgiFormatFromFormat_(desc->format),
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = {
				.MipLevels = (UINT)-1,
			},
		};
		ctx->device->CreateShaderResourceView(image_impl->resource, &srv_desc, {ptr});
	}
	else if (desc->type == R4_DescriptorType_ImageShaderReadWrite)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
			.Format = DxgiFormatFromFormat_(desc->format),
			.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
			.Texture2D = {},
		};
		ctx->device->CreateUnorderedAccessView(image_impl->resource, NULL, &uav_desc, {ptr});
	}
	else
		Unreachable();

	return {
		.d3d12 = { ptr },
	};
}

API void
R4_FreeImageView(R4_Context* ctx, R4_ImageView* image_view)
{
	Trace();
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uint64 heap_start = ctx->cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = image_view->d3d12.srv_uav_ptr;
	SafeAssert(ptr >= heap_start);
	ptr -= heap_start;
	ptr /= increment;
	SafeAssert(ptr >= 0 && ptr <= INT32_MAX);
	FreeFromViewPool_(&ctx->cbv_srv_uav_pool, (int32)ptr);

	*image_view = {};
}

// =============================================================================
// =============================================================================
// Render target views & depth stencil views
API R4_RenderTargetView
R4_MakeRenderTargetView(R4_Context* ctx, R4_Result* r, R4_RenderTargetViewDesc const* desc)
{
	Trace();
	R4_D3D12_Image* image_impl = FetchFromCppHandlePool_(&ctx->hpool_image, desc->image, NULL);
	if (!CheckHandleFetch_(ctx, desc->image, r))
		return {};
	R4_RenderTargetView handle = NULL;
	intz index;
	R4_D3D12_RenderTargetView* impl = AllocateFromCppHandlePool_(&ctx->hpool_rtv, &handle, &index);
	if (!CheckHandle_(ctx, handle, r))
		return NULL;
	SafeAssert(index >= 0);
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	uint64 heap_start = ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = heap_start + (uint64)index * increment;
	ctx->device->CreateRenderTargetView(image_impl->resource, NULL, {ptr});
	impl->rtv_ptr = ptr;
	return handle;
}

API void
R4_FreeRenderTargetView(R4_Context* ctx, R4_RenderTargetView render_target_view)
{
	Trace();
	void* impl = FetchFromCppHandlePool_(&ctx->hpool_rtv, render_target_view, NULL);
	if (impl)
		FreeFromHandlePool_(&ctx->hpool_rtv, render_target_view);
}

API R4_DepthStencilView
R4_MakeDepthStencilView(R4_Context* ctx, R4_Result* r, R4_DepthStencilViewDesc const* desc)
{
	Trace();
	R4_D3D12_Image* image_impl = FetchFromCppHandlePool_(&ctx->hpool_image, desc->image, NULL);
	if (!CheckHandleFetch_(ctx, desc->image, r))
		return {};
	R4_DepthStencilView handle = NULL;
	intz index;
	R4_D3D12_DepthStencilView* impl = AllocateFromCppHandlePool_(&ctx->hpool_dsv, &handle, &index);
	if (!CheckHandle_(ctx, handle, r))
		return NULL;
	SafeAssert(index >= 0);
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	uint64 heap_start = ctx->dsv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = heap_start + (uint64)index * increment;
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
		.Format = DxgiFormatFromFormat_(desc->format),
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Flags = D3D12_DSV_FLAG_NONE,
		.Texture2D = {},
	};
	ctx->device->CreateDepthStencilView(image_impl->resource, &dsv_desc, {ptr});
	impl->dsv_ptr = ptr;
	return handle;
}

API void
R4_FreeDepthStencilView(R4_Context* ctx, R4_DepthStencilView depth_stencil_view)
{
	Trace();
	void* impl = FetchFromCppHandlePool_(&ctx->hpool_rtv, depth_stencil_view, NULL);
	if (impl)
		FreeFromHandlePool_(&ctx->hpool_rtv, depth_stencil_view);
}

// =============================================================================
// =============================================================================
// Samplers
API R4_Sampler
R4_MakeSampler(R4_Context* ctx, R4_Result* r, R4_SamplerDesc const* desc)
{
	Trace();
	int32 index = AllocateFromViewPool_(&ctx->sampler_pool);
	SafeAssert(index >= 0);
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	uint64 heap_start = ctx->sampler_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = heap_start + (uint64)index * increment;
	D3D12_SAMPLER_DESC sampler_desc = {
		.Filter = D3d12FilterFromFilters_(desc->min_filter, desc->mag_filter, desc->mip_filter),
		.AddressU = D3d12AddressModeFromAddressMode_(desc->addr_u),
		.AddressV = D3d12AddressModeFromAddressMode_(desc->addr_v),
		.AddressW = D3d12AddressModeFromAddressMode_(desc->addr_w),
		.MipLODBias = desc->mip_lod_bias,
		.MaxAnisotropy = (UINT)desc->max_anisotropy,
		.ComparisonFunc = D3d12CompFuncFromCompFunc_(desc->comparison_func),
		.MinLOD = desc->min_lod,
		.MaxLOD = desc->max_lod,
	};
	ctx->device->CreateSampler(&sampler_desc, {ptr});
	return { .d3d12 = { ptr } };
}

API void
R4_FreeSampler(R4_Context* ctx, R4_Sampler* sampler)
{
	Trace();
	uint64 increment = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	uint64 heap_start = ctx->sampler_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	uint64 ptr = sampler->d3d12.sampler_ptr;
	SafeAssert(ptr >= heap_start);
	ptr -= heap_start;
	ptr /= increment;
	SafeAssert(ptr >= 0 && ptr <= INT32_MAX);
	FreeFromViewPool_(&ctx->sampler_pool, (int32)ptr);

	*sampler = {};
}

// =============================================================================
// =============================================================================
// Bind Layout
API R4_BindLayout
R4_MakeBindLayout(R4_Context* ctx, R4_Result* r, R4_BindLayoutDesc const* desc)
{
	Trace();
	R4_D3D12_BindLayout layout = {
		.visibility = (uint32)D3d12VisibilityFromVisibility_(desc->shader_visibility),
		.sampler_entry_count = 0,
		.resc_entry_count = 0,
		.entries = {},
	};
	intz entry_count = 0;
	static_assert(ArrayLength(desc->entries) == ArrayLength(layout.entries), "these should be the same");

	int32 sampler_offset_from_table_start = 0;
	for (intz i = 0; i < ArrayLength(desc->entries); ++i)
	{
		R4_BindLayoutEntry const* entry = &desc->entries[i];
		if (!entry->descriptor_type)
			break;
		if (entry->descriptor_type != R4_DescriptorType_Sampler)
			continue;
		SafeAssert(entry->descriptor_count >= 0);
		SafeAssert(entry->start_shader_slot >= 0);

		int32 offset = sampler_offset_from_table_start;
		sampler_offset_from_table_start += entry->descriptor_count;
		SafeAssert(offset >= 0);
		layout.entries[entry_count++] = {
			.type = (uint16)D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
			.first_slot = (uint16)entry->start_shader_slot,
			.count = (uint32)entry->descriptor_count,
			.offset_from_table_start = (uint32)offset,
		};
		++layout.sampler_entry_count;
	}

	int32 resc_offset_from_table_start = 0;
	for (intz i = 0; i < ArrayLength(desc->entries); ++i)
	{
		R4_BindLayoutEntry const* entry = &desc->entries[i];
		if (!entry->descriptor_type)
			break;
		SafeAssert(entry->descriptor_count >= 0);
		SafeAssert(entry->start_shader_slot >= 0);
		if (entry->descriptor_type == R4_DescriptorType_Sampler)
			continue;
		D3D12_DESCRIPTOR_RANGE_TYPE range_type = (D3D12_DESCRIPTOR_RANGE_TYPE)0;
		switch (entry->descriptor_type)
		{
			default: Unreachable(); break;
			case R4_DescriptorType_Sampler: range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
			case R4_DescriptorType_ImageSampled:
			case R4_DescriptorType_ImageShaderStorage:
			case R4_DescriptorType_BufferShaderStorage:
			case R4_DescriptorType_DynamicBufferShaderStorage: range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
			case R4_DescriptorType_ImageShaderReadWrite:
			case R4_DescriptorType_BufferShaderReadWrite:
			case R4_DescriptorType_DynamicBufferShaderReadWrite: range_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
			case R4_DescriptorType_UniformBuffer:
			case R4_DescriptorType_DynamicUniformBuffer: range_type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
		}

		int32 offset = resc_offset_from_table_start;
		resc_offset_from_table_start += entry->descriptor_count;
		SafeAssert(offset >= 0);
		layout.entries[entry_count++] = {
			.type = (uint16)range_type,
			.first_slot = (uint16)entry->start_shader_slot,
			.count = (uint32)entry->descriptor_count,
			.offset_from_table_start = (uint32)offset,
		};
		++layout.resc_entry_count;
	}

	return {
		.d3d12 = layout,
	};
}

API void
R4_FreeBindLayout(R4_Context* ctx, R4_BindLayout* bind_layout)
{
	Trace();
	*bind_layout = {};
}

// =============================================================================
// =============================================================================
// Pipeline Layout
API R4_PipelineLayout
R4_MakePipelineLayout(R4_Context* ctx, R4_Result* r, R4_PipelineLayoutDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID3D12RootSignature* rootsig = NULL;
	ID3D10Blob* rootsig_blob = NULL;

	D3D12_ROOT_PARAMETER root_params[ArrayLength(desc->push_constants) + 64] = {};
	intz root_params_count = 0;
	// intz resc_table_count = 0;
	// intz sampler_table_count = 0;
	intz const_count = 0;

	for (intz i = 0; i < ArrayLength(desc->push_constants); ++i)
	{
		R4_PipelineLayoutPushConstant const* push_const = &desc->push_constants[i];
		if (!push_const->count)
			break;
		SafeAssert(push_const->count >= 0);
		SafeAssert(push_const->start_index >= 0);
		++const_count;
		root_params[root_params_count++] = {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = {
				.ShaderRegister = (UINT32)push_const->start_index,
				.RegisterSpace = 0,
				.Num32BitValues = (UINT32)push_const->count,
			},
			.ShaderVisibility = D3d12VisibilityFromVisibility_(push_const->shader_visibility),
		};
	}

	D3D12_DESCRIPTOR_RANGE ranges[64*3*2] = {};
	intz range_count = 0;
	for (intz i = 0; i < ArrayLength(desc->bind_layouts); ++i)
	{
		R4_BindLayout* bind_layout = desc->bind_layouts[i];
		if (!bind_layout)
			break;
		R4_D3D12_BindLayout* d3d12_layout = &bind_layout->d3d12;

		if (d3d12_layout->sampler_entry_count)
		{
			intz first_range = range_count;
			for (intz j = 0; j < d3d12_layout->sampler_entry_count; ++j)
			{
				if (!d3d12_layout->entries[j].count)
					break;
				SafeAssert(d3d12_layout->entries[j].type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);
				ranges[range_count++] = D3D12_DESCRIPTOR_RANGE {
					.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
					.NumDescriptors = d3d12_layout->entries[j].count,
					.BaseShaderRegister = d3d12_layout->entries[j].first_slot,
					.RegisterSpace = (UINT)j,
					.OffsetInDescriptorsFromTableStart = d3d12_layout->entries[j].offset_from_table_start,
				};
			}
			intz last_range = range_count;

			root_params[root_params_count++] = D3D12_ROOT_PARAMETER {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable = {
					.NumDescriptorRanges = (UINT)(last_range - first_range),
					.pDescriptorRanges = &ranges[first_range],
				},
				.ShaderVisibility = (D3D12_SHADER_VISIBILITY)d3d12_layout->visibility,
			};
		}

		if (d3d12_layout->resc_entry_count)
		{
			intz first_range = range_count;
			for (intz j = 0; j < d3d12_layout->resc_entry_count; ++j)
			{
				intz jj = d3d12_layout->sampler_entry_count + j;
				if (!d3d12_layout->entries[jj].count)
					break;
				SafeAssert(d3d12_layout->entries[jj].type != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);
				ranges[range_count++] = D3D12_DESCRIPTOR_RANGE {
					.RangeType = (D3D12_DESCRIPTOR_RANGE_TYPE)d3d12_layout->entries[jj].type,
					.NumDescriptors = d3d12_layout->entries[jj].count,
					.BaseShaderRegister = d3d12_layout->entries[jj].first_slot,
					.RegisterSpace = (UINT)j,
					.OffsetInDescriptorsFromTableStart = d3d12_layout->entries[jj].offset_from_table_start,
				};
			}
			intz last_range = range_count;

			root_params[root_params_count++] = D3D12_ROOT_PARAMETER {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable = {
					.NumDescriptorRanges = (UINT)(last_range - first_range),
					.pDescriptorRanges = &ranges[first_range],
				},
				.ShaderVisibility = (D3D12_SHADER_VISIBILITY)d3d12_layout->visibility,
			};
		}
	}

	D3D12_ROOT_SIGNATURE_DESC rootsig_desc = {
		.NumParameters = (UINT)root_params_count,
		.pParameters = root_params,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};
	hr = D3D12SerializeRootSignature(&rootsig_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootsig_blob, NULL);
	if (!CheckHr_(ctx, hr, r))
		return {};
	hr = ctx->device->CreateRootSignature(0, rootsig_blob->GetBufferPointer(), rootsig_blob->GetBufferSize(), IID_PPV_ARGS(&rootsig));
	rootsig_blob->Release();
	if (!CheckHr_(ctx, hr, r))
		return {};

	R4_D3D12_PipelineLayout layout = {
		.rootsig = rootsig,
		.root_const_count = (uint32)const_count,
		.bind_layouts = {},
	};
	for (intz i = 0; i < ArrayLength(desc->bind_layouts); ++i)
	{
		R4_BindLayout* bind_layout = desc->bind_layouts[i];
		if (!bind_layout)
			break;
		layout.bind_layouts[i] = bind_layout->d3d12;
	}

	return {
		.d3d12 = layout,
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
API R4_Pipeline
R4_MakeGraphicsPipeline(R4_Context* ctx, R4_Result* r, R4_GraphicsPipelineDesc const* desc)
{
	Trace();
	HRESULT hr;
	ID3D12PipelineState* pipeline = NULL;
	
	D3D12_INPUT_ELEMENT_DESC input_layout[ArrayLength(desc->vertex_inputs)] = {};
	UINT input_layout_size = 0;
	for (intsize i = 0; i < ArrayLength(desc->vertex_inputs); ++i)
	{
		R4_VertexInput const* vinput = &desc->vertex_inputs[i];
		if (!vinput->format)
			break;
		SafeAssert((UINT)vinput->input_slot <= INT32_MAX);
		SafeAssert((UINT)vinput->byte_offset <= INT32_MAX);
		SafeAssert((UINT)vinput->instance_step_rate <= INT32_MAX);
		input_layout[i] = {
			.SemanticName = "VINPUT",
			.SemanticIndex = input_layout_size,
			.Format = DxgiFormatFromFormat_(vinput->format),
			.InputSlot = (UINT)vinput->input_slot,
			.AlignedByteOffset = (UINT)vinput->byte_offset,
			.InputSlotClass = (vinput->instance_step_rate != 0) ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = (UINT)vinput->instance_step_rate,
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
		R4_RenderTargetBlendMode const* rt = &desc->blend_rendertargets[i];

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
			.RenderTargetWriteMask = rt->write_mask,
		};
	}

	SafeAssert((UINT)desc->rendertarget_count <= INT32_MAX);
	SafeAssert((UINT)desc->sample_count <= INT32_MAX);
	SafeAssert((UINT)desc->sample_quality <= INT32_MAX);
	R4_GraphicsPipelineShaders const* shaders = &desc->shaders_dxil;
	SafeAssert(shaders->vertex.size >= 0);
	SafeAssert(shaders->fragment.size >= 0);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {
		.pRootSignature = desc->pipeline_layout->d3d12.rootsig,
		.VS = { shaders->vertex.data, (uintz)shaders->vertex.size },
		.PS = { shaders->fragment.data, (uintz)shaders->fragment.size },
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
		.PrimitiveTopologyType = D3d12TopologyTypeFromPrimitiveType_(desc->primitive),
		.NumRenderTargets = (UINT)desc->rendertarget_count,
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
			.Count = (UINT)desc->sample_count,
			.Quality = (UINT)desc->sample_quality,
		},
		.NodeMask = 1,
		.CachedPSO = {},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};

	hr = ctx->device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pipeline));
	if (!CheckHr_(ctx, hr, r))
		return {};
	return {
		.d3d12 = { pipeline },
	};
}

API R4_Pipeline R4_MakeComputePipeline (R4_Context* ctx, R4_Result* r, R4_ComputePipelineDesc  const* desc);
API R4_Pipeline R4_MakeMeshPipeline    (R4_Context* ctx, R4_Result* r, R4_MeshPipelineDesc     const* desc);

API void
R4_FreePipeline(R4_Context* ctx, R4_Pipeline* pipeline)
{
	Trace();
	if (pipeline->d3d12.pipeline)
		pipeline->d3d12.pipeline->Release();
	*pipeline = {};
}

// =============================================================================
// =============================================================================
// Descriptor heap & descriptor sets
API R4_DescriptorHeap
R4_MakeDescriptorHeap(R4_Context* ctx, R4_Result* r, R4_DescriptorHeapDesc const* desc)
{
	Trace();
	SafeAssert(desc->max_set_count >= 0);
	SafeAssert(desc->buffering_count > 0 && desc->buffering_count <= 4);
	HRESULT hr;
	int64 sampler_count = 0;
	int64 resc_count = 0;

	for (intz i = 0; i < ArrayLength(desc->pools); ++i)
	{
		R4_DescriptorHeapTypePool const* pool = &desc->pools[i];
		if (!pool->type)
			break;
		SafeAssert(pool->count >= 0);
		if (pool->type == R4_DescriptorType_Sampler)
			sampler_count += pool->count;
		else
			resc_count += pool->count;
	}

	SafeAssert(sampler_count >= 0 && sampler_count <= (int64)UINT_MAX / desc->buffering_count);
	SafeAssert(resc_count >= 0 && resc_count <= (int64)UINT_MAX / desc->buffering_count);
	ID3D12DescriptorHeap* resc_heap = NULL;
	ID3D12DescriptorHeap* sampler_heap = NULL;
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = (UINT)(resc_count * desc->buffering_count),
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		.NodeMask = 1,
	};
	hr = ctx->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&resc_heap));
	if (!CheckHr_(ctx, hr, r))
		return {};
	heap_desc.NumDescriptors = (UINT)(resc_count * desc->buffering_count);
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	hr = ctx->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&sampler_heap));
	if (!CheckHr_(ctx, hr, r))
	{
		resc_heap->Release();
		return {};
	}
	return {
		.d3d12 = {
			.sampler_heap = sampler_heap,
			.view_heap = resc_heap,
			.sampler_heap_buffer_size = sampler_count,
			.view_heap_buffer_size = resc_count,
			.sampler_buffering_offsets = {},
			.view_buffering_offsets = {},
			.buffering_count = desc->buffering_count,
		},
	};
}

API void
R4_FreeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap)
{
	Trace();
	if (descriptor_heap->d3d12.sampler_heap)
		descriptor_heap->d3d12.sampler_heap->Release();
	if (descriptor_heap->d3d12.view_heap)
		descriptor_heap->d3d12.view_heap->Release();
	*descriptor_heap = {};
}

API R4_DescriptorSet
R4_MakeDescriptorSet(R4_Context* ctx, R4_Result* r, R4_DescriptorSetDesc const* desc)
{
	Trace();
	R4_D3D12_DescriptorHeap* heap = &desc->descriptor_heap->d3d12;
	R4_D3D12_BindLayout* bind_layout = &desc->bind_layout->d3d12;
	intz buffer_index = desc->heap_buffering_index;
	SafeAssert(buffer_index >= 0 && buffer_index < 4);
	int64* sampler_offset_ptr = &heap->sampler_buffering_offsets[buffer_index];
	int64* view_offset_ptr = &heap->view_buffering_offsets[buffer_index];
	int64 sampler_offset = *sampler_offset_ptr;
	int64 view_offset = *view_offset_ptr;
	SafeAssert(sampler_offset >= 0);
	SafeAssert(view_offset >= 0);

	for (intz i = 0; i < ArrayLength(bind_layout->entries); ++i)
	{
		if (!bind_layout->entries[i].count)
			break;
		if (bind_layout->entries[i].type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
			*sampler_offset_ptr += bind_layout->entries[i].count;
		else
			*view_offset_ptr += bind_layout->entries[i].count;
	}

	sampler_offset += heap->sampler_heap_buffer_size * buffer_index;
	view_offset += heap->view_heap_buffer_size * buffer_index;
	sampler_offset *= ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	view_offset *= ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return {
		.d3d12 = {
			.sampler_cpu_ptr = (UINT64)sampler_offset + heap->sampler_heap->GetCPUDescriptorHandleForHeapStart().ptr,
			.sampler_gpu_ptr = (UINT64)sampler_offset + heap->sampler_heap->GetGPUDescriptorHandleForHeapStart().ptr,
			.view_cpu_ptr = (UINT64)view_offset + heap->view_heap->GetCPUDescriptorHandleForHeapStart().ptr,
			.view_gpu_ptr = (UINT64)view_offset + heap->view_heap->GetGPUDescriptorHandleForHeapStart().ptr,
		},
	};
}

API void
R4_FreeDescriptorSet(R4_Context* ctx, R4_DescriptorSet* descriptor_set)
{
	Trace();
	*descriptor_set = {};
}

API void
R4_UpdateDescriptorSets(R4_Context* ctx, intz write_count, R4_DescriptorSetWrite const writes[], intz copy_count, R4_DescriptorSetCopy const copies[])
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	intz total_write_count;

	// NOTE(ljre): CBV SRV UAV descriptors copy
	total_write_count = 0;
	for (intz i = 0; i < write_count; ++i)
	{
		if (!writes[i].samplers)
			total_write_count += writes[i].count;
	}
	if (total_write_count)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE* dst_range_starts = ArenaPushArray(scratch.arena, D3D12_CPU_DESCRIPTOR_HANDLE, total_write_count);
		D3D12_CPU_DESCRIPTOR_HANDLE* src_range_starts = ArenaPushArray(scratch.arena, D3D12_CPU_DESCRIPTOR_HANDLE, total_write_count);
		UINT* dst_range_counts = ArenaPushArray(scratch.arena, UINT, total_write_count);
		UINT* src_range_counts = ArenaPushArray(scratch.arena, UINT, total_write_count);
		intz write_index = 0;
		for (intz i = 0; i < write_count; ++i)
		{
			R4_DescriptorSetWrite const* write = &writes[i];
			if (write->type == R4_DescriptorType_Sampler)
				continue;

			R4_D3D12_BindLayout* bind_layout = &write->bind_layout->d3d12;
			uint64 table_start_offset = UINT64_MAX;
			D3D12_DESCRIPTOR_RANGE_TYPE expected_type = (D3D12_DESCRIPTOR_RANGE_TYPE)0;
			switch (write->type)
			{
				default: Unreachable(); break;
				case R4_DescriptorType_ImageSampled:
				case R4_DescriptorType_ImageShaderStorage:
				case R4_DescriptorType_BufferShaderStorage:
				case R4_DescriptorType_DynamicBufferShaderStorage: expected_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
				case R4_DescriptorType_ImageShaderReadWrite:
				case R4_DescriptorType_BufferShaderReadWrite:
				case R4_DescriptorType_DynamicBufferShaderReadWrite: expected_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
				case R4_DescriptorType_UniformBuffer:
				case R4_DescriptorType_DynamicUniformBuffer: expected_type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
			}
			for (intz j = 0; j < bind_layout->resc_entry_count; ++j)
			{
				intz index = j + bind_layout->sampler_entry_count;
				intz entry_first_slot = bind_layout->entries[index].first_slot;
				intz entry_count = bind_layout->entries[index].count;
				if (bind_layout->entries[index].type == expected_type &&
					entry_first_slot + j < entry_count)
				{
					table_start_offset = bind_layout->entries[index].offset_from_table_start + (uint64)(j + entry_first_slot) * ctx->resc_descriptor_size;
					break;
				}
			}
			SafeAssert(table_start_offset != UINT64_MAX);

			for (intz j = 0; j < write->count; ++j)
			{
				uint64 cpu_ptr;

				if (write->buffers)
					cpu_ptr = write->buffers[j].buffer->d3d12.cbv_srv_uav_ptr;
				else if (write->images)
					cpu_ptr = write->images[j].image_view->d3d12.srv_uav_ptr;
				else
					continue;

				SafeAssert(write_index < total_write_count);
				src_range_starts[write_index] = {cpu_ptr};
				src_range_counts[write_index] = 1;
				dst_range_starts[write_index] = {write->dst_set->d3d12.view_cpu_ptr + table_start_offset};
				dst_range_counts[write_index] = 1;
				++write_index;
			}
		}
		ctx->device->CopyDescriptors(
			total_write_count, dst_range_starts, dst_range_counts,
			total_write_count, src_range_starts, src_range_counts,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ArenaRestore(scratch);
	}

	// NOTE(ljre): Samplers descriptors copy
	total_write_count = 0;
	for (intz i = 0; i < write_count; ++i)
	{
		if (writes[i].samplers)
			total_write_count += writes[i].count;
	}
	if (total_write_count)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE* dst_range_starts = ArenaPushArray(scratch.arena, D3D12_CPU_DESCRIPTOR_HANDLE, total_write_count);
		D3D12_CPU_DESCRIPTOR_HANDLE* src_range_starts = ArenaPushArray(scratch.arena, D3D12_CPU_DESCRIPTOR_HANDLE, total_write_count);
		UINT* dst_range_counts = ArenaPushArray(scratch.arena, UINT, total_write_count);
		UINT* src_range_counts = ArenaPushArray(scratch.arena, UINT, total_write_count);
		intz write_index = 0;
		for (intz i = 0; i < write_count; ++i)
		{
			R4_DescriptorSetWrite const* write = &writes[i];
			if (write->type != R4_DescriptorType_Sampler)
				continue;

			R4_D3D12_BindLayout* bind_layout = &write->bind_layout->d3d12;
			uint64 table_start_offset = UINT64_MAX;
			for (intz j = 0; j < bind_layout->sampler_entry_count; ++j)
			{
				intz index = j;
				intz entry_first_slot = bind_layout->entries[index].first_slot;
				intz entry_count = bind_layout->entries[index].count;
				if (bind_layout->entries[index].type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER &&
					j + entry_first_slot < entry_count)
				{
					table_start_offset = bind_layout->entries[index].offset_from_table_start + (uint64)(j + entry_first_slot) * ctx->resc_descriptor_size;
					break;
				}
			}
			SafeAssert(table_start_offset != UINT64_MAX);
			for (intz j = 0; j < write->count; ++j)
			{
				uint64 cpu_ptr;

				if (write->samplers)
					cpu_ptr = write->samplers[j]->d3d12.sampler_ptr;
				else
					continue;

				SafeAssert(write_index < total_write_count);
				src_range_starts[write_index] = {cpu_ptr};
				src_range_counts[write_index] = 1;
				dst_range_starts[write_index] = {write->dst_set->d3d12.sampler_cpu_ptr + table_start_offset};
				dst_range_counts[write_index] = 1;
				++write_index;
			}
		}
		ctx->device->CopyDescriptors(
			total_write_count, dst_range_starts, dst_range_counts,
			total_write_count, src_range_starts, src_range_counts,
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		ArenaRestore(scratch);
	}
}

API void
R4_ResetDescriptorHeapBuffering(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap, int32 buffer_index)
{
	Trace();
	SafeAssert(buffer_index >= 0 && buffer_index < 4);
	descriptor_heap->d3d12.sampler_buffering_offsets[buffer_index] = 0;
	descriptor_heap->d3d12.view_buffering_offsets[buffer_index] = 0;
}

// =============================================================================
// =============================================================================
// Command allocator & command lists
API R4_CommandAllocator
R4_MakeCommandAllocator(R4_Context* ctx, R4_Result* r, R4_Queue* target_queue)
{
	Trace();
	HRESULT hr;
	ID3D12CommandAllocator* cmdalloc = NULL;
	hr = ctx->device->CreateCommandAllocator((D3D12_COMMAND_LIST_TYPE)target_queue->d3d12.command_type, IID_PPV_ARGS(&cmdalloc));
	if (!CheckHr_(ctx, hr, r))
		return {};
	return {
		.d3d12 = { cmdalloc },
	};
}

API void
R4_FreeCommandAllocator(R4_Context* ctx, R4_CommandAllocator* cmd_allocator)
{
	Trace();
	if (cmd_allocator->d3d12.allocator)
		cmd_allocator->d3d12.allocator->Release();
	*cmd_allocator = {};
}

API R4_CommandList
R4_MakeCommandList(R4_Context* ctx, R4_Result* r, R4_CommandListType type, R4_CommandAllocator* allocator)
{
	Trace();
	HRESULT hr;
	ID3D12GraphicsCommandList* cmdlist = NULL;
	hr = ctx->device->CreateCommandList(1, D3d12CommandListTypeFromType_(type), allocator->d3d12.allocator, NULL, IID_PPV_ARGS(&cmdlist));
	if (!CheckHr_(ctx, hr, r))
		return {};
	hr = cmdlist->Close();
	if (!CheckHr_(ctx, hr, r))
		return {};
	return {
		.d3d12 = { cmdlist },
	};
}

API void
R4_FreeCommandList(R4_Context* ctx, R4_CommandAllocator* cmdalloc, R4_CommandList* cmd_list)
{
	Trace();
	if (cmd_list->d3d12.list)
		cmd_list->d3d12.list->Release();
	*cmd_list = {};
}

// =============================================================================
// =============================================================================
// Command list recording
API void
R4_ResetCommandAllocator(R4_Context* ctx, R4_Result* r, R4_CommandAllocator* cmd_allocator)
{
	Trace();
	HRESULT hr;
	hr = cmd_allocator->d3d12.allocator->Reset();
	if (!CheckHr_(ctx, hr, r))
		return;
}

API void
R4_BeginCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist, R4_CommandAllocator* cmd_allocator)
{
	Trace();
	HRESULT hr;
	hr = cmdlist->d3d12.list->Reset(cmd_allocator->d3d12.allocator, NULL);
	if (!CheckHr_(ctx, hr, r))
		return;
}

API void
R4_EndCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist)
{
	Trace();
	HRESULT hr;
	hr = cmdlist->d3d12.list->Close();
	if (!CheckHr_(ctx, hr, r))
		return;
}

API void
R4_CmdBeginRenderpass(R4_Context* ctx, R4_CommandList* cmdlist, R4_Renderpass const* renderpass)
{
	Trace();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs_to_bind[ArrayLength(renderpass->render_targets)] = {};
	intz rtvs_to_bind_count = 0;
	for (intz i = 0; i < ArrayLength(renderpass->render_targets); ++i)
	{
		R4_RenderpassRenderTarget const* rendertarget = &renderpass->render_targets[i];
		if (!rendertarget->render_target_view)
			break;
		R4_D3D12_RenderTargetView* impl = FetchFromCppHandlePool_(&ctx->hpool_rtv, rendertarget->render_target_view, NULL);
		if (CheckHandleFetch_(ctx, impl, NULL))
			rtvs_to_bind[rtvs_to_bind_count++] = { impl->rtv_ptr };
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
	if (renderpass->depth_stencil.depth_stencil_view)
	{
		R4_D3D12_DepthStencilView* impl = FetchFromCppHandlePool_(&ctx->hpool_dsv, renderpass->depth_stencil.depth_stencil_view, NULL);
		dsv = { impl->dsv_ptr };
	}
	cmdlist->d3d12.list->OMSetRenderTargets((UINT)rtvs_to_bind_count, rtvs_to_bind, FALSE, dsv.ptr ? &dsv : NULL);

	for (intz i = 0; i < ArrayLength(renderpass->render_targets); ++i)
	{
		R4_RenderpassRenderTarget const* rendertarget = &renderpass->render_targets[i];
		if (!rendertarget->render_target_view)
			break;
		if (rendertarget->load == R4_AttachmentLoadOp_Clear)
			cmdlist->d3d12.list->ClearRenderTargetView(rtvs_to_bind[i], rendertarget->clear_color, 0, NULL);
	}
	if (renderpass->depth_stencil.depth_stencil_view && renderpass->depth_stencil.load == R4_AttachmentLoadOp_Clear)
	{
		D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
		float32 depth = renderpass->depth_stencil.clear_depth;
		uint32 stencil = renderpass->depth_stencil.clear_stencil;
		if (!renderpass->no_depth)
			flags |= D3D12_CLEAR_FLAG_DEPTH;
		if (!renderpass->no_stencil)
			flags |= D3D12_CLEAR_FLAG_STENCIL;
		cmdlist->d3d12.list->ClearDepthStencilView(dsv, flags, depth, stencil, 0, NULL);
	}
}

API void
R4_CmdEndRenderpass(R4_Context* ctx, R4_CommandList* cmdlist)
{
	Trace();
	// TODO(ljre): nothing to do here i believe?
}

API void
R4_CmdResourceBarrier(R4_Context* ctx, R4_CommandList* cmdlist, R4_ResourceBarriers const* barriers)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	D3D12_RESOURCE_BARRIER* first_barrier = (D3D12_RESOURCE_BARRIER*)ArenaEndAligned(scratch.arena, alignof(D3D12_RESOURCE_BARRIER));

	for (intz i = 0; i < barriers->buffer_barrier_count; ++i)
	{
		R4_BufferBarrier const* barrier = &barriers->buffer_barriers[i];
		// NOTE(ljre): in d3d12, resources in a upload heap can only be in `GENERIC_READ` state
		if (barrier->from_state == R4_ResourceState_Preinitialized)
			continue;
		R4_D3D12_Buffer* buffer_impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, barrier->buffer, NULL);
		if (!CheckHandleFetch_(ctx, buffer_impl, NULL))
			continue;

		ArenaPushStructInit(scratch.arena, D3D12_RESOURCE_BARRIER, {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition = {
				.pResource = buffer_impl->resource,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3d12StatesFromResourceStates_(barrier->from_state),
				.StateAfter = D3d12StatesFromResourceStates_(barrier->to_state),
			},
		});
	}

	for (intz i = 0; i < barriers->image_barrier_count; ++i)
	{
		R4_ImageBarrier const* barrier = &barriers->image_barriers[i];
		R4_D3D12_Image* image_impl = FetchFromCppHandlePool_(&ctx->hpool_image, barrier->image, NULL);
		if (!CheckHandleFetch_(ctx, image_impl, NULL))
			continue;

		SafeAssert(barrier->subresource >= 0);
		ArenaPushStructInit(scratch.arena, D3D12_RESOURCE_BARRIER, {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition = {
				.pResource = image_impl->resource,
				.Subresource = (UINT)barrier->subresource,
				.StateBefore = D3d12StatesFromResourceStates_(barrier->from_state),
				.StateAfter = D3d12StatesFromResourceStates_(barrier->to_state),
			},
		});
	}

	D3D12_RESOURCE_BARRIER* last_barrier = (D3D12_RESOURCE_BARRIER*)ArenaEnd(scratch.arena);
	intz barrier_count = last_barrier - first_barrier;
	SafeAssert(barrier_count >= 0 && barrier_count <= UINT_MAX);
	cmdlist->d3d12.list->ResourceBarrier((UINT)barrier_count, first_barrier);
	ArenaRestore(scratch);
}

API void
R4_CmdSetDescriptorHeap(R4_Context* ctx, R4_CommandList* cmdlist, R4_DescriptorHeap* heap)
{
	Trace();
	ID3D12DescriptorHeap* heaps[2] = {
		heap->d3d12.sampler_heap,
		heap->d3d12.view_heap,
	};
	cmdlist->d3d12.list->SetDescriptorHeaps(2, heaps);
}

API void
R4_CmdSetPrimitiveType(R4_Context* ctx, R4_CommandList* cmdlist, R4_PrimitiveType type)
{
	Trace();
	cmdlist->d3d12.list->IASetPrimitiveTopology(D3d12TopologyFromPrimitiveType_(type));
}

API void
R4_CmdSetViewports(R4_Context* ctx, R4_CommandList* cmdlist, intz count, R4_Viewport const viewports[])
{
	Trace();
	SafeAssert(count >= 0 && count <= 8);
	D3D12_VIEWPORT d3d12viewports[8] = {};
	for (intz i = 0; i < count; ++i)
	{
		d3d12viewports[i] = {
			.TopLeftX = viewports[i].x,
			.TopLeftY = viewports[i].y,
			.Width = viewports[i].width,
			.Height = viewports[i].height,
			.MinDepth = viewports[i].min_depth,
			.MaxDepth = viewports[i].max_depth,
		};
	}
	cmdlist->d3d12.list->RSSetViewports((UINT)count, d3d12viewports);
}

API void
R4_CmdSetScissors(R4_Context* ctx, R4_CommandList* cmdlist, intz count, R4_Rect const rects[])
{
	Trace();
	SafeAssert(count >= 0 && count <= 8);
	D3D12_RECT d3d12rects[8] = {};
	for (intz i = 0; i < count; ++i)
	{
		SafeAssert(rects[i].width >= 0);
		SafeAssert(rects[i].height >= 0);
		d3d12rects[i] = {
			.left = rects[i].x,
			.top = rects[i].y,
			.right = rects[i].x + rects[i].width,
			.bottom = rects[i].y + rects[i].height,
		};
	}
	cmdlist->d3d12.list->RSSetScissorRects((UINT)count, d3d12rects);
}

API void
R4_CmdSetPipeline(R4_Context* ctx, R4_CommandList* cmdlist, R4_Pipeline* pipeline)
{
	Trace();
	cmdlist->d3d12.list->SetPipelineState(pipeline->d3d12.pipeline);
}

API void
R4_CmdSetPipelineLayout(R4_Context* ctx, R4_CommandList* cmdlist, R4_PipelineLayout* pipeline_layout)
{
	Trace();
	cmdlist->d3d12.list->SetGraphicsRootSignature(pipeline_layout->d3d12.rootsig);
}

API void
R4_CmdSetVertexBuffers(R4_Context* ctx, R4_CommandList* cmdlist, intz first_slot, intz count, R4_VertexBuffer const buffers[])
{
	Trace();
	SafeAssert(first_slot >= 0);
	D3D12_VERTEX_BUFFER_VIEW vbuffers[16] = {};
	SafeAssert(count >= 0 && count <= ArrayLength(vbuffers));
	for (intz i = 0; i < count; ++i)
	{
		R4_D3D12_Buffer* buffer_impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, buffers[i].buffer, NULL);
		if (!CheckHandleFetch_(ctx, buffer_impl, NULL))
			continue;
		SafeAssert(buffers[i].offset >= 0);
		SafeAssert(buffers[i].size >= 0 && buffers[i].size <= UINT_MAX);
		SafeAssert(buffers[i].stride >= 0 && buffers[i].stride <= UINT_MAX);
		vbuffers[i] = {
			.BufferLocation = buffer_impl->resource->GetGPUVirtualAddress() + (UINT64)buffers[i].offset,
			.SizeInBytes = (UINT)buffers[i].size,
			.StrideInBytes = (UINT)buffers[i].stride,
		};
	}
	cmdlist->d3d12.list->IASetVertexBuffers((UINT)first_slot, (UINT)count, vbuffers);
}

API void
R4_CmdSetIndexBuffer(R4_Context* ctx, R4_CommandList* cmdlist, R4_IndexBuffer const* buffer)
{
	Trace();
	SafeAssert(buffer->offset >= 0);
	SafeAssert(buffer->size >= 0 && buffer->size <= UINT_MAX);
	R4_D3D12_Buffer* buffer_impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, buffer->buffer, NULL);
	if (!CheckHandleFetch_(ctx, buffer_impl, NULL))
		return;
	D3D12_INDEX_BUFFER_VIEW ibuffer = {
		.BufferLocation = buffer_impl->resource->GetGPUVirtualAddress() + (UINT64)buffer->offset,
		.SizeInBytes = (UINT)buffer->size,
		.Format = DxgiFormatFromFormat_(buffer->index_format),
	};
	cmdlist->d3d12.list->IASetIndexBuffer(&ibuffer);
}

API void
R4_CmdPushConstants(R4_Context* ctx, R4_CommandList* cmdlist, R4_PushConstant const* push_constant)
{
	Trace();
	SafeAssert(push_constant->constant_index >= 0);
	SafeAssert(push_constant->offset >= 0);
	SafeAssert(push_constant->count >= 0);
	cmdlist->d3d12.list->SetGraphicsRoot32BitConstants(
		(UINT)push_constant->constant_index,
		(UINT)push_constant->count,
		push_constant->u32_constants,
		(UINT)push_constant->offset);
}

API void
R4_CmdSetDescriptorSets(R4_Context* ctx, R4_CommandList* cmdlist, R4_DescriptorSets const* sets)
{
	Trace();
	SafeAssert(sets->first_set >= 0);
	SafeAssert(sets->count >= 0);
	SafeAssert(sets->dynamic_offset_count >= 0);
	ID3D12GraphicsCommandList* list = cmdlist->d3d12.list;
	R4_D3D12_PipelineLayout* layout = &sets->pipeline_layout->d3d12;
	for (intz i = 0; i < sets->count; ++i)
	{
		R4_D3D12_BindLayout* bind_layout = &layout->bind_layouts[sets->first_set + i];
		UINT table_index = 0;
		table_index += layout->root_const_count;
		table_index += (UINT)sets->first_set;
		table_index += (UINT)i;

		R4_D3D12_DescriptorSet* set = &sets->sets[i]->d3d12;
		if (set->sampler_gpu_ptr)
			list->SetGraphicsRootDescriptorTable(table_index, {sets->sets[i]->d3d12.sampler_gpu_ptr});
		if (set->view_gpu_ptr)
			list->SetGraphicsRootDescriptorTable(table_index + (UINT)bind_layout->sampler_entry_count, {sets->sets[i]->d3d12.view_gpu_ptr});
	}
}

API void R4_CmdComputeSetPipelineLayout(R4_Context* ctx, R4_CommandList* cmdlist, R4_PipelineLayout* pipeline_layout);
API void R4_CmdComputePushConstants    (R4_Context* ctx, R4_CommandList* cmdlist, R4_PushConstant const* push_constant);
API void R4_CmdComputeSetDescriptorSet (R4_Context* ctx, R4_CommandList* cmdlist, R4_DescriptorSet* descriptor_set);

API void
R4_CmdDraw(R4_CommandList* cmdlist, int64 start_vertex, int64 vertex_count, int64 start_instance, int64 instance_count)
{
	Trace();
	SafeAssert(start_vertex >= 0 && start_vertex <= UINT_MAX);
	SafeAssert(vertex_count >= 0 && vertex_count <= UINT_MAX);
	SafeAssert(start_instance >= 0 && start_instance <= UINT_MAX);
	SafeAssert(instance_count >= 0 && instance_count <= UINT_MAX);
	cmdlist->d3d12.list->DrawInstanced((UINT)vertex_count, (UINT)instance_count, (UINT)start_vertex, (UINT)start_instance);
}

API void
R4_CmdDrawIndexed(R4_CommandList* cmdlist, int64 start_index, int64 index_count, int64 start_instance, int64 instance_count, int64 base_vertex)
{
	Trace();
	SafeAssert(start_index >= 0 && start_index <= UINT_MAX);
	SafeAssert(index_count >= 0 && index_count <= UINT_MAX);
	SafeAssert(start_instance >= 0 && start_instance <= UINT_MAX);
	SafeAssert(instance_count >= 0 && instance_count <= UINT_MAX);
	SafeAssert(base_vertex >= INT32_MIN && base_vertex <= INT32_MAX);
	cmdlist->d3d12.list->DrawIndexedInstanced((UINT)index_count, (UINT)instance_count, (UINT)start_index, (INT)base_vertex, (UINT)start_instance);
}

API void
R4_CmdDispatch(R4_CommandList* cmdlist, int64 x, int64 y, int64 z)
{
	Trace();
	SafeAssert(x >= 0 && x <= UINT_MAX);
	SafeAssert(y >= 0 && y <= UINT_MAX);
	SafeAssert(z >= 0 && z <= UINT_MAX);
	cmdlist->d3d12.list->Dispatch((UINT)x, (UINT)y, (UINT)z);
}

API void R4_CmdDispatchMesh        (R4_CommandList* cmdlist, int64 x, int64 y, int64 z);

API void
R4_CmdCopyBuffer(R4_Context* ctx, R4_CommandList* cmdlist, R4_Buffer dest, R4_Buffer source, intz region_count, R4_BufferCopyRegion const regions[])
{
	Trace();
	R4_D3D12_Buffer* dest_impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, dest, NULL);
	if (!CheckHandleFetch_(ctx, dest, NULL))
		return;
	R4_D3D12_Buffer* source_impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, source, NULL);
	if (!CheckHandleFetch_(ctx, source_impl, NULL))
		return;
	for (intz i = 0; i < region_count; ++i)
	{
		R4_BufferCopyRegion const* region = &regions[i];
		SafeAssert(region->dst_offset >= 0);
		SafeAssert(region->src_offset >= 0);
		SafeAssert(region->size >= 0);
		cmdlist->d3d12.list->CopyBufferRegion(
			dest_impl->resource, (UINT64)region->dst_offset,
			source_impl->resource, (UINT64)region->src_offset,
			(UINT64)region->size);
	}
}

API void
R4_CmdCopyImage(R4_Context* ctx, R4_CommandList* cmdlist, R4_Image dest, R4_Image source, intz region_count, R4_ImageCopyRegion const regions[])
{
	Trace();
	R4_D3D12_Image* dest_impl = FetchFromCppHandlePool_(&ctx->hpool_image, dest, NULL);
	if (!CheckHandleFetch_(ctx, dest, NULL))
		return;
	R4_D3D12_Image* source_impl = FetchFromCppHandlePool_(&ctx->hpool_image, source, NULL);
	if (!CheckHandleFetch_(ctx, source_impl, NULL))
		return;
	for (intz i = 0; i < region_count; ++i)
	{
		R4_ImageCopyRegion const* region = &regions[i];
		SafeAssert(region->dst_subresource >= 0);
		SafeAssert(region->src_subresource >= 0);
		SafeAssert(region->dst_x >= 0);
		SafeAssert(region->dst_y >= 0);
		SafeAssert(region->dst_z >= 0);
		D3D12_TEXTURE_COPY_LOCATION dst_loc = {
			.pResource = dest_impl->resource,
			.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = (UINT)region->dst_subresource,
		};
		D3D12_TEXTURE_COPY_LOCATION src_loc = {
			.pResource = source_impl->resource,
			.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = (UINT)region->src_subresource,
		};
		D3D12_BOX src_box = {
			.left = (UINT)region->src_x,
			.top = (UINT)region->src_y,
			.front = (UINT)region->src_z,
			.right = (UINT)(region->src_x + region->width),
			.bottom = (UINT)(region->src_y + region->height),
			.back = (UINT)(region->src_z + region->depth),
		};
		cmdlist->d3d12.list->CopyTextureRegion(
			&dst_loc, (UINT)region->dst_x, (UINT)region->dst_y, (UINT)region->dst_z,
			&src_loc, &src_box);
	}
}

API void
R4_CmdCopyBufferToImage(R4_Context* ctx, R4_CommandList* cmdlist, R4_Image dest, R4_Buffer source, intz region_count, R4_BufferImageCopyRegion const regions[])
{
	Trace();
	R4_D3D12_Image* dest_impl = FetchFromCppHandlePool_(&ctx->hpool_image, dest, NULL);
	if (!CheckHandleFetch_(ctx, dest, NULL))
		return;
	R4_D3D12_Buffer* source_impl = FetchFromCppHandlePool_(&ctx->hpool_buffer, source, NULL);
	if (!CheckHandleFetch_(ctx, source_impl, NULL))
		return;
	for (intz i = 0; i < region_count; ++i)
	{
		R4_BufferImageCopyRegion const* region = &regions[i];
		SafeAssert(region->image_subresource >= 0);
		D3D12_TEXTURE_COPY_LOCATION dst_loc = {
			.pResource = dest_impl->resource,
			.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = (UINT)region->image_subresource,
		};
		D3D12_TEXTURE_COPY_LOCATION src_loc = {
			.pResource = source_impl->resource,
			.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint = {
				.Offset = (UINT64)region->buffer_offset,
				.Footprint = {
					.Format = DxgiFormatFromFormat_(region->image_format),
					.Width = (UINT32)region->buffer_width_in_pixels,
					.Height = (UINT32)region->buffer_height_in_pixels,
					.Depth = 1,
					.RowPitch = (UINT32)RowPitchFromWidthAndFormat_(region->buffer_width_in_pixels, region->image_format),
				},
			},
		};
		D3D12_BOX src_box = {
			.left = 0,
			.top = 0,
			.front = 0,
			.right = (UINT)region->image_width,
			.bottom = (UINT)region->image_height,
			.back = (UINT)(region->image_depth ? region->image_depth : 1),
		};
		cmdlist->d3d12.list->CopyTextureRegion(
			&dst_loc, (UINT)region->image_x, (UINT)region->image_y, (UINT)region->image_z,
			&src_loc, &src_box);
	}
}
