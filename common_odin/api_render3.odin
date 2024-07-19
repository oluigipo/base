package common_odin

R3_Usage :: enum i32{
	Immutable = 0,
	Dynamic,
	GpuReadWrite,
}
R3_BindingFlag_VertexBuffer :: 0x0001
R3_BindingFlag_IndexBuffer :: 0x0002
R3_BindingFlag_UniformBuffer :: 0x0004
R3_BindingFlag_StructuredBuffer :: 0x0008
R3_BindingFlag_ShaderResource :: 0x0010
R3_BindingFlag_UnorderedAccess :: 0x0020
R3_BindingFlag_RenderTarget :: 0x0040
R3_BindingFlag_DepthStencil :: 0x0080
R3_BindingFlag_Indirect :: 0x0100
R3_Format :: enum i32{
	Null = 0,
	U8x1Norm,
	U8x1Norm_ToAlpha,
	U8x2Norm,
	U8x4Norm,
	U8x4Norm_Srgb,
	U8x4Norm_Bgrx,
	U8x4Norm_Bgra,
	U8x1,
	U8x2,
	U8x4,
	I16x2Norm,
	I16x4Norm,
	I16x2,
	I16x4,
	U16x1Norm,
	U16x2Norm,
	U16x4Norm,
	U16x1,
	U16x2,
	U16x4,
	U32x1,
	U32x2,
	U32x4,
	F16x2,
	F16x4,
	F32x1,
	F32x2,
	F32x3,
	F32x4,
	D16,
	D24S8,
	BC1,
	BC2,
	BC3,
	BC4,
	BC5,
	BC6,
	BC7,
	_Count,
}
R3_TextureFiltering :: enum i32{
	Null = 0,
	Nearest,
	Linear,
	Anisotropic,
}
R3_ContextInfo :: struct
{
	backend_api: string,
	driver_renderer: string,
	driver_adapter: string,
	driver_version: string,
	max_texture_size: u32,
	max_render_target_textures: u32,
	max_textures_per_drawcall: u32,
	max_dispatch_x: u32,
	max_dispatch_y: u32,
	max_dispatch_z: u32,
	max_anisotropy_level: u32,
	supported_texture_formats: [2]u64,
	supported_render_target_formats: [2]u64,
	has_instancing: bool,
	has_base_vertex: bool,
	has_32bit_index: bool,
	has_separate_alpha_blend: bool,
	has_compute_pipeline: bool,
}
R3_Context :: struct {}
R3_ContextDesc :: struct
{
	window: ^OS_Window,
}
R3_LayoutDesc :: struct
{
	offset: u32,
	format: R3_Format,
	buffer_slot: u32,
	divisor: u32,
}
R3_Texture :: struct
{
	width: i32,
	height: i32,
	depth: i32,
	format: R3_Format,
	d3d11_tex2d: ^struct {},
	d3d11_tex3d: ^struct {},
	d3d11_srv: ^struct {},
	d3d11_uav: ^struct {},
	gl_id: u32,
	gl_renderbuffer_id: u32,
}
R3_Buffer :: struct
{
	d3d11_buffer: ^struct {},
	d3d11_srv: ^struct {},
	d3d11_uav: ^struct {},
	gl_id: u32,
}
R3_RenderTarget :: struct
{
	d3d11_rtvs: [8]^struct {},
	d3d11_dsv: ^struct {},
	gl_id: u32,
}
R3_Pipeline :: struct
{
	d3d11_blend: ^struct {},
	d3d11_rasterizer: ^struct {},
	d3d11_depth_stencil: ^struct {},
	d3d11_input_layout: ^struct {},
	d3d11_vs: ^struct {},
	d3d11_ps: ^struct {},
	gl_blend: bool,
	gl_cullface: bool,
	gl_depthtest: bool,
	gl_scissor: bool,
	gl_program: u32,
	gl_src: u32,
	gl_dst: u32,
	gl_op: u32,
	gl_src_alpha: u32,
	gl_dst_alpha: u32,
	gl_op_alpha: u32,
	gl_polygon_mode: u32,
	gl_cull_mode: u32,
	gl_frontface: u32,
	gl_layout: [16]R3_LayoutDesc,
}
R3_ComputePipeline :: struct
{
	d3d11_cs: ^struct {},
	gl_program: u32,
}
R3_Sampler :: struct
{
	d3d11_sampler: ^struct {},
	gl_sampler: u32,
	gl_filtering: R3_TextureFiltering,
}
R3_TextureDesc :: struct
{
	width: i32,
	height: i32,
	depth: i32,
	format: R3_Format,
	usage: R3_Usage,
	binding_flags: u32,
	mipmap_count: i32,
	initial_data: rawptr,
}
R3_BufferDesc :: struct
{
	size: u32,
	binding_flags: u32,
	usage: R3_Usage,
	struct_size: u32,
	initial_data: rawptr,
}
R3_RenderTargetDesc :: struct
{
	color_textures: [8]^R3_Texture,
	depth_stencil_texture: ^R3_Texture,
}
R3_SamplerDesc :: struct
{
	filtering: R3_TextureFiltering,
	anisotropy: f32,
}
R3_BlendFunc :: enum i32{
	Null = 0,
	Zero,
	One,
	SrcColor,
	InvSrcColor,
	DstColor,
	InvDstColor,
	SrcAlpha,
	InvSrcAlpha,
	DstAlpha,
	InvDstAlpha,
}
R3_BlendOp :: enum i32{
	Add = 0,
	Subtract,
}
R3_FillMode :: enum i32{
	Solid = 0,
	Wireframe,
}
R3_CullMode :: enum i32{
	None = 0,
	Front,
	Back,
}
R3_PipelineDesc :: struct
{
	flag_cw_frontface: bool,
	flag_depth_test: bool,
	rendertargets: [8]struct
	{
		enable_blend: bool,
		src: R3_BlendFunc,
		dst: R3_BlendFunc,
		op: R3_BlendOp,
		src_alpha: R3_BlendFunc,
		dst_alpha: R3_BlendFunc,
		op_alpha: R3_BlendOp,
	},
	fill_mode: R3_FillMode,
	cull_mode: R3_CullMode,
	glsl: struct
	{
		vs: []u8,
		fs: []u8,
	},
	dx50: struct
	{
		vs: []u8,
		ps: []u8,
	},
	dx40: struct
	{
		vs: []u8,
		ps: []u8,
	},
	dx40_93: struct
	{
		vs: []u8,
		ps: []u8,
	},
	dx40_91: struct
	{
		vs: []u8,
		ps: []u8,
	},
	input_layout: [16]R3_LayoutDesc,
}
R3_ComputePipelineDesc :: struct
{
	glsl: string,
	dx50: []u8,
	dx40: []u8,
}
R3_Viewport :: struct
{
	x: f32,
	y: f32,
	width: f32,
	height: f32,
	min_depth: f32,
	max_depth: f32,
}
R3_ResourceView :: struct
{
	buffer: ^R3_Buffer,
	texture: ^R3_Texture,
}
R3_UnorderedView :: struct
{
	buffer: ^R3_Buffer,
	texture: ^R3_Texture,
}
R3_ClearDesc :: struct
{
	color: [4]f32,
	depth: f32,
	stencil: u32,
	flag_color: bool,
	flag_depth: bool,
	flag_stencil: bool,
}
R3_VertexInputs :: struct
{
	ibuffer: ^R3_Buffer,
	index_format: R3_Format,
	vbuffers: [16]struct
	{
		buffer: ^R3_Buffer,
		offset: u32,
		stride: u32,
	},
}
R3_UniformBuffer :: struct
{
	buffer: ^R3_Buffer,
	size: u32,
	offset: u32,
}
R3_PrimitiveType :: enum i32{
	TriangleList = 0,
	TriangleStrip,
	LineList,
	LineStrip,
	PointList,
}
R3_VideoDecoder :: struct
{
	width: u32,
	height: u32,
	fps_num: u32,
	fps_den: u32,
	frame_count: u64,
	d3d11_mfsource: rawptr,
}
R3_VideoDecoderDesc :: struct
{
	filepath: string,
}
R3_MappedResource :: struct
{
	memory: rawptr,
	size: uint,
	row_pitch: uint,
	depth_pitch: uint,
}
R3_MapKind :: enum i32{
	Read,
	Write,
	ReadWrite,
	Discard,
	NoOverwrite,
}
R3_TextContext :: struct
{
	d2d_factory: ^struct {},
	d2d_rendertarget: ^struct {},
	d2d_brush: ^struct {},
	dw_factory: ^struct {},
	rt_width: i32,
	rt_height: i32,
}
R3_Font :: struct
{
	dw_text_format: ^struct {},
}
R3_TextContextDesc :: struct
{
	rendertarget: ^R3_Texture,
}
R3_FontWeight :: enum i32{
	Regular = 0,
}
R3_FontStyle :: enum i32{
	Normal = 0,
}
R3_FontStretch :: enum i32{
	Normal = 0,
}
R3_FontDesc :: struct
{
	name: string,
	weight: R3_FontWeight,
	style: R3_FontStyle,
	stretch: R3_FontStretch,
	pt: f32,
}
@(default_calling_convention="c")
foreign {
	R3_D3D11_MakeContext :: proc "c"(arena: ^Arena, #by_ptr desc: R3_ContextDesc) -> ^R3_Context ---;
	R3_GL_MakeContext :: proc "c"(arena: ^Arena, #by_ptr desc: R3_ContextDesc) -> ^R3_Context ---;
	R3_MGL_MakeContext :: proc "c"(arena: ^Arena, #by_ptr desc: R3_ContextDesc) -> ^R3_Context ---;
	R3_QueryInfo :: proc "c"(ctx: ^R3_Context) -> R3_ContextInfo ---;
	R3_ResizeBuffers :: proc "c"(ctx: ^R3_Context) ---;
	R3_Present :: proc "c"(ctx: ^R3_Context) ---;
	R3_FreeContext :: proc "c"(ctx: ^R3_Context) ---;
	R3_IsDeviceLost :: proc "c"(ctx: ^R3_Context) -> bool ---;
	R3_MakeTexture :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_TextureDesc) -> R3_Texture ---;
	R3_MakeBuffer :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_BufferDesc) -> R3_Buffer ---;
	R3_MakeRenderTarget :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_RenderTargetDesc) -> R3_RenderTarget ---;
	R3_MakePipeline :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_PipelineDesc) -> R3_Pipeline ---;
	R3_MakeComputePipeline :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_ComputePipelineDesc) -> R3_ComputePipeline ---;
	R3_MakeSampler :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_SamplerDesc) -> R3_Sampler ---;
	R3_UpdateBuffer :: proc "c"(ctx: ^R3_Context, buffer: ^R3_Buffer, memory: rawptr, size: u32) ---;
	R3_UpdateTexture :: proc "c"(ctx: ^R3_Context, texture: ^R3_Texture, memory: rawptr, size: u32, slice: u32) ---;
	R3_FreeTexture :: proc "c"(ctx: ^R3_Context, texture: ^R3_Texture) ---;
	R3_FreeBuffer :: proc "c"(ctx: ^R3_Context, buffer: ^R3_Buffer) ---;
	R3_FreeRenderTarget :: proc "c"(ctx: ^R3_Context, rendertarget: ^R3_RenderTarget) ---;
	R3_FreePipeline :: proc "c"(ctx: ^R3_Context, pipeline: ^R3_Pipeline) ---;
	R3_FreeComputePipeline :: proc "c"(ctx: ^R3_Context, pipeline: ^R3_ComputePipeline) ---;
	R3_FreeSampler :: proc "c"(ctx: ^R3_Context, sampler: ^R3_Sampler) ---;
	R3_SetViewports :: proc "c"(ctx: ^R3_Context, count: int, viewports: [^]R3_Viewport) ---;
	R3_SetPipeline :: proc "c"(ctx: ^R3_Context, pipeline: ^R3_Pipeline) ---;
	R3_SetRenderTarget :: proc "c"(ctx: ^R3_Context, rendertarget: ^R3_RenderTarget) ---;
	R3_SetVertexInputs :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_VertexInputs) ---;
	R3_SetUniformBuffers :: proc "c"(ctx: ^R3_Context, count: int, buffers: [^]R3_UniformBuffer) ---;
	R3_SetResourceViews :: proc "c"(ctx: ^R3_Context, count: int, views: [^]R3_ResourceView) ---;
	R3_SetSamplers :: proc "c"(ctx: ^R3_Context, count: int, samplers: [^]^R3_Sampler) ---;
	R3_SetPrimitiveType :: proc "c"(ctx: ^R3_Context, type: R3_PrimitiveType) ---;
	R3_Clear :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_ClearDesc) ---;
	R3_Draw :: proc "c"(ctx: ^R3_Context, start_vertex: u32, vertex_count: u32, start_instance: u32, instance_count: u32) ---;
	R3_DrawIndexed :: proc "c"(ctx: ^R3_Context, start_index: u32, index_count: u32, start_instance: u32, instance_count: u32, base_vertex: i32) ---;
	R3_SetComputePipeline :: proc "c"(ctx: ^R3_Context, pipeline: ^R3_ComputePipeline) ---;
	R3_SetComputeUniformBuffers :: proc "c"(ctx: ^R3_Context, count: int, buffers: [^]R3_UniformBuffer) ---;
	R3_SetComputeResourceViews :: proc "c"(ctx: ^R3_Context, count: int, views: [^]R3_ResourceView) ---;
	R3_SetComputeUnorderedViews :: proc "c"(ctx: ^R3_Context, count: int, views: [^]R3_UnorderedView) ---;
	R3_Dispatch :: proc "c"(ctx: ^R3_Context, x: u32, y: u32, z: u32) ---;
	R3_MakeVideoDecoder :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_VideoDecoderDesc) -> R3_VideoDecoder ---;
	R3_FreeVideoDecoder :: proc "c"(ctx: ^R3_Context, video: ^R3_VideoDecoder) ---;
	R3_SetDecoderPosition :: proc "c"(ctx: ^R3_Context, video: ^R3_VideoDecoder, frame_index: u64) ---;
	R3_DecodeFrame :: proc "c"(ctx: ^R3_Context, video: ^R3_VideoDecoder, output_texture: ^R3_Texture, out_timestamp: ^u64) -> bool ---;
	R3_MapBuffer :: proc "c"(ctx: ^R3_Context, buffer: ^R3_Buffer, map_kind: R3_MapKind) -> R3_MappedResource ---;
	R3_UnmapBuffer :: proc "c"(ctx: ^R3_Context, buffer: ^R3_Buffer, written_offset: uintptr, written_size: uintptr) ---;
	R3_MapTexture :: proc "c"(ctx: ^R3_Context, texture: ^R3_Texture, slice: u32, map_kind: R3_MapKind) -> R3_MappedResource ---;
	R3_UnmapTexture :: proc "c"(ctx: ^R3_Context, texture: ^R3_Texture, slice: u32) ---;
	R3_CopyBuffer :: proc "c"(ctx: ^R3_Context, src: ^R3_Buffer, src_offset: u32, dst: ^R3_Buffer, dst_offset: u32, size: u32) ---;
	R3_CopyTexture2D :: proc "c"(ctx: ^R3_Context, src: ^R3_Texture, src_x: u32, src_y: u32, dst: ^R3_Texture, dst_x: u32, dst_y: u32, width: u32, height: u32) ---;
	R3_TextRequiredBindingFlags :: proc "c"() -> u32 ---;
	R3_TextRequiredUsage :: proc "c"() -> R3_Usage ---;
	R3_DWRITE_MakeTextContext :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_TextContextDesc) -> R3_TextContext ---;
	R3_FT_MakeTextContext :: proc "c"(ctx: ^R3_Context, #by_ptr desc: R3_TextContextDesc) -> R3_TextContext ---;
	R3_MakeFont :: proc "c"(ctx: ^R3_Context, text_ctx: ^R3_TextContext, #by_ptr desc: R3_FontDesc) -> R3_Font ---;
	R3_FreeTextContext :: proc "c"(ctx: ^R3_Context, text_ctx: ^R3_TextContext) ---;
	R3_FreeFont :: proc "c"(ctx: ^R3_Context, font: ^R3_Font) ---;
	R3_TextBegin :: proc "c"(ctx: ^R3_Context, text_ctx: ^R3_TextContext) ---;
	R3_TextDrawGlyphs :: proc "c"(ctx: ^R3_Context, text_ctx: ^R3_TextContext, str: string, font: ^R3_Font, transform: [^][3]f32) ---;
	R3_TextEnd :: proc "c"(ctx: ^R3_Context, text_ctx: ^R3_TextContext) ---;
}
