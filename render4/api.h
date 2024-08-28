#ifndef RENDER4_API2_H
#define RENDER4_API2_H

#include "common_basic.h"
#include "render4_d3d12.h"
#include "render4_vulkan.h"

// =============================================================================
// =============================================================================
// Resources handles
union R4_Queue            { R4_D3D12_Queue            d3d12; R4_VK_Queue            vk; } typedef R4_Queue;
union R4_Buffer           { R4_D3D12_Buffer           d3d12; R4_VK_Buffer           vk; } typedef R4_Buffer;
union R4_Image            { R4_D3D12_Image            d3d12; R4_VK_Image            vk; } typedef R4_Image;
union R4_Heap             { R4_D3D12_Heap             d3d12; R4_VK_Heap             vk; } typedef R4_Heap;
union R4_CommandAllocator { R4_D3D12_CommandAllocator d3d12; R4_VK_CommandAllocator vk; } typedef R4_CommandAllocator;
union R4_CommandList      { R4_D3D12_CommandList      d3d12; R4_VK_CommandList      vk; } typedef R4_CommandList;
union R4_Pipeline         { R4_D3D12_Pipeline         d3d12; R4_VK_Pipeline         vk; } typedef R4_Pipeline;
union R4_PipelineLayout   { R4_D3D12_PipelineLayout   d3d12; R4_VK_PipelineLayout   vk; } typedef R4_PipelineLayout;
union R4_BindLayout       { R4_D3D12_BindLayout       d3d12; R4_VK_BindLayout       vk; } typedef R4_BindLayout;
union R4_DescriptorHeap   { R4_D3D12_DescriptorHeap   d3d12; R4_VK_DescriptorHeap   vk; } typedef R4_DescriptorHeap;
union R4_DescriptorSet    { R4_D3D12_DescriptorSet    d3d12; R4_VK_DescriptorSet    vk; } typedef R4_DescriptorSet;
union R4_BufferView       { R4_D3D12_BufferView       d3d12; R4_VK_BufferView       vk; } typedef R4_BufferView;
union R4_ImageView        { R4_D3D12_ImageView        d3d12; R4_VK_ImageView        vk; } typedef R4_ImageView;
union R4_Sampler          { R4_D3D12_Sampler          d3d12; R4_VK_Sampler          vk; } typedef R4_Sampler;
union R4_RenderTargetView { R4_D3D12_RenderTargetView d3d12; R4_VK_RenderTargetView vk; } typedef R4_RenderTargetView;
union R4_DepthStencilView { R4_D3D12_DepthStencilView d3d12; R4_VK_DepthStencilView vk; } typedef R4_DepthStencilView;

// Generic resource, you should set either buffer or image (not both)
struct R4_Resource
{
	R4_Buffer* buffer;
	R4_Image* image;

#ifdef __cplusplus
	inline constexpr R4_Resource() : buffer(NULL), image(NULL) {}
	inline constexpr R4_Resource(R4_Buffer* resc) : buffer(resc), image(NULL) {}
	inline constexpr R4_Resource(R4_Image*  resc) : buffer(NULL), image(resc) {}
#endif
}
typedef R4_Resource;

#ifdef __cplusplus
static inline R4_Resource R4_BufferResource(R4_Buffer* buffer) { return buffer; }
static inline R4_Resource R4_ImageResource (R4_Image* image)   { return image; }
#else
static inline R4_Resource R4_BufferResource(R4_Buffer* buffer) { return (R4_Resource) { .buffer = buffer }; }
static inline R4_Resource R4_ImageResource (R4_Image*  image)  { return (R4_Resource) { .image  = image  }; }
#endif

// Returns true if resource is null (aka if the handle is zeroed)
#define R4_IsNull(resource) R4_IsNullImpl_(resource, (intz)sizeof(resource))
static inline bool
R4_IsNullImpl_(void const* impl, intz size)
{
	uint8 const* as_uint8 = (uint8 const*)impl;
	for (intz i = 0; i < size; ++i)
	{
		if (as_uint8[i] != 0)
			return false;
	}
	return true;
}

// =============================================================================
// =============================================================================
// Basic context information
struct R4_Context typedef R4_Context;

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
	
	String adapter_description;
	String adapter_driver_version;
	uint32 adapter_device_id;
	uint32 adapter_vendor_id;
	uint32 adapter_driver_version_product;
	uint32 adapter_driver_version_major;
	uint32 adapter_driver_version_minor;
	uint32 adapter_driver_version_patch;

	R4_ContextProfile context_profile;
	R4_ShaderProfile shader_profile;

	int64 default_heap_budget;
	int64 upload_heap_budget;
	int64 readback_heap_budget;

	bool adapter_is_igpu;
	bool has_compute_queue;
	bool has_copy_queue;
	bool has_mesh_shaders;
	bool has_raytracing;
}
typedef R4_ContextInfo;

// =============================================================================
// =============================================================================
// Basic enums
enum R4_Result
{
	R4_Result_Ok = 0,
	R4_Result_Failure,
	R4_Result_DeviceLost,
	R4_Result_DeviceOutOfMemory,
	R4_Result_AllocatorError,
	R4_Result_AllocatorOutOfMemory,
	R4_Result_DeviceCreationFailed,
}
typedef R4_Result;

enum R4_Format
{
	R4_Format_Null = 0,
	
	R4_Format_R8_UNorm,
	R4_Format_R8G8_UNorm,
	R4_Format_R8G8B8A8_UNorm,
	R4_Format_R8G8B8A8_SRgb,
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

enum
{
	R4_ResourceUsageFlag_VertexBuffer    = 1<<0,
	R4_ResourceUsageFlag_IndexBuffer     = 1<<1,
	R4_ResourceUsageFlag_UniformBuffer   = 1<<2,
	R4_ResourceUsageFlag_IndirectBuffer  = 1<<3,
	R4_ResourceUsageFlag_ShaderStorage   = 1<<4,
	R4_ResourceUsageFlag_ShaderReadWrite = 1<<5,
	R4_ResourceUsageFlag_TransferSrc     = 1<<6,
	R4_ResourceUsageFlag_TransferDst     = 1<<7,
	R4_ResourceUsageFlag_SampledImage    = 1<<8,
	R4_ResourceUsageFlag_RenderTarget    = 1<<9,
	R4_ResourceUsageFlag_DepthStencil    = 1<<10,
};

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
	R4_ResourceState_RenderTarget,
	R4_ResourceState_DepthStencil,
	R4_ResourceState_TransferDst,
	R4_ResourceState_TransferSrc,
}
typedef R4_ResourceState;

enum R4_DescriptorType
{
	R4_DescriptorType_Null = 0,                     // D3D12,   Vulkan
	R4_DescriptorType_Sampler,                      // SAMPLER, SAMPLER
	R4_DescriptorType_ImageSampled,                 // SRV,     SAMPLED_IMAGE
	R4_DescriptorType_ImageShaderStorage,           // SRV,     STORAGE_IMAGE
	R4_DescriptorType_ImageShaderReadWrite,         // UAV,     STORAGE_IMAGE
	R4_DescriptorType_UniformBuffer,                // CBV,     UNIFORM_BUFFER
	R4_DescriptorType_BufferShaderStorage,          // SRV,     STORAGE_BUFFER
	R4_DescriptorType_BufferShaderReadWrite,        // UAV,     STORAGE_BUFFER
	R4_DescriptorType_DynamicUniformBuffer,         // CBV,     UNIFORM_BUFFER_DYNAMIC
	R4_DescriptorType_DynamicBufferShaderStorage,   // SRV,     STORAGE_BUFFER_DYNAMIC
	R4_DescriptorType_DynamicBufferShaderReadWrite, // UAV,     STORAGE_BUFFER_DYNAMIC
}
typedef R4_DescriptorType;

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

enum R4_PrimitiveType
{
	R4_PrimitiveType_PointList,
	R4_PrimitiveType_LineList,
	R4_PrimitiveType_LineStrip,
	R4_PrimitiveType_TriangleList,
	R4_PrimitiveType_TriangleStrip,
	R4_PrimitiveType_TriangleFan,
	R4_PrimitiveType_Patch,
}
typedef R4_PrimitiveType;

enum R4_ShaderVisibility
{
	R4_ShaderVisibility_All = 0,
	R4_ShaderVisibility_Vertex,
	R4_ShaderVisibility_Pixel,
	R4_ShaderVisibility_Mesh,
}
typedef R4_ShaderVisibility;

enum
{
	R4_StageMask_Null = 0,
	R4_StageMask_All = (1U<<31) - 1,
	
	R4_StageMask_DrawIndirect = 1<<0,
	R4_StageMask_VertexInput  = 1<<1,
	R4_StageMask_IndexInput   = 1<<2,
	R4_StageMask_RenderTarget = 1<<3,
	R4_StageMask_DepthStencil = 1<<4,
	R4_StageMask_Transfer     = 1<<5,
	R4_StageMask_Resolve      = 1<<6,

	R4_StageMask_VertexShader        = 1<<15,
	R4_StageMask_PixelShader         = 1<<16,
	R4_StageMask_ComputeShader       = 1<<17,
	R4_StageMask_AmplificationShader = 1<<18,
	R4_StageMask_MeshShader          = 1<<19,
};

enum
{
	R4_AccessMask_Null = 0,
	R4_AccessMask_Common = (1U<<31) - 1,

	R4_AccessMask_Present         = 1<<0,
	R4_AccessMask_VertexBuffer    = 1<<1,
	R4_AccessMask_IndexBuffer     = 1<<2,
	R4_AccessMask_UniformBuffer   = 1<<3,
	R4_AccessMask_ShaderRead      = 1<<4,
	R4_AccessMask_ShaderWrite     = 1<<5,
	R4_AccessMask_SampledImage    = 1<<6,
	R4_AccessMask_RenderTarget    = 1<<7,
	R4_AccessMask_DepthStencil    = 1<<8,
	R4_AccessMask_TransferDst     = 1<<9,
	R4_AccessMask_TransferSrc     = 1<<10,
};

// =============================================================================
// =============================================================================
// Context creation
struct R4_ContextDesc
{
	bool prefer_igpu;
	bool debug_mode;

	struct OS_Window* os_window;
	R4_Format backbuffer_format;
	intz backbuffer_count;
	R4_Image* out_backbuffer_images;
	R4_RenderTargetView* out_backbuffer_rtvs;
	
	// defaults: 1024 (1<<10)
	int32 max_render_target_views;
	int32 max_depth_stencil_views;
	int32 max_samplers;
	// defaults: 1048576 (1<<20)
	int32 max_image_views;
	int32 max_buffer_views;

	R4_Queue* out_graphics_queue;
	R4_Queue* out_compute_queue;
	R4_Queue* out_copy_queue;
	R4_ContextInfo* out_info;
	String* out_error_explanation;
}
typedef R4_ContextDesc;

API R4_Context* R4_MakeContext(Allocator allocator, R4_Result* r, R4_ContextDesc const* desc);

API R4_ContextInfo R4_GetContextInfo(R4_Context* ctx);
API bool R4_IsDeviceLost(R4_Context* ctx, R4_Result* r);
API void R4_DestroyContext(R4_Context* ctx, R4_Result* r);
API void R4_PresentAndSync(R4_Context* ctx, R4_Result* r, int32 sync_interval);
API intz R4_AcquireNextBackbuffer(R4_Context* ctx, R4_Result* r);
API void R4_ExecuteCommandLists(R4_Context* ctx, R4_Result* r, R4_Queue* queue, bool last_submission, intz cmdlist_count, R4_CommandList* const cmdlists[]);
API void R4_WaitRemainingWorkOnQueue(R4_Context* ctx, R4_Result* r, R4_Queue* queue);

// =============================================================================
// =============================================================================
// Heap management
enum R4_HeapType
{
	R4_HeapType_Default = 0,
	R4_HeapType_Upload,
	R4_HeapType_Readback,
}
typedef R4_HeapType;

API R4_Heap R4_MakeHeap(R4_Context* ctx, R4_Result* r, R4_HeapType type, int64 size);
API void R4_FreeHeap(R4_Context* ctx, R4_Heap* heap);

// =============================================================================
// =============================================================================
// Buffers & Images
struct R4_BufferDesc
{
	int64 size;
	uint32 usage_flags;
}
typedef R4_BufferDesc;

struct R4_UploadBufferDesc
{
	R4_Heap* heap;
	int64 heap_offset;
	R4_BufferDesc buffer_desc;
}
typedef R4_UploadBufferDesc;

struct R4_PlacedBufferDesc
{
	R4_Heap* heap;
	int64 heap_offset;
	R4_ResourceState initial_state;
	R4_BufferDesc buffer_desc;
}
typedef R4_PlacedBufferDesc;

API R4_Buffer R4_MakeUploadBuffer(R4_Context* ctx, R4_Result* r, R4_UploadBufferDesc const* desc);
API R4_Buffer R4_MakePlacedBuffer(R4_Context* ctx, R4_Result* r, R4_PlacedBufferDesc const* desc);
API void R4_FreeBuffer(R4_Context* ctx, R4_Buffer* buffer);

enum R4_ImageDimension
{
	R4_ImageDimension_Null = 0,
	R4_ImageDimension_Texture1D,
	R4_ImageDimension_Texture2D,
	R4_ImageDimension_Texture3D,
}
typedef R4_ImageDimension;

struct R4_ImageDesc
{
	int64 width;
	int64 height;
	int32 depth;
	int32 mip_levels;
	int32 array_size;
	R4_Format format;
	R4_ImageDimension dimension;
	uint32 usage_flags;
}
typedef R4_ImageDesc;

struct R4_PlacedImageDesc
{
	R4_Heap* heap;
	int64 heap_offset;
	R4_ResourceState initial_state;
	R4_ImageDesc image_desc;
	float32 clear_color[4];
	float32 clear_depth;
	uint32 clear_stencil;
}
typedef R4_PlacedImageDesc;

API R4_Image R4_MakePlacedImage(R4_Context* ctx, R4_Result* r, R4_PlacedImageDesc const* desc);
API void R4_FreeImage(R4_Context* ctx, R4_Image* image);

struct R4_MemoryRequirements
{
	int64 size;
	int64 alignment;
}
typedef R4_MemoryRequirements;

API R4_MemoryRequirements R4_GetMemoryRequirementsFromBufferDesc(R4_Context *ctx, R4_BufferDesc const* desc);
API R4_MemoryRequirements R4_GetMemoryRequirementsFromBuffer    (R4_Context* ctx, R4_Buffer* buffer);
API R4_MemoryRequirements R4_GetMemoryRequirementsFromImageDesc (R4_Context *ctx, R4_ImageDesc const* desc);
API R4_MemoryRequirements R4_GetMemoryRequirementsFromImage     (R4_Context *ctx, R4_Image* image);

API void R4_MapResource  (R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource, intz size, void** out_memory);
API void R4_UnmapResource(R4_Context* ctx, R4_Result* r, R4_Resource resource, int32 subresource);

// =============================================================================
// =============================================================================
// Buffer views & Image views
struct R4_BufferViewDesc
{
	R4_Buffer* buffer;
}
typedef R4_BufferViewDesc;

API R4_BufferView R4_MakeBufferView(R4_Context* ctx, R4_Result* r, R4_BufferViewDesc const* desc);
API void R4_FreeBufferView(R4_Context* ctx, R4_BufferView* buffer_view);

struct R4_ImageViewDesc
{
	R4_Image* image;
	R4_Format format;
}
typedef R4_ImageViewDesc;

API R4_ImageView R4_MakeImageView(R4_Context* ctx, R4_Result* r, R4_ImageViewDesc const* desc);
API void R4_FreeImageView(R4_Context* ctx, R4_ImageView* image_view);

// =============================================================================
// =============================================================================
// Render target views & depth stencil views
struct R4_RenderTargetViewDesc
{
	R4_Image* image;
	R4_Format format;
}
typedef R4_RenderTargetViewDesc;

API R4_RenderTargetView R4_MakeRenderTargetView(R4_Context* ctx, R4_Result* r, R4_RenderTargetViewDesc const* desc);
API void R4_FreeRenderTargetView(R4_Context* ctx, R4_RenderTargetView* render_target_view);

struct R4_DepthStencilViewDesc
{
	R4_Image* image;
	R4_Format format;
}
typedef R4_DepthStencilViewDesc;

API R4_DepthStencilView R4_MakeDepthStencilView(R4_Context* ctx, R4_Result* r, R4_DepthStencilViewDesc const* desc);
API void R4_FreeDepthStencilView(R4_Context* ctx, R4_DepthStencilView* depth_stencil_view);

// =============================================================================
// =============================================================================
// Samplers
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

API R4_Sampler R4_MakeSampler(R4_Context* ctx, R4_Result* r, R4_SamplerDesc const* desc);
API void R4_FreeSampler(R4_Context* ctx, R4_Sampler* sampler);

// =============================================================================
// =============================================================================
// Bind Layout
struct R4_BindLayoutEntry
{
	R4_DescriptorType descriptor_type;
	int32 descriptor_count;
	int32 start_shader_slot;
}
typedef R4_BindLayoutEntry;

struct R4_BindLayoutDesc
{
	R4_ShaderVisibility shader_visibility;
	R4_BindLayoutEntry entries[32];
}
typedef R4_BindLayoutDesc;

API R4_BindLayout R4_MakeBindLayout(R4_Context* ctx, R4_Result* r, R4_BindLayoutDesc const* desc);
API void R4_FreeBindLayout(R4_Context* ctx, R4_BindLayout* bind_layout);

// =============================================================================
// =============================================================================
// Pipeline Layout
struct R4_PipelineLayoutPushConstant
{
	R4_ShaderVisibility shader_visibility;
	int32 start_index;
	int32 count;
}
typedef R4_PipelineLayoutPushConstant;

struct R4_PipelineLayoutDesc
{
	R4_PipelineLayoutPushConstant push_constants[64];
	R4_BindLayout* bind_layouts[64];
}
typedef R4_PipelineLayoutDesc;

API R4_PipelineLayout R4_MakePipelineLayout(R4_Context* ctx, R4_Result* r, R4_PipelineLayoutDesc const* desc);
API void R4_FreePipelineLayout(R4_Context* ctx, R4_PipelineLayout* pipeline_layout);

// =============================================================================
// =============================================================================
// Pipeline
struct R4_ShaderBytecode
{
	void const* data;
	intz size;
}
typedef R4_ShaderBytecode;

struct R4_VertexInput
{
	R4_Format format;
	intz input_slot;
	intz byte_offset;
	intz instance_step_rate;
}
typedef R4_VertexInput;

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

enum R4_LogicOp
{
	R4_LogicOp_Null = 0,
}
typedef R4_LogicOp;

struct R4_RenderTargetBlendMode
{
	bool enable_blend;
	bool enable_logic_op;
	uint8 write_mask;
	R4_BlendFunc src, dst;
	R4_BlendFunc src_alpha, dst_alpha;
	R4_BlendOp op, op_alpha;
	R4_LogicOp logic_op;
}
typedef R4_RenderTargetBlendMode;

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

struct R4_GraphicsPipelineDesc
{
	R4_PipelineLayout* pipeline_layout;

	R4_ShaderBytecode dxil_vs, dxil_ps;
	R4_ShaderBytecode spirv_vs, spirv_ps;

	R4_VertexInput vertex_inputs[16];
	R4_PrimitiveType primitive;

	intz sample_count;
	intz sample_quality;
	uint32 sample_mask;

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
	R4_RenderTargetBlendMode blend_rendertargets[8];

	intz rendertarget_count;
	R4_Format rendertarget_formats[8];
	R4_Format depthstencil_format;
}
typedef R4_GraphicsPipelineDesc;

struct R4_ComputePipelineDesc
{
	R4_PipelineLayout* pipeline_layout;
	R4_ShaderBytecode dxil_cs;
	R4_ShaderBytecode spirv_cs;
}
typedef R4_ComputePipelineDesc;

struct R4_MeshPipelineDesc
{

}
typedef R4_MeshPipelineDesc;

API R4_Pipeline R4_MakeGraphicsPipeline(R4_Context* ctx, R4_Result* r, R4_GraphicsPipelineDesc const* desc);
API R4_Pipeline R4_MakeComputePipeline (R4_Context* ctx, R4_Result* r, R4_ComputePipelineDesc  const* desc);
API R4_Pipeline R4_MakeMeshPipeline    (R4_Context* ctx, R4_Result* r, R4_MeshPipelineDesc     const* desc);
API void R4_FreePipeline(R4_Context* ctx, R4_Pipeline* pipeline);

// =============================================================================
// =============================================================================
// Descriptor heap & descriptor sets
struct R4_DescriptorHeapTypePool
{
	R4_DescriptorType type;
	int32 count;
}
typedef R4_DescriptorHeapTypePool;

struct R4_DescriptorHeapDesc
{
	int32 buffering_count; // max: 4
	int32 max_set_count;
	R4_DescriptorHeapTypePool pools[32];
}
typedef R4_DescriptorHeapDesc;

API R4_DescriptorHeap R4_MakeDescriptorHeap(R4_Context* ctx, R4_Result* r, R4_DescriptorHeapDesc const* desc);
API void R4_FreeDescriptorHeap(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap);

struct R4_DescriptorSetDesc
{
	R4_BindLayout* bind_layout;
	R4_DescriptorHeap* descriptor_heap;
	intz heap_buffering_index;
}
typedef R4_DescriptorSetDesc;

API R4_DescriptorSet R4_MakeDescriptorSet(R4_Context* ctx, R4_Result* r, R4_DescriptorSetDesc const* desc);
API void R4_FreeDescriptorSet(R4_Context* ctx, R4_DescriptorSet* descriptor_set);

struct R4_DescriptorBuffer
{
	R4_Buffer* buffer;
	int64 offset;
	int64 size;
}
typedef R4_DescriptorBuffer;

struct R4_DescriptorImage
{
	R4_ImageView* image_view;
	R4_ResourceState state;
}
typedef R4_DescriptorImage;

struct R4_DescriptorSetWrite
{
	R4_DescriptorSet* dst_set;
	int32 dst_binding;
	int32 dst_array_element;
	R4_DescriptorType type;
	int32 count;
	R4_DescriptorBuffer const* buffers;
	R4_DescriptorImage const* images;
	R4_Sampler* const* samplers;
}
typedef R4_DescriptorSetWrite;

struct R4_DescriptorSetCopy
{

}
typedef R4_DescriptorSetCopy;

API void R4_UpdateDescriptorSets(R4_Context* ctx, intz write_count, R4_DescriptorSetWrite const writes[], intz copy_count, R4_DescriptorSetCopy const copies[]);
API void R4_ResetDescriptorHeapBuffering(R4_Context* ctx, R4_DescriptorHeap* descriptor_heap, int32 buffer_index);

// =============================================================================
// =============================================================================
// Command allocator & command lists
enum R4_CommandListType
{
	R4_CommandListType_Graphics = 0,
	R4_CommandListType_Compute,
	R4_CommandListType_Copy,
}
typedef R4_CommandListType;

API R4_CommandAllocator R4_MakeCommandAllocator(R4_Context* ctx, R4_Result* r, R4_Queue* target_queue);
API void R4_FreeCommandAllocator(R4_Context* ctx, R4_CommandAllocator* cmd_allocator);

API R4_CommandList R4_MakeCommandList(R4_Context* ctx, R4_Result* r, R4_CommandListType, R4_CommandAllocator* allocator);
API void R4_FreeCommandList(R4_Context* ctx, R4_CommandAllocator* cmdalloc, R4_CommandList* cmd_list);

// =============================================================================
// =============================================================================
// Command list recording
API void R4_ResetCommandAllocator(R4_Context* ctx, R4_Result* r, R4_CommandAllocator* cmd_allocator);
API void R4_BeginCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist, R4_CommandAllocator* cmd_allocator);
API void R4_EndCommandList(R4_Context* ctx, R4_Result* r, R4_CommandList* cmdlist);

struct R4_Rect
{
	int32 x, y;
	int32 width, height;
}
typedef R4_Rect;

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

struct R4_RenderpassRenderTarget
{
	R4_RenderTargetView* render_target_view;
	R4_AttachmentLoadOp load;
	R4_AttachmentStoreOp store;
	float32 clear_color[4];
}
typedef R4_RenderpassRenderTarget;

struct R4_RenderpassDepthStencil
{
	R4_DepthStencilView* depth_stencil_view;
	R4_AttachmentLoadOp load;
	R4_AttachmentStoreOp store;
	float32 clear_depth;
	uint32 clear_stencil;
}
typedef R4_RenderpassDepthStencil;

struct R4_Renderpass
{
	R4_RenderpassRenderTarget render_targets[8];
	R4_RenderpassDepthStencil depth_stencil;
	R4_Rect render_area;
	bool no_depth;
	bool no_stencil;
}
typedef R4_Renderpass;

API void R4_CmdBeginRenderpass(R4_CommandList* cmdlist, R4_Renderpass const* renderpass);
API void R4_CmdEndRenderpass  (R4_CommandList* cmdlist);

struct R4_Viewport
{
	float32 x, y;
	float32 width, height;
	float32 min_depth;
	float32 max_depth;
}
typedef R4_Viewport;

struct R4_VertexBuffer
{
	R4_Buffer* buffer;
	int64 offset;
	int64 size;
	int64 stride;
}
typedef R4_VertexBuffer;

struct R4_IndexBuffer
{
	R4_Buffer* buffer;
	int64 offset;
	int64 size;
	R4_Format index_format;
}
typedef R4_IndexBuffer;

struct R4_ImageBarrier
{
	R4_Image* image;
	int32 subresource;
	R4_ResourceState from_state;
	uint32 from_access;
	uint32 from_stage;
	R4_ResourceState to_state;
	uint32 to_access;
	uint32 to_stage;
}
typedef R4_ImageBarrier;

struct R4_BufferBarrier
{
	R4_Buffer* buffer;
	R4_ResourceState from_state;
	uint32 from_access;
	uint32 from_stage;
	R4_ResourceState to_state;
	uint32 to_access;
	uint32 to_stage;
}
typedef R4_BufferBarrier;

struct R4_ResourceBarriers
{
	intz image_barrier_count;
	R4_ImageBarrier const* image_barriers;
	intz buffer_barrier_count;
	R4_BufferBarrier const* buffer_barriers;
}
typedef R4_ResourceBarriers;

struct R4_PushConstant
{
	R4_PipelineLayout* pipeline_layout;
	R4_ShaderVisibility shader_visibility;
	int32 constant_index;
	int32 offset; // in DWORDs
	int32 count; // in DWORDs
	void const* u32_constants;
}
typedef R4_PushConstant;

struct R4_DescriptorSets
{
	R4_PipelineLayout* pipeline_layout;
	int32 first_set;
	int32 count;
	R4_DescriptorSet* const* sets;
	int32 dynamic_offset_count;
	int32 const* dynamic_offsets;
}
typedef R4_DescriptorSets;

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

struct R4_BufferCopyRegion
{
	int64 src_offset;
	int64 dst_offset;
	int64 size;
}
typedef R4_BufferCopyRegion;

struct R4_ImageCopyRegion
{
	int32 src_x, src_y, src_z;
	int32 src_subresource;
	int32 dst_x, dst_y, dst_z;
	int32 dst_subresource;
	int32 width, height, depth;
}
typedef R4_ImageCopyRegion;

struct R4_BufferImageCopyRegion
{
	int64 buffer_offset;
	int32 buffer_width_in_pixels;
	int32 buffer_height_in_pixels;

	R4_Format image_format;
	int32 image_x, image_y, image_z;
	int32 image_width, image_height, image_depth;
	int32 image_subresource;
}
typedef R4_BufferImageCopyRegion;

API void R4_CmdCopyBuffer       (R4_CommandList* cmdlist, R4_Buffer* dest, R4_Buffer* source, intz region_count, R4_BufferCopyRegion const regions[]);
API void R4_CmdCopyImage        (R4_CommandList* cmdlist, R4_Image* dest, R4_Image* source, intz region_count, R4_ImageCopyRegion const regions[]);
API void R4_CmdCopyBufferToImage(R4_CommandList* cmdlist, R4_Image* dest, R4_Buffer* source, intz region_count, R4_BufferImageCopyRegion const regions[]);

#endif