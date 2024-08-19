#include "common.h"
#include "api_render4.h"

#include <webgpu.h>

struct R4_Context
{
	WGPUInstance instance;
	WGPUSurface surface;
	WGPUAdapter adapter;
	WGPUDevice device;
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

API R4_Context*
R4_WEBGPU_MakeContext(Allocator allocator, R4_ContextDesc* desc)
{
	Trace();
	R4_Context ctx = {};
	WGPUFuture future; // wtf i do with this???

	ctx.instance = wgpuCreateInstance(&(WGPUInstanceDescriptor) {});
	ctx.surface = wgpuInstanceCreateSurface(ctx.instance, &(WGPUSurfaceDescriptor) {
		.label = "MainSurface",
	});
	future = wgpuInstanceRequestAdapter(ctx.instance, &(WGPURequestAdapterOptions) {
		.backendType = WGPUBackendType_Undefined,
		.powerPreference = WGPUPowerPreference_HighPerformance,
		.compatibleSurface = ctx.surface,
	}, (WGPURequestAdapterCallbackInfo) {
		.callback = RequestAdapterCallback_,
		.userdata1 = &ctx,
	});
	wgpuInstanceProcessEvents(ctx.instance);
	(void)future;
	if (!ctx.adapter)
		goto lbl_error;

	WGPUFeatureName required_features[] = {
		WGPUFeatureName_TextureCompressionBC,
	};
	future = wgpuAdapterRequestDevice(ctx.adapter, &(WGPUDeviceDescriptor) {
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
	wgpuInstanceProcessEvents(ctx.instance);
	(void)future;
	if (!ctx.device)
		goto lbl_error;

	WGPUQueue queue = wgpuDeviceGetQueue(ctx.device);
	for (intz i = 0; i < ArrayLength(desc->swapchains); ++i)
	{
		if (!desc->swapchains[i].out_swapchain)
			break;
		desc->swapchains[i].out_graphics_queue->wgpu_queue = queue;
	}
	for (intz i = 0; i < ArrayLength(desc->queues); ++i)
	{
		if (!desc->queues[i].out_queue)
			break;
		desc->queues[i].out_queue->wgpu_queue = queue;
	}

	AllocatorError err;
	R4_Context* result = AllocatorAlloc(&allocator, sizeof(R4_Context), alignof(R4_Context), &err);
	if (!result)
		goto lbl_error;

	*result = ctx;
	return result;

lbl_error:
	return NULL;
}
