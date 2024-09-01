#include "common.h"
#include "api.h"

#include <webgpu.h>

struct R4_Context
{
	WGPUInstance instance;
	WGPUSurface surface;
	WGPUAdapter adapter;
	WGPUDevice device;

	void* user_data;
	R4_OnErrorCallback* on_error;

	R4_ContextInfo info;
};

static void
RequestAdapterCallback_(
	WGPURequestAdapterStatus status,
	WGPUAdapter adapter,
	char const* message,
	WGPU_NULLABLE void* userdata1,
	WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE
{
	Trace();
	SafeAssert(status == WGPURequestAdapterStatus_Success);
	R4_Context* ctx = userdata1;
	ctx->adapter = adapter;
}

static void
RequestDeviceCallback_(
	WGPURequestDeviceStatus status,
	WGPUDevice device,
	char const* message,
	WGPU_NULLABLE void* userdata1,
	WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE
{
	Trace();
	SafeAssert(status == WGPURequestDeviceStatus_Success);
	R4_Context* ctx = userdata1;
	ctx->device = device;
}

// =============================================================================
// =============================================================================
// Context creation
API R4_Context*
R4_MakeContext(Allocator allocator, R4_Result* r, R4_ContextDesc const* desc)
{
	Trace();
	AllocatorError err;
	R4_Context* ctx = AllocatorAlloc(&allocator, sizeof(R4_Context), alignof(R4_Context), &err);
	if (!ctx)
		return NULL;

	WGPUFuture future; // wtf i do with this???

	ctx->instance = wgpuCreateInstance(&(WGPUInstanceDescriptor) {});
	ctx->surface = wgpuInstanceCreateSurface(ctx->instance, &(WGPUSurfaceDescriptor) {
		.label = "MainSurface",
	});
	future = wgpuInstanceRequestAdapter(ctx->instance, &(WGPURequestAdapterOptions) {
		.backendType = WGPUBackendType_Undefined,
		.powerPreference = WGPUPowerPreference_HighPerformance,
		.compatibleSurface = ctx->surface,
	}, (WGPURequestAdapterCallbackInfo) {
		.callback = RequestAdapterCallback_,
		.userdata1 = ctx,
	});
	wgpuInstanceProcessEvents(ctx->instance);
	(void)future;
	if (!ctx->adapter)
		goto lbl_error;

	WGPUFeatureName required_features[] = {
		WGPUFeatureName_TextureCompressionBC,
	};
	future = wgpuAdapterRequestDevice(ctx->adapter, &(WGPUDeviceDescriptor) {
		.label = "MainDevice",
		.defaultQueue = {
			.label = "DefaultQueue",
		},
		.requiredFeatureCount = ArrayLength(required_features),
		.requiredFeatures = required_features,
	}, (WGPURequestDeviceCallbackInfo) {
		.callback = RequestDeviceCallback_,
		.userdata1 = &ctx,
	});
	wgpuInstanceProcessEvents(ctx->instance);
	(void)future;
	if (!ctx->device)
		goto lbl_error;

	WGPUQueue queue = wgpuDeviceGetQueue(ctx->device);

	return ctx;

lbl_error:
	return NULL;
}

API R4_ContextInfo
R4_GetContextInfo(R4_Context* ctx)
{
	Trace();
	return ctx->info;
}

API bool R4_IsDeviceLost(R4_Context* ctx, R4_Result* r);
API void R4_DestroyContext(R4_Context* ctx, R4_Result* r);
API void R4_PresentAndSync(R4_Context* ctx, R4_Result* r, int32 sync_interval);
API intz R4_AcquireNextBackbuffer(R4_Context* ctx, R4_Result* r);
API void R4_ExecuteCommandLists(R4_Context* ctx, R4_Result* r, R4_Queue* queue, bool last_submission, intz cmdlist_count, R4_CommandList* const cmdlists[]);
API void R4_WaitRemainingWorkOnQueue(R4_Context* ctx, R4_Result* r, R4_Queue* queue);

// =============================================================================
// =============================================================================
// Heap management
API R4_Heap R4_MakeHeap(R4_Context* ctx, R4_Result* r, R4_HeapType type, int64 size);
API void R4_FreeHeap(R4_Context* ctx, R4_Heap* heap);

// =============================================================================
// =============================================================================
// Buffers & Images
API R4_Buffer R4_MakeUploadBuffer(R4_Context* ctx, R4_Result* r, R4_UploadBufferDesc const* desc);
API R4_Buffer R4_MakePlacedBuffer(R4_Context* ctx, R4_Result* r, R4_PlacedBufferDesc const* desc);
API void R4_FreeBuffer(R4_Context* ctx, R4_Buffer* buffer);

API R4_Image R4_MakePlacedImage(R4_Context* ctx, R4_Result* r, R4_PlacedImageDesc const* desc);
API void R4_FreeImage(R4_Context* ctx, R4_Image* image);

API R4_MemoryRequirements R4_GetMemoryRequirementsFromBufferDesc(R4_Context *ctx, R4_BufferDesc const* desc);
API R4_MemoryRequirements R4_GetMemoryRequirementsFromBuffer    (R4_Context* ctx, R4_Buffer* buffer);
API R4_MemoryRequirements R4_GetMemoryRequirementsFromImageDesc (R4_Context *ctx, R4_ImageDesc const* desc);
API R4_MemoryRequirements R4_GetMemoryRequirementsFromImage     (R4_Context *ctx, R4_Image* image);

API void R4_MapResource  (R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource, intz size, void** out_memory);
API void R4_UnmapResource(R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource);

// =============================================================================
// =============================================================================
// Buffer views & Image views

API R4_BufferView R4_MakeBufferView(R4_Context* ctx, R4_Result* r, R4_BufferViewDesc const* desc);
API void R4_FreeBufferView(R4_Context* ctx, R4_BufferView* buffer_view);

API R4_ImageView R4_MakeImageView(R4_Context* ctx, R4_Result* r, R4_ImageViewDesc const* desc);
API void R4_FreeImageView(R4_Context* ctx, R4_ImageView* image_view);

// =============================================================================
// =============================================================================
// Render target views & depth stencil views

API R4_RenderTargetView R4_MakeRenderTargetView(R4_Context* ctx, R4_Result* r, R4_RenderTargetViewDesc const* desc);
API void R4_FreeRenderTargetView(R4_Context* ctx, R4_RenderTargetView* render_target_view);

API R4_DepthStencilView R4_MakeDepthStencilView(R4_Context* ctx, R4_Result* r, R4_DepthStencilViewDesc const* desc);
API void R4_FreeDepthStencilView(R4_Context* ctx, R4_DepthStencilView* depth_stencil_view);

// =============================================================================
// =============================================================================
// Samplers

API R4_Sampler R4_MakeSampler(R4_Context* ctx, R4_Result* r, R4_SamplerDesc const* desc);
API void R4_FreeSampler(R4_Context* ctx, R4_Sampler* sampler);

// =============================================================================
// =============================================================================
// Bind Layout

API R4_BindLayout R4_MakeBindLayout(R4_Context* ctx, R4_Result* r, R4_BindLayoutDesc const* desc);
API void R4_FreeBindLayout(R4_Context* ctx, R4_BindLayout* bind_layout);

// =============================================================================
// =============================================================================
// Pipeline Layout

API R4_PipelineLayout R4_MakePipelineLayout(R4_Context* ctx, R4_Result* r, R4_PipelineLayoutDesc const* desc);
API void R4_FreePipelineLayout(R4_Context* ctx, R4_PipelineLayout* pipeline_layout);

// =============================================================================
// =============================================================================
// Pipeline

API R4_Pipeline R4_MakeGraphicsPipeline(R4_Context* ctx, R4_Result* r, R4_GraphicsPipelineDesc const* desc);
API R4_Pipeline R4_MakeComputePipeline (R4_Context* ctx, R4_Result* r, R4_ComputePipelineDesc  const* desc);
API R4_Pipeline R4_MakeMeshPipeline    (R4_Context* ctx, R4_Result* r, R4_MeshPipelineDesc     const* desc);
API void R4_FreePipeline(R4_Context* ctx, R4_Pipeline* pipeline);

// =============================================================================
// =============================================================================
// Descriptor heap & descriptor sets

API R4_DescriptorHeap R4_MakeDescriptorHeap(R4_Context* ctx, R4_Result* r, R4_DescriptorHeapDesc const* desc);
API void R4_FreeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap);

API R4_DescriptorSet R4_MakeDescriptorSet(R4_Context* ctx, R4_Result* r, R4_DescriptorSetDesc const* desc);
API void R4_FreeDescriptorSet(R4_Context* ctx, R4_DescriptorSet* descriptor_set);

API void R4_UpdateDescriptorSets(R4_Context* ctx, intz write_count, R4_DescriptorSetWrite const writes[], intz copy_count, R4_DescriptorSetCopy const copies[]);
API void R4_ResetDescriptorHeapBuffering(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap, int32 buffer_index);

// =============================================================================
// =============================================================================
// Command allocator & command lists

API R4_CommandAllocator R4_MakeCommandAllocator(R4_Context* ctx, R4_Result* r, R4_Queue* target_queue);
API void R4_FreeCommandAllocator(R4_Context* ctx, R4_CommandAllocator* cmd_allocator);

API R4_CommandList R4_MakeCommandList(R4_Context* ctx, R4_Result* r, R4_CommandListType type, R4_CommandAllocator* allocator);
API void R4_FreeCommandList(R4_Context* ctx, R4_CommandAllocator* cmdalloc, R4_CommandList* cmd_list);

// =============================================================================
// =============================================================================
// Command list recording
API void R4_ResetCommandAllocator(R4_Context* ctx, R4_Result* r, R4_CommandAllocator* cmd_allocator);
API void R4_BeginCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist, R4_CommandAllocator* cmd_allocator);
API void R4_EndCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist);

API void R4_CmdBeginRenderpass(R4_CommandList* cmdlist, R4_Renderpass const* renderpass);
API void R4_CmdEndRenderpass  (R4_CommandList* cmdlist);

API void R4_CmdResourceBarrier     (R4_CommandList* cmdlist, R4_ResourceBarriers const* barriers);
API void R4_CmdSetDescriptorHeap   (R4_CommandList* cmdlist, R4_DescriptorHeap* heap);
API void R4_CmdSetPrimitiveType    (R4_CommandList* cmdlist, R4_PrimitiveType type);
API void R4_CmdSetViewports        (R4_CommandList* cmdlist, intz count, R4_Viewport const viewports[]);
API void R4_CmdSetScissors         (R4_CommandList* cmdlist, intz count, R4_Rect const rects[]);
API void R4_CmdSetPipeline         (R4_CommandList* cmdlist, R4_Pipeline* pipeline);
API void R4_CmdSetPipelineLayout   (R4_CommandList* cmdlist, R4_PipelineLayout* pipeline_layout);
API void R4_CmdSetVertexBuffers    (R4_CommandList* cmdlist, intz first_slot, intz count, R4_VertexBuffer const buffers[]);
API void R4_CmdSetIndexBuffer      (R4_CommandList* cmdlist, R4_IndexBuffer const* buffer);
API void R4_CmdPushConstants       (R4_CommandList* cmdlist, R4_PushConstant const* push_constant);
API void R4_CmdSetDescriptorSets   (R4_CommandList* cmdlist, R4_DescriptorSets const* sets);
API void R4_CmdComputeSetPipelineLayout(R4_CommandList* cmdlist, R4_PipelineLayout* pipeline_layout);
API void R4_CmdComputePushConstants    (R4_CommandList* cmdlist, R4_PushConstant const* push_constant);
API void R4_CmdComputeSetDescriptorSet (R4_CommandList* cmdlist, R4_DescriptorSet* descriptor_set);

API void R4_CmdDraw                (R4_CommandList* cmdlist, int64 start_vertex, int64 vertex_count, int64 start_instance, int64 instance_count);
API void R4_CmdDrawIndexed         (R4_CommandList* cmdlist, int64 start_index, int64 index_count, int64 start_instance, int64 instance_count, int64 base_vertex);
API void R4_CmdDispatch            (R4_CommandList* cmdlist, int64 x, int64 y, int64 z);
API void R4_CmdDispatchMesh        (R4_CommandList* cmdlist, int64 x, int64 y, int64 z);

API void R4_CmdCopyBuffer       (R4_CommandList* cmdlist, R4_Buffer* dest, R4_Buffer* source, intz region_count, R4_BufferCopyRegion const regions[]);
API void R4_CmdCopyImage        (R4_CommandList* cmdlist, R4_Image* dest, R4_Image* source, intz region_count, R4_ImageCopyRegion const regions[]);
API void R4_CmdCopyBufferToImage(R4_CommandList* cmdlist, R4_Image* dest, R4_Buffer* source, intz region_count, R4_BufferImageCopyRegion const regions[]);
