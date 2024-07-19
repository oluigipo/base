#ifndef API_RENDER3_H
#define API_RENDER3_H

#include "common.h"

//~ R3_ API
enum R3_Usage
{
	R3_Usage_Immutable = 0,
	R3_Usage_Dynamic,
	R3_Usage_GpuReadWrite,
}
typedef R3_Usage;

enum
{
	R3_BindingFlag_VertexBuffer = 0x0001,
	R3_BindingFlag_IndexBuffer = 0x0002,
	R3_BindingFlag_UniformBuffer = 0x0004,
	R3_BindingFlag_StructuredBuffer = 0x0008,
	R3_BindingFlag_ShaderResource = 0x0010,
	R3_BindingFlag_UnorderedAccess = 0x0020,
	R3_BindingFlag_RenderTarget = 0x0040,
	R3_BindingFlag_DepthStencil = 0x0080,
	R3_BindingFlag_Indirect = 0x0100,
};

enum R3_Format
{
	R3_Format_Null = 0,
	
	R3_Format_U8x1Norm,
	R3_Format_U8x1Norm_ToAlpha,
	R3_Format_U8x2Norm,
	R3_Format_U8x4Norm,
	R3_Format_U8x4Norm_Srgb,
	R3_Format_U8x4Norm_Bgrx,
	R3_Format_U8x4Norm_Bgra,
	R3_Format_U8x1,
	R3_Format_U8x2,
	R3_Format_U8x4,
	R3_Format_I16x2Norm,
	R3_Format_I16x4Norm,
	R3_Format_I16x2,
	R3_Format_I16x4,
	R3_Format_U16x1Norm,
	R3_Format_U16x2Norm,
	R3_Format_U16x4Norm,
	R3_Format_U16x1,
	R3_Format_U16x2,
	R3_Format_U16x4,
	R3_Format_U32x1,
	R3_Format_U32x2,
	R3_Format_U32x4,
	
	R3_Format_F16x2,
	R3_Format_F16x4,
	R3_Format_F32x1,
	R3_Format_F32x2,
	R3_Format_F32x3,
	R3_Format_F32x4,
	
	R3_Format_D16,
	R3_Format_D24S8,

	R3_Format_BC1,
	R3_Format_BC2,
	R3_Format_BC3,
	R3_Format_BC4,
	R3_Format_BC5,
	R3_Format_BC6,
	R3_Format_BC7,

	R3_Format__Count,
}
typedef R3_Format;

enum R3_TextureFiltering
{
	R3_TextureFiltering_Null = 0,
	R3_TextureFiltering_Nearest,
	R3_TextureFiltering_Linear,
	R3_TextureFiltering_Anisotropic,
}
typedef R3_TextureFiltering;

struct R3_ContextInfo
{
	String backend_api;
	String driver_renderer;
	String driver_adapter;
	String driver_version;
	
	uint32 max_texture_size;
	uint32 max_render_target_textures;
	uint32 max_textures_per_drawcall;
	uint32 max_dispatch_x;
	uint32 max_dispatch_y;
	uint32 max_dispatch_z;
	uint32 max_anisotropy_level;

	uint64 supported_texture_formats      [2];
	uint64 supported_render_target_formats[2];
	
	bool has_instancing;
	bool has_base_vertex;
	bool has_32bit_index;
	bool has_separate_alpha_blend;
	bool has_compute_pipeline;
}
typedef R3_ContextInfo;

struct R3_Context typedef R3_Context;

struct R3_ContextDesc
{
	struct OS_Window* window;
}
typedef R3_ContextDesc;

API R3_Context* R3_D3D11_MakeContext(Arena* arena, R3_ContextDesc const* desc);
API R3_Context* R3_GL_MakeContext   (Arena* arena, R3_ContextDesc const* desc);
API R3_Context* R3_MGL_MakeContext  (Arena* arena, R3_ContextDesc const* desc);
//API void R3_D3D11_RecoverDevice(R3_Context* ctx, OS_D3D11Api const* d3d11_api);
//API void R3_GL_RecoverDevice(R3_Context* ctx, OS_OpenGLApi const* ogl_api);
API R3_ContextInfo R3_QueryInfo(R3_Context* ctx);
API void R3_ResizeBuffers(R3_Context* ctx);
API void R3_Present(R3_Context* ctx);
API void R3_FreeContext(R3_Context* ctx);
// NOTE(ljre): Call regularly to check for DEVICE LOST errors. When called, returns false if everything is
//             ok, or true if the device was lost and you need to recreate the context.
API bool R3_IsDeviceLost(R3_Context* ctx);

struct R3_LayoutDesc
{
	uint32 offset;
	R3_Format format;
	uint32 buffer_slot;
	uint32 divisor;
}
typedef R3_LayoutDesc;

// =============================================================================
// Resources
struct R3_Texture
{
	int32 width, height, depth;
	R3_Format format;

	struct ID3D11Texture2D* d3d11_tex2d;
	struct ID3D11Texture3D* d3d11_tex3d;
	struct ID3D11ShaderResourceView* d3d11_srv;
	struct ID3D11UnorderedAccessView* d3d11_uav;

	uint32 gl_id;
	uint32 gl_renderbuffer_id;
}
typedef R3_Texture;

struct R3_Buffer
{
	struct ID3D11Buffer* d3d11_buffer;
	struct ID3D11ShaderResourceView* d3d11_srv;
	struct ID3D11UnorderedAccessView* d3d11_uav;

	uint32 gl_id;
}
typedef R3_Buffer;

struct R3_RenderTarget
{
	struct ID3D11RenderTargetView* d3d11_rtvs[8];
	struct ID3D11DepthStencilView* d3d11_dsv;

	uint32 gl_id;
}
typedef R3_RenderTarget;

struct R3_Pipeline
{
	struct ID3D11BlendState* d3d11_blend;
	struct ID3D11RasterizerState* d3d11_rasterizer;
	struct ID3D11DepthStencilState* d3d11_depth_stencil;
	struct ID3D11InputLayout* d3d11_input_layout;
	struct ID3D11VertexShader* d3d11_vs;
	struct ID3D11PixelShader* d3d11_ps;

	bool gl_blend;
	bool gl_cullface;
	bool gl_depthtest;
	bool gl_scissor;
	uint32 gl_program;
	uint32 gl_src, gl_dst, gl_op;
	uint32 gl_src_alpha, gl_dst_alpha, gl_op_alpha;
	uint32 gl_polygon_mode;
	uint32 gl_cull_mode;
	uint32 gl_frontface;
	R3_LayoutDesc gl_layout[16];
}
typedef R3_Pipeline;

struct R3_ComputePipeline
{
	struct ID3D11ComputeShader* d3d11_cs;

	uint32 gl_program;
}
typedef R3_ComputePipeline;

struct R3_Sampler
{
	struct ID3D11SamplerState* d3d11_sampler;

	uint32 gl_sampler;
	R3_TextureFiltering gl_filtering;
}
typedef R3_Sampler;

// =============================================================================
// Resource Management
struct R3_TextureDesc
{
	int32 width, height, depth;
	R3_Format format;
	R3_Usage usage;
	uint32 binding_flags;
	int32 mipmap_count;
	
	void const* initial_data;
}
typedef R3_TextureDesc;

struct R3_BufferDesc
{
	uint32 size;
	uint32 binding_flags;
	R3_Usage usage;
	uint32 struct_size;
	
	void const* initial_data;
}
typedef R3_BufferDesc;

struct R3_RenderTargetDesc
{
	R3_Texture* color_textures[8];
	R3_Texture* depth_stencil_texture;
}
typedef R3_RenderTargetDesc;

struct R3_SamplerDesc
{
	R3_TextureFiltering filtering;
	float32 anisotropy;
}
typedef R3_SamplerDesc;

enum R3_BlendFunc
{
	R3_BlendFunc_Null = 0,
	
	R3_BlendFunc_Zero,
	R3_BlendFunc_One,
	R3_BlendFunc_SrcColor,
	R3_BlendFunc_InvSrcColor,
	R3_BlendFunc_DstColor,
	R3_BlendFunc_InvDstColor,
	R3_BlendFunc_SrcAlpha,
	R3_BlendFunc_InvSrcAlpha,
	R3_BlendFunc_DstAlpha,
	R3_BlendFunc_InvDstAlpha,
}
typedef R3_BlendFunc;

enum R3_BlendOp
{
	R3_BlendOp_Add = 0,
	R3_BlendOp_Subtract,
}
typedef R3_BlendOp;

enum R3_FillMode
{
	R3_FillMode_Solid = 0,
	R3_FillMode_Wireframe,
}
typedef R3_FillMode;

enum R3_CullMode
{
	R3_CullMode_None = 0,
	R3_CullMode_Front,
	R3_CullMode_Back,
}
typedef R3_CullMode;

struct R3_PipelineDesc
{
	bool flag_cw_frontface;
	bool flag_depth_test;
	//bool flag_scissor : 1;
	
	struct
	{
		bool enable_blend;
		R3_BlendFunc src;
		R3_BlendFunc dst;
		R3_BlendOp op;
		R3_BlendFunc src_alpha;
		R3_BlendFunc dst_alpha;
		R3_BlendOp op_alpha;
	} rendertargets[8];
	
	R3_FillMode fill_mode;
	R3_CullMode cull_mode;
	
	struct
	{
		// NOTE(ljre): These buffers should contain null-terminated strings.
		//             The bufffer size INCLUDES the null-terminator!
		Buffer vs, fs;
	} glsl;
	struct
	{
		Buffer vs, ps;
	} dx50, dx40, dx40_93, dx40_91;
	
	R3_LayoutDesc input_layout[16];
}
typedef R3_PipelineDesc;

struct R3_ComputePipelineDesc
{
	String glsl;
	Buffer dx50, dx40;
}
typedef R3_ComputePipelineDesc;

API R3_Texture         R3_MakeTexture        (R3_Context* ctx, R3_TextureDesc         const* desc);
API R3_Buffer          R3_MakeBuffer         (R3_Context* ctx, R3_BufferDesc          const* desc);
API R3_RenderTarget    R3_MakeRenderTarget   (R3_Context* ctx, R3_RenderTargetDesc    const* desc);
API R3_Pipeline        R3_MakePipeline       (R3_Context* ctx, R3_PipelineDesc        const* desc);
API R3_ComputePipeline R3_MakeComputePipeline(R3_Context* ctx, R3_ComputePipelineDesc const* desc);
API R3_Sampler         R3_MakeSampler        (R3_Context* ctx, R3_SamplerDesc         const* desc);

API void R3_UpdateBuffer (R3_Context* ctx, R3_Buffer* buffer, void const* memory, uint32 size);
API void R3_UpdateTexture(R3_Context* ctx, R3_Texture* texture, void const* memory, uint32 size, uint32 slice);

API void R3_FreeTexture        (R3_Context* ctx, R3_Texture* texture);
API void R3_FreeBuffer         (R3_Context* ctx, R3_Buffer* buffer);
API void R3_FreeRenderTarget   (R3_Context* ctx, R3_RenderTarget* rendertarget);
API void R3_FreePipeline       (R3_Context* ctx, R3_Pipeline* pipeline);
API void R3_FreeComputePipeline(R3_Context* ctx, R3_ComputePipeline* pipeline);
API void R3_FreeSampler        (R3_Context* ctx, R3_Sampler* sampler);

// =============================================================================
// Commands
struct R3_Viewport
{
	float32 x, y;
	float32 width, height;
	float32 min_depth, max_depth;
}
typedef R3_Viewport;

struct R3_ResourceView
{
	R3_Buffer* buffer;
	R3_Texture* texture;
}
typedef R3_ResourceView;

struct R3_UnorderedView
{
	R3_Buffer* buffer;
	R3_Texture* texture;
}
typedef R3_UnorderedView;

struct R3_ClearDesc
{
	float32 color[4];
	float32 depth;
	uint32 stencil;

	bool flag_color;
	bool flag_depth;
	bool flag_stencil;
}
typedef R3_ClearDesc;

struct R3_VertexInputs
{
	R3_Buffer* ibuffer;
	R3_Format index_format;
	struct
	{
		R3_Buffer* buffer;
		uint32 offset;
		uint32 stride;
	} vbuffers[16];
}
typedef R3_VertexInputs;

struct R3_UniformBuffer
{
	R3_Buffer* buffer;
	uint32 size;
	uint32 offset;
}
typedef R3_UniformBuffer;

enum R3_PrimitiveType
{
	R3_PrimitiveType_TriangleList = 0,
	R3_PrimitiveType_TriangleStrip,
	R3_PrimitiveType_LineList,
	R3_PrimitiveType_LineStrip,
	R3_PrimitiveType_PointList,
}
typedef R3_PrimitiveType;

API void R3_SetViewports(R3_Context* ctx, intsize count, R3_Viewport viewports[]);
API void R3_SetPipeline(R3_Context* ctx, R3_Pipeline* pipeline);
API void R3_SetRenderTarget(R3_Context* ctx, R3_RenderTarget* rendertarget);
API void R3_SetVertexInputs(R3_Context* ctx, R3_VertexInputs const* desc);
API void R3_SetUniformBuffers(R3_Context* ctx, intsize count, R3_UniformBuffer buffers[]);
API void R3_SetResourceViews(R3_Context* ctx, intsize count, R3_ResourceView views[]);
API void R3_SetSamplers(R3_Context* ctx, intsize count, R3_Sampler* samplers[]);
API void R3_SetPrimitiveType(R3_Context* ctx, R3_PrimitiveType type);
API void R3_Clear(R3_Context* ctx, R3_ClearDesc const* desc);
API void R3_Draw(R3_Context* ctx, uint32 start_vertex, uint32 vertex_count, uint32 start_instance, uint32 instance_count);
API void R3_DrawIndexed(R3_Context* ctx, uint32 start_index, uint32 index_count, uint32 start_instance, uint32 instance_count, int32 base_vertex);

API void R3_SetComputePipeline(R3_Context* ctx, R3_ComputePipeline* pipeline);
API void R3_SetComputeUniformBuffers(R3_Context* ctx, intsize count, R3_UniformBuffer buffers[]);
API void R3_SetComputeResourceViews(R3_Context* ctx, intsize count, R3_ResourceView views[]);
API void R3_SetComputeUnorderedViews(R3_Context* ctx, intsize count, R3_UnorderedView views[]);
API void R3_Dispatch(R3_Context* ctx, uint32 x, uint32 y, uint32 z);

// =============================================================================
// =============================================================================
// Video
struct R3_VideoDecoder
{
	uint32 width, height;
	uint32 fps_num, fps_den;
	uint64 frame_count;

	void* d3d11_mfsource;
}
typedef R3_VideoDecoder;

struct R3_VideoDecoderDesc
{
	String filepath;
}
typedef R3_VideoDecoderDesc;

API R3_VideoDecoder R3_MakeVideoDecoder(R3_Context* ctx, R3_VideoDecoderDesc const* desc);
API void R3_FreeVideoDecoder(R3_Context* ctx, R3_VideoDecoder* video);
API void R3_SetDecoderPosition(R3_Context* ctx, R3_VideoDecoder* video, uint64 frame_index);

// NOTE(ljre): 'output_texture' should be either a valid texture or be zero-initialized. If it's a valid
//             texture, it'll be free'd with R3_FreeTexture() before the new contents are stored.
//             You can keep passing the same struct over and over again to iterate through the video's frames.
//
//             'out_timestamp' will contain the frame timestamp in the video. This timestamp is 0 for the first
//             frame and will advance roughly 'OS_TickRate() * fps_den / fps_num' ticks per frame.
//
//             Returns true if we're not at the end of the video yet.
API bool R3_DecodeFrame(R3_Context* ctx, R3_VideoDecoder* video, R3_Texture* output_texture, uint64* out_timestamp);

// =============================================================================
// =============================================================================
// Resource Mapping & Copying
struct R3_MappedResource
{
	void* memory;
	uintsize size;
	uintsize row_pitch;
	uintsize depth_pitch;
}
typedef R3_MappedResource;

enum R3_MapKind
{
	R3_MapKind_Read,
	R3_MapKind_Write,
	R3_MapKind_ReadWrite,
	R3_MapKind_Discard,
	R3_MapKind_NoOverwrite,
}
typedef R3_MapKind;

API R3_MappedResource R3_MapBuffer   (R3_Context* ctx, R3_Buffer* buffer, R3_MapKind map_kind);
API void              R3_UnmapBuffer (R3_Context* ctx, R3_Buffer* buffer, uintptr written_offset, uintptr written_size);
API R3_MappedResource R3_MapTexture  (R3_Context* ctx, R3_Texture* texture, uint32 slice, R3_MapKind map_kind);
API void              R3_UnmapTexture(R3_Context* ctx, R3_Texture* texture, uint32 slice);

API void R3_CopyBuffer(R3_Context* ctx, R3_Buffer* src, uint32 src_offset, R3_Buffer* dst, uint32 dst_offset, uint32 size);
API void R3_CopyTexture2D(R3_Context* ctx, R3_Texture* src, uint32 src_x, uint32 src_y, R3_Texture* dst, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height);

// =============================================================================
// =============================================================================
// Font drawing
struct R3_TextContext
{
	struct ID2D1Factory1* d2d_factory;
	struct ID2D1RenderTarget* d2d_rendertarget;
	struct ID2D1SolidColorBrush* d2d_brush;
	struct IDWriteFactory* dw_factory;
	int32 rt_width, rt_height;
}
typedef R3_TextContext;

struct R3_Font
{
	struct IDWriteTextFormat* dw_text_format;
}
typedef R3_Font;

struct R3_TextContextDesc
{
	R3_Texture* rendertarget;
}
typedef R3_TextContextDesc;

enum R3_FontWeight
{
	R3_FontWeight_Regular = 0,
}
typedef R3_FontWeight;

enum R3_FontStyle
{
	R3_FontStyle_Normal = 0,
}
typedef R3_FontStyle;

enum R3_FontStretch
{
	R3_FontStretch_Normal = 0,
}
typedef R3_FontStretch;

struct R3_FontDesc
{
	String name;
	R3_FontWeight weight;
	R3_FontStyle style;
	R3_FontStretch stretch;
	float32 pt;
}
typedef R3_FontDesc;

API uint32   R3_TextRequiredBindingFlags(void);
API R3_Usage R3_TextRequiredUsage(void);

API R3_TextContext R3_DWRITE_MakeTextContext(R3_Context* ctx, R3_TextContextDesc const* desc);
API R3_TextContext R3_FT_MakeTextContext(R3_Context* ctx, R3_TextContextDesc const* desc);
API R3_Font R3_MakeFont(R3_Context* ctx, R3_TextContext* text_ctx, R3_FontDesc const* desc);

API void R3_FreeTextContext(R3_Context* ctx, R3_TextContext* text_ctx);
API void R3_FreeFont(R3_Context* ctx, R3_Font* font);

API void R3_TextBegin(R3_Context* ctx, R3_TextContext* text_ctx);
API void R3_TextDrawGlyphs(R3_Context* ctx, R3_TextContext* text_ctx, String str, R3_Font* font, float32 const transform[2][3]);
API void R3_TextEnd(R3_Context* ctx, R3_TextContext* text_ctx);

#endif //API_RENDER3_H
