#include "common.h"
#include "api.h"

#ifdef CONFIG_OS_WINDOWS
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include "api_os_win32.h"
#	define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include "render4_handlepool.h"

struct R4_Context
{
	Allocator allocator;
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkSemaphore swapchain_sem;
	VkSemaphore swapchain_image_sem;
	uint32 swapchain_image_index;
	VkQueue present_queue;
	VkFence completion_fences[8];
	int32 heap_default;
	int32 heap_upload;
	int32 heap_readback;

	void* user_data;
	R4_OnErrorCallback* on_error;
	R4_ContextInfo info;

	HandlePool_ hpool_heap;
	HandlePool_ hpool_rtv;
	HandlePool_ hpool_dsv;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugCallback_(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
	void* user_data)
{
	String prefix = {};
	String extra = {};

	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		prefix = Str("[ERROR]");
	else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		prefix = Str("[WARN]");
	else
		prefix = Str("[INFO]");

	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
		extra = Str("");
	else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		extra = Str(" (validation)");
	else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		extra = Str(" (performance)");
	else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
		extra = Str(" (device addr binding)");

	OS_LogErr("%S vulkan callback%S: %s\n", prefix, extra, callback_data->pMessage);
	if (Assert_IsDebuggerPresent_() && severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		Debugbreak();
	return VK_FALSE;
}

static bool
CheckResult_(R4_Context* ctx, VkResult vkr, R4_Result* r)
{
	if (vkr == VK_SUCCESS)
	{
		if (r)
			*r = R4_Result_Ok;
		return true;
	}

	R4_Result final_result;
	if (vkr == VK_ERROR_DEVICE_LOST || vkr == VK_ERROR_SURFACE_LOST_KHR)
		final_result = R4_Result_DeviceLost;
	else
		final_result = R4_Result_Failure;

	if (r)
		*r = final_result;

	if (ctx->on_error)
		ctx->on_error(ctx->user_data, final_result, r);

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

	if (r)
	{
		if (err == AllocatorError_OutOfMemory)
			*r = R4_Result_AllocatorOutOfMemory;
		else
			*r = R4_Result_AllocatorError;
	}
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

	if (r)
		*r = R4_Result_InvalidHandle;
	if (ctx->on_error)
		ctx->on_error(ctx->user_data, R4_Result_InvalidHandle, r);
	return false;
}

static VkFormat
VulkanFormatFromFormat_(R4_Format format)
{
	VkFormat result = VK_FORMAT_UNDEFINED;

	switch (format)
	{
		case R4_Format_Null: break;

		case R4_Format_R8_UNorm: result = VK_FORMAT_R8_UNORM; break;
		case R4_Format_R8G8_UNorm: result = VK_FORMAT_R8G8_UNORM; break;
		case R4_Format_R8G8B8A8_UNorm: result = VK_FORMAT_R8G8B8A8_UNORM; break;
		case R4_Format_R8G8B8A8_SRgb: result = VK_FORMAT_R8G8B8A8_SRGB; break;
		case R4_Format_R8_UInt: result = VK_FORMAT_R8_UINT; break;
		case R4_Format_R8G8_UInt: result = VK_FORMAT_R8G8_UINT; break;
		case R4_Format_R8G8B8A8_UInt: result = VK_FORMAT_R8G8B8A8_UINT; break;

		case R4_Format_R16_SNorm: result = VK_FORMAT_R16_SNORM; break;
		case R4_Format_R16G16_SNorm: result = VK_FORMAT_R16G16_SNORM; break;
		case R4_Format_R16G16B16A16_SNorm: result = VK_FORMAT_R16G16B16A16_SNORM; break;
		case R4_Format_R16_SInt: result = VK_FORMAT_R16_SINT; break;
		case R4_Format_R16G16_SInt: result = VK_FORMAT_R16G16_SINT; break;
		case R4_Format_R16G16B16A16_SInt: result = VK_FORMAT_R16G16B16A16_SINT; break;

		case R4_Format_R32_UInt: result = VK_FORMAT_R32_UINT; break;

		case R4_Format_R32_Float: result = VK_FORMAT_R32_SFLOAT; break;
		case R4_Format_R32G32_Float: result = VK_FORMAT_R32G32_SFLOAT; break;
		case R4_Format_R32G32B32_Float: result = VK_FORMAT_R32G32B32_SFLOAT; break;
		case R4_Format_R32G32B32A32_Float: result = VK_FORMAT_R32G32B32A32_SFLOAT; break;
		case R4_Format_R16G16_Float: result = VK_FORMAT_R16G16_SFLOAT; break;
		case R4_Format_R16G16B16A16_Float: result = VK_FORMAT_R16G16B16A16_SFLOAT; break;

		case R4_Format_D16_UNorm: result = VK_FORMAT_D16_UNORM; break;
		case R4_Format_D24_UNorm_S8_UInt: result = VK_FORMAT_D24_UNORM_S8_UINT; break;
	}

	return result;
}

static VkDescriptorType
VulkanDescriptorTypeFromType_(R4_DescriptorType type)
{
	VkDescriptorType result = 0;
	switch (type)
	{
		case 0: Unreachable(); break;
		case R4_DescriptorType_Sampler: result = VK_DESCRIPTOR_TYPE_SAMPLER; break;
		case R4_DescriptorType_ImageShaderReadWrite:
		case R4_DescriptorType_ImageShaderStorage: result = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
		case R4_DescriptorType_DynamicBufferShaderReadWrite:
		case R4_DescriptorType_DynamicBufferShaderStorage: result = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC; break;
		case R4_DescriptorType_BufferShaderReadWrite:
		case R4_DescriptorType_BufferShaderStorage: result = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
		case R4_DescriptorType_ImageSampled: result = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; break;
		case R4_DescriptorType_UniformBuffer: result = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
		case R4_DescriptorType_DynamicUniformBuffer: result = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; break;
	}
	return result;
}

static VkShaderStageFlags
VulkanStageFlagsFromVisibility_(R4_ShaderVisibility visibility)
{
	VkShaderStageFlags result = VK_SHADER_STAGE_ALL_GRAPHICS;

	switch (visibility)
	{
		case R4_ShaderVisibility_All: break;
		case R4_ShaderVisibility_Vertex: result = VK_SHADER_STAGE_VERTEX_BIT; break;
		case R4_ShaderVisibility_Pixel: result = VK_SHADER_STAGE_FRAGMENT_BIT; break;
		case R4_ShaderVisibility_Mesh: result = VK_SHADER_STAGE_MESH_BIT_EXT; break;
	}

	return result;
}

static VkImageLayout
VulkanImageLayoutFromResourceState_(R4_ResourceState state)
{
	VkImageLayout result = 0;

	switch (state)
	{
		default: Unreachable(); break;
		case R4_ResourceState_Null:   result = VK_IMAGE_LAYOUT_UNDEFINED; break;
		case R4_ResourceState_Preinitialized: result = VK_IMAGE_LAYOUT_PREINITIALIZED; break;
		case R4_ResourceState_Common: result = VK_IMAGE_LAYOUT_GENERAL; break;
		case R4_ResourceState_ShaderStorage:
		case R4_ResourceState_SampledImage: result = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; break;
		case R4_ResourceState_TransferSrc: result = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; break;
		case R4_ResourceState_TransferDst: result = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; break;
		case R4_ResourceState_RenderTarget: result = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; break;
		case R4_ResourceState_DepthStencil: result = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; break;
		case R4_ResourceState_Present: result = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; break;
	}

	return result;
}

static VkPrimitiveTopology
VulkanPrimitiveTopologyFromType_(R4_PrimitiveType type)
{
	VkPrimitiveTopology result = 0;

	switch (type)
	{
		default: Unreachable(); break;
		case R4_PrimitiveType_PointList: result = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case R4_PrimitiveType_LineList: result = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case R4_PrimitiveType_LineStrip: result = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
		case R4_PrimitiveType_TriangleList: result = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
		case R4_PrimitiveType_TriangleStrip: result = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		case R4_PrimitiveType_TriangleFan: result = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
		case R4_PrimitiveType_Patch: result = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
	}

	return result;
}

static VkBufferCreateInfo
VulkanBufferDescFromBufferDesc_(R4_BufferDesc const* desc)
{
	SafeAssert(desc->size >= 0);

	VkBufferUsageFlags usage_flags = 0;
	if (desc->usage_flags & R4_ResourceUsageFlag_VertexBuffer)
		usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_IndexBuffer)
		usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_UniformBuffer)
		usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (desc->usage_flags & (R4_ResourceUsageFlag_ShaderStorage | R4_ResourceUsageFlag_ShaderReadWrite))
		usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_IndirectBuffer)
		usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_TransferSrc)
		usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_TransferDst)
		usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VkBufferCreateInfo buffer_desc = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.size = (uint64)desc->size,
		.usage = usage_flags,
	};
	return buffer_desc;
}

static VkImageCreateInfo
VulkanImageDescFromImageDesc_(R4_ImageDesc const* desc, R4_ResourceState initial_state)
{
	VkImageUsageFlags usage_flags = 0;
	if (desc->usage_flags & R4_ResourceUsageFlag_TransferDst)
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_TransferSrc)
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_ShaderStorage)
		usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_SampledImage)
		usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_RenderTarget)
		usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (desc->usage_flags & R4_ResourceUsageFlag_DepthStencil)
		usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	if (usage_flags == VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		tiling = VK_IMAGE_TILING_LINEAR;

	SafeAssert(desc->width >= 0 && desc->width <= UINT32_MAX);
	SafeAssert(desc->height >= 0 && desc->height <= UINT32_MAX);
	SafeAssert(desc->depth >= 0 && desc->depth <= UINT32_MAX);
	SafeAssert(desc->mip_levels >= 0 && desc->mip_levels <= UINT32_MAX);
	VkImageCreateInfo image_desc = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType =
			(desc->dimension == R4_ImageDimension_Texture1D) ? VK_IMAGE_TYPE_1D :
			(desc->dimension == R4_ImageDimension_Texture2D) ? VK_IMAGE_TYPE_2D :
			(desc->dimension == R4_ImageDimension_Texture3D) ? VK_IMAGE_TYPE_3D :
			0,
		.extent = {
			.width = (uint32)desc->width,
			.height = desc->height ? (uint32)desc->height : 1,
			.depth = desc->depth ? (uint32)desc->depth : 1,
		},
		.mipLevels = desc->mip_levels ? (uint32)desc->mip_levels : 1,
		.arrayLayers = 1,
		.format = VulkanFormatFromFormat_(desc->format),
		.usage = usage_flags,
		.tiling = tiling,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.initialLayout = VulkanImageLayoutFromResourceState_(initial_state),
	};
	return image_desc;
}

static VkPipelineStageFlags2
VulkanStageMaskFromStageMask_(uint32 stage_mask)
{
	Trace();
	if (stage_mask == 0)
		return VK_PIPELINE_STAGE_2_NONE;
	if (stage_mask == R4_StageMask_All)
		return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkPipelineStageFlags2 result = 0;

	if (stage_mask & R4_StageMask_DrawIndirect)
		result |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	if (stage_mask & R4_StageMask_VertexInput)
		result |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
	if (stage_mask & R4_StageMask_IndexInput)
		result |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
	if (stage_mask & R4_StageMask_RenderTarget)
		result |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (stage_mask & R4_StageMask_DepthStencil)
		result |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;
	if (stage_mask & R4_StageMask_Transfer)
		result |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	if (stage_mask & R4_StageMask_Resolve)
		result |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
	if (stage_mask & R4_StageMask_VertexShader)
		result |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	if (stage_mask & R4_StageMask_PixelShader)
		result |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	if (stage_mask & R4_StageMask_ComputeShader)
		result |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	if (stage_mask & R4_StageMask_AmplificationShader)
		result |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
	if (stage_mask & R4_StageMask_MeshShader)
		result |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;

	return result;
}

static VkAccessFlags2
VulkanAccessMaskFromAccessMask_(uint32 access_mask)
{
	Trace();
	if (access_mask == 0)
		return VK_ACCESS_2_NONE;

	// if (access_mask == R4_AccessMask_Common)
	// return ~0;

	VkAccessFlags2 result = 0;

	if (access_mask & R4_AccessMask_Present)
		result |= VK_ACCESS_2_NONE;
	if (access_mask & R4_AccessMask_VertexBuffer)
		result |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
	if (access_mask & R4_AccessMask_IndexBuffer)
		result |= VK_ACCESS_2_INDEX_READ_BIT;
	if (access_mask & R4_AccessMask_UniformBuffer)
		result |= VK_ACCESS_2_UNIFORM_READ_BIT;
	if (access_mask & R4_AccessMask_ShaderRead)
		result |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	if (access_mask & R4_AccessMask_ShaderWrite)
		result |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	if (access_mask & R4_AccessMask_SampledImage)
		result |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	if (access_mask & R4_AccessMask_RenderTarget)
		result |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	if (access_mask & R4_AccessMask_DepthStencil)
		result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	if (access_mask & R4_AccessMask_TransferDst)
		result |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
	if (access_mask & R4_AccessMask_TransferSrc)
		result |= VK_ACCESS_2_TRANSFER_READ_BIT;

	return result;
}

struct MemoryOffsetPair_
{
	VkDeviceMemory memory;
	uint64 offset;
}
typedef MemoryOffsetPair_;

static MemoryOffsetPair_
VulkanMemoryOffsetPairFromResource_(R4_Resource resource)
{
	MemoryOffsetPair_ result = {};
	if (resource.buffer)
	{
		result.memory = resource.buffer->vk.memory;
		result.offset = resource.buffer->vk.offset;
	}
	else if (resource.image)
	{
		result.memory = resource.image->vk.memory;
		result.offset = resource.image->vk.offset;
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
	SafeAssert(desc->out_backbuffer_images);
	SafeAssert(desc->out_graphics_queue);

	VkResult vkr;
	AllocatorError err;
	R4_Context* ctx = AllocatorAlloc(&allocator, sizeof(R4_Context), alignof(R4_Context), &err);
	if (!CheckAllocatorErr_(err, r))
		return NULL;
	ctx->allocator = allocator;
	ctx->on_error = desc->on_error;
	ctx->user_data = desc->user_data;

	// =============================================================================
	// instance creation & debug messenger
	char const* extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef CONFIG_OS_WINDOWS
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef CONFIG_DEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
	};
	VkInstanceCreateInfo instance_desc = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &(VkApplicationInfo) {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "ApplicationName",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "ApplicatinEngine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_3,
		},
#ifdef CONFIG_DEBUG
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = (char const*[1]) {
			"VK_LAYER_KHRONOS_validation",
		},
#endif
		.enabledExtensionCount = ArrayLength(extensions),
		.ppEnabledExtensionNames = extensions,
	};
	vkr = vkCreateInstance(&instance_desc, NULL, &ctx->instance);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;

#ifdef CONFIG_DEBUG
	PFN_vkCreateDebugUtilsMessengerEXT proc_create_debug_messenger = (void*)vkGetInstanceProcAddr(ctx->instance, "vkCreateDebugUtilsMessengerEXT");
	if (proc_create_debug_messenger)
	{
		VkDebugUtilsMessengerCreateInfoEXT debug_messenger_desc = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = VulkanDebugCallback_,
			.pUserData = NULL,
		};
		vkr = proc_create_debug_messenger(ctx->instance, &debug_messenger_desc, NULL, &ctx->debug_messenger);
		if (vkr != VK_SUCCESS)
			ctx->debug_messenger = NULL;
	}
	if (!ctx->debug_messenger)
		OS_LogErr("[WARN] vulkan: debug messenger is NOT enabled\n");
#endif

	// =============================================================================
	// device selection
	VkPhysicalDevice available_devices[32];
	uint32 available_devices_count = ArrayLength(available_devices);
	vkr = vkEnumeratePhysicalDevices(ctx->instance, &available_devices_count, available_devices);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;

	int32 best_device_score = -1;
	int32 best_device = -1;
	bool best_device_is_igpu = false;
	for (intz i = 0; i < (intz)available_devices_count; ++i)
	{
		VkPhysicalDeviceProperties properties = {};
		VkPhysicalDeviceFeatures features = {};
		vkGetPhysicalDeviceProperties(available_devices[i], &properties);
		vkGetPhysicalDeviceFeatures(available_devices[i], &features);
		bool ok = true;
		ok &= features.fullDrawIndexUint32;
		ok &= features.independentBlend;
		ok &= features.robustBufferAccess;
		ok &= features.sampleRateShading;
		int32 score = 0;
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score |= (desc->prefer_igpu) ? 0x8000 : 0x4000;
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			score |= (desc->prefer_igpu) ? 0x8000 : 0x4000;
		if (ok && score > best_device_score)
		{
			best_device = i;
			best_device_score = score;
			best_device_is_igpu = (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
		}
	}
	if (best_device == -1)
	{
		if (r)
			*r = R4_Result_DeviceCreationFailed;
		goto lbl_error;
	}
	ctx->physical_device = available_devices[best_device];

	// =============================================================================
	// check for extensions we need or expose
	intz extensions_to_enable_count = 1;
	char const* extensions_to_enable[32] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	bool has_raytracing = false;
	bool has_mesh_shaders = false;
	uint32 available_extensions_count = 0;
	vkEnumerateDeviceExtensionProperties(ctx->physical_device, NULL, &available_extensions_count, NULL);
	VkExtensionProperties* available_extensions = AllocatorAllocArray(
			&allocator,
			available_extensions_count,
			sizeof(VkExtensionProperties),
			alignof(VkExtensionProperties),
			&err);
	if (!CheckAllocatorErr_(err, r))
		goto lbl_error;
	vkEnumerateDeviceExtensionProperties(ctx->physical_device, NULL, &available_extensions_count, available_extensions);
	for (intz i = 0; i < (intz)available_extensions_count; ++i)
	{
		char const* extname_cstr = available_extensions[i].extensionName;
		intz namelen = MemoryStrnlen(extname_cstr, ArrayLength(available_extensions[i].extensionName));
		String extname = StrMake(namelen, extname_cstr);
		if (StringEquals(extname, Str(VK_EXT_MESH_SHADER_EXTENSION_NAME)))
			has_mesh_shaders = true;
		else if (
			StringEquals(extname, Str(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)) ||
			StringEquals(extname, Str(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)) ||
			StringEquals(extname, Str(VK_KHR_RAY_QUERY_EXTENSION_NAME)))
		{
			has_raytracing = true;
		}
		else if (StringEquals(extname, Str(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)))
		{}
		else
			continue;
		extensions_to_enable[extensions_to_enable_count++] = extname_cstr;
		if (extensions_to_enable_count >= ArrayLength(extensions_to_enable))
			break;
	}
	AllocatorFreeArray(&allocator, sizeof(VkExtensionProperties), available_extensions, available_extensions_count, &err);

	// =============================================================================
	// crazy logic to find the best heaps
	VkPhysicalDeviceMemoryProperties memory_props = {};
	vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &memory_props);
	intz biggest_default_type_index = -1;
	intz biggest_upload_type_index = -1;
	intz biggest_readback_type_index = -1;
	uint32 default_bits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	uint32 upload_bits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uint32 readback_bits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	for (intz i = 0; i < (intz)memory_props.memoryTypeCount; ++i)
	{
		VkMemoryType* type = &memory_props.memoryTypes[i];
		VkMemoryHeap* heap = &memory_props.memoryHeaps[type->heapIndex];
		if ((type->propertyFlags & default_bits) == default_bits)
		{
			if (biggest_default_type_index == -1 || heap->size > memory_props.memoryHeaps[biggest_default_type_index].size)
				biggest_default_type_index = i;
		}
		if ((type->propertyFlags & upload_bits) == upload_bits)
		{
			if (biggest_upload_type_index == -1)
				biggest_upload_type_index = i;
			else if ((type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
				!(memory_props.memoryTypes[biggest_upload_type_index].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			{
				biggest_upload_type_index = i;
			}
			else if (heap->size > memory_props.memoryHeaps[biggest_upload_type_index].size)
				biggest_upload_type_index = i;
		}
		if ((type->propertyFlags & readback_bits) == readback_bits)
		{
			if (biggest_readback_type_index == -1 || heap->size > memory_props.memoryHeaps[biggest_readback_type_index].size)
				biggest_readback_type_index = i;
		}
	}
	if (biggest_default_type_index == -1 ||
		biggest_upload_type_index == -1 ||
		biggest_readback_type_index == -1)
	{
		if (r)
			*r = R4_Result_DeviceCreationFailed;
		goto lbl_error;
	}

	ctx->heap_default = biggest_default_type_index;
	ctx->heap_upload = biggest_upload_type_index;
	ctx->heap_readback = biggest_readback_type_index;

	// =============================================================================
	// surface
#ifdef CONFIG_OS_WINDOWS
	VkWin32SurfaceCreateInfoKHR surface_desc = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hwnd = OS_W32_HwndFromWindow(*desc->os_window),
		.hinstance = GetModuleHandleW(NULL),
	};
	vkr = vkCreateWin32SurfaceKHR(ctx->instance, &surface_desc, NULL, &ctx->surface);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;
#endif

	// =============================================================================
	// queues
	int32 present_family = -1;
	int32 graphics_family = -1;
	int32 compute_family = -1;
	int32 copy_family = -1;
	VkQueueFamilyProperties queue_families[32];
	uint32 queue_families_count = ArrayLength(queue_families);
	vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_families_count, queue_families);
	for (intz i = 0; i < (intz)queue_families_count; ++i)
	{
		VkQueueFlags queue_flags = queue_families[i].queueFlags;
		if (graphics_family == -1 && queue_flags & VK_QUEUE_GRAPHICS_BIT)
			graphics_family = i;
		if (compute_family == -1 && (queue_flags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == VK_QUEUE_COMPUTE_BIT)
			compute_family = i;
		if (copy_family == -1 && (queue_flags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT)) == VK_QUEUE_TRANSFER_BIT)
			copy_family = i;

		if (present_family == -1)
		{
			VkBool32 has_present = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(ctx->physical_device, i, ctx->surface, &has_present);
			if (has_present)
				present_family = i;
		}
	}
	if (present_family == -1 || graphics_family == -1)
	{
		if (r)
			*r = R4_Result_DeviceCreationFailed;
		goto lbl_error;
	}

	VkDeviceQueueCreateInfo device_queue_descs[4] = {
		[0] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = (uint32)graphics_family,
			.queueCount = 1,
			.pQueuePriorities = &(float32) { 1 },
		},
		[1] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = (uint32)present_family,
			.queueCount = 1,
			.pQueuePriorities = &(float32) { 1 },
		},
	};

	intz queue_count = 2;
	if (present_family == graphics_family)
		queue_count = 1;
	if (compute_family != -1 && desc->out_compute_queue)
	{
		device_queue_descs[queue_count++] = (VkDeviceQueueCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = (uint32)compute_family,
			.queueCount = 1,
			.pQueuePriorities = &(float32) { 1 },
		};
	}
	if (copy_family != -1 && desc->out_copy_queue)
	{
		device_queue_descs[queue_count++] = (VkDeviceQueueCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = (uint32)copy_family,
			.queueCount = 1,
			.pQueuePriorities = &(float32) { 1 },
		};
	}

	// =============================================================================
	// device creation
	VkDeviceCreateInfo device_desc = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = queue_count,
		.pQueueCreateInfos = device_queue_descs,
		.pEnabledFeatures = &(VkPhysicalDeviceFeatures) {
			.fullDrawIndexUint32 = VK_TRUE,
			.independentBlend = VK_TRUE,
			.robustBufferAccess = VK_TRUE,
			.sampleRateShading = VK_TRUE,
			.samplerAnisotropy = VK_TRUE,
		},
		.enabledExtensionCount = extensions_to_enable_count,
		.ppEnabledExtensionNames = extensions_to_enable,
		.pNext = &(VkPhysicalDeviceDynamicRenderingFeatures) {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.dynamicRendering = VK_TRUE,
			.pNext = &(VkPhysicalDeviceSynchronization2Features) {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
				.synchronization2 = VK_TRUE,
				.pNext = &(VkPhysicalDeviceDescriptorIndexingFeatures) {
					.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
					.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
					.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
					.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
					.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
					.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
					.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
				},
			},
		},
	};
	vkr = vkCreateDevice(ctx->physical_device, &device_desc, NULL, &ctx->device);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;

	// =============================================================================
	// queue retrieval
	MemoryZero(desc->out_graphics_queue, sizeof(R4_Queue));
	vkGetDeviceQueue(ctx->device, (uint32)graphics_family, 0, &desc->out_graphics_queue->vk.queue);
	if (graphics_family == present_family)
		ctx->present_queue = desc->out_graphics_queue->vk.queue;
	else
		vkGetDeviceQueue(ctx->device, (uint32)present_family, 0, &ctx->present_queue);
	if (desc->out_compute_queue)
	{
		MemoryZero(desc->out_compute_queue, sizeof(R4_Queue));
		vkGetDeviceQueue(ctx->device, (uint32)compute_family, 0, &desc->out_compute_queue->vk.queue);
	}
	if (desc->out_copy_queue)
	{
		MemoryZero(desc->out_copy_queue, sizeof(R4_Queue));
		vkGetDeviceQueue(ctx->device, (uint32)copy_family, 0, &desc->out_copy_queue->vk.queue);
	}

	// =============================================================================
	// swapchain
	R4_Format format = desc->backbuffer_format;
	intz buffer_count = desc->backbuffer_count;
	
	VkSurfaceCapabilitiesKHR surface_caps;
	vkr = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device, ctx->surface, &surface_caps);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;

	VkSurfaceFormatKHR formats[32] = {};
	uint32 format_count = ArrayLength(formats);
	vkr = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface, &format_count, formats);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;
	SafeAssert(format_count > 0);

	for (intz j = 0; j < (intz)format_count; ++j)
	{
		if (formats[j].format == VulkanFormatFromFormat_(format))
		{
			formats[0] = formats[j];
			break;
		}
	}
	VkPresentModeKHR present_modes[32] = {};
	uint32 present_mode_count = ArrayLength(present_modes);
	vkr = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device, ctx->surface, &present_mode_count, present_modes);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;
	SafeAssert(present_mode_count > 0);

	for (intsize j = 0; j < present_mode_count; ++j)
	{
		if (present_modes[j] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			present_modes[0] = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}
	VkSwapchainCreateInfoKHR swapchain_desc = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = ctx->surface,
		.minImageCount = Clamp(buffer_count, surface_caps.minImageCount, surface_caps.maxImageCount),
		.imageFormat = formats[0].format,
		.imageColorSpace = formats[0].colorSpace,
		.imageExtent = surface_caps.maxImageExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.preTransform = surface_caps.currentTransform,
		.presentMode = present_modes[0],
		.clipped = VK_TRUE,
	};
	uint32 families[] = { (uint32)graphics_family, (uint32)present_family };
	if (graphics_family != present_family)
	{
		swapchain_desc.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_desc.queueFamilyIndexCount = ArrayLength(families);
		swapchain_desc.pQueueFamilyIndices = families;
	}
	vkr = vkCreateSwapchainKHR(ctx->device, &swapchain_desc, NULL, &ctx->swapchain);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;

	VkSemaphoreCreateInfo sem_desc = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	vkr = vkCreateSemaphore(ctx->device, &sem_desc, NULL, &ctx->swapchain_sem);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;
	vkr = vkCreateSemaphore(ctx->device, &sem_desc, NULL, &ctx->swapchain_image_sem);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;
	ctx->swapchain_image_index = 0;

	// =============================================================================
	// completion fences
	SafeAssert(desc->backbuffer_count <= ArrayLength(ctx->completion_fences));
	for (intz i = 0; i < desc->backbuffer_count; ++i)
	{
		VkFenceCreateInfo fence_desc = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		vkr = vkCreateFence(ctx->device, &fence_desc, NULL, &ctx->completion_fences[i]);
		if (!CheckResult_(ctx, vkr, r))
			goto lbl_error;
	}

	// =============================================================================
	// backbuffer images
	VkImage backbuffers[8] = {};
	SafeAssert(desc->backbuffer_count <= ArrayLength(backbuffers));
	uint32 count = (uint32)desc->backbuffer_count;
	vkr = vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &count, backbuffers);
	if (!CheckResult_(ctx, vkr, r))
		goto lbl_error;
	SafeAssert((intz)count == desc->backbuffer_count);
	for (intz i = 0; i < desc->backbuffer_count; ++i)
		desc->out_backbuffer_images[i] = (R4_Image) { .vk.image = backbuffers[i] };

	// =============================================================================
	// create default image views if requested
	if (desc->out_backbuffer_rtvs)
	{

	}

	// =============================================================================
	// context info
	ctx->info = (R4_ContextInfo) {
		.backend_api = StrInit("render4_vulkan"),
		// TODO(ljre): Adapter stuff
		.context_profile = R4_ContextProfile_Vulkan13,
		.shader_profile = R4_ShaderProfile_Spirv14,
		.default_heap_budget = (int64)memory_props.memoryHeaps[biggest_default_type_index].size,
		.upload_heap_budget = (int64)memory_props.memoryHeaps[biggest_upload_type_index].size,
		.readback_heap_budget = (int64)memory_props.memoryHeaps[biggest_readback_type_index].size,
		.adapter_is_igpu = best_device_is_igpu,
		.has_compute_queue = (compute_family != -1),
		.has_copy_queue = (copy_family != -1),
		.has_mesh_shaders = has_mesh_shaders,
		.has_raytracing = has_raytracing,
	};

	// =============================================================================
	// create handle pools
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

	ctx->hpool_heap = InitHandlePool_(1024, sizeof(R4_VK_Heap), allocator, &err);
	if (!CheckAllocatorErr_(err, r))
		goto lbl_error;
	ctx->hpool_rtv = InitHandlePool_(max_rtvs, sizeof(R4_VK_RenderTargetView), allocator, &err);
	if (!CheckAllocatorErr_(err, r))
		goto lbl_error;
	ctx->hpool_dsv = InitHandlePool_(max_dsvs, sizeof(R4_VK_DepthStencilView), allocator, &err);
	if (!CheckAllocatorErr_(err, r))
		goto lbl_error;

	return ctx;

lbl_error:
	R4_DestroyContext(ctx, r);
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
	if (r)
		*r = R4_Result_Ok;
	return false;
}

API void
R4_DestroyContext(R4_Context* ctx, R4_Result* r)
{
	Trace();
	for (intz i = 0; i < ctx->hpool_heap.allocated_count; ++i)
	{
		R4_VK_Heap* impl = FetchByIndexFromHandlePool_(&ctx->hpool_heap, i);
		if (impl && impl->memory)
			vkFreeMemory(ctx->device, impl->memory, NULL);
	}
	for (intz i = 0; i < ctx->hpool_rtv.allocated_count; ++i)
	{
		R4_VK_RenderTargetView* impl = FetchByIndexFromHandlePool_(&ctx->hpool_rtv, i);
		if (impl && impl->image_view)
			vkDestroyImageView(ctx->device, impl->image_view, NULL);
	}
	for (intz i = 0; i < ctx->hpool_dsv.allocated_count; ++i)
	{
		R4_VK_DepthStencilView* impl = FetchByIndexFromHandlePool_(&ctx->hpool_dsv, i);
		if (impl && impl->image_view)
			vkDestroyImageView(ctx->device, impl->image_view, NULL);
	}
	for (intz i = 0; i < ArrayLength(ctx->completion_fences); ++i)
		if (ctx->completion_fences[i])
			vkDestroyFence(ctx->device, ctx->completion_fences[i], NULL);
	if (ctx->swapchain_image_sem)
		vkDestroySemaphore(ctx->device, ctx->swapchain_image_sem, NULL);
	if (ctx->swapchain_sem)
		vkDestroySemaphore(ctx->device, ctx->swapchain_sem, NULL);
	if (ctx->swapchain)
		vkDestroySwapchainKHR(ctx->device, ctx->swapchain, NULL);
	if (ctx->device)
		vkDestroyDevice(ctx->device, NULL);
	if (ctx->surface)
		vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
	if (ctx->instance)
		vkDestroyInstance(ctx->instance, NULL);
	if (ctx->allocator.proc)
	{
		Allocator allocator = ctx->allocator;
		AllocatorError err;
		AllocatorFree(&allocator, ctx, sizeof(R4_Context), &err);
		if (!CheckAllocatorErr_(err, r))
			return;
	}
}

API void
R4_PresentAndSync(R4_Context* ctx, R4_Result* r, int32 sync_interval)
{
	Trace();
	VkResult vkr;
	VkPresentInfoKHR present_desc = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = (VkSemaphore[1]) { ctx->swapchain_sem },
		.swapchainCount = 1,
		.pSwapchains = (VkSwapchainKHR[1]) { ctx->swapchain },
		.pImageIndices = &ctx->swapchain_image_index,
	};
	vkr = vkQueuePresentKHR(ctx->present_queue, &present_desc);
	CheckResult_(ctx, vkr, r);
}

API intz
R4_AcquireNextBackbuffer(R4_Context* ctx, R4_Result* r)
{
	Trace();
	VkResult vkr;
	vkr = vkAcquireNextImageKHR(ctx->device, ctx->swapchain, UINT64_MAX, ctx->swapchain_image_sem, NULL, &ctx->swapchain_image_index);
	if (!CheckResult_(ctx, vkr, r))
		return -1;
	vkr = vkWaitForFences(ctx->device, 1, &ctx->completion_fences[ctx->swapchain_image_index], VK_TRUE, UINT64_MAX);
	if (!CheckResult_(ctx, vkr, r))
		return -1;
	vkr = vkResetFences(ctx->device, 1, &ctx->completion_fences[ctx->swapchain_image_index]);
	if (!CheckResult_(ctx, vkr, r))
		return -1;
	SafeAssert(ctx->swapchain_image_index < 8);
	return (intz)ctx->swapchain_image_index;
}

API void
R4_ExecuteCommandLists(R4_Context* ctx, R4_Result* r, R4_Queue* queue, bool last_submission, intz cmdlist_count, R4_CommandList* const cmdlists[])
{
	Trace();
	VkResult vkr;
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkCommandBuffer* cmdlists_to_submit = ArenaPushArray(scratch.arena, VkCommandBuffer, cmdlist_count);
	for (intsize i = 0 ; i < cmdlist_count; ++i)
		cmdlists_to_submit[i] = cmdlists[i]->vk.buffer;
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = cmdlist_count,
		.pCommandBuffers = cmdlists_to_submit,
	};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	if (last_submission)
	{
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &ctx->swapchain_sem;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &ctx->swapchain_image_sem;
		submit_info.pWaitDstStageMask = wait_stages;
	}
	vkr = vkQueueSubmit(queue->vk.queue, 1, &submit_info, ctx->completion_fences[ctx->swapchain_image_index]);
	ArenaRestore(scratch);
	CheckResult_(ctx, vkr, r);
}

API void
R4_WaitRemainingWorkOnQueue(R4_Context* ctx, R4_Result* r, R4_Queue* queue)
{
	Trace();
	VkResult vkr;
	vkr = vkQueueWaitIdle(queue->vk.queue);
	if (!CheckResult_(ctx, vkr, r))
		return;
}

// =============================================================================
// =============================================================================
// Heap management
API R4_Heap
R4_MakeHeap(R4_Context* ctx, R4_Result* r, R4_HeapType type, int64 size)
{
	Trace();
	VkResult vkr;
	VkDeviceMemory memory = NULL;
	R4_Heap handle = NULL;
	R4_VK_Heap* heap_impl = AllocateFromHandlePool_(&ctx->hpool_heap, &handle, NULL);
	if (!CheckHandleCreation_(ctx, handle, r))
		return NULL;

	uint32 memory_type;
	switch (type)
	{
		case R4_HeapType_Default:  memory_type = (uint32)ctx->heap_default; break;
		case R4_HeapType_Upload:   memory_type = (uint32)ctx->heap_upload; break;
		case R4_HeapType_Readback: memory_type = (uint32)ctx->heap_readback; break;
		default: Unreachable(); break;
	}
	SafeAssert(size >= 0);
	size = AlignUp(size, 64U<<10);
	SafeAssert(size >= 0);
	VkMemoryAllocateInfo memory_desc = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.memoryTypeIndex = memory_type,
		.allocationSize = (uint64)size,
	};
	vkr = vkAllocateMemory(ctx->device, &memory_desc, NULL, &memory);
	if (!CheckResult_(ctx, vkr, r))
	{
		FreeFromHandlePool_(&ctx->hpool_heap, handle);
		return NULL;
	}
	heap_impl->memory = memory;
	return handle;
}

API void
R4_FreeHeap(R4_Context* ctx, R4_Heap heap)
{
	Trace();
	R4_VK_Heap* heap_impl = FetchFromHandlePool_(&ctx->hpool_heap, heap, NULL);
	if (heap_impl)
	{
		vkFreeMemory(ctx->device, heap_impl->memory, NULL);
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
	VkResult vkr;
	VkBuffer buffer = NULL;

	R4_VK_Heap* heap_impl = FetchFromHandlePool_(&ctx->hpool_heap, desc->heap, NULL);
	if (!CheckHandleFetch_(ctx, heap_impl, r))
		return (R4_Buffer) {};
	VkDeviceMemory device_memory = heap_impl->memory;
	VkDeviceSize offset = (uint64)desc->heap_offset;

	VkBufferCreateInfo buffer_desc = VulkanBufferDescFromBufferDesc_(&desc->buffer_desc);
	vkr = vkCreateBuffer(ctx->device, &buffer_desc, NULL, &buffer);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_Buffer) {};
	vkr = vkBindBufferMemory(ctx->device, buffer, device_memory, offset);
	if (!CheckResult_(ctx, vkr, r))
	{
		vkDestroyBuffer(ctx->device, buffer, NULL);
		return (R4_Buffer) {};
	}

	return (R4_Buffer) {
		.vk = {
			.buffer = buffer,
			.memory = device_memory,
			.offset = offset,
		},
	};
}

API R4_Buffer
R4_MakePlacedBuffer(R4_Context* ctx, R4_Result* r, R4_PlacedBufferDesc const* desc)
{
	SafeAssert(desc->heap_offset >= 0);
	VkResult vkr;
	VkBuffer buffer = NULL;
	R4_VK_Heap* heap_impl = FetchFromHandlePool_(&ctx->hpool_heap, desc->heap, NULL);
	if (!CheckHandleFetch_(ctx, heap_impl, r))
		return (R4_Buffer) {};
	VkDeviceMemory device_memory = heap_impl->memory;
	VkDeviceSize offset = (uint64)desc->heap_offset;

	VkBufferCreateInfo buffer_desc = VulkanBufferDescFromBufferDesc_(&desc->buffer_desc);
	vkr = vkCreateBuffer(ctx->device, &buffer_desc, NULL, &buffer);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_Buffer) {};
	vkr = vkBindBufferMemory(ctx->device, buffer, device_memory, offset);
	if (!CheckResult_(ctx, vkr, r))
	{
		vkDestroyBuffer(ctx->device, buffer, NULL);
		return (R4_Buffer) {};
	}

	return (R4_Buffer) {
		.vk = {
			.buffer = buffer,
			.memory = device_memory,
			.offset = offset,
		},
	};
}

API void
R4_FreeBuffer(R4_Context* ctx, R4_Buffer* buffer)
{
	Trace();
	// TODO
}

API R4_Image
R4_MakePlacedImage(R4_Context* ctx, R4_Result* r, R4_PlacedImageDesc const* desc)
{
	Trace();
	SafeAssert(desc->heap_offset >= 0);
	VkResult vkr;
	VkImage image = NULL;
	R4_VK_Heap* heap_impl = FetchFromHandlePool_(&ctx->hpool_heap, desc->heap, NULL);
	if (!CheckHandleFetch_(ctx, heap_impl, r))
		return (R4_Image) {};
	VkDeviceMemory device_memory = heap_impl->memory;
	VkDeviceSize offset = (uint64)desc->heap_offset;

	VkImageCreateInfo image_info = VulkanImageDescFromImageDesc_(&desc->image_desc, desc->initial_state);
	vkr = vkCreateImage(ctx->device, &image_info, NULL, &image);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_Image) {};
	vkr = vkBindImageMemory(ctx->device, image, device_memory, offset);
	if (!CheckResult_(ctx, vkr, r))
	{
		vkDestroyImage(ctx->device, image, NULL);
		return (R4_Image) {};
	}

	return (R4_Image) {
		.vk = {
			.image = image,
			.memory = device_memory,
			.offset = offset,
		},
	};
}

API void
R4_FreeImage(R4_Context* ctx, R4_Image* image)
{
	Trace();
	// TODO
}

API R4_MemoryRequirements
R4_GetMemoryRequirementsFromBufferDesc(R4_Context *ctx, R4_BufferDesc const* desc)
{
	Trace();
	VkMemoryRequirements2 requirements = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
	};
	VkBufferCreateInfo buffer_desc = VulkanBufferDescFromBufferDesc_(desc);
	VkDeviceBufferMemoryRequirements requirements_desc = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS,
		.pCreateInfo = &buffer_desc,
	};
	vkGetDeviceBufferMemoryRequirements(ctx->device, &requirements_desc, &requirements);
	return (R4_MemoryRequirements) {
		.size = (int64)requirements.memoryRequirements.size,
		.alignment = (int64)requirements.memoryRequirements.alignment,
	};
}

API R4_MemoryRequirements R4_GetMemoryRequirementsFromBuffer(R4_Context* ctx, R4_Buffer* buffer);

API R4_MemoryRequirements
R4_GetMemoryRequirementsFromImageDesc(R4_Context *ctx, R4_ImageDesc const* desc)
{
	Trace();
	VkMemoryRequirements2 requirements = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
	};
	VkImageCreateInfo image_desc = VulkanImageDescFromImageDesc_(desc, 0);
	VkDeviceImageMemoryRequirements requirements_desc = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS,
		.pCreateInfo = &image_desc,
	};
	vkGetDeviceImageMemoryRequirements(ctx->device, &requirements_desc, &requirements);
	return (R4_MemoryRequirements) {
		.size = (int64)requirements.memoryRequirements.size,
		.alignment = (int64)requirements.memoryRequirements.alignment,
	};
}

API R4_MemoryRequirements R4_GetMemoryRequirementsFromImage(R4_Context *ctx, R4_Image* image);

API void
R4_MapResource(R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource, intz size, void** out_memory)
{
	Trace();
	SafeAssert(size >= 0);
	VkResult vkr;
	MemoryOffsetPair_ pair = VulkanMemoryOffsetPairFromResource_(resource);
	vkr = vkMapMemory(ctx->device, pair.memory, pair.offset, (uintz)size, 0, out_memory);
	if (!CheckResult_(ctx, vkr, r))
		return;
}

API void
R4_UnmapResource(R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource)
{
	Trace();
	MemoryOffsetPair_ pair = VulkanMemoryOffsetPairFromResource_(resource);
	vkUnmapMemory(ctx->device, pair.memory);
}

// =============================================================================
// =============================================================================
// Buffer views & Image views
API R4_BufferView R4_MakeBufferView(R4_Context* ctx, R4_Result* r, R4_BufferViewDesc const* desc);
API void R4_FreeBufferView(R4_Context* ctx, R4_BufferView* buffer_view);

API R4_ImageView
R4_MakeImageView(R4_Context* ctx, R4_Result* r, R4_ImageViewDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkImageView view = NULL;
	VkImageViewCreateInfo view_desc = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = desc->image->vk.image,
		.format = VulkanFormatFromFormat_(desc->format),
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1,
		},
	};
	vkr = vkCreateImageView(ctx->device, &view_desc, NULL, &view);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_ImageView) {};
	return (R4_ImageView) {
		.vk.view = view,
	};
}

API void R4_FreeImageView(R4_Context* ctx, R4_ImageView* image_view);

// =============================================================================
// =============================================================================
// Render target views & depth stencil views
API R4_RenderTargetView
R4_MakeRenderTargetView(R4_Context* ctx, R4_Result* r, R4_RenderTargetViewDesc const* desc)
{
	Trace();
	R4_RenderTargetView handle = NULL;
	R4_VK_RenderTargetView* impl = AllocateFromHandlePool_(&ctx->hpool_rtv, &handle, NULL);
	if (!CheckHandleCreation_(ctx, handle, r))
		return NULL;

	VkResult vkr;
	VkImageView view = NULL;
	VkImageViewCreateInfo view_desc = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = desc->image->vk.image,
		.format = VulkanFormatFromFormat_(desc->format),
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1,
		},
	};
	vkr = vkCreateImageView(ctx->device, &view_desc, NULL, &view);
	if (!CheckResult_(ctx, vkr, r))
	{
		FreeFromHandlePool_(&ctx->hpool_rtv, handle);
		return NULL;
	}

	impl->image_view = view;
	return handle;
}

API void
R4_FreeRenderTargetView(R4_Context* ctx, R4_RenderTargetView render_target_view)
{
	Trace();
	R4_VK_RenderTargetView* impl = FetchFromHandlePool_(&ctx->hpool_rtv, render_target_view, NULL);
	if (impl)
	{
		vkDestroyImageView(ctx->device, impl->image_view, NULL);
		FreeFromHandlePool_(&ctx->hpool_rtv, render_target_view);
	}
}

API R4_DepthStencilView
R4_MakeDepthStencilView(R4_Context* ctx, R4_Result* r, R4_DepthStencilViewDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkImageView view = NULL;
	return NULL;
}

API void
R4_FreeDepthStencilView(R4_Context* ctx, R4_DepthStencilView depth_stencil_view)
{
	R4_VK_DepthStencilView* impl = FetchFromHandlePool_(&ctx->hpool_dsv, depth_stencil_view, NULL);
	if (impl)
	{
		vkDestroyImageView(ctx->device, impl->image_view, NULL);
		FreeFromHandlePool_(&ctx->hpool_dsv, depth_stencil_view);
	}
}

// =============================================================================
// =============================================================================
// Samplers
API R4_Sampler
R4_MakeSampler(R4_Context* ctx, R4_Result* r, R4_SamplerDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkSampler sampler = NULL;
	static VkSamplerAddressMode const address_mode_table[] = {
		[R4_AddressMode_Wrap] = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		[R4_AddressMode_Mirror] = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		[R4_AddressMode_Clamp] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		[R4_AddressMode_Border] = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		[R4_AddressMode_MirrorOnce] = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
	};
	static VkFilter const filter_table[] = {
		[R4_Filter_Nearest] = VK_FILTER_NEAREST,
		[R4_Filter_Linear] = VK_FILTER_LINEAR,
		[R4_Filter_Anisotropic] = VK_FILTER_LINEAR,
	};
	static VkSamplerMipmapMode mipmap_mode_table[] = {
		[R4_Filter_Nearest] = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		[R4_Filter_Linear] = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		[R4_Filter_Anisotropic] = VK_SAMPLER_MIPMAP_MODE_LINEAR,
	};
	VkSamplerCreateInfo sampler_desc = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.addressModeU = address_mode_table[desc->addr_u],
		.addressModeV = address_mode_table[desc->addr_v],
		.addressModeW = address_mode_table[desc->addr_w],
		.minFilter = filter_table[desc->min_filter],
		.magFilter = filter_table[desc->mag_filter],
		.mipmapMode = mipmap_mode_table[desc->mip_filter],
		.mipLodBias = desc->mip_lod_bias,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.minLod = desc->min_lod,
		.maxLod = desc->max_lod,
		.compareEnable = (desc->comparison_func != R4_ComparisonFunc_Null),
		.compareOp = 0, // TODO
		.anisotropyEnable = (desc->min_filter == R4_Filter_Anisotropic || desc->mag_filter == R4_Filter_Anisotropic),
		.maxAnisotropy = (float32)desc->max_anisotropy,
	};
	vkr = vkCreateSampler(ctx->device, &sampler_desc, NULL, &sampler);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_Sampler) {};
	return (R4_Sampler) {
		.vk.sampler = sampler,
	};
}

API void R4_FreeSampler(R4_Context* ctx, R4_Sampler* sampler);

// =============================================================================
// =============================================================================
// Bind Layout
API R4_BindLayout
R4_MakeBindLayout(R4_Context* ctx, R4_Result* r, R4_BindLayoutDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkDescriptorSetLayout layout = NULL;

	VkDescriptorSetLayoutBinding bindings[ArrayLength(desc->entries)] = {};
	intz binding_count = 0;
	for (intz i = 0; i < ArrayLength(desc->entries); ++i)
	{
		R4_BindLayoutEntry const* entry = &desc->entries[i];
		if (!entry->descriptor_type)
			break;
		SafeAssert(entry->descriptor_count >= 0);
		SafeAssert(entry->start_shader_slot >= 0);

		bindings[binding_count++] = (VkDescriptorSetLayoutBinding) {
			.descriptorType = VulkanDescriptorTypeFromType_(entry->descriptor_type),
			.descriptorCount = (uint32)entry->descriptor_count,
			.binding = i,
			.stageFlags = VulkanStageFlagsFromVisibility_(desc->shader_visibility),
		};
	}

	VkDescriptorSetLayoutCreateInfo layout_desc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = binding_count,
		.pBindings = bindings,
	};
	vkr = vkCreateDescriptorSetLayout(ctx->device, &layout_desc, NULL, &layout);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_BindLayout) {};

	return (R4_BindLayout) {
		.vk.layout = layout,
	};
}

API void
R4_FreeBindLayout(R4_Context* ctx, R4_BindLayout* bind_layout)
{
	Trace();
	if (bind_layout->vk.layout)
		vkDestroyDescriptorSetLayout(ctx->device, bind_layout->vk.layout, NULL);
	*bind_layout = (R4_BindLayout) {};
}

// =============================================================================
// =============================================================================
// Pipeline Layout
API R4_PipelineLayout
R4_MakePipelineLayout(R4_Context* ctx, R4_Result* r, R4_PipelineLayoutDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkPipelineLayout layout = NULL;

	VkPushConstantRange const_ranges[ArrayLength(desc->push_constants)] = {};
	VkDescriptorSetLayout descriptor_layouts[ArrayLength(desc->bind_layouts)] = {};
	intz const_range_count = 0;
	intz descriptor_layout_count = 0;

	for (intz i = 0; i < ArrayLength(desc->push_constants); ++i)
	{
		R4_PipelineLayoutPushConstant const* constant = &desc->push_constants[i];
		if (!constant->count)
			break;
		SafeAssert(constant->count >= 0 && constant->count <= INT32_MAX/4);
		SafeAssert(constant->start_index >= 0 && constant->start_index <= INT32_MAX/4);
		const_ranges[const_range_count++] = (VkPushConstantRange) {
			.offset = (uint32)(constant->start_index * 4),
			.size = (uint32)(constant->count * 4),
			.stageFlags = VulkanStageFlagsFromVisibility_(constant->shader_visibility),
		};
	}

	for (intz i = 0; i < ArrayLength(desc->bind_layouts); ++i)
	{
		R4_BindLayout* bind_layout = desc->bind_layouts[i];
		if (!bind_layout)
			break;
		descriptor_layouts[descriptor_layout_count++] = bind_layout->vk.layout;
	}

	VkPipelineLayoutCreateInfo layout_desc = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = const_range_count,
		.pPushConstantRanges = const_ranges,
		.setLayoutCount = descriptor_layout_count,
		.pSetLayouts = descriptor_layouts,
	};
	vkr = vkCreatePipelineLayout(ctx->device, &layout_desc, NULL, &layout);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_PipelineLayout) {};
	return (R4_PipelineLayout) {
		.vk.layout = layout,
	};
}

API void
R4_FreePipelineLayout(R4_Context* ctx, R4_PipelineLayout* pipeline_layout)
{
	Trace();
	if (pipeline_layout->vk.layout)
		vkDestroyPipelineLayout(ctx->device, pipeline_layout->vk.layout, NULL);
	*pipeline_layout = (R4_PipelineLayout) {};
}

// =============================================================================
// =============================================================================
// Pipeline
API R4_Pipeline
R4_MakeGraphicsPipeline(R4_Context* ctx, R4_Result* r, R4_GraphicsPipelineDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkPipeline pipeline = NULL;
	VkShaderModule shader_vs = NULL;
	VkShaderModule shader_ps = NULL;

	VkPipelineShaderStageCreateInfo shader_stages[2] = {};
	intz shader_stage_count = 0;
	VkVertexInputAttributeDescription vertex_attribs[ArrayLength(desc->vertex_inputs)] = {};
	intz vertex_attribs_count = 0;
	VkVertexInputBindingDescription vertex_bindings[16] = {};
	intz vertex_bindings_count = 0;

	R4_GraphicsPipelineShaders const* shaders = &desc->shaders_spirv;

	if (shaders->vertex.size)
	{
		SafeAssert(shaders->vertex.size > 0);
		VkShaderModuleCreateInfo shader_vs_desc = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = (uintz)shaders->vertex.size,
			.pCode = (uint32 const*)shaders->vertex.data,
		};
		vkr = vkCreateShaderModule(ctx->device, &shader_vs_desc, NULL, &shader_vs);
		if (!CheckResult_(ctx, vkr, r))
			return (R4_Pipeline) {};
		shader_stages[shader_stage_count++] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shader_vs,
			.pName = "Vertex",
		};
	}
	if (shaders->fragment.size)
	{
		SafeAssert(shaders->fragment.size > 0);
		VkShaderModuleCreateInfo shader_ps_desc = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = (uintz)shaders->fragment.size,
			.pCode = (uint32 const*)shaders->fragment.data,
		};
		vkr = vkCreateShaderModule(ctx->device, &shader_ps_desc, NULL, &shader_ps);
		if (!CheckResult_(ctx, vkr, r))
		{
			if (shader_vs)
				vkDestroyShaderModule(ctx->device, shader_vs, NULL);
			return (R4_Pipeline) {};
		}
		shader_stages[shader_stage_count++] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shader_ps,
			.pName = "Pixel",
		};
	}

	for (intz i = 0; i < ArrayLength(desc->vertex_inputs); ++i)
	{
		R4_VertexInput const* input = &desc->vertex_inputs[i];
		if (!input->format)
			break;
		VkVertexInputRate input_rate;
		intz binding_index = -1;

		if (input->instance_step_rate)
			input_rate = VK_VERTEX_INPUT_RATE_INSTANCE;
		else
			input_rate = VK_VERTEX_INPUT_RATE_VERTEX;

		for (intz j = 0; j < vertex_bindings_count; ++j)
		{
			VkVertexInputBindingDescription* binding = &vertex_bindings[j];
			if (binding->binding == input->input_slot && binding->inputRate == input_rate)
				binding_index = j;
		}
		if (binding_index == -1)
		{
			binding_index = vertex_bindings_count++;
			vertex_bindings[binding_index] = (VkVertexInputBindingDescription) {
				.binding = input->input_slot,
				.inputRate = input_rate,
				.stride = 0,
			};
		}

		vertex_attribs[vertex_attribs_count++] = (VkVertexInputAttributeDescription) {
			.binding = (uint32)binding_index,
			.format = VulkanFormatFromFormat_(input->format),
			.location = i,
			.offset = (uint32)input->byte_offset,
		};
	}

	VkGraphicsPipelineCreateInfo pipeline_desc = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &(VkPipelineRenderingCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = desc->rendertarget_count,
			.pColorAttachmentFormats = (VkFormat[8]) {
				VulkanFormatFromFormat_(desc->rendertarget_formats[0]),
				VulkanFormatFromFormat_(desc->rendertarget_formats[1]),
				VulkanFormatFromFormat_(desc->rendertarget_formats[2]),
				VulkanFormatFromFormat_(desc->rendertarget_formats[3]),
				VulkanFormatFromFormat_(desc->rendertarget_formats[4]),
				VulkanFormatFromFormat_(desc->rendertarget_formats[5]),
				VulkanFormatFromFormat_(desc->rendertarget_formats[6]),
				VulkanFormatFromFormat_(desc->rendertarget_formats[7]),
			},
			.depthAttachmentFormat = VulkanFormatFromFormat_(desc->depthstencil_format),
			.stencilAttachmentFormat = VulkanFormatFromFormat_(desc->depthstencil_format),
			.viewMask = 0, // TODO
		},
		.pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 3,
			.pDynamicStates = (VkDynamicState[]) {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
			},
		},
		.stageCount = shader_stage_count,
		.pStages = shader_stages,
		.pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexAttributeDescriptionCount = vertex_attribs_count,
			.pVertexAttributeDescriptions = vertex_attribs,
			.vertexBindingDescriptionCount = vertex_bindings_count,
			.pVertexBindingDescriptions = vertex_bindings,
		},
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		},
		.pViewportState = &(VkPipelineViewportStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = desc->rendertarget_count,
			.scissorCount = desc->rendertarget_count,
		},
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = (desc->rast_fill_mode == R4_FillMode_Solid) ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE,
			.lineWidth = 1.0f,
			.cullMode =
				(desc->rast_cull_mode == R4_CullMode_None) ? VK_CULL_MODE_NONE :
				(desc->rast_cull_mode == R4_CullMode_Back) ? VK_CULL_MODE_BACK_BIT :
				(desc->rast_cull_mode == R4_CullMode_Front) ? VK_CULL_MODE_FRONT_BIT :
				0,
			.frontFace = (desc->rast_cw_frontface) ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE,
		},
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.minSampleShading = 1.0f,
		},
		.pDepthStencilState = NULL,
		.pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = desc->rendertarget_count,
			.pAttachments = (VkPipelineColorBlendAttachmentState[8]) {
				[0] = {
					.blendEnable = desc->blend_rendertargets[0].enable_blend,
					.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
				},
			},
		},
		.layout = desc->pipeline_layout->vk.layout,
		.basePipelineIndex = -1,
	};
	vkr = vkCreateGraphicsPipelines(ctx->device, NULL, 1, &pipeline_desc, NULL, &pipeline);
	if (shader_vs)
		vkDestroyShaderModule(ctx->device, shader_vs, NULL);
	if (shader_ps)
		vkDestroyShaderModule(ctx->device, shader_ps, NULL);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_Pipeline) {};

	return (R4_Pipeline) {
		.vk.pso = pipeline,
	};
}

API R4_Pipeline
R4_MakeComputePipeline(R4_Context* ctx, R4_Result* r, R4_ComputePipelineDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkPipeline pipeline = NULL;
	// TODO

	return (R4_Pipeline) {
		.vk.pso = pipeline,
	};
}

API R4_Pipeline
R4_MakeMeshPipeline(R4_Context* ctx, R4_Result* r, R4_MeshPipelineDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkPipeline pipeline = NULL;
	// TODO

	return (R4_Pipeline) {
		.vk.pso = pipeline,
	};
}

API void
R4_FreePipeline(R4_Context* ctx, R4_Pipeline* pipeline)
{
	Trace();
	if (pipeline->vk.pso)
		vkDestroyPipeline(ctx->device, pipeline->vk.pso, NULL);
	*pipeline = (R4_Pipeline) {};
}

// =============================================================================
// =============================================================================
// Descriptor heap & descriptor sets
API R4_DescriptorHeap
R4_MakeDescriptorHeap(R4_Context* ctx, R4_Result* r, R4_DescriptorHeapDesc const* desc)
{
	Trace();
	SafeAssert(desc->buffering_count >= 0 && desc->buffering_count <= 4);
	SafeAssert(desc->max_set_count >= 0);
	VkResult vkr;
	VkDescriptorPool pool = NULL;

	VkDescriptorPoolSize pool_sizes[ArrayLength(desc->pools)] = {};
	intz pool_sizes_count = 0;
	for (intz i = 0; i < ArrayLength(desc->pools); ++i)
	{
		R4_DescriptorHeapTypePool const* type_pool = &desc->pools[i];
		if (!type_pool->type)
			break;
		SafeAssert(type_pool->count >= 0);
		pool_sizes[pool_sizes_count++] = (VkDescriptorPoolSize) {
			.descriptorCount = (uint32)type_pool->count,
			.type = VulkanDescriptorTypeFromType_(type_pool->type),
		};
	}

	int32 max_sets = desc->buffering_count * desc->max_set_count;
	SafeAssert(max_sets >= 0);
	VkDescriptorPoolCreateInfo pool_desc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = (uint32)pool_sizes_count,
		.pPoolSizes = pool_sizes,
		.maxSets = (uint32)max_sets,
	};
	vkr = vkCreateDescriptorPool(ctx->device, &pool_desc, NULL, &pool);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_DescriptorHeap) {};

	return (R4_DescriptorHeap) {
		.vk.pool = pool,
	};
}

API void
R4_FreeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap)
{
	Trace();
	// TODO
}

API R4_DescriptorSet
R4_MakeDescriptorSet(R4_Context* ctx, R4_Result* r, R4_DescriptorSetDesc const* desc)
{
	Trace();
	VkResult vkr;
	VkDescriptorSet set = NULL;

	VkDescriptorSetAllocateInfo set_desc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = desc->descriptor_heap->vk.pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &desc->bind_layout->vk.layout,
	};
	vkr = vkAllocateDescriptorSets(ctx->device, &set_desc, &set);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_DescriptorSet) {};

	return (R4_DescriptorSet) {
		.vk.set = set,
	};
}

API void
R4_FreeDescriptorSet(R4_Context* ctx, R4_DescriptorSet* descriptor_set)
{
	Trace();
	// TODO
}

API void
R4_UpdateDescriptorSets(R4_Context* ctx, intz write_count, R4_DescriptorSetWrite const writes[], intz copy_count, R4_DescriptorSetCopy const copies[])
{
	Trace();
	SafeAssert(write_count >= 0 && write_count <= UINT32_MAX);
	SafeAssert(copy_count >= 0 && copy_count <= UINT32_MAX);
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkWriteDescriptorSet* vkwrites = ArenaPushArray(scratch.arena, VkWriteDescriptorSet, write_count);
	VkCopyDescriptorSet* vkcopies = ArenaPushArray(scratch.arena, VkCopyDescriptorSet, copy_count);

	for (intz i = 0; i < write_count; ++i)
	{
		R4_DescriptorSetWrite const* write = &writes[i];
		SafeAssert(write->dst_binding >= 0);
		SafeAssert(write->dst_array_element >= 0);
		SafeAssert(write->count >= 0);
		VkDescriptorBufferInfo* vkbuffers = NULL;
		VkDescriptorImageInfo* vkimages = NULL;

		if (write->buffers)
		{
			vkbuffers = ArenaPushArray(scratch.arena, VkDescriptorBufferInfo, write->count);
			for (intz j = 0; j < write->count; ++j)
			{
				SafeAssert(write->buffers[j].offset >= 0);
				SafeAssert(write->buffers[j].size >= 0);
				vkbuffers[i] = (VkDescriptorBufferInfo) {
					.buffer = write->buffers[j].buffer->vk.buffer,
					.offset = (uint64)write->buffers[j].offset,
					.range = (uint64)write->buffers[j].size,
				};
			}
		}
		else if (write->images)
		{
			vkimages = ArenaPushArray(scratch.arena, VkDescriptorImageInfo, write->count);
			for (intz j = 0; j < write->count; ++j)
			{
				vkimages[j] = (VkDescriptorImageInfo) {
					.imageView = write->images[j].image_view->vk.view,
					.imageLayout = VulkanImageLayoutFromResourceState_(write->images[j].state),
				};
			}
		}
		else if (write->samplers)
		{
			vkimages = ArenaPushArray(scratch.arena, VkDescriptorImageInfo, write->count);
			for (intz j = 0; j < write->count; ++j)
			{
				vkimages[j] = (VkDescriptorImageInfo) {
					.sampler = write->samplers[j]->vk.sampler,
				};
			}
		}
		vkwrites[i] = (VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = write->dst_set->vk.set,
			.dstBinding = (uint32)write->dst_binding,
			.dstArrayElement = (uint32)write->dst_array_element,
			.descriptorType = VulkanDescriptorTypeFromType_(write->type),
			.descriptorCount = (uint32)write->count,
			.pBufferInfo = vkbuffers,
			.pImageInfo = vkimages,
		};
	}

	for (intz i = 0; i < copy_count; ++i)
	{
		// TODO
	}

	vkUpdateDescriptorSets(ctx->device, write_count, vkwrites, copy_count, vkcopies);
	ArenaRestore(scratch);
}

API void
R4_ResetDescriptorHeapBuffering(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap, int32 buffer_index)
{
	Trace();
	// NOTE(ljre): nothing to do
}

// =============================================================================
// =============================================================================
// Command allocator & command lists
API R4_CommandAllocator
R4_MakeCommandAllocator(R4_Context* ctx, R4_Result* r, R4_Queue* target_queue)
{
	Trace();
	VkResult vkr;
	VkCommandPool cmdpool = NULL;
	VkCommandPoolCreateInfo cmdpool_desc = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = target_queue->vk.family_index,
	};
	vkr = vkCreateCommandPool(ctx->device, &cmdpool_desc, NULL, &cmdpool);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_CommandAllocator) {};
	return (R4_CommandAllocator) {
		.vk.pool = cmdpool,
	};
}

API void
R4_FreeCommandAllocator(R4_Context* ctx, R4_CommandAllocator* cmd_allocator)
{
	Trace();
}

API R4_CommandList
R4_MakeCommandList(R4_Context* ctx, R4_Result* r, R4_CommandListType type, R4_CommandAllocator* allocator)
{
	Trace();
	VkResult vkr;
	VkCommandBuffer cmdbuffer = NULL;
	VkCommandBufferAllocateInfo cmdbuffer_desc = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandBufferCount = 1,
		.commandPool = allocator->vk.pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	};
	vkr = vkAllocateCommandBuffers(ctx->device, &cmdbuffer_desc, &cmdbuffer);
	if (!CheckResult_(ctx, vkr, r))
		return (R4_CommandList) {};
	return (R4_CommandList) {
		.vk.buffer = cmdbuffer,
	};
}

API void
R4_FreeCommandList(R4_Context* ctx, R4_CommandAllocator* cmdalloc, R4_CommandList* cmd_list)
{
	Trace();
}

// =============================================================================
// =============================================================================
// Command list recording
API void
R4_ResetCommandAllocator(R4_Context* ctx, R4_Result* r, R4_CommandAllocator* cmd_allocator)
{
	Trace();
	VkResult vkr;
	vkr = vkResetCommandPool(ctx->device, cmd_allocator->vk.pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
	if (!CheckResult_(ctx, vkr, r))
		return;
}

API void
R4_BeginCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist, R4_CommandAllocator* cmd_allocator)
{
	Trace();
	VkResult vkr;
	vkr = vkResetCommandBuffer(cmdlist->vk.buffer, 0);
	if (!CheckResult_(ctx, vkr, r))
		return;
	VkCommandBufferBeginInfo begin_desc = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	vkr = vkBeginCommandBuffer(cmdlist->vk.buffer, &begin_desc);
	if (!CheckResult_(ctx, vkr, r))
		return;
}

API void
R4_EndCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist)
{
	Trace();
	VkResult vkr;
	vkr = vkEndCommandBuffer(cmdlist->vk.buffer);
	if (!CheckResult_(ctx, vkr, r))
		return;
}

API void
R4_CmdBeginRenderpass(R4_Context* ctx, R4_CommandList* cmdlist, R4_Renderpass const* renderpass)
{
	Trace();

	static VkAttachmentStoreOp const store_table[] = {
		[R4_AttachmentStoreOp_Null] = 0,
		[R4_AttachmentStoreOp_Store] = VK_ATTACHMENT_STORE_OP_STORE,
		[R4_AttachmentStoreOp_DontCare] = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	};
	static VkAttachmentLoadOp const load_table[] = {
		[R4_AttachmentLoadOp_Null] = 0,
		[R4_AttachmentLoadOp_Clear] = VK_ATTACHMENT_LOAD_OP_CLEAR,
		[R4_AttachmentLoadOp_Load] = VK_ATTACHMENT_LOAD_OP_LOAD,
		[R4_AttachmentLoadOp_DontCare] = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	};

	VkRenderingAttachmentInfo color_attachments[8] = {};
	intsize color_attachment_count = 0;
	// TODO
	VkRenderingAttachmentInfo depth_attachment = {};
	VkRenderingAttachmentInfo stencil_attachment = {};
	bool has_depth = false;
	bool has_stencil = false;
	// TODO

	for (intsize i = 0; i < ArrayLength(renderpass->render_targets); ++i)
	{
		R4_RenderpassRenderTarget const* render_target = &renderpass->render_targets[i];
		if (!render_target->render_target_view)
			break;
		R4_VK_RenderTargetView* impl = FetchFromHandlePool_(&ctx->hpool_rtv, render_target->render_target_view, NULL);
		if (CheckHandleFetch_(ctx, impl, NULL))
			color_attachments[color_attachment_count++] = (VkRenderingAttachmentInfo) {
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.loadOp = load_table[render_target->load],
				.storeOp = store_table[render_target->store],
				.imageView = impl->image_view,
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.clearValue.color.float32 = {
					render_target->clear_color[0],
					render_target->clear_color[1],
					render_target->clear_color[2],
					render_target->clear_color[3],
				},
			};
	}

	if (renderpass->depth_stencil.depth_stencil_view)
	{
		R4_RenderpassDepthStencil const* depth_stencil = &renderpass->depth_stencil;
		R4_VK_DepthStencilView* impl = FetchFromHandlePool_(&ctx->hpool_dsv, depth_stencil->depth_stencil_view, NULL);
		if (!renderpass->no_depth)
			has_depth = true;
		if (!renderpass->no_stencil)
			has_stencil = true;

		stencil_attachment = depth_attachment = (VkRenderingAttachmentInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.loadOp = load_table[depth_stencil->load],
			.storeOp = store_table[depth_stencil->store],
			.imageView = impl->image_view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.clearValue.depthStencil = {
				.depth = depth_stencil->clear_depth,
				.stencil = depth_stencil->clear_stencil,
			},
		};
	}

	SafeAssert(renderpass->render_area.width  >= 0);
	SafeAssert(renderpass->render_area.height >= 0);
	VkRenderingInfo rendering_desc = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.colorAttachmentCount = color_attachment_count,
		.pColorAttachments = color_attachments,
		.pDepthAttachment = has_depth ? &depth_attachment : NULL,
		.pStencilAttachment = has_stencil ? &stencil_attachment : NULL,
		.layerCount = 1,
		.renderArea = {
			.offset = { renderpass->render_area.x, renderpass->render_area.y },
			.extent = { (uint32)renderpass->render_area.width, (uint32)renderpass->render_area.height },
		},
	};
	vkCmdBeginRendering(cmdlist->vk.buffer, &rendering_desc);
}

API void
R4_CmdEndRenderpass(R4_Context* ctx, R4_CommandList* cmdlist)
{
	Trace();
	vkCmdEndRendering(cmdlist->vk.buffer);
}

API void
R4_CmdResourceBarrier(R4_CommandList* cmdlist, R4_ResourceBarriers const* barriers)
{
	Trace();
	SafeAssert(barriers->buffer_barrier_count >= 0 && barriers->buffer_barrier_count <= UINT32_MAX);
	SafeAssert(barriers->image_barrier_count >= 0 && barriers->image_barrier_count <= UINT32_MAX);
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkBufferMemoryBarrier2* buffer_barriers = ArenaPushArray(scratch.arena, VkBufferMemoryBarrier2, barriers->buffer_barrier_count);
	VkImageMemoryBarrier2* image_barriers = ArenaPushArray(scratch.arena, VkImageMemoryBarrier2, barriers->image_barrier_count);

	for (intz i = 0; i < barriers->buffer_barrier_count; ++i)
	{
		R4_BufferBarrier const* barrier = &barriers->buffer_barriers[i];
		buffer_barriers[i] = (VkBufferMemoryBarrier2) {
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.buffer = barrier->buffer->vk.buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
			.srcAccessMask = VulkanAccessMaskFromAccessMask_(barrier->from_access),
			.dstAccessMask = VulkanAccessMaskFromAccessMask_(barrier->to_access),
			.srcStageMask = VulkanStageMaskFromStageMask_(barrier->from_stage),
			.dstStageMask = VulkanStageMaskFromStageMask_(barrier->to_stage),
		};
	}

	for (intz i = 0; i < barriers->image_barrier_count; ++i)
	{
		R4_ImageBarrier const* barrier = &barriers->image_barriers[i];
		SafeAssert(barrier->subresource >= 0);
		image_barriers[i] = (VkImageMemoryBarrier2) {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = barrier->image->vk.image,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1,
				.levelCount = 1,
				.baseArrayLayer = (uint32)barrier->subresource,
			},
			.oldLayout = VulkanImageLayoutFromResourceState_(barrier->from_state),
			.newLayout = VulkanImageLayoutFromResourceState_(barrier->to_state),
			.srcAccessMask = VulkanAccessMaskFromAccessMask_(barrier->from_access),
			.dstAccessMask = VulkanAccessMaskFromAccessMask_(barrier->to_access),
			.srcStageMask = VulkanStageMaskFromStageMask_(barrier->from_stage),
			.dstStageMask = VulkanStageMaskFromStageMask_(barrier->to_stage),
		};
	}

	VkDependencyInfo dependency_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.bufferMemoryBarrierCount = (uint32)barriers->buffer_barrier_count,
		.pBufferMemoryBarriers = buffer_barriers,
		.imageMemoryBarrierCount = (uint32)barriers->image_barrier_count,
		.pImageMemoryBarriers = image_barriers,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = NULL,
	};
	vkCmdPipelineBarrier2(cmdlist->vk.buffer, &dependency_info);
	ArenaRestore(scratch);
}

API void
R4_CmdSetDescriptorHeap(R4_CommandList* cmdlist, R4_DescriptorHeap* heap)
{
	Trace();
	// NOTE(ljre): nothing is needed
}

API void
R4_CmdSetPrimitiveType(R4_CommandList* cmdlist, R4_PrimitiveType type)
{
	Trace();
	vkCmdSetPrimitiveTopology(cmdlist->vk.buffer, VulkanPrimitiveTopologyFromType_(type));
}

API void
R4_CmdSetViewports(R4_CommandList* cmdlist, intz count, R4_Viewport const viewports[])
{
	Trace();
	SafeAssert(count > 0 && count <= 8);
	VkViewport vkviewports[8] = {};
	for (intz i = 0; i < count; ++i)
	{
		R4_Viewport const* viewport = &viewports[i];
		vkviewports[i] = (VkViewport) {
			.x = viewport->x,
			.y = viewport->y + viewport->height,
			.width = viewport->width,
			.height = -viewport->height,
			.minDepth = viewport->min_depth,
			.maxDepth = viewport->max_depth,
		};
	}
	vkCmdSetViewport(cmdlist->vk.buffer, 0, (uint32)count, vkviewports);
}

API void
R4_CmdSetScissors(R4_CommandList* cmdlist, intz count, R4_Rect const rects[])
{
	Trace();
	SafeAssert(count > 0 && count <= 8);
	VkRect2D vkrects[8] = {};
	for (intsize i = 0; i < count; ++i)
	{
		SafeAssert(rects[i].width >= 0);
		SafeAssert(rects[i].height >= 0);
		vkrects[i] = (VkRect2D) {
			.offset.x = rects[i].x,
			.offset.y = rects[i].y,
			.extent.width = (uint32)rects[i].width,
			.extent.height = (uint32)rects[i].height,
		};
	}
	vkCmdSetScissor(cmdlist->vk.buffer, 0, count, vkrects);
}

API void
R4_CmdSetPipeline(R4_CommandList* cmdlist, R4_Pipeline* pipeline)
{
	Trace();
	vkCmdBindPipeline(cmdlist->vk.buffer, pipeline->vk.bind_point, pipeline->vk.pso);
}

API void
R4_CmdSetPipelineLayout(R4_CommandList* cmdlist, R4_PipelineLayout* pipeline_layout)
{
	Trace();
	// NOTE(ljre): nothing to do
}

API void
R4_CmdSetVertexBuffers(R4_CommandList* cmdlist, intz first_slot, intz count, R4_VertexBuffer const buffers[])
{
	Trace();
	SafeAssert(first_slot >= 0 && first_slot <= UINT32_MAX);
	SafeAssert(count >= 0 && count <= UINT32_MAX);
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkBuffer* vkbuffers = ArenaPushArray(scratch.arena, VkBuffer, count);
	VkDeviceSize* vkoffsets = ArenaPushArray(scratch.arena, VkDeviceSize, count);
	VkDeviceSize* vkstrides = ArenaPushArray(scratch.arena, VkDeviceSize, count);
	VkDeviceSize* vksizes = ArenaPushArray(scratch.arena, VkDeviceSize, count);
	for (intsize i = 0; i < count; ++i)
	{
		SafeAssert(buffers[i].offset >= 0);
		SafeAssert(buffers[i].stride >= 0);
		SafeAssert(buffers[i].size >= 0);
		vkbuffers[i] = buffers[i].buffer->vk.buffer;
		vkoffsets[i] = (uint64)buffers[i].offset;
		vkstrides[i] = (uint64)buffers[i].stride;
		vksizes[i] = (uint64)buffers[i].size;
	}
	vkCmdBindVertexBuffers2(cmdlist->vk.buffer, (uint32)first_slot, (uint32)count, vkbuffers, vkoffsets, vksizes, vkstrides);
	ArenaRestore(scratch);
}

API void
R4_CmdSetIndexBuffer(R4_CommandList* cmdlist, R4_IndexBuffer const* buffer)
{
	Trace();
	SafeAssert(buffer->offset >= 0);
	VkIndexType type = VK_INDEX_TYPE_UINT16;
	if (buffer->index_format == R4_Format_R32_UInt)
		type = VK_INDEX_TYPE_UINT32;
	vkCmdBindIndexBuffer(cmdlist->vk.buffer, buffer->buffer->vk.buffer, (uint64)buffer->offset, type);
}

API void
R4_CmdPushConstants(R4_CommandList* cmdlist, R4_PushConstant const* push_constant)
{
	Trace();
	SafeAssert(push_constant->offset >= 0 && push_constant->offset <= UINT32_MAX/4);
	SafeAssert(push_constant->count >= 0 && push_constant->count <= UINT32_MAX/4);
	vkCmdPushConstants(
		cmdlist->vk.buffer,
		push_constant->pipeline_layout->vk.layout,
		VulkanStageFlagsFromVisibility_(push_constant->shader_visibility),
		(uint32)(4 * push_constant->offset),
		(uint32)(4 * push_constant->count),
		push_constant->u32_constants);
}

API void
R4_CmdSetDescriptorSets(R4_CommandList* cmdlist, R4_DescriptorSets const* sets)
{
	Trace();
	SafeAssert(sets->first_set >= 0);
	SafeAssert(sets->count >= 0);
	SafeAssert(sets->dynamic_offset_count >= 0);
	for (intz i = 0; i < sets->dynamic_offset_count; ++i)
		SafeAssert(sets->dynamic_offsets[i] >= 0);
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkDescriptorSet* vksets = ArenaPushArray(scratch.arena, VkDescriptorSet, sets->count);
	for (intz i = 0; i < sets->count; ++i)
		vksets[i] = sets->sets[i]->vk.set;

	vkCmdBindDescriptorSets(
		cmdlist->vk.buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		sets->pipeline_layout->vk.layout,
		(uint32)sets->first_set,
		(uint32)sets->count,
		vksets,
		(uint32)sets->dynamic_offset_count,
		(uint32 const*)sets->dynamic_offsets);
	ArenaRestore(scratch);
}

API void R4_CmdComputeSetPipelineLayout(R4_CommandList* cmdlist, R4_PipelineLayout* pipeline_layout);
API void R4_CmdComputePushConstants    (R4_CommandList* cmdlist, R4_PushConstant const* push_constant);
API void R4_CmdComputeSetDescriptorSet (R4_CommandList* cmdlist, R4_DescriptorSet* descriptor_set);

API void
R4_CmdDraw(R4_CommandList* cmdlist, int64 start_vertex, int64 vertex_count, int64 start_instance, int64 instance_count)
{
	Trace();
	SafeAssert(start_vertex >= 0 && start_vertex <= UINT32_MAX);
	SafeAssert(vertex_count >= 0 && vertex_count <= UINT32_MAX);
	SafeAssert(start_instance >= 0 && start_instance <= UINT32_MAX);
	SafeAssert(instance_count >= 0 && instance_count <= UINT32_MAX);
	vkCmdDraw(cmdlist->vk.buffer, (uint32)vertex_count, (uint32)instance_count, (uint32)start_vertex, (uint32)start_instance);
}

API void
R4_CmdDrawIndexed(R4_CommandList* cmdlist, int64 start_index, int64 index_count, int64 start_instance, int64 instance_count, int64 base_vertex)
{
	Trace();
	SafeAssert(start_index >= 0 && start_index <= UINT32_MAX);
	SafeAssert(index_count >= 0 && index_count <= UINT32_MAX);
	SafeAssert(start_instance >= 0 && start_instance <= UINT32_MAX);
	SafeAssert(instance_count >= 0 && instance_count <= UINT32_MAX);
	SafeAssert(base_vertex >= INT32_MIN && base_vertex <= INT32_MAX);
	vkCmdDrawIndexed(cmdlist->vk.buffer, (uint32)index_count, (uint32)instance_count, (uint32)start_index, (int32)base_vertex, (uint32)start_instance);
}

API void R4_CmdDispatch            (R4_CommandList* cmdlist, int64 x, int64 y, int64 z);
API void R4_CmdDispatchMesh        (R4_CommandList* cmdlist, int64 x, int64 y, int64 z);

API void
R4_CmdCopyBuffer(R4_CommandList* cmdlist, R4_Buffer* dest, R4_Buffer* source, intz region_count, R4_BufferCopyRegion const regions[])
{
	Trace();
	SafeAssert(region_count >= 0 && region_count <= UINT32_MAX);
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkBufferCopy* vkregions = ArenaPushArray(scratch.arena, VkBufferCopy, region_count);
	for (intz i = 0; i < region_count; ++i)
	{
		R4_BufferCopyRegion const* region = &regions[i];
		SafeAssert(region->src_offset >= 0);
		SafeAssert(region->dst_offset >= 0);
		SafeAssert(region->size >= 0);
		vkregions[i] = (VkBufferCopy) {
			.dstOffset = (uint64)region->dst_offset,
			.srcOffset = (uint64)region->src_offset,
			.size = (uint64)region->size,
		};
	}
	vkCmdCopyBuffer(cmdlist->vk.buffer, source->vk.buffer, dest->vk.buffer, (uint32)region_count, vkregions);
	ArenaRestore(scratch);
}

API void
R4_CmdCopyImage(R4_CommandList* cmdlist, R4_Image* dest, R4_Image* source, intz region_count, R4_ImageCopyRegion const regions[])
{
	Trace();
	SafeAssert(region_count >= 0 && region_count <= UINT32_MAX);
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkImageCopy* vkregions = ArenaPushArray(scratch.arena, VkImageCopy, region_count);
	for (intz i = 0; i < region_count; ++i)
	{
		R4_ImageCopyRegion const* region = &regions[i];
		SafeAssert(region->src_subresource >= 0);
		SafeAssert(region->dst_subresource >= 0);
		SafeAssert(region->width >= 0);
		SafeAssert(region->height >= 0);
		SafeAssert(region->depth >= 0);
		vkregions[i] = (VkImageCopy) {
			.srcOffset = { region->src_x, region->src_y, region->src_z },
			.dstOffset = { region->dst_x, region->dst_y, region->dst_z },
			.extent = {
				.width = (uint32)region->width,
				.height = (uint32)region->height,
				.depth = (uint32)(region->depth ? region->depth : 1),
			},
			.srcSubresource = {
				.baseArrayLayer = (uint32)region->src_subresource,
				.layerCount = 1,
				.mipLevel = 0, // TODO
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			},
			.dstSubresource = {
				.baseArrayLayer = (uint32)region->dst_subresource,
				.layerCount = 1,
				.mipLevel = 0, // TODO
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			},
		};
	}
	vkCmdCopyImage(
		cmdlist->vk.buffer,
		source->vk.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dest->vk.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		(uint32)region_count, vkregions);
	ArenaRestore(scratch);
}

API void
R4_CmdCopyBufferToImage(R4_CommandList* cmdlist, R4_Image* dest, R4_Buffer* source, intz region_count, R4_BufferImageCopyRegion const regions[])
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkBufferImageCopy* vkregions = ArenaPushArray(scratch.arena, VkBufferImageCopy, region_count);
	for (intz i = 0; i < region_count; ++i)
	{
		R4_BufferImageCopyRegion const* region = &regions[i];
		SafeAssert(region->image_width >= 0);
		SafeAssert(region->image_height >= 0);
		SafeAssert(region->image_depth >= 0);
		SafeAssert(region->buffer_offset >= 0);
		SafeAssert(region->buffer_width_in_pixels >= 0);
		SafeAssert(region->buffer_height_in_pixels >= 0);
		SafeAssert(region->image_subresource >= 0);
		vkregions[i] = (VkBufferImageCopy) {
			.bufferOffset = (uint64)region->buffer_offset,
			.bufferRowLength = (uint32)region->buffer_width_in_pixels,
			.bufferImageHeight = (uint32)region->buffer_height_in_pixels,
			.imageSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseArrayLayer = (uint32)region->image_subresource,
				.layerCount = 1,
			},
			.imageOffset = { region->image_x, region->image_y, region->image_z },
			.imageExtent = {
				(uint32)region->image_width,
				(uint32)region->image_height,
				(uint32)(region->image_depth ? region->image_depth : 1),
			},
		};
	}
	vkCmdCopyBufferToImage(
		cmdlist->vk.buffer,
		source->vk.buffer,
		dest->vk.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		region_count, vkregions);
	ArenaRestore(scratch);
}
