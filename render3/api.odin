package render3

import c "src:common_odin"

Usage :: enum i32
{
	Immutable = 0,
	Dynamic,
	GpuReadWrite,
}

BindingFlag :: enum
{
	VertexBuffer = 0,
	IndexBuffer = 1,
	UniformBuffer = 2,
	StructuredBuffer = 3,
	ShaderResource = 4,
	UnorderedAccess = 5,
	RenderTarget = 6,
	DepthStencil = 7,
	Indirect = 8,
}

BindingFlags :: bit_set[BindingFlag; u32]

Format :: enum i32
{
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

TextureFiltering :: enum i32
{
	Null = 0,
	Nearest,
	Linear,
	Anisotropic,
}

ContextInfo :: struct
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

Context :: struct {}

ContextDesc :: struct
{
	window: ^c.OS_Window,
}

LayoutDesc :: struct
{
	offset: u32,
	format: Format,
	buffer_slot: u32,
	divisor: u32,
}

Texture :: struct
{
	width: i32,
	height: i32,
	depth: i32,
	format: Format,
	d3d11_tex2d: ^struct {},
	d3d11_tex3d: ^struct {},
	d3d11_srv: ^struct {},
	d3d11_uav: ^struct {},
	gl_id: u32,
	gl_renderbuffer_id: u32,
}

Buffer :: struct
{
	d3d11_buffer: ^struct {},
	d3d11_srv: ^struct {},
	d3d11_uav: ^struct {},
	gl_id: u32,
}

RenderTarget :: struct
{
	d3d11_rtvs: [8]^struct {},
	d3d11_dsv: ^struct {},
	gl_id: u32,
}

Pipeline :: struct
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
	gl_layout: [16]LayoutDesc,
}

ComputePipeline :: struct
{
	d3d11_cs: ^struct {},
	gl_program: u32,
}

Sampler :: struct
{
	d3d11_sampler: ^struct {},
	gl_sampler: u32,
	gl_filtering: TextureFiltering,
}

TextureDesc :: struct
{
	width: i32,
	height: i32,
	depth: i32,
	format: Format,
	usage: Usage,
	binding_flags: BindingFlags,
	mipmap_count: i32,
	initial_data: rawptr,
}

BufferDesc :: struct
{
	size: u32,
	binding_flags: BindingFlags,
	usage: Usage,
	struct_size: u32,
	initial_data: rawptr,
}

RenderTargetDesc :: struct
{
	color_textures: [8]^Texture,
	depth_stencil_texture: ^Texture,
}

SamplerDesc :: struct
{
	filtering: TextureFiltering,
	anisotropy: f32,
}

BlendFunc :: enum i32
{
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

BlendOp :: enum i32
{
	Add = 0,
	Subtract,
}

FillMode :: enum i32
{
	Solid = 0,
	Wireframe,
}

CullMode :: enum i32
{
	None = 0,
	Front,
	Back,
}

PipelineDesc :: struct
{
	flag_cw_frontface: bool,
	flag_depth_test: bool,
	rendertargets: [8]struct
	{
		enable_blend: bool,
		src: BlendFunc,
		dst: BlendFunc,
		op: BlendOp,
		src_alpha: BlendFunc,
		dst_alpha: BlendFunc,
		op_alpha: BlendOp,
	},
	fill_mode: FillMode,
	cull_mode: CullMode,
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
	input_layout: [16]LayoutDesc,
}

ComputePipelineDesc :: struct
{
	glsl: string,
	dx50: []u8,
	dx40: []u8,
}

Viewport :: struct
{
	x: f32,
	y: f32,
	width: f32,
	height: f32,
	min_depth: f32,
	max_depth: f32,
}

ResourceView :: struct
{
	buffer: ^Buffer,
	texture: ^Texture,
}

UnorderedView :: struct
{
	buffer: ^Buffer,
	texture: ^Texture,
}

ClearDesc :: struct
{
	color: [4]f32,
	depth: f32,
	stencil: u32,
	flag_color: bool,
	flag_depth: bool,
	flag_stencil: bool,
}

VertexInputs :: struct
{
	ibuffer: ^Buffer,
	index_format: Format,
	vbuffers: [16]struct
	{
		buffer: ^Buffer,
		offset: u32,
		stride: u32,
	},
}

UniformBuffer :: struct
{
	buffer: ^Buffer,
	size: u32,
	offset: u32,
}

PrimitiveType :: enum i32
{
	TriangleList = 0,
	TriangleStrip,
	LineList,
	LineStrip,
	PointList,
}

VideoDecoder :: struct
{
	width: u32,
	height: u32,
	fps_num: u32,
	fps_den: u32,
	frame_count: u64,
	d3d11_mfsource: rawptr,
}

VideoDecoderDesc :: struct
{
	filepath: string,
}

MappedResource :: struct
{
	memory: rawptr,
	size: uint,
	row_pitch: uint,
	depth_pitch: uint,
}

MapKind :: enum i32
{
	Read,
	Write,
	ReadWrite,
	Discard,
	NoOverwrite,
}

TextContext :: struct
{
	d2d_factory: ^struct {},
	d2d_rendertarget: ^struct {},
	d2d_brush: ^struct {},
	dw_factory: ^struct {},
	rt_width: i32,
	rt_height: i32,
}

Font :: struct
{
	dw_text_format: ^struct {},
}

TextContextDesc :: struct
{
	rendertarget: ^Texture,
}

FontWeight :: enum i32
{
	Regular = 0,
}

FontStyle :: enum i32
{
	Normal = 0,
}

FontStretch :: enum i32
{
	Normal = 0,
}

FontDesc :: struct
{
	name: string,
	weight: FontWeight,
	style: FontStyle,
	stretch: FontStretch,
	pt: f32,
}

UpdateBufferSlice :: proc(ctx: ^Context, buffer: ^Buffer, data: []$T) {
	size := len(data) * size_of(data[0])
	assert(size >= 0 && size <= cast(int) max(u32))
	UpdateBuffer_(ctx, buffer, raw_data(data), cast(u32) size)
}
UpdateBufferStruct :: proc(ctx: ^Context, buffer: ^Buffer, data: ^$T) {
	UpdateBuffer_(ctx, buffer, data, cast(u32) size_of(data^))
}
UpdateBufferPtr :: proc(ctx: ^Context, buffer: ^Buffer, data: rawptr, size: int) {
	assert(size >= 0 && size <= cast(int) max(u32))
	UpdateBuffer_(ctx, buffer, data, cast(u32) size)
}

UpdateBuffer :: proc {
	UpdateBufferSlice,
	UpdateBufferStruct,
	UpdateBufferPtr,
}

UpdateTexture :: proc(ctx: ^Context, texture: ^Texture, data: []$T, #any_int slice: int) {
	size := len(data) * sizeof(data[0])
	assert(size >= 0 && size <= max(u32))
	assert(slice >= 0 && slice <= max(u32))
	UpdateTexture_(ctx, texture, raw_data(data), cast(u32) size, cast(u32) slice)
}

SetViewports :: proc(ctx: ^Context, viewports: []Viewport) {
	SetViewports_(ctx, len(viewports), raw_data(viewports))
}

SetUniformBuffers :: proc(ctx: ^Context, buffers: []UniformBuffer) {
	SetUniformBuffers_(ctx, len(buffers), raw_data(buffers))
}

SetResourceViews :: proc(ctx: ^Context, views: []ResourceView) {
	SetResourceViews_(ctx, len(views), raw_data(views))
}

SetSamplers :: proc(ctx: ^Context, samplers: []^Sampler) {
	SetSamplers_(ctx, len(samplers), raw_data(samplers))
}

SetComputeUniformBuffers :: proc(ctx: ^Context, buffers: []UniformBuffer) {
	SetComputeUniformBuffers_(ctx, len(buffers), raw_data(buffers))
}

SetComputeResourceViews :: proc(ctx: ^Context, views: []ResourceView) {
	SetComputeResourceViews_(ctx, len(views), raw_data(views))
}

SetComputeUnorderedViews :: proc(ctx: ^Context, views: []UnorderedView) {
	SetComputeUnorderedViews_(ctx, len(views), raw_data(views))
}

@(default_calling_convention="c", link_prefix="R3_")
foreign {
	D3D11_MakeContext :: proc "c"(arena: ^c.Arena, #by_ptr desc: ContextDesc) -> ^Context ---;
	GL_MakeContext :: proc "c"(arena: ^c.Arena, #by_ptr desc: ContextDesc) -> ^Context ---;
	MGL_MakeContext :: proc "c"(arena: ^c.Arena, #by_ptr desc: ContextDesc) -> ^Context ---;
	
	QueryInfo :: proc "c"(ctx: ^Context) -> ContextInfo ---;
	ResizeBuffers :: proc "c"(ctx: ^Context) ---;
	Present :: proc "c"(ctx: ^Context) ---;
	FreeContext :: proc "c"(ctx: ^Context) ---;
	IsDeviceLost :: proc "c"(ctx: ^Context) -> bool ---;
	
	MakeTexture :: proc "c"(ctx: ^Context, #by_ptr desc: TextureDesc) -> Texture ---;
	MakeBuffer :: proc "c"(ctx: ^Context, #by_ptr desc: BufferDesc) -> Buffer ---;
	MakeRenderTarget :: proc "c"(ctx: ^Context, #by_ptr desc: RenderTargetDesc) -> RenderTarget ---;
	MakePipeline :: proc "c"(ctx: ^Context, #by_ptr desc: PipelineDesc) -> Pipeline ---;
	MakeComputePipeline :: proc "c"(ctx: ^Context, #by_ptr desc: ComputePipelineDesc) -> ComputePipeline ---;
	MakeSampler :: proc "c"(ctx: ^Context, #by_ptr desc: SamplerDesc) -> Sampler ---;
	
	@(link_name="R3_UpdateBuffer")
	UpdateBuffer_ :: proc "c"(ctx: ^Context, buffer: ^Buffer, memory: rawptr, size: u32) ---;
	@(link_name="R3_UpdateTexture")
	UpdateTexture_ :: proc "c"(ctx: ^Context, texture: ^Texture, memory: rawptr, size: u32, slice: u32) ---;
	
	FreeTexture :: proc "c"(ctx: ^Context, texture: ^Texture) ---;
	FreeBuffer :: proc "c"(ctx: ^Context, buffer: ^Buffer) ---;
	FreeRenderTarget :: proc "c"(ctx: ^Context, rendertarget: ^RenderTarget) ---;
	FreePipeline :: proc "c"(ctx: ^Context, pipeline: ^Pipeline) ---;
	FreeComputePipeline :: proc "c"(ctx: ^Context, pipeline: ^ComputePipeline) ---;
	FreeSampler :: proc "c"(ctx: ^Context, sampler: ^Sampler) ---;
	
	@(link_name="R3_SetViewports")
	SetViewports_ :: proc "c"(ctx: ^Context, count: int, viewports: [^]Viewport) ---;
	SetPipeline :: proc "c"(ctx: ^Context, pipeline: ^Pipeline) ---;
	SetRenderTarget :: proc "c"(ctx: ^Context, rendertarget: ^RenderTarget) ---;
	SetVertexInputs :: proc "c"(ctx: ^Context, #by_ptr desc: VertexInputs) ---;
	@(link_name="R3_SetUniformBuffers")
	SetUniformBuffers_ :: proc "c"(ctx: ^Context, count: int, buffers: [^]UniformBuffer) ---;
	@(link_name="R3_SetResourceViews")
	SetResourceViews_ :: proc "c"(ctx: ^Context, count: int, views: [^]ResourceView) ---;
	@(link_name="R3_SetSamplers")
	SetSamplers_ :: proc "c"(ctx: ^Context, count: int, samplers: [^]^Sampler) ---;
	SetPrimitiveType :: proc "c"(ctx: ^Context, type: PrimitiveType) ---;
	
	Clear :: proc "c"(ctx: ^Context, #by_ptr desc: ClearDesc) ---;
	Draw :: proc "c"(ctx: ^Context, start_vertex: u32, vertex_count: u32, start_instance: u32, instance_count: u32) ---;
	DrawIndexed :: proc "c"(ctx: ^Context, start_index: u32, index_count: u32, start_instance: u32, instance_count: u32, base_vertex: i32) ---;
	SetComputePipeline :: proc "c"(ctx: ^Context, pipeline: ^ComputePipeline) ---;
	@(link_name="R3_SetComputeUniformBuffers")
	SetComputeUniformBuffers_ :: proc "c"(ctx: ^Context, count: int, buffers: [^]UniformBuffer) ---;
	@(link_name="R3_SetComputeResourceViews")
	SetComputeResourceViews_ :: proc "c"(ctx: ^Context, count: int, views: [^]ResourceView) ---;
	@(link_name="R3_SetComputeUnorderedViews")
	SetComputeUnorderedViews_ :: proc "c"(ctx: ^Context, count: int, views: [^]UnorderedView) ---;
	Dispatch :: proc "c"(ctx: ^Context, x: u32, y: u32, z: u32) ---;
	
	MakeVideoDecoder :: proc "c"(ctx: ^Context, #by_ptr desc: VideoDecoderDesc) -> VideoDecoder ---;
	FreeVideoDecoder :: proc "c"(ctx: ^Context, video: ^VideoDecoder) ---;
	SetDecoderPosition :: proc "c"(ctx: ^Context, video: ^VideoDecoder, frame_index: u64) ---;
	DecodeFrame :: proc "c"(ctx: ^Context, video: ^VideoDecoder, output_texture: ^Texture, out_timestamp: ^u64) -> bool ---;
	
	MapBuffer :: proc "c"(ctx: ^Context, buffer: ^Buffer, map_kind: MapKind) -> MappedResource ---;
	UnmapBuffer :: proc "c"(ctx: ^Context, buffer: ^Buffer, written_offset: uintptr, written_size: uintptr) ---;
	MapTexture :: proc "c"(ctx: ^Context, texture: ^Texture, slice: u32, map_kind: MapKind) -> MappedResource ---;
	UnmapTexture :: proc "c"(ctx: ^Context, texture: ^Texture, slice: u32) ---;
	CopyBuffer :: proc "c"(ctx: ^Context, src: ^Buffer, src_offset: u32, dst: ^Buffer, dst_offset: u32, size: u32) ---;
	CopyTexture2D :: proc "c"(ctx: ^Context, src: ^Texture, src_x: u32, src_y: u32, dst: ^Texture, dst_x: u32, dst_y: u32, width: u32, height: u32) ---;
	
	TextRequiredBindingFlags :: proc "c"() -> u32 ---;
	TextRequiredUsage :: proc "c"() -> Usage ---;
	
	DWRITE_MakeTextContext :: proc "c"(ctx: ^Context, #by_ptr desc: TextContextDesc) -> TextContext ---;
	FT_MakeTextContext :: proc "c"(ctx: ^Context, #by_ptr desc: TextContextDesc) -> TextContext ---;
	
	MakeFont :: proc "c"(ctx: ^Context, text_ctx: ^TextContext, #by_ptr desc: FontDesc) -> Font ---;
	FreeTextContext :: proc "c"(ctx: ^Context, text_ctx: ^TextContext) ---;
	FreeFont :: proc "c"(ctx: ^Context, font: ^Font) ---;
	
	TextBegin :: proc "c"(ctx: ^Context, text_ctx: ^TextContext) ---;
	TextDrawGlyphs :: proc "c"(ctx: ^Context, text_ctx: ^TextContext, str: string, font: ^Font, transform: [^][3]f32) ---;
	TextEnd :: proc "c"(ctx: ^Context, text_ctx: ^TextContext) ---;
}
