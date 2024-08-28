#ifndef API_RENDER4_H
#define API_RENDER4_H
#include "common.h"

struct R4_Context typedef R4_Context;
struct OS_Window;

enum R4_ContextProfile
{
	R4_ContextProfile_Null = 0,
	R4_ContextProfile_Vulkan11,
	R4_ContextProfile_Vulkan12,
	R4_ContextProfile_Vulkan13,
	R4_ContextProfile_D3D110,
	R4_ContextProfile_D3D111,
	R4_ContextProfile_D3D120,
	R4_ContextProfile_D3D121,
	R4_ContextProfile_D3D122,
	R4_ContextProfile_WebGpu,
	R4_ContextProfile_Metal2,
	R4_ContextProfile_Metal3,
}
typedef R4_ContextProfile;

enum R4_ShaderProfile
{
	R4_ShaderProfile_Null = 0,
	R4_ShaderProfile_Spirv13,
	R4_ShaderProfile_Spirv14,
	R4_ShaderProfile_Dxil51,
	R4_ShaderProfile_Dxil60,
	R4_ShaderProfile_Dxil61,
	R4_ShaderProfile_Dxil62,
	R4_ShaderProfile_Dxil63,
	R4_ShaderProfile_Dxil64,
	R4_ShaderProfile_Dxil65,
	R4_ShaderProfile_Dxil66,
	R4_ShaderProfile_Dxil67,
}
typedef R4_ShaderProfile;

struct R4_ContextInfo
{
	String backend_api;

	R4_ContextProfile context_profile;
	R4_ShaderProfile shader_profile;

	bool has_mesh_shaders;
	bool has_raytracing;
}
typedef R4_ContextInfo;

struct R4_DeviceLostInfo
{
	String reason;
	uint64 api_error_code;
}
typedef R4_DeviceLostInfo;

enum R4_Format
{
	R4_Format_Null = 0,
	
	R4_Format_R8_UNorm,
	R4_Format_R8G8_UNorm,
	R4_Format_R8G8B8A8_UNorm,
	R4_Format_R8G8B8A8_Srgb,
	R4_Format_R8_UInt,
	R4_Format_R8G8_UInt,
	R4_Format_R8G8B8A8_UInt,
	R4_Format_R16_SNorm,
	R4_Format_R16G16_SNorm,
	R4_Format_R16G16B16A16_SNorm,
	R4_Format_R16_SInt,
	R4_Format_R16G16_SInt,
	R4_Format_R16G16B16A16_SInt,
	R4_Format_R32_UInt,

	R4_Format_R32_Float,
	R4_Format_R32G32_Float,
	R4_Format_R32G32B32_Float,
	R4_Format_R32G32B32A32_Float,
	R4_Format_R16G16_Float,
	R4_Format_R16G16B16A16_Float,

	R4_Format_D16_UNorm,
	R4_Format_D24_UNorm_S8_UInt,
}
typedef R4_Format;

enum R4_CommandListKind
{
	R4_CommandListKind_Null = 0,
	R4_CommandListKind_Graphics,
	R4_CommandListKind_Compute,
	R4_CommandListKind_Copy,
}
typedef R4_CommandListKind;

enum R4_ResourceState
{
	R4_ResourceState_Null = 0,
	R4_ResourceState_Preinitialized,
	R4_ResourceState_Common,
	R4_ResourceState_Present,
	R4_ResourceState_VertexBuffer,
	R4_ResourceState_IndexBuffer,
	R4_ResourceState_UniformBuffer,
	R4_ResourceState_ShaderStorage,
	R4_ResourceState_ShaderReadWrite,
	R4_ResourceState_SampledImage,
	R4_ResourceState_ColorAttachment,
	R4_ResourceState_DepthStencilAttachment,
	R4_ResourceState_TransferDst,
	R4_ResourceState_TransferSrc,
	R4_ResourceState_GenericReadForTransfer,
}
typedef R4_ResourceState;

struct R4_Queue typedef R4_Queue;
struct R4_Fence typedef R4_Fence;
struct R4_Event typedef R4_Event;
//struct R4_Semaphore typedef R4_Semaphore;
struct R4_CommandAllocator typedef R4_CommandAllocator;
struct R4_CommandList typedef R4_CommandList;
struct R4_DescriptorSet typedef R4_DescriptorSet;
struct R4_DescriptorHeap typedef R4_DescriptorHeap;
struct R4_RootSignature typedef R4_RootSignature;
struct R4_Pipeline typedef R4_Pipeline;
struct R4_Heap typedef R4_Heap;

struct R4_Buffer typedef R4_Buffer;
struct R4_Image typedef R4_Image;
struct R4_ImageView typedef R4_ImageView;
struct R4_Sampler typedef R4_Sampler;

struct R4_ViewHeap typedef R4_ViewHeap;
struct R4_ViewHeap
{
	struct ID3D12DescriptorHeap* d3d12_heap_rtv;
	struct ID3D12DescriptorHeap* d3d12_heap_dsv;
	struct ID3D12DescriptorHeap* d3d12_heap_cbv_srv_uav;
	struct ID3D12DescriptorHeap* d3d12_heap_sampler;
	// nothing for vk, wgpu
};

struct R4_Queue
{
	union
	{
		struct ID3D12CommandQueue* d3d12_queue;
		struct
		{
			struct VkQueue_T* vk_queue;
			struct VkQueue_T* vk_present_queue;
			uint32 family_index;
		};
		struct WGPUQueueImpl* wgpu_queue;
	};
	R4_CommandListKind kind;
};

struct R4_Fence
{
	union
	{
		struct
		{
			struct ID3D12Fence* d3d12_fence;
			void* d3d12_event;
		};
		struct VkFence_T* vk_fence;
	};
	uint64 counter;
};

struct R4_Event
{
	void* win32_event;
	struct VkEvent_T* vk_event;
};

//struct R4_Semaphore
//{
	//struct VkSemaphore_T* vk_sem;
//};

struct R4_CommandAllocator
{
	struct ID3D12CommandAllocator* d3d12_allocator;
	struct VkCommandPool_T* vk_pool;
};

struct R4_CommandList
{
	struct ID3D12GraphicsCommandList* d3d12_list;
	struct VkCommandBuffer_T* vk_buffer;
};

struct R4_DescriptorSet
{
	uint64 d3d12_gpuptr;
	uint64 d3d12_cpuptr;
	uint64 vk_device_memory;
};

struct R4_DescriptorHeap
{
	struct ID3D12DescriptorHeap* d3d12_heap;
	struct VkDescriptorPool_T* vk_pool;
};

struct R4_RootSignature
{
	struct ID3D12RootSignature* d3d12_rootsig;
	struct VkPipelineLayout_T* vk_pipeline_layout;
	struct VkDescriptorSetLayout_T* vk_descriptor_set_layout;
};

struct R4_Pipeline
{
	struct ID3D12PipelineState* d3d12_pso;
	struct VkPipeline_T* vk_pso;
	uint32 vk_bindpoint;
};

struct R4_Buffer
{
	struct ID3D12Resource* d3d12_resource;
	struct VkBuffer_T* vk_buffer;
	struct VkDeviceMemory_T* vk_device_memory;
	uint64 vk_offset;
};

struct R4_Image
{
	struct ID3D12Resource* d3d12_resource;
	struct VkImage_T* vk_image;
	struct VkDeviceMemory_T* vk_device_memory;
	uint64 vk_offset;
};

struct R4_ImageView
{
	uint64 d3d12_rtv_ptr;
	uint64 d3d12_dsv_ptr;
	uint64 d3d12_srv_uav_ptr;
	struct VkImageView_T* vk_view;
};

struct R4_Sampler
{
	uint64 d3d12_sampler_ptr;
	struct VkSampler_T* vk_sampler;
};

struct R4_Resource
{
	R4_Buffer* buffer;
	R4_Image* image;

	#ifdef __cplusplus
	R4_Resource() : buffer(NULL), image(NULL) {}
	R4_Resource(R4_Buffer* buffer) : buffer(buffer), image(NULL) {}
	R4_Resource(R4_Image* image) : buffer(NULL), image(image) {}
	#endif
}
typedef R4_Resource;

struct R4_Heap
{
	struct ID3D12Heap* d3d12_heap;
	struct VkDeviceMemory_T* vk_address;
}
typedef R4_Heap;

struct R4_GraphicsPipelineInputLayout
{
	R4_Format format;
	intz input_slot;
	intz byte_offset;
	intz instance_step_rate;
}
typedef R4_GraphicsPipelineInputLayout;

enum R4_BlendFunc
{
	R4_BlendFunc_Null = 0,
	
	R4_BlendFunc_Zero,
	R4_BlendFunc_One,
	R4_BlendFunc_SrcColor,
	R4_BlendFunc_InvSrcColor,
	R4_BlendFunc_DstColor,
	R4_BlendFunc_InvDstColor,
	R4_BlendFunc_SrcAlpha,
	R4_BlendFunc_InvSrcAlpha,
	R4_BlendFunc_DstAlpha,
	R4_BlendFunc_InvDstAlpha,
	R4_BlendFunc_SrcAlphaSat,
	R4_BlendFunc_BlendFactor,
	R4_BlendFunc_InvBlendFactor,
	R4_BlendFunc_Src1Color,
	R4_BlendFunc_InvSrc1Color,
	R4_BlendFunc_Src1Alpha,
	R4_BlendFunc_InvSrc1Alpha,
	R4_BlendFunc_AlphaFactor,
	R4_BlendFunc_InvAlphaFactor,
}
typedef R4_BlendFunc;

enum R4_BlendOp
{
	R4_BlendOp_Add = 0,
	R4_BlendOp_Subtract,
	R4_BlendOp_RevSubtract,
	R4_BlendOp_Min,
	R4_BlendOp_Max,
}
typedef R4_BlendOp;

enum R4_FillMode
{
	R4_FillMode_Solid = 0,
	R4_FillMode_Wireframe,
}
typedef R4_FillMode;

enum R4_CullMode
{
	R4_CullMode_None = 0,
	R4_CullMode_Front,
	R4_CullMode_Back,
}
typedef R4_CullMode;

enum R4_ComparisonFunc
{
	R4_ComparisonFunc_Null = 0,

	R4_ComparisonFunc_Never,
	R4_ComparisonFunc_Always,
	R4_ComparisonFunc_Equal,
	R4_ComparisonFunc_NotEqual,
	R4_ComparisonFunc_Less,
	R4_ComparisonFunc_LessEqual,
	R4_ComparisonFunc_Greater,
	R4_ComparisonFunc_GreaterEqual,
}
typedef R4_ComparisonFunc;

enum R4_PrimitiveTopology
{
	R4_PrimitiveTopology_PointList,
	R4_PrimitiveTopology_LineList,
	R4_PrimitiveTopology_LineStrip,
	R4_PrimitiveTopology_TriangleList,
	R4_PrimitiveTopology_TriangleStrip,
	R4_PrimitiveTopology_TriangleFan,
	R4_PrimitiveTopology_Patch,
}
typedef R4_PrimitiveTopology;

enum R4_LogicOp
{
	R4_LogicOp_Null = 0,
}
typedef R4_LogicOp;

struct R4_GraphicsPipelineRenderTargetBlendDesc
{
	bool enable_blend;
	bool enable_logic_op;
	R4_BlendFunc src, dst;
	R4_BlendFunc src_alpha, dst_alpha;
	R4_BlendOp op, op_alpha;
	R4_LogicOp logic_op;
	uint8 rendertarget_write_mask;
}
typedef R4_GraphicsPipelineRenderTargetBlendDesc;

struct R4_ShaderBytecode
{
	void const* data;
	intz size;
}
typedef R4_ShaderBytecode;

struct R4_GraphicsPipelineDesc
{
	R4_RootSignature const* rootsig;

	R4_ShaderBytecode vs_dxil, ps_dxil;
	R4_ShaderBytecode vs_spirv, ps_spirv;

	R4_GraphicsPipelineInputLayout input_layout[64];
	R4_PrimitiveTopology primitive_topology;

	intz sample_count;
	intz sample_quality;
	uint32 sample_mask;
	uint32 node_mask;

	R4_FillMode rast_fill_mode;
	R4_CullMode rast_cull_mode;
	int32 rast_depth_bias;
	float32 rast_depth_bias_clamp;
	float32 rast_slope_scaled_depth_bias;
	uint32 rast_forced_sample_count;
	bool rast_cw_frontface;
	bool rast_enable_depth_clip;
	bool rast_enable_multisample;
	bool rast_enable_antialiased_line;
	bool rast_enable_conservative_mode;

	bool depth_enable;
	bool depth_disable_writes;
	bool depth_enable_bounds_test;
	R4_ComparisonFunc depth_comparison_func;

	bool stencil_enable;
	uint8 stencil_read_mask;
	uint8 stencil_write_mask;

	bool blend_enable_alpha_to_coverage;
	bool blend_enable_independent_blend;
	R4_GraphicsPipelineRenderTargetBlendDesc blend_rendertargets[8];

	intz rendertarget_count;
	R4_Format rendertarget_formats[8];
	R4_Format depthstencil_format;
}
typedef R4_GraphicsPipelineDesc;

struct R4_ComputePipelineDesc
{

}
typedef R4_ComputePipelineDesc;

enum R4_ShaderVisibility
{
	R4_ShaderVisibility_All = 0,
	R4_ShaderVisibility_Vertex,
	R4_ShaderVisibility_Pixel,
	R4_ShaderVisibility_Mesh,
}
typedef R4_ShaderVisibility;

enum R4_DescriptorType
{
	R4_DescriptorType_Null = 0,
	R4_DescriptorType_Sampler,
	R4_DescriptorType_ImageSampled,
	R4_DescriptorType_ImageShaderStorage,
	R4_DescriptorType_ImageShaderReadWrite,
	R4_DescriptorType_UniformBuffer,
	R4_DescriptorType_BufferShaderStorage,
	R4_DescriptorType_BufferShaderReadWrite,
	R4_DescriptorType_DynamicUniformBuffer,
	R4_DescriptorType_DynamicBufferShaderStorage,
	R4_DescriptorType_DynamicBufferShaderReadWrite,
}
typedef R4_DescriptorType;

struct R4_RootParameter
{
	R4_ShaderVisibility visibility;
	int32 offset;
	int32 count; // in uint32s
}
typedef R4_RootParameter;

struct R4_RootSignatureDescriptorTable
{
	R4_ShaderVisibility visibility;
	R4_DescriptorType type;
	int32 offset_in_table;
	int32 count;
	int32 start_shader_slot;
}
typedef R4_RootSignatureDescriptorTable;

struct R4_RootSignatureDesc
{
	R4_RootParameter params[64];
	R4_RootSignatureDescriptorTable descriptor_tables[64];
}
typedef R4_RootSignatureDesc;

struct R4_Swapchain
{
	struct IDXGISwapChain3* d3d12_swapchain;
	struct VkSwapchainKHR_T* vk_swapchain;
	struct VkSemaphore_T* vk_sem;
	struct VkSemaphore_T* vk_image_available;
	struct VkSurfaceKHR_T* vk_surface;
	uint32 vk_image_index;
}
typedef R4_Swapchain;

enum R4_DescriptorHeapType
{
	R4_DescriptorHeapType_CbvSrvUav = 0,
	R4_DescriptorHeapType_Sampler   = 1,
	R4_DescriptorHeapType_Rtv       = 2,
	R4_DescriptorHeapType_Dsv       = 3,
	R4_DescriptorHeapType__Count,
}
typedef R4_DescriptorHeapType;

enum
{
	R4_DescriptorHeapFlag_ShaderVisible = 0x0001,
};

struct R4_DescriptorHeapDesc
{
	int64 descriptor_count;
	R4_DescriptorHeapType type;
	uint32 flags;
}
typedef R4_DescriptorHeapDesc;

enum R4_ImageDimension
{
	R4_ImageDimension_Null = 0,
	R4_ImageDimension_Texture1D,
	R4_ImageDimension_Texture2D,
	R4_ImageDimension_Texture3D,
}
typedef R4_ImageDimension;

enum
{
	R4_ResourceUsageFlag_VertexBuffer    = 1<<0,
	R4_ResourceUsageFlag_IndexBuffer     = 1<<1,
	R4_ResourceUsageFlag_ConstantBuffer  = 1<<2,
	R4_ResourceUsageFlag_IndirectBuffer  = 1<<3,
	R4_ResourceUsageFlag_ShaderStorage   = 1<<4,
	R4_ResourceUsageFlag_ShaderReadWrite = 1<<5,
	R4_ResourceUsageFlag_TransferSrc     = 1<<6,
	R4_ResourceUsageFlag_TransferDst     = 1<<7,
	R4_ResourceUsageFlag_SampledImage    = 1<<8,
	R4_ResourceUsageFlag_ColorAttachment = 1<<9,
	R4_ResourceUsageFlag_DepthStencilAttachment = 1<<10,
};

struct R4_ImageDesc
{
	R4_ImageDimension dimension;
	int64 width;
	int64 height;
	int32 depth;
	int32 mip_levels;
	R4_Format format;
	uint32 usage;
}
typedef R4_ImageDesc;

struct R4_PlacedImageDesc
{
	R4_ImageDesc image_desc;
	R4_Heap* heap;
	int64 heap_offset;
	R4_ResourceState initial_state;
	struct
	{
		R4_Format format;
		float32 color[4];
		float32 depth;
		uint8 stencil;
	} optimized_clear_value;
}
typedef R4_PlacedImageDesc;

enum R4_HeapKind
{
	R4_HeapKind_Null = 0,
	R4_HeapKind_Default,
	R4_HeapKind_Upload,
	R4_HeapKind_Readback,
}
typedef R4_HeapKind;

struct R4_HeapDesc
{
	R4_HeapKind kind;
	int64 size;
}
typedef R4_HeapDesc;

struct R4_BufferDesc
{
	int64 size;
	uint32 usage;
}
typedef R4_BufferDesc;

struct R4_PlacedBufferDesc
{
	R4_Heap* heap;
	int64 heap_offset;
	R4_ResourceState initial_state;
	R4_BufferDesc buffer_desc;
}
typedef R4_PlacedBufferDesc;

struct R4_ViewHeapDesc
{
	int64 rtv_count;
	int64 dsv_count;
	int64 cbv_srv_uav_count;
	int64 sampler_count;
}
typedef R4_ViewHeapDesc;

enum R4_ImageViewType
{
	R4_ImageViewType_Null = 0,
	R4_ImageViewType_RenderTarget,
	R4_ImageViewType_DepthStencil,
}
typedef R4_ImageViewType;

struct R4_ImageViewDesc
{
	R4_Image* image;
	R4_Format format;
	R4_ImageViewType type;
}
typedef R4_ImageViewDesc;

enum R4_Filter
{
	R4_Filter_Nearest = 0,
	R4_Filter_Linear,
	R4_Filter_Anisotropic,
}
typedef R4_Filter;

enum R4_AddressMode
{
	R4_AddressMode_Wrap = 0,
	R4_AddressMode_Mirror,
	R4_AddressMode_Clamp,
	R4_AddressMode_Border,
	R4_AddressMode_MirrorOnce,
}
typedef R4_AddressMode;

struct R4_SamplerDesc
{
	R4_Filter min_filter;
	R4_Filter mag_filter;
	R4_Filter mip_filter;
	R4_AddressMode addr_u;
	R4_AddressMode addr_v;
	R4_AddressMode addr_w;
	float32 mip_lod_bias;
	int32 max_anisotropy;
	R4_ComparisonFunc comparison_func;
	float32 border_color[4];
	float32 min_lod;
	float32 max_lod;
}
typedef R4_SamplerDesc;

struct R4_ContextDesc
{
	bool prefer_igpu_if_possible;
	struct
	{
		R4_Format format;
		intz buffer_count;
		struct OS_Window* window;
		R4_Swapchain* out_swapchain;
		R4_Queue* out_graphics_queue;
	} swapchains[32];
	struct
	{
		R4_CommandListKind kind;
		R4_Queue* out_queue;
	} queues[32];
}
typedef R4_ContextDesc;

API R4_Context* R4_D3D12_MakeContext(Allocator allocator, R4_ContextDesc* desc);
API R4_Context* R4_VK_MakeContext(Allocator allocator, R4_ContextDesc* desc);
API R4_Context* R4_WEBGPU_MakeContext(Allocator allocator, R4_ContextDesc* desc);
API R4_Context* R4_MTL_MakeContext(Allocator allocator, R4_ContextDesc* desc);

API R4_ContextInfo R4_QueryInfo(R4_Context* ctx);
API void R4_DestroyContext(R4_Context* ctx);
API bool R4_DeviceLost(R4_Context* ctx, R4_DeviceLostInfo* out_info);
API bool R4_RecoverDevice(R4_Context* ctx);

API R4_Fence R4_MakeFence(R4_Context* ctx);
API R4_CommandAllocator R4_MakeCommandAllocator(R4_Context* ctx, R4_Queue* queue);
API R4_CommandList R4_MakeCommandList(R4_Context* ctx, R4_CommandListKind list_kind, R4_CommandAllocator* command_allocator);
API R4_Pipeline R4_MakeGraphicsPipeline(R4_Context* ctx, R4_GraphicsPipelineDesc const* desc);
API R4_Pipeline R4_MakeComputePipeline(R4_Context* ctx, R4_ComputePipelineDesc const* desc);
API R4_RootSignature R4_MakeRootSignature(R4_Context* ctx, R4_RootSignatureDesc const* desc);
API R4_DescriptorHeap R4_MakeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeapDesc const* desc);
API R4_Heap R4_MakeHeap(R4_Context* ctx, R4_HeapDesc const* desc);
API R4_Image R4_MakePlacedImage(R4_Context* ctx, R4_PlacedImageDesc const* desc);
API R4_Buffer R4_MakePlacedBuffer(R4_Context* ctx, R4_PlacedBufferDesc const* desc);
API R4_ViewHeap R4_MakeViewHeap(R4_Context* ctx, R4_ViewHeapDesc const* desc);
API R4_ImageView R4_MakeImageViewAt(R4_Context* ctx, R4_ViewHeap* view_heap, int64 index, R4_ImageViewDesc const* desc);
API R4_Sampler R4_MakeSamplerAt(R4_Context* ctx, R4_ViewHeap* view_heap, int64 index, R4_SamplerDesc const* desc);

// API intz R4_BulkMakeGraphicsPipeline(R4_Context* ctx, intz count, R4_Pipeline* out_pipelines[], R4_GraphicsPipelineDesc const descs[]);

API void R4_FreeFence(R4_Context* ctx, R4_Fence* fence);
API void R4_FreeCommandAllocator(R4_Context* ctx, R4_CommandAllocator* allocator);
API void R4_FreeCommandList(R4_Context* ctx, R4_CommandAllocator* allocator, R4_CommandList* cmdlist);
API void R4_FreePipeline(R4_Context* ctx, R4_Pipeline* pipeline);
API void R4_FreeRootSignature(R4_Context* ctx, R4_RootSignature* rootsig);
API void R4_FreeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeap* heap);
API void R4_FreeHeap(R4_Context* ctx, R4_Heap* heap);
API void R4_FreeBuffer(R4_Context* ctx, R4_Buffer* buffer);
API void R4_FreeImage(R4_Context* ctx, R4_Image* image);
API void R4_FreeViewHeap(R4_Context* ctx, R4_ViewHeap* view_heap);
API void R4_FreeImageView(R4_Context* ctx, R4_ImageView* image_view);
API void R4_FreeSampler(R4_Context* ctx, R4_Sampler* sampler);

API intz R4_GetDescriptorSize(R4_Context* ctx, R4_DescriptorHeapType type);
API int64 R4_GetDescriptorHeapStart(R4_Context* ctx, R4_DescriptorHeap* heap);
API intz R4_GetSwapchainBuffers(R4_Context* ctx, R4_Swapchain* swapchain, intz image_count, R4_Image* out_images);

struct R4_MemoryRequirements
{
	int64 size;
	int64 alignment;
}
typedef R4_MemoryRequirements;

API R4_MemoryRequirements R4_GetBufferMemoryRequirements(R4_Context* ctx, R4_BufferDesc const* desc);
API R4_MemoryRequirements R4_GetImageMemoryRequirements(R4_Context* ctx, R4_ImageDesc const* desc);

struct R4_Range
{
	uint64 begin;
	uint64 end;
}
typedef R4_Range;

//API void R4_MapMemory    (R4_Context* ctx, R4_Heap* heap, uint64 offset, uint64 size);
//API void R4_UnmapMemory  (R4_Context* ctx, R4_Heap* heap);
API void R4_MapResource  (R4_Context* ctx, R4_Resource resource, uint32 subresource, uint64 size, void** out_memory);
API void R4_UnmapResource(R4_Context* ctx, R4_Resource resource, uint32 subresource);

struct R4_Viewport
{
	float32 x, y;
	float32 width, height;
	float32 min_depth, max_depth;
}
typedef R4_Viewport;

struct R4_TransitionBarrier
{
	uint32 subresource;
	R4_ResourceState from;
	R4_ResourceState to;
}
typedef R4_TransitionBarrier;

struct R4_AliasingBarrier
{
	R4_Resource to;
}
typedef R4_AliasingBarrier;

enum R4_BarrierType
{
	R4_BarrierType_Null = 0,
	R4_BarrierType_Transition,
	R4_BarrierType_Aliasing,
	R4_BarrierType_Uav,
}
typedef R4_BarrierType;

struct R4_ResourceBarrier
{
	R4_Resource resource;
	R4_BarrierType type;
	union
	{
		R4_TransitionBarrier transition;
		R4_AliasingBarrier aliasing;
		R4_Resource uav_barrier;
	};
}
typedef R4_ResourceBarrier;

enum R4_AttachmentLoadOp
{
	R4_AttachmentLoadOp_Null = 0,
	R4_AttachmentLoadOp_Load,
	R4_AttachmentLoadOp_Clear,
	R4_AttachmentLoadOp_DontCare,
}
typedef R4_AttachmentLoadOp;

enum R4_AttachmentStoreOp
{
	R4_AttachmentStoreOp_Null = 0,
	R4_AttachmentStoreOp_Store,
	R4_AttachmentStoreOp_DontCare,
}
typedef R4_AttachmentStoreOp;

struct R4_RenderpassAttachment
{
	R4_ImageView* image_view;
	R4_AttachmentLoadOp load;
	R4_AttachmentStoreOp store;
	float32 clear_color[4];
	float32 clear_depth;
	uint8 clear_stencil;
}
typedef R4_RenderpassAttachment;

struct R4_Rect
{
	int32 x, y;
	int32 width, height;
}
typedef R4_Rect;

struct R4_BeginRenderpassDesc
{
	R4_RenderpassAttachment color_attachments[8];
	R4_RenderpassAttachment depth_stencil_attachment;
	R4_Rect render_area;

	bool no_depth;
	bool no_stencil;
}
typedef R4_BeginRenderpassDesc;

struct R4_VertexBuffer
{
	R4_Buffer* buffer;
	uint64 offset;
	uint32 size;
	uint32 stride;
}
typedef R4_VertexBuffer;

struct R4_CopyRegion
{
	int64 src_x, src_y, src_z;
	int64 dst_x, dst_y, dst_z;
	int64 width, height, depth;
	int32 src_subresource;
	int32 dst_subresource;
}
typedef R4_CopyRegion;

API bool   R4_WaitFence                (R4_Context* ctx, R4_Fence* fence, uint32 timeout_ms);
API uint32 R4_GetCurrentBackBufferIndex(R4_Context* ctx, R4_Swapchain* swapchain);
API void   R4_ResetCommandAllocator    (R4_Context* ctx, R4_CommandAllocator* allocator);

API void R4_BeginCommandList     (R4_Context* ctx, R4_CommandList* cmdlist, R4_CommandAllocator* allocator);
API void R4_EndCommandList       (R4_Context* ctx, R4_CommandList* cmdlist);
API void R4_ExecuteCommandLists  (R4_Context* ctx, R4_Queue* queue, R4_Fence* completion_fence, R4_Swapchain* swapchain_to_signal, intsize cmdlist_count, R4_CommandList* cmdlists[]);
API void R4_Present              (R4_Context* ctx, R4_Queue* queue, R4_Swapchain* swapchain, uint32 sync_interval);

API void R4_CmdSetPrimitiveTopology  (R4_Context* ctx, R4_CommandList* cmdlist, R4_PrimitiveTopology topology);
//API void R4_CmdSetRenderTargets      (R4_Context* ctx, R4_CommandList* cmdlist, intsize rt_count, R4_ImageView* rt_images[], R4_ImageView* ds_image);
API void R4_CmdSetViewports          (R4_Context* ctx, R4_CommandList* cmdlist, intsize viewport_count, R4_Viewport const* viewports);
API void R4_CmdSetScissors           (R4_Context* ctx, R4_CommandList* cmdlist, intsize scissor_count, R4_Rect const* scissors);
API void R4_CmdSetPipeline           (R4_Context* ctx, R4_CommandList* cmdlist, R4_Pipeline* pipeline);
API void R4_CmdSetRootSignature      (R4_Context* ctx, R4_CommandList* cmdlist, R4_RootSignature* rootsig);
API void R4_CmdSetVertexBuffers      (R4_Context* ctx, R4_CommandList* cmdlist, intz first_slot, intz slot_count, R4_VertexBuffer const* buffers);
API void R4_CmdSetIndexBuffer        (R4_Context* ctx, R4_CommandList* cmdlist, R4_Buffer* buffer, uint64 offset, uint32 size, R4_Format format);
API void R4_CmdDraw                  (R4_Context* ctx, R4_CommandList* cmdlist, uint32 start_vertex, uint32 vertex_count, uint32 start_instance, uint32 instance_count);
API void R4_CmdDrawIndexed           (R4_Context* ctx, R4_CommandList* cmdlist, uint32 start_index, uint32 index_count, uint32 start_instance, uint32 instance_count, int32 base_vertex);
API void R4_CmdDispatch              (R4_Context* ctx, R4_CommandList* cmdlist, uint32 x, uint32 y, uint32 z);
API void R4_CmdResourceBarrier       (R4_Context* ctx, R4_CommandList* cmdlist, intz barrier_count, R4_ResourceBarrier* barriers[]);

struct R4_RootArgument
{
	R4_RootSignature* rootsig;
	R4_ShaderVisibility visibility;
	uint32 param_index;
	uint32 dest_offset;
	uint32 count;
	void const* u32_args;
}
typedef R4_RootArgument;

API void R4_CmdSetGraphicsRootConstantsU32(R4_Context* ctx, R4_CommandList* cmdlist, R4_RootArgument const* arg);

struct R4_Offset3D
{
	int32 x, y, z;
}
typedef R4_Offset3D;

struct R4_Size3D
{
	int32 x, y, z;
}
typedef R4_Size3D;

struct R4_BufferImageRegion
{
	int64 buffer_offset;
	int32 buffer_width;
	int32 buffer_height;
	R4_Format image_format;
	R4_Offset3D image_offset;
	R4_Size3D image_size;
	int32 image_subresource;
}
typedef R4_BufferImageRegion;

API void R4_CmdCopyBuffer(R4_Context* ctx, R4_CommandList* cmdlist, R4_Buffer* dest, uint64 dest_offset, R4_Buffer* source, uint64 source_offset, uint64 size);
API void R4_CmdCopyImage (R4_Context* ctx, R4_CommandList* cmdlist, R4_Image* dest, R4_Image* source, intz region_count, R4_CopyRegion const regions[]);
API void R4_CmdCopyBufferToImage(R4_Context* ctx, R4_CommandList* cmdlist, R4_Image* dest, R4_Buffer* buffer, intz region_count, R4_BufferImageRegion const regions[]);
API void R4_CmdBarrier   (R4_Context* ctx, R4_CommandList* cmdlist, intsize barrier_count, R4_ResourceBarrier const* barriers);

API void R4_CmdBeginRenderpass(R4_Context* ctx, R4_CommandList* cmdlist, R4_BeginRenderpassDesc const* desc);
API void R4_CmdEndRenderpass(R4_Context* ctx, R4_CommandList* cmdlist);

#ifdef __cplusplus
static inline R4_Resource R4_BufferResource(R4_Buffer* buffer) { return buffer; }
static inline R4_Resource R4_ImageResource(R4_Image* image) { return image; }
#else
static inline R4_Resource R4_BufferResource(R4_Buffer* buffer) { return (R4_Resource) { .buffer = buffer }; }
static inline R4_Resource R4_ImageResource(R4_Image* image) { return (R4_Resource) { .image = image }; }
#endif

#endif