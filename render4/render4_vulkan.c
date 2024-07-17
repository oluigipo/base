#include "common.h"
#include "api_os.h"
#include "api_render4.h"

#ifdef CONFIG_OS_WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include "api_os_win32.h"
#	define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

struct R4_Context
{
	VkInstance instance;
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkDebugUtilsMessengerEXT debug_messenger;
	uint64 memory_types[4];

	alignas(64) VkResult device_lost_reason;
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
CheckResult_(R4_Context* ctx, VkResult r)
{
	if (r != VK_SUCCESS)
	{
		__atomic_store_n(&ctx->device_lost_reason, r, __ATOMIC_RELEASE);
		return false;
	}
	return true;
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

static VkImageLayout
VulkanImageLayoutFromResourceState_(uint32 flags)
{
	VkImageLayout result = 0;

	if (!flags)
		return VK_IMAGE_LAYOUT_UNDEFINED;
	if (flags == R4_ResourceState_Common)
		return VK_IMAGE_LAYOUT_GENERAL;
	if (flags & R4_ResourceState_Present)
		result |= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	if (flags & R4_ResourceState_RenderTarget)
		result |= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

static VkBufferCreateInfo
VulkanBufferDescFromResourceDesc_(R4_ResourceDesc const* desc)
{
	VkBufferUsageFlags usage_flags = 0;
	if (desc->usage & R4_ResourceUsageFlag_VertexBuffer)
		usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (desc->usage & R4_ResourceUsageFlag_IndexBuffer)
		usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (desc->usage & R4_ResourceUsageFlag_ConstantBuffer)
		usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (desc->usage & R4_ResourceUsageFlag_ShaderResource)
		usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (desc->usage & R4_ResourceUsageFlag_IndirectBuffer)
		usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	if (desc->usage & R4_ResourceUsageFlag_TransferSrc)
		usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (desc->usage & R4_ResourceUsageFlag_TransferDst)
		usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VkBufferCreateInfo buffer_desc = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.size = desc->width,
		.usage = usage_flags,
	};
	return buffer_desc;
}

static VkImageCreateInfo
VulkanImageDescFromResourceDesc_(R4_ResourceDesc const* desc)
{
	VkImageCreateInfo image_desc = {};
	return image_desc;
}

// =============================================================================
// =============================================================================
API R4_Context*
R4_VK_MakeContext(Arena* output_arena, R4_ContextDesc* desc)
{
	Trace();
	VkResult r;
	VkInstance instance = NULL;
	VkDebugUtilsMessengerEXT debug_messenger = NULL;
	VkPhysicalDevice physical_device = NULL;
	VkDevice device = NULL;
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));

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
	r = vkCreateInstance(&instance_desc, NULL, &instance);
	if (r != VK_SUCCESS)
		goto lbl_error;

#ifdef CONFIG_DEBUG
	PFN_vkCreateDebugUtilsMessengerEXT proc_create_debug_messenger = (void*)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
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
		r = proc_create_debug_messenger(instance, &debug_messenger_desc, NULL, &debug_messenger);
		if (r != VK_SUCCESS)
			debug_messenger = NULL;
	}
	if (!debug_messenger)
		OS_LogErr("[WARN] vulkan: debug messenger is NOT enabled\n");
#endif

	VkPhysicalDevice available_devices[32];
	uint32 available_devices_count = ArrayLength(available_devices);
	r = vkEnumeratePhysicalDevices(instance, &available_devices_count, available_devices);
	if (r != VK_SUCCESS)
		goto lbl_error;

	int32 best_device_score = -1;
	int32 best_device = -1;
	for (intsize i = 0; i < available_devices_count; ++i)
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
			score |= 0x8000;
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			score |= 0x4000;
		if (ok && score > best_device_score)
		{
			best_device = i;
			best_device_score = score;
		}
	}
	if (best_device == -1)
		goto lbl_error;
	physical_device = available_devices[best_device];

	intsize extensions_to_enable_count = 1;
	char const* extensions_to_enable[32] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	uint32 available_extensions_count = 0;
	vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extensions_count, NULL);
	VkExtensionProperties* available_extensions = ArenaPushArray(scratch.arena, VkExtensionProperties, available_extensions_count);
	vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extensions_count, available_extensions);
	for (intsize i = 0; i < available_extensions_count; ++i)
	{
		char const* extname_cstr = available_extensions[i].extensionName;
		uintsize namelen = MemoryStrnlen(extname_cstr, ArrayLength(available_extensions[i].extensionName));
		String extname = StrMake(namelen, extname_cstr);
		if (StringEquals(extname, Str(VK_EXT_MESH_SHADER_EXTENSION_NAME)) ||
			StringEquals(extname, Str(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)) ||
			StringEquals(extname, Str(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)) ||
			StringEquals(extname, Str(VK_KHR_RAY_QUERY_EXTENSION_NAME)) ||
			StringEquals(extname, Str(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)))
		{
			extensions_to_enable[extensions_to_enable_count++] = extname_cstr;
		}
		if (extensions_to_enable_count >= ArrayLength(extensions_to_enable))
			break;
	}

	VkPhysicalDeviceMemoryProperties memory_props = {};
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_props);
	intsize biggest_default_type_index = -1;
	intsize biggest_upload_type_index = -1;
	intsize biggest_readback_type_index = -1;
	VkMemoryPropertyFlagBits default_bits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VkMemoryPropertyFlagBits upload_bits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryPropertyFlagBits readback_bits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	for (intsize i = 0; i < memory_props.memoryTypeCount; ++i)
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
				!(type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
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
		goto lbl_error;
	}

	VkSurfaceKHR surfaces[ArrayLength(desc->swapchains)] = {};
	int32 present_family = -1;
	int32 everything_family = -1;
	for (intsize i = 0; i < ArrayLength(desc->swapchains); ++i)
	{
		if (!desc->swapchains[i].window)
			break;

#ifdef CONFIG_OS_WINDOWS
		VkWin32SurfaceCreateInfoKHR surface_desc = {
			.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.hwnd = OS_W32_HwndFromWindow(*desc->swapchains[i].window),
			.hinstance = GetModuleHandleW(NULL),
		};
		r = vkCreateWin32SurfaceKHR(instance, &surface_desc, NULL, &surfaces[i]);
		if (r != VK_SUCCESS)
			goto lbl_error;
#endif
	}

	VkQueueFamilyProperties queue_families[32];
	uint32 queue_families_count = ArrayLength(queue_families);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families);
	for (intsize i = 0; i < queue_families_count; ++i)
	{
		VkQueueFlags desired_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		if ((queue_families[i].queueFlags & desired_flags) == desired_flags)
			everything_family = i;

		if (present_family == -1)
		{
			VkBool32 has_present = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surfaces[0], &has_present);
			if (has_present)
				present_family = i;
		}
	}
	if (present_family == -1 || everything_family == -1)
		goto lbl_error;

	VkDeviceQueueCreateInfo device_queue_descs[] = {
		[0] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = everything_family,
			.queueCount = 1,
			.pQueuePriorities = &(float32) { 1 },
		},
		[1] = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = present_family,
			.queueCount = 1,
			.pQueuePriorities = &(float32) { 1 },
		},
	};

	uint32 queue_count = 2;
	if (present_family == everything_family)
		queue_count = 1;

	for (intsize i = 0; i < ArrayLength(desc->queues); ++i)
	{
		if (!desc->queues[i].out_queue)
			break;
		++device_queue_descs[0].queueCount;
	}

	VkDeviceCreateInfo device_desc = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = queue_count,
		.pQueueCreateInfos = device_queue_descs,
		.pEnabledFeatures = &(VkPhysicalDeviceFeatures) {
			.fullDrawIndexUint32 = VK_TRUE,
			.independentBlend = VK_TRUE,
			.robustBufferAccess = VK_TRUE,
			.sampleRateShading = VK_TRUE,
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
	r = vkCreateDevice(physical_device, &device_desc, NULL, &device);
	if (r != VK_SUCCESS)
		goto lbl_error;

	for (intsize i = 0; i < ArrayLength(desc->swapchains); ++i)
	{
		if (!desc->swapchains[i].window)
			break;

		R4_Format format = desc->swapchains[i].format;
		uint32 buffer_count = desc->swapchains[i].buffer_count;
		R4_Swapchain* out_swapchain = desc->swapchains[i].out_swapchain;
		R4_Queue* out_graphics_queue = desc->swapchains[i].out_graphics_queue;
		
		VkSurfaceCapabilitiesKHR surface_caps;
		r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surfaces[i], &surface_caps);
		if (r != VK_SUCCESS)
			goto lbl_error;
		VkSurfaceFormatKHR formats[32] = {};
		uint32 format_count = ArrayLength(formats);
		r = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surfaces[i], &format_count, formats);
		if (r != VK_SUCCESS || format_count == 0)
			goto lbl_error;
		for (intsize j = 0; j < format_count; ++j)
			if (formats[j].format == VulkanFormatFromFormat_(format))
			{
				formats[0] = formats[j];
				break;
			}
		VkPresentModeKHR present_modes[32] = {};
		uint32 present_mode_count = ArrayLength(present_modes);
		r = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surfaces[i], &present_mode_count, present_modes);
		if (r != VK_SUCCESS || present_mode_count == 0)
			goto lbl_error;
		for (intsize j = 0; j < present_mode_count; ++j)
			if (present_modes[j] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				present_modes[0] = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
		VkSwapchainCreateInfoKHR swapchain_desc = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surfaces[i],
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
		uint32 families[] = { everything_family, present_family };
		if (everything_family != present_family)
		{
			swapchain_desc.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchain_desc.queueFamilyIndexCount = ArrayLength(families);
			swapchain_desc.pQueueFamilyIndices = families;
		}
		r = vkCreateSwapchainKHR(device, &swapchain_desc, NULL, &out_swapchain->vk_swapchain);
		if (r != VK_SUCCESS)
			goto lbl_error;

		VkSemaphoreCreateInfo sem_desc = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		r = vkCreateSemaphore(device, &sem_desc, NULL, &out_swapchain->vk_sem);
		if (r != VK_SUCCESS)
			goto lbl_error;
		r = vkCreateSemaphore(device, &sem_desc, NULL, &out_swapchain->vk_image_available);
		if (r != VK_SUCCESS)
			goto lbl_error;

		out_swapchain->vk_image_index = 0;
		out_swapchain->vk_surface = surfaces[i];
		vkGetDeviceQueue(device, present_family, 0, &out_graphics_queue->vk_present_queue);
		vkGetDeviceQueue(device, everything_family, 0, &out_graphics_queue->vk_queue);
		out_graphics_queue->family_index = everything_family;
	}
	
	uint32 curr_queue_index = 0;
	for (intsize i = 0; i < ArrayLength(desc->queues); ++i)
	{
		if (!desc->queues[i].out_queue)
			break;
		R4_CommandListKind kind = desc->queues[i].kind;
		R4_Queue* out_queue = desc->queues[i].out_queue;
		if (kind == R4_CommandListKind_Compute)
			vkGetDeviceQueue(device, everything_family, curr_queue_index++, &out_queue->vk_queue);
		else if (kind == R4_CommandListKind_Copy)
			vkGetDeviceQueue(device, everything_family, curr_queue_index++, &out_queue->vk_queue);
	}

	ArenaRestore(scratch);
	return ArenaPushStructInit(output_arena, R4_Context, {
		.instance = instance,
		.device = device,
		.debug_messenger = debug_messenger,
		.memory_types = {
			[R4_HeapKind_Null] = UINT32_MAX,
			[R4_HeapKind_Default] = biggest_default_type_index,
			[R4_HeapKind_Upload] = biggest_upload_type_index,
			[R4_HeapKind_Readback] = biggest_readback_type_index,
		},
	});

lbl_error:
	ArenaRestore(scratch);
	// for (intsize i = 0; i < ArrayLength(image_views); ++i)
	// 	if (image_views[i])
	// 		vkDestroyImageView(device, image_views[i], NULL);
	// for (intsize i = 0; i < ArrayLength(images); ++i)
	// 	if (images[i])
	// 		vkDestroyImage(device, images[i], NULL);
	// if (swapchain)
	// 	vkDestroySwapchainKHR(device, swapchain, NULL);
	// if (device)
	// 	vkDestroyDevice(device, NULL);
	// if (surface)
	// 	vkDestroySurfaceKHR(instance, surface, NULL);
	// // if (debug_messenger)
	// 	// vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, NULL);
	// if (instance)
	// 	vkDestroyInstance(instance, NULL);
	return NULL;
}

API void
R4_DestroyContext(R4_Context* ctx)
{
	Trace();
	if (ctx->device)
		vkDestroyDevice(ctx->device, NULL);
	// if (ctx->debug_messenger)
		// vkDestroyDebugUtilsMessengerEXT(ctx->instance, ctx->debug_messenger, NULL);
	if (ctx->instance)
		vkDestroyInstance(ctx->instance, NULL);

	*ctx = (R4_Context) {};
}

API R4_ContextInfo
R4_QueryInfo(R4_Context* ctx)
{
	Trace();
	R4_ContextInfo info = {
		.backend_api = StrInit("Vulkan 1.1 backend"),
	};

	// TODO(ljre)

	return info;
}

API bool
R4_DeviceLost(R4_Context* ctx, R4_DeviceLostInfo* out_info)
{
	Trace();
	VkResult r = __atomic_load_n(&ctx->device_lost_reason, __ATOMIC_ACQUIRE);
	if (r != VK_SUCCESS)
	{
		out_info->api_error_code = (uint64)r;
		out_info->reason = Str("some API call was not successful");
		return true;
	}
	return false;
}

API bool
R4_RecoverDevice(R4_Context* ctx)
{
	Trace();

	// TODO(ljre)
	return false;
}

// =============================================================================
// =============================================================================
API R4_Fence
R4_MakeFence(R4_Context* ctx)
{
	Trace();
	VkResult r;
	VkFence fence = NULL;

	VkFenceCreateInfo fence_desc = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};
	r = vkCreateFence(ctx->device, &fence_desc, NULL, &fence);
	if (!CheckResult_(ctx, r))
		return (R4_Fence) {};

	return (R4_Fence) {
		.vk_fence = fence,
	};
}

API R4_CommandAllocator
R4_MakeCommandAllocator(R4_Context* ctx, R4_Queue* queue)
{
	Trace();
	VkResult r;
	VkCommandPool pool = NULL;

	VkCommandPoolCreateInfo pool_desc = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queue->family_index,
	};
	r = vkCreateCommandPool(ctx->device, &pool_desc, NULL, &pool);
	if (!CheckResult_(ctx, r))
		return (R4_CommandAllocator) {};

	return (R4_CommandAllocator) {
		.vk_pool = pool,
	};
}

API R4_CommandList
R4_MakeCommandList(R4_Context* ctx, R4_CommandListKind list_kind, R4_CommandAllocator* command_allocator)
{
	Trace();
	VkResult r;
	VkCommandBuffer buffer = NULL;

	VkCommandBufferAllocateInfo buffer_desc = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandBufferCount = 1,
		.commandPool = command_allocator->vk_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	};
	r = vkAllocateCommandBuffers(ctx->device, &buffer_desc, &buffer);
	if (!CheckResult_(ctx, r))
		return (R4_CommandList) {};

	return (R4_CommandList) {
		.vk_buffer = buffer,
	};
}

API R4_Heap
R4_MakeHeap(R4_Context* ctx, R4_HeapDesc const* desc)
{
	Trace();
	VkResult r;
	VkDeviceMemory memory = NULL;
	SafeAssert(desc->kind >= 0 && desc->kind < ArrayLength(ctx->memory_types));
	VkMemoryAllocateInfo memory_desc = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.memoryTypeIndex = ctx->memory_types[desc->kind],
		.allocationSize = AlignUp(desc->size, 64<<10),
	};
	r = vkAllocateMemory(ctx->device, &memory_desc, NULL, &memory);
	if (!CheckResult_(ctx, r))
		return (R4_Heap) {};
	return (R4_Heap) {
		.vk_address = memory,
	};
}

API R4_DescriptorHeap
R4_MakeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeapDesc const* desc)
{
	Trace();
	VkResult r;
	VkDescriptorPool pool = NULL;
	VkDescriptorPoolCreateInfo pool_desc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pPoolSizes = (VkDescriptorPoolSize[]) {

		},
	};
	r = vkCreateDescriptorPool(ctx->device, &pool_desc, NULL, &pool);
	if (!CheckResult_(ctx, r))
		return (R4_DescriptorHeap) {};
	return (R4_DescriptorHeap) {
		.vk_pool = pool,
	};
}

API R4_Resource
R4_MakePlacedResource(R4_Context* ctx, R4_PlacedResourceDesc const* desc)
{
	Trace();
	VkResult r;
	VkBuffer buffer = NULL;
	VkImage image = NULL;
	VkDeviceMemory device_memory = desc->heap->vk_address;
	VkDeviceSize offset = desc->heap_offset;

	if (desc->resource_desc.kind == R4_ResourceKind_Buffer)
	{
		VkBufferCreateInfo buffer_desc = VulkanBufferDescFromResourceDesc_(&desc->resource_desc);
		r = vkCreateBuffer(ctx->device, &buffer_desc, NULL, &buffer);
		if (!CheckResult_(ctx, r))
			return (R4_Resource) {};
		r = vkBindBufferMemory(ctx->device, buffer, desc->heap->vk_address, desc->heap_offset);
		if (!CheckResult_(ctx, r))
		{
			vkDestroyBuffer(ctx->device, buffer, NULL);
			return (R4_Resource) {};
		}
	}
	else
	{
		VkImageCreateInfo image_info = VulkanImageDescFromResourceDesc_(&desc->resource_desc);
		r = vkCreateImage(ctx->device, &image_info, NULL, &image);
		if (!CheckResult_(ctx, r))
			return (R4_Resource) {};
		r = vkBindImageMemory(ctx->device, image, desc->heap->vk_address, desc->heap_offset);
		if (!CheckResult_(ctx, r))
		{
			vkDestroyImage(ctx->device, image, NULL);
			return (R4_Resource) {};
		}
	}

	return (R4_Resource) {
		.vk_buffer = buffer,
		.vk_image = image,
		.vk_device_memory = device_memory,
		.vk_offset = offset,
	};
}

API uint32
R4_GetSwapchainBuffers(R4_Context* ctx, R4_Swapchain* swapchain, intsize image_count, R4_Resource* out_resources)
{
	Trace();
	VkResult r;
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkImage* images = ArenaPushArray(scratch.arena, VkImage, image_count);
	uint32 count = image_count;
	r = vkGetSwapchainImagesKHR(ctx->device, swapchain->vk_swapchain, &count, images);
	ArenaRestore(scratch);
	if (!CheckResult_(ctx, r))
		return 0;
	for (intsize i = 0; i < count; ++i)
	{
		out_resources[i] = (R4_Resource) {
			.vk_image = images[i],
		};
	}
	return count;
}

API R4_DescriptorHeap
R4_CreateRenderTargetViewsFromResources(R4_Context* ctx, R4_Format format, intsize resource_count, R4_Resource* resources, R4_RenderTargetView* out_rtvs)
{
	Trace();
	VkResult r;
	for (intsize i = 0; i < resource_count; ++i)
	{
		VkImageViewCreateInfo view_desc = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = resources[i].vk_image,
			.format = VulkanFormatFromFormat_(format),
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
		VkImageView view = NULL;
		r = vkCreateImageView(ctx->device, &view_desc, NULL, &view);
		if (!CheckResult_(ctx, r))
			return (R4_DescriptorHeap) {};
		out_rtvs[i] = (R4_RenderTargetView) {
			.vk_view = view,
			.vk_image = resources[i].vk_image,
		};
	}
	return (R4_DescriptorHeap) {};
}

API R4_RootSignature
R4_MakeRootSignature(R4_Context* ctx, R4_RootSignatureDesc const* desc)
{
	Trace();
	VkResult r;
	VkPipelineLayout layout = NULL;
	VkPushConstantRange ranges[ArrayLength(desc->params)] = {};
	intsize ranges_count = 0;
	for (intsize i = 0; i < ArrayLength(desc->params); ++i)
	{
		R4_RootParameter const* param = &desc->params[i];
		if (!param->type)
			break;
		VkShaderStageFlags visibility = VulkanStageFlagsFromVisibility_(param->visibility);
		switch (param->type)
		{
			case R4_RootParameterType_Constants:
			{
				ranges[ranges_count++] = (VkPushConstantRange) {
					.stageFlags = visibility,
					.offset = param->constants.offset * 4,
					.size = param->constants.count * 4,
				};
			} break;
		}
	}

	VkPipelineLayoutCreateInfo layout_desc = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = ranges_count,
		.pPushConstantRanges = ranges,
	};
	r = vkCreatePipelineLayout(ctx->device, &layout_desc, NULL, &layout);
	if (!CheckResult_(ctx, r))
		return (R4_RootSignature) {};
	return (R4_RootSignature) {
		.vk_pipeline_layout = layout,
	};
}

API R4_Pipeline
R4_MakeGraphicsPipeline(R4_Context* ctx, R4_GraphicsPipelineDesc const* desc)
{
	Trace();
	VkResult r;
	VkPipeline pipeline = NULL;
	VkPipelineShaderStageCreateInfo shader_stages[2] = {};
	intsize shader_stage_count = 0;

	if (desc->vs_spirv.size)
	{
		VkShaderModule shader_vs = NULL;
		VkShaderModuleCreateInfo shader_vs_desc = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = desc->vs_spirv.size,
			.pCode = (uint32 const*)desc->vs_spirv.data,
		};
		r = vkCreateShaderModule(ctx->device, &shader_vs_desc, NULL, &shader_vs);
		if (!CheckResult_(ctx, r))
			return (R4_Pipeline) {};
		shader_stages[shader_stage_count++] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shader_vs,
			.pName = "Vertex",
		};
	}
	if (desc->ps_spirv.size)
	{
		VkShaderModule shader_ps = NULL;
		VkShaderModuleCreateInfo shader_ps_desc = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = desc->ps_spirv.size,
			.pCode = (uint32 const*)desc->ps_spirv.data,
		};
		r = vkCreateShaderModule(ctx->device, &shader_ps_desc, NULL, &shader_ps);
		if (!CheckResult_(ctx, r))
			return (R4_Pipeline) {};
		shader_stages[shader_stage_count++] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shader_ps,
			.pName = "Pixel",
		};
	}

	VkVertexInputAttributeDescription vertex_attribs[ArrayLength(desc->input_layout)] = {};
	intsize vertex_attribs_count = 0;
	VkVertexInputBindingDescription vertex_bindings[64] = {};
	intsize vertex_bindings_count = 0;
	for (intsize i = 0; i < ArrayLength(desc->input_layout); ++i)
	{
		R4_GraphicsPipelineInputLayout const* input = &desc->input_layout[i];
		if (!input->format)
			break;

		VkVertexInputRate input_rate;
		if (input->instance_step_rate)
			input_rate = VK_VERTEX_INPUT_RATE_INSTANCE;
		else
			input_rate = VK_VERTEX_INPUT_RATE_VERTEX;

		intsize binding_index = -1;
		for (intsize j = 0; j < vertex_bindings_count; ++j)
		{
			VkVertexInputBindingDescription* binding = &vertex_bindings[j];
			if (binding->binding == input->input_slot && binding->inputRate == input_rate)
				binding_index = j;
		}
		if (binding_index == -1)
		{
			VkVertexInputBindingDescription binding = {
				.binding = input->input_slot,
				.inputRate = input_rate,
				.stride = 0,
			};
			binding_index = vertex_bindings_count;
			vertex_bindings[vertex_bindings_count++] = binding;
		}

		vertex_attribs[vertex_attribs_count++] = (VkVertexInputAttributeDescription) {
			.binding = binding_index,
			.format = VulkanFormatFromFormat_(input->format),
			.location = i,
			.offset = input->byte_offset,
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
		.layout = desc->rootsig->vk_pipeline_layout,
		.basePipelineIndex = -1,
	};
	r = vkCreateGraphicsPipelines(ctx->device, NULL, 1, &pipeline_desc, NULL, &pipeline);
	if (!CheckResult_(ctx, r))
		return (R4_Pipeline) {};
	return (R4_Pipeline) {
		.vk_pso = pipeline,
		.vk_bindpoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
	};
}

API void
R4_FreeFence(R4_Context* ctx, R4_Fence* fence)
{
	Trace();
	if (fence->vk_fence)
		vkDestroyFence(ctx->device, fence->vk_fence, NULL);
	*fence = (R4_Fence) {};
}

API void
R4_FreeCommandAllocator(R4_Context* ctx, R4_CommandAllocator* allocator)
{
	Trace();
	if (allocator->vk_pool)
		vkDestroyCommandPool(ctx->device, allocator->vk_pool, NULL);
	*allocator = (R4_CommandAllocator) {};
}

API void
R4_FreeCommandList(R4_Context* ctx, R4_CommandAllocator* allocator, R4_CommandList* cmdlist)
{
	Trace();
	if (cmdlist->vk_buffer)
		vkFreeCommandBuffers(ctx->device, allocator->vk_pool, 1, &cmdlist->vk_buffer);
	*cmdlist = (R4_CommandList) {};
}

API void
R4_FreePipeline(R4_Context* ctx, R4_Pipeline* pipeline)
{
	Trace();
	if (pipeline->vk_pso)
		vkDestroyPipeline(ctx->device, pipeline->vk_pso, NULL);
	*pipeline = (R4_Pipeline) {};
}

API void
R4_FreeRootSignature(R4_Context* ctx, R4_RootSignature* rootsig)
{
	Trace();
	if (rootsig->vk_pipeline_layout)
		vkDestroyPipelineLayout(ctx->device, rootsig->vk_pipeline_layout, NULL);
	*rootsig = (R4_RootSignature) {};
}

API void
R4_FreeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeap* heap)
{
	Trace();
	if (heap->vk_pool)
		vkDestroyDescriptorPool(ctx->device, heap->vk_pool, NULL);
	*heap = (R4_DescriptorHeap) {};
}

API void
R4_FreeHeap(R4_Context* ctx, R4_Heap* heap)
{
	Trace();
	if (heap->vk_address)
		vkFreeMemory(ctx->device, heap->vk_address, NULL);
	*heap = (R4_Heap) {};
}

API void
R4_FreeResource(R4_Context* ctx, R4_Resource* resource)
{
	Trace();
	if (resource->vk_buffer)
		vkDestroyBuffer(ctx->device, resource->vk_buffer, NULL);
	if (resource->vk_image)
		vkDestroyImage(ctx->device, resource->vk_image, NULL);
	*resource = (R4_Resource) {};
}

// =============================================================================
// =============================================================================
API void
R4_MapResource(R4_Context* ctx, R4_Resource* resource, uint32 subresource, uint64 size, void** out_memory)
{
	Trace();
	VkResult r;
	r = vkMapMemory(ctx->device, resource->vk_device_memory, resource->vk_offset, size, 0, out_memory);
	if (!CheckResult_(ctx, r))
		return;
}

API void
R4_UnmapResource(R4_Context* ctx, R4_Resource* resource, uint32 subresource)
{
	Trace();
	vkUnmapMemory(ctx->device, resource->vk_device_memory);
}

API R4_MemoryRequirements
R4_GetResourceMemoryRequirements(R4_Context* ctx, R4_ResourceDesc const* desc)
{
	Trace();
	VkMemoryRequirements2 requirements = {};
	if (desc->kind == R4_ResourceKind_Buffer)
	{
		VkBufferCreateInfo buffer_desc = VulkanBufferDescFromResourceDesc_(desc);
		VkDeviceBufferMemoryRequirements requirements_desc = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS,
			.pCreateInfo = &buffer_desc,
		};
		vkGetDeviceBufferMemoryRequirements(ctx->device, &requirements_desc, &requirements);
	}
	else
	{
		VkImageCreateInfo image_desc = VulkanImageDescFromResourceDesc_(desc);
		VkDeviceImageMemoryRequirements requirements_desc = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS,
			.pCreateInfo = &image_desc,
		};
		vkGetDeviceImageMemoryRequirements(ctx->device, &requirements_desc, &requirements);
	}
	return (R4_MemoryRequirements) {
		.size = requirements.memoryRequirements.size,
		.alignment = requirements.memoryRequirements.alignment,
	};
}

// =============================================================================
// =============================================================================
API bool
R4_WaitFence(R4_Context* ctx, R4_Fence* fence, uint32 timeout_ms)
{
	Trace();
	VkResult r;
	uint64 timeout_ns;
	if (timeout_ms == UINT32_MAX)
		timeout_ns = UINT64_MAX;
	else
		timeout_ns = (uint64)timeout_ms * 1000000;
	r = vkWaitForFences(ctx->device, 1, &fence->vk_fence, VK_TRUE, timeout_ns);
	if (r == VK_TIMEOUT || r == VK_NOT_READY)
		return false;
	if (!CheckResult_(ctx, r))
		return false;
	r = vkResetFences(ctx->device, 1, &fence->vk_fence);
	CheckResult_(ctx, r);
	return true;
}

API uint32
R4_GetCurrentBackBufferIndex(R4_Context* ctx, R4_Swapchain* swapchain)
{
	Trace();
	VkResult r;
	r = vkAcquireNextImageKHR(ctx->device, swapchain->vk_swapchain, UINT64_MAX, swapchain->vk_image_available, NULL, &swapchain->vk_image_index);
	if (!CheckResult_(ctx, r))
		return UINT32_MAX;
	return swapchain->vk_image_index;
}

API void
R4_ResetCommandAllocator(R4_Context* ctx, R4_CommandAllocator* allocator)
{
	Trace();
	VkResult r;
	r = vkResetCommandPool(ctx->device, allocator->vk_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
	if (!CheckResult_(ctx, r))
		return;
}

API void
R4_BeginCommandList(R4_Context* ctx, R4_CommandList* cmdlist, R4_CommandAllocator* allocator)
{
	Trace();
	VkResult r;
	r = vkResetCommandBuffer(cmdlist->vk_buffer, 0);
	if (!CheckResult_(ctx, r))
		return;
	VkCommandBufferBeginInfo begin_desc = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	r = vkBeginCommandBuffer(cmdlist->vk_buffer, &begin_desc);
	if (!CheckResult_(ctx, r))
		return;
}

API void
R4_EndCommandList(R4_Context* ctx, R4_CommandList* cmdlist)
{
	Trace();
	VkResult r;
	r = vkEndCommandBuffer(cmdlist->vk_buffer);
	CheckResult_(ctx, r);
}

API void
R4_ExecuteCommandLists(R4_Context* ctx, R4_Queue* queue, R4_Fence* completion_fence, R4_Swapchain* swapchain_to_signal, intsize cmdlist_count, R4_CommandList* cmdlists[])
{
	Trace();
	VkResult r;
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkCommandBuffer* cmdlists_to_submit = ArenaPushArray(scratch.arena, VkCommandBuffer, cmdlist_count);
	for (intsize i = 0 ; i < cmdlist_count; ++i)
		cmdlists_to_submit[i] = cmdlists[i]->vk_buffer;
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = cmdlist_count,
		.pCommandBuffers = cmdlists_to_submit,
	};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	if (swapchain_to_signal)
	{
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &swapchain_to_signal->vk_sem;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &swapchain_to_signal->vk_image_available;
		submit_info.pWaitDstStageMask = wait_stages;
	}
	r = vkQueueSubmit(queue->vk_queue, 1, &submit_info, completion_fence->vk_fence);
	ArenaRestore(scratch);
	CheckResult_(ctx, r);
}

API void
R4_Present(R4_Context* ctx, R4_Queue* queue, R4_Swapchain* swapchain, uint32 swap_interval)
{
	Trace();
	VkResult r;
	VkPresentInfoKHR present_desc = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = (VkSemaphore[1]) { swapchain->vk_sem },
		.swapchainCount = 1,
		.pSwapchains = (VkSwapchainKHR[1]) { swapchain->vk_swapchain },
		.pImageIndices = &swapchain->vk_image_index,
	};
	r = vkQueuePresentKHR(queue->vk_present_queue, &present_desc);
	CheckResult_(ctx, r);
}

API void
R4_CmdSetPrimitiveTopology(R4_Context* ctx, R4_CommandList* cmdlist, R4_PrimitiveTopology topology)
{
	Trace();
	vkCmdSetPrimitiveTopology(cmdlist->vk_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}

API void
R4_CmdBeginRenderpass(R4_Context* ctx, R4_CommandList* cmdlist, R4_BeginRenderpassDesc const* desc)
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
	// TODO

	for (intsize i = 0; i < ArrayLength(desc->color_attachments); ++i)
	{
		R4_RenderpassAttachment const* attachment = &desc->color_attachments[i];
		if (!attachment->rendertarget)
			break;

		color_attachments[color_attachment_count++] = (VkRenderingAttachmentInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.loadOp = load_table[attachment->load],
			.storeOp = store_table[attachment->store],
			.imageView = attachment->rendertarget->vk_view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.clearValue.color.float32 = {
				attachment->clear_color[0],
				attachment->clear_color[1],
				attachment->clear_color[2],
				attachment->clear_color[3],
			},
		};
	}

	VkRenderingInfo rendering_desc = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.colorAttachmentCount = color_attachment_count,
		.pColorAttachments = color_attachments,
		.pDepthAttachment = NULL,
		.pStencilAttachment = NULL,
		.layerCount = 1,
		.renderArea = {
			.offset = { desc->render_area.x, desc->render_area.y },
			.extent = { desc->render_area.width, desc->render_area.height },
		},
	};
	vkCmdBeginRendering(cmdlist->vk_buffer, &rendering_desc);
}

API void
R4_CmdEndRenderpass(R4_Context* ctx, R4_CommandList* cmdlist)
{
	Trace();
	vkCmdEndRendering(cmdlist->vk_buffer);
}

API void
R4_CmdSetPipeline(R4_Context* ctx, R4_CommandList* cmdlist, R4_Pipeline* pipeline)
{
	Trace();
	vkCmdBindPipeline(cmdlist->vk_buffer, pipeline->vk_bindpoint, pipeline->vk_pso);
}

API void
R4_CmdSetRootSignature(R4_Context* ctx, R4_CommandList* cmdlist, R4_RootSignature* rootsig)
{
	Trace();
	// NOTE(ljre): Nothing to do. vkCmdBindPipeline already sets the root signature.
}

API void
R4_CmdSetViewports(R4_Context* ctx, R4_CommandList* cmdlist, intsize viewport_count, R4_Viewport const* viewports)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkViewport* vkviewports = ArenaPushArray(scratch.arena, VkViewport, viewport_count);
	for (intsize i = 0; i < viewport_count; ++i)
	{
		vkviewports[i] = (VkViewport) {
			.x = viewports[i].x,
			.y = viewports[i].y + viewports[i].height,
			.width = viewports[i].width,
			.height = -viewports[i].height,
			.minDepth = viewports[i].min_depth,
			.maxDepth = viewports[i].max_depth,
		};
	}
	vkCmdSetViewport(cmdlist->vk_buffer, 0, viewport_count, vkviewports);
	ArenaRestore(scratch);
}

API void
R4_CmdSetScissors(R4_Context* ctx, R4_CommandList* cmdlist, intsize scissor_count, R4_Rect const* scissors)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkRect2D* rects = ArenaPushArray(scratch.arena, VkRect2D, scissor_count);
	for (intsize i = 0; i < scissor_count; ++i)
	{
		rects[i] = (VkRect2D) {
			.offset.x = scissors[i].x,
			.offset.y = scissors[i].y,
			.extent.width = scissors[i].width,
			.extent.height = scissors[i].height,
		};
	}
	vkCmdSetScissor(cmdlist->vk_buffer, 0, scissor_count, rects);
	ArenaRestore(scratch);
}

API void
R4_CmdDraw(R4_Context* ctx, R4_CommandList* cmdlist, uint32 start_vertex, uint32 vertex_count, uint32 start_instance, uint32 instance_count)
{
	Trace();
	vkCmdDraw(cmdlist->vk_buffer, vertex_count, instance_count, start_vertex, start_instance);
}

API void
R4_CmdDrawIndexed(R4_Context* ctx, R4_CommandList* cmdlist, uint32 start_index, uint32 index_count, uint32 start_instance, uint32 instance_count, int32 base_vertex)
{
	Trace();
	vkCmdDrawIndexed(cmdlist->vk_buffer, index_count, instance_count, start_index, base_vertex, start_instance);
}

API void
R4_CmdDispatch(R4_Context* ctx, R4_CommandList* cmdlist, uint32 x, uint32 y, uint32 z)
{
	Trace();
	vkCmdDispatch(cmdlist->vk_buffer, x, y, z);
}

API void
R4_CmdSetVertexBuffers(R4_Context* ctx, R4_CommandList* cmdlist, uint32 first_slot, uint32 slot_count, R4_VertexBuffer const* buffers)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
	VkBuffer* vkbuffers = ArenaPushArray(scratch.arena, VkBuffer, slot_count);
	VkDeviceSize* vkoffsets = ArenaPushArray(scratch.arena, VkDeviceSize, slot_count);
	VkDeviceSize* vkstrides = ArenaPushArray(scratch.arena, VkDeviceSize, slot_count);
	VkDeviceSize* vksizes = ArenaPushArray(scratch.arena, VkDeviceSize, slot_count);
	for (intsize i = 0; i < slot_count; ++i)
	{
		vkbuffers[i] = buffers[i].resource->vk_buffer;
		vkoffsets[i] = buffers[i].offset;
		vkstrides[i] = buffers[i].stride;
		vksizes[i] = buffers[i].size;
	}
	vkCmdBindVertexBuffers2(cmdlist->vk_buffer, first_slot, slot_count, vkbuffers, vkoffsets, vksizes, vkstrides);
	ArenaRestore(scratch);
}

API void
R4_CmdSetIndexBuffer(R4_Context* ctx, R4_CommandList* cmdlist, R4_Resource* buffer, uint64 offset, uint32 size, R4_Format format)
{
	Trace();
	VkIndexType type = VK_INDEX_TYPE_UINT16;
	if (format == R4_Format_R32_UInt)
		type = VK_INDEX_TYPE_UINT32;
	vkCmdBindIndexBuffer(cmdlist->vk_buffer, buffer->vk_buffer, offset, type);
}

API void
R4_CmdSetGraphicsRootConstantsU32(R4_Context* ctx, R4_CommandList* cmdlist, R4_RootArgument const* arg)
{
	Trace();
	vkCmdPushConstants(
		cmdlist->vk_buffer,
		arg->rootsig->vk_pipeline_layout,
		VulkanStageFlagsFromVisibility_(arg->visibility),
		4 * arg->dest_offset,
		4 * arg->count,
		arg->u32_args);
}

API void
R4_CmdCopyBuffer(R4_Context* ctx, R4_CommandList* cmdlist, R4_Resource* dest, uint64 dest_offset, R4_Resource* source, uint64 source_offset, uint64 size)
{
	Trace();
	vkCmdCopyBuffer(cmdlist->vk_buffer, source->vk_buffer, dest->vk_buffer, 1, &(VkBufferCopy) {
		.dstOffset = dest_offset,
		.srcOffset = source_offset,
		.size = size,
	});
}

API void
R4_CmdBarrier(R4_Context* ctx, R4_CommandList *cmdlist, intsize barrier_count, const R4_ResourceBarrier *barriers)
{
	Trace();
	VkMemoryBarrier2 memory_barriers[64] = {};
	intsize memory_barrier_count = 0;
	VkImageMemoryBarrier2 image_barriers[64] = {};
	intsize image_barrier_count = 0;
	VkBufferMemoryBarrier2 buffer_barriers[64] = {};
	intsize buffer_barrier_count = 0;

	for (intsize i = 0; i < barrier_count; ++i)
	{
		R4_ResourceBarrier const* barrier = &barriers[i];
		if (!barrier->type)
			break;

		if (barrier->resource->vk_buffer)
		{
			switch (barrier->type)
			{
				case R4_BarrierType_Transition:
				{
					R4_TransitionBarrier const* transition = &barrier->transition;
					VkBufferMemoryBarrier2 vkbarrier = {
						.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
						.buffer = barrier->resource->vk_buffer,
						.offset = 0,
						.size = VK_WHOLE_SIZE,
					};

					if (transition->from == R4_ResourceState_CopyDest)
					{
						vkbarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
						vkbarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
					}

					if (transition->to == R4_ResourceState_VertexBuffer)
					{
						vkbarrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
						vkbarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
					}

					buffer_barriers[buffer_barrier_count++] = vkbarrier;
				} break;
			}
		}
		else if (barrier->resource->vk_image)
		{
			switch (barrier->type)
			{
				default: Unreachable(); break;
				case R4_BarrierType_Transition:
				{
					R4_TransitionBarrier const* transition = &barrier->transition;
					VkImageMemoryBarrier2 vkbarrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.image = barrier->resource->vk_image,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.layerCount = 1,
							.levelCount = 1,
							.baseArrayLayer = transition->subresource,
						},
						.oldLayout = VulkanImageLayoutFromResourceState_(transition->from),
						.newLayout = VulkanImageLayoutFromResourceState_(transition->to),
					};

					if (transition->from == R4_ResourceState_Null)
						vkbarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
					else if (transition->from == R4_ResourceState_RenderTarget)
					{
						vkbarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
						vkbarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
					}

					if (transition->to == R4_ResourceState_RenderTarget)
					{
						vkbarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
						vkbarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
					}
					else if (transition->to == R4_ResourceState_Present)
						vkbarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

					image_barriers[image_barrier_count++] = vkbarrier;
				} break;
			}
		}
	}

	if (memory_barrier_count || image_barrier_count || buffer_barrier_count)
	{
		VkDependencyInfo barrier_desc = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.memoryBarrierCount = memory_barrier_count,
			.pMemoryBarriers = memory_barriers,
			.imageMemoryBarrierCount = image_barrier_count,
			.pImageMemoryBarriers = image_barriers,
			.bufferMemoryBarrierCount = buffer_barrier_count,
			.pBufferMemoryBarriers = buffer_barriers,
		};
		vkCmdPipelineBarrier2(cmdlist->vk_buffer, &barrier_desc);
	}
}