#include "common.h"
#include "api_os.h"
#include "api_os_opengl.h"
#include "api_render3.h"

struct VertexAttrib_
{
	bool is_enabled;
	bool is_normalized;
	bool is_float;
	GLenum type;
	int32 elem_count; // 1, 2, 3, or 4
	uint32 offset;
	uint32 divisor;
	uint32 buffer_slot;
}
typedef VertexAttrib_;

struct R3_Context
{
    OS_OpenGLApi api;
	int32 glversion;
	R3_ContextInfo info;
	bool has_framebuffer;
	bool has_texstorage;
	bool has_explicit_attrib_location;
	bool has_uniformbuffer;
	bool has_anisotropy;

	uint32 global_vao;
	uint32 curr_program;
	int32 ubo_indices[16];
	int32 texture_indices[16];

	GLenum curr_prim;
	GLenum curr_index_type;
	VertexAttrib_ curr_attribs[16];
};

#ifdef CONFIG_DEBUG
static void APIENTRY
OglDebugMessageCallback_(GLenum source, GLenum type, GLuint id, GLenum severity,
	GLsizei length, const GLchar* message, const void* user_param)
{
	char const* type_str = "Debug Message";
	
	if (type == GL_DEBUG_TYPE_ERROR)
		type_str = "Error";
	
	Log(LOG_WARN, "render3: OpenGL %s:\n\tType: 0x%x\n\tID: %u\n\tSeverity: 0x%x\n\tMessage: %.*s", type_str, type, id, severity, (int)length, message);

	if (type == GL_DEBUG_TYPE_ERROR && Assert_IsDebuggerPresent_())
		Debugbreak();
}
#endif

static GLenum
OglFormatToGLEnum_(R3_Format format, GLenum* out_unsized_format, GLenum* out_datatype)
{
	GLenum result = 0;
	GLenum unsized = 0;
	GLenum datatype = GL_UNSIGNED_BYTE;
	
	switch (format)
	{
		default: SafeAssert(!"Invalid R3_Format"); break;
		case R3_Format_Null: result = 0; break;
		
		case R3_Format_U8x1Norm:         result = GL_R8;    unsized = GL_RED;   break;
		case R3_Format_U8x1Norm_ToAlpha: result = GL_ALPHA; unsized = GL_ALPHA; break;
		case R3_Format_U8x2Norm:         result = GL_RG8;   unsized = GL_RG;    break;
		case R3_Format_U8x4Norm:         result = GL_RGBA8; unsized = GL_RGBA;  break;
		case R3_Format_U8x4Norm_Srgb:    result = GL_SRGB8_ALPHA8; unsized = GL_RGBA;  break;
		case R3_Format_U8x4Norm_Bgrx:    result = 0x93A1;   unsized = 0x80E1;   break; // TODO(ljre): Technically BGRA, not BGRX. But we only use it for video decoding, which OpenGL on windows is not possible.
		case R3_Format_U8x1:             result = GL_R8I;   unsized = GL_RED;   break;
		case R3_Format_U8x2:             result = GL_RG8I;  unsized = GL_RG;    break;
		case R3_Format_U8x4:             result = GL_RGBA8I; unsized = GL_RGBA; break;
		//case R3_Format_I16x2Norm:        result = GL_RG16; unsized = GL_RG; break;
		//case R3_Format_I16x4Norm:        result = GL_RGBA16; unsized = GL_RGBA; break;
		case R3_Format_I16x2:            result = GL_RG16I; unsized = GL_RG; datatype = GL_UNSIGNED_SHORT; break;
		case R3_Format_I16x4:            result = GL_RGBA16I; unsized = GL_RGBA; datatype = GL_UNSIGNED_SHORT; break;
		//case R3_Format_U16x1Norm:        result = GL_R16; unsized = GL_RED; datatype = GL_UNSIGNED_SHORT; break;
		//case R3_Format_U16x2Norm:        result = DXGI_FORMAT_R16G16_UNORM;        size = 4; break;
		//case R3_Format_U16x4Norm:        result = DXGI_FORMAT_R16G16B16A16_UNORM;  size = 8; break;
		case R3_Format_U16x2:            result = GL_RG16UI; unsized = GL_RG; datatype = GL_UNSIGNED_SHORT; break;
		case R3_Format_U16x4:            result = GL_RGBA16UI; unsized = GL_RGBA; datatype = GL_UNSIGNED_SHORT; break;
		case R3_Format_U32x1:            result = GL_R32I; unsized = GL_RED; datatype = GL_UNSIGNED_INT; break;
		case R3_Format_U32x2:            result = GL_RG32I; unsized = GL_RG; datatype = GL_UNSIGNED_INT; break;
		case R3_Format_U32x4:            result = GL_RGBA32I; unsized = GL_RGBA; datatype = GL_UNSIGNED_INT; break;
		
		case R3_Format_F16x2: result = GL_RG16F; unsized = GL_RG; datatype = GL_HALF_FLOAT; break;
		case R3_Format_F16x4: result = GL_RGBA16F; unsized = GL_RGBA; datatype = GL_HALF_FLOAT; break;
		case R3_Format_F32x1: result = GL_R32F; unsized = GL_RED; datatype = GL_FLOAT; break;
		case R3_Format_F32x2: result = GL_RG32F; unsized = GL_RG; datatype = GL_FLOAT; break;
		case R3_Format_F32x3: result = GL_RGB32F; unsized = GL_RGB; datatype = GL_FLOAT; break;
		case R3_Format_F32x4: result = GL_RGBA32F; unsized = GL_RGBA; datatype = GL_FLOAT; break;
		
		case R3_Format_D16:
		{
			result = GL_DEPTH_COMPONENT16;
			unsized = GL_DEPTH_COMPONENT;
			datatype = GL_UNSIGNED_SHORT;
		} break;
		case R3_Format_D24S8:
		{
			result = GL_DEPTH24_STENCIL8;
			unsized = GL_DEPTH_STENCIL;
			datatype = GL_UNSIGNED_INT_24_8;
		} break;
	}
	
	if (out_unsized_format)
		*out_unsized_format = unsized;
	if (out_datatype)
		*out_datatype = datatype;
	
	return result;
}

static void
OglRebindTextures_(R3_Context* ctx)
{
	for (intz i = 0; i < 16; ++i)
	{
		char name[64] = {};
		String prefix = StrInit("uTexture");
		SafeAssert(prefix.size+3 < sizeof(name));
		MemoryCopy(name, prefix.data, prefix.size);
		if (i == 0)
			name[prefix.size] = '0';
		else if (i < 10)
			name[prefix.size] = '0' + i;
		else
		{
			name[prefix.size+0] = '0' + i/10;
			name[prefix.size+1] = '0' + i%10;
		}

		int32 location = ctx->api.glGetUniformLocation(ctx->curr_program, name);
		if (location)
			ctx->api.glUniform1i(location, i);
	}
}

//------------------------------------------------------------------------
API R3_Context*
R3_GL_MakeContext(Arena* arena, R3_ContextDesc const* desc)
{
    R3_Context* ctx = ArenaPushStruct(arena, R3_Context);
    if (!ctx)
        return NULL;
    if (!OS_MakeOpenGLApi(&ctx->api, &(OS_OpenGLApiDesc) {
	    	.window = *desc->window,
	    	.swap_interval = 1
    	}))
    {
    	ArenaPop(arena, ctx);
    	return NULL;
    }
    
    //------------------------------------------------------------------------
	// Setup
#ifdef CONFIG_DEBUG
	if (ctx->api.glDebugMessageCallback)
	{
		ctx->api.glEnable(GL_DEBUG_OUTPUT);
		ctx->api.glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		ctx->api.glDebugMessageCallback(OglDebugMessageCallback_, ctx);
	}
#endif
	ctx->api.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	//------------------------------------------------------------------------
	// Base capabilities
	R3_ContextInfo info = {
		.backend_api = StrInit("OpenGL"),
	};
	
	int32 major, minor;
	ctx->api.glGetIntegerv(GL_MAJOR_VERSION, &major);
	ctx->api.glGetIntegerv(GL_MINOR_VERSION, &minor);
	ctx->glversion = major*10 + minor;
	
	ctx->api.glGetIntegerv(GL_MAX_TEXTURE_SIZE, (int32*)&info.max_texture_size);
	ctx->api.glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, (int32*)&info.max_render_target_textures);
	info.max_texture_size = ClampMin(info.max_texture_size, 2048);
	info.max_render_target_textures = ClampMin(info.max_render_target_textures, 4);
	
	uint8 const* vendor_cstr = ctx->api.glGetString(GL_VENDOR);
	uint8 const* renderer_cstr = ctx->api.glGetString(GL_RENDERER);
	uint8 const* version_cstr = ctx->api.glGetString(GL_VERSION);
	(void)vendor_cstr;
	if (renderer_cstr)
		info.driver_adapter = info.driver_renderer = StringFromCString((char const*)renderer_cstr);
	if (version_cstr)
		info.driver_version = StringFromCString((char const*)version_cstr);
	
	// TODO(ljre): Fill in supported texture formats
	info.supported_texture_formats[0] |= (1U << R3_Format_U8x1Norm);
	info.supported_texture_formats[0] |= (1U << R3_Format_U8x1Norm_ToAlpha);
	info.supported_texture_formats[0] |= (1U << R3_Format_U8x4Norm);

	if (!ctx->api.is_es)
	{
		info.backend_api = Str("OpenGL");
		info.supported_texture_formats[0] |= (1U << R3_Format_D16);
		SafeAssert(ctx->glversion >= 20);

		if (ctx->glversion >= 20)
		{
			info.has_separate_alpha_blend = true;
			info.has_32bit_index = true;
		}

		if (ctx->glversion >= 21)
		{
		}

		if (ctx->glversion >= 30)
		{
			ctx->has_framebuffer = true;
			info.supported_texture_formats[0] |= (1U << R3_Format_U8x1Norm);
			info.supported_texture_formats[0] |= (1U << R3_Format_U8x2Norm);
			info.supported_texture_formats[0] |= (1U << R3_Format_D24S8);
			info.supported_texture_formats[0] |= (1U << R3_Format_F16x2);
			info.supported_texture_formats[0] |= (1U << R3_Format_F16x4);
		}

		if (ctx->glversion >= 31)
		{
			ctx->has_uniformbuffer = true; // This is why our baseline is 3.1
			info.has_instancing = true;
		}

		if (ctx->glversion >= 32)
		{
			info.has_base_vertex = true;
		}

		if (ctx->glversion >= 33)
		{
			ctx->has_explicit_attrib_location = true;
		}
		
		if (ctx->glversion >= 42)
		{
			ctx->has_texstorage = true;
		}

		if (ctx->glversion >= 43)
		{
			info.has_compute_pipeline = true;
			GLint data_x = 0;
			GLint data_y = 0;
			GLint data_z = 0;
			ctx->api.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &data_x);
			ctx->api.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &data_y);
			ctx->api.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &data_z);
			info.max_dispatch_x = data_x;
			info.max_dispatch_y = data_y;
			info.max_dispatch_z = data_z;
		}
	}
	else
	{
		info.backend_api = Str("OpenGL ES");
		SafeAssert(ctx->glversion >= 20);
		
		if (ctx->glversion >= 20)
		{
			info.has_separate_alpha_blend = true;
			ctx->has_framebuffer = true;
		}

		if (ctx->glversion >= 30)
		{
			info.has_instancing = true;
			info.has_32bit_index = true;
			info.supported_texture_formats[0] |= (1U << R3_Format_D16);
			info.supported_texture_formats[0] |= (1U << R3_Format_D24S8);
			info.supported_texture_formats[0] |= (1U << R3_Format_U8x1Norm);
			info.supported_texture_formats[0] |= (1U << R3_Format_U8x2Norm);
			ctx->has_texstorage = true;
			ctx->has_uniformbuffer = true; // This is why our baseline is 3.0
			ctx->has_explicit_attrib_location = true;
		}
		
		if (ctx->glversion >= 31)
		{
			info.has_compute_pipeline = true;
			GLint data_x = 0;
			GLint data_y = 0;
			GLint data_z = 0;
			ctx->api.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &data_x);
			ctx->api.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &data_y);
			ctx->api.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &data_z);
			info.max_dispatch_x = data_x;
			info.max_dispatch_y = data_y;
			info.max_dispatch_z = data_z;
		}
		
		if (ctx->glversion >= 32)
		{
			// NOTE(ljre): glDrawXXXBaseVertex came after compute shaders?????
			info.has_base_vertex = true;
		}
	}

	//------------------------------------------------------------------------
	// Checking for extensions
	int32 extension_count = 0;
	ctx->api.glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
	for (int32 i = 0; i < extension_count; ++i)
	{
		GLubyte const* name_cstr = ctx->api.glGetStringi(GL_EXTENSIONS, (uint32)i);
		String name = StringFromCString((char const*)name_cstr);

		if (StringEquals(name, Str("GL_ARB_texture_storage")))
			ctx->has_texstorage = true;
		else if (StringEquals(name, Str("GL_ARB_uniform_buffer_object")))
			ctx->has_uniformbuffer = true;
		else if (StringEquals(name, Str("GL_ARB_framebuffer_object")))
			ctx->has_framebuffer = true;
		else if (StringEquals(name, Str("GL_EXT_texture_filter_anisotropic")))
		{
			ctx->has_anisotropy = true;
			GLint value = 0;
			ctx->api.glGetIntegerv(0x84FF /*GL_MAX_TEXTURE_MAX_ANISOTROPY*/, &value);
			info.max_anisotropy_level = value;
		}
		else if (StringEquals(name, Str("GL_ARB_explicit_attrib_location")))
			ctx->has_explicit_attrib_location = true;
	}
	
	//------------------------------------------------------------------------
	// Making sure all the minimum features are supported
	ctx->info = info;
	SafeAssert(
		ctx->has_uniformbuffer &&
		ctx->has_explicit_attrib_location &&
		ctx->has_framebuffer);

	ctx->api.glGenVertexArrays(1, &ctx->global_vao);
	ctx->api.glBindVertexArray(ctx->global_vao);

    return ctx;
}

API R3_ContextInfo
R3_QueryInfo(R3_Context *ctx)
{
	Trace();
	return ctx->info;
}

API void
R3_ResizeBuffers(R3_Context *ctx)
{
	Trace();
	ctx->api.resize_buffers(&ctx->api);
}

API void
R3_Present(R3_Context *ctx)
{
	Trace();
	ctx->api.present(&ctx->api);
}

API void
R3_FreeContext(R3_Context *ctx)
{
	Trace();
	OS_FreeOpenGLApi(&ctx->api);
}

//------------------------------------------------------------------------
API R3_Texture
R3_MakeTexture(R3_Context* ctx, R3_TextureDesc const* desc)
{
    Trace();
    R3_Texture out = {};

	GLenum unsized_format, datatype;
	GLenum format = OglFormatToGLEnum_(desc->format, &unsized_format, &datatype);
	SafeAssert(format && unsized_format && datatype);
	bool can_be_renderbuffer =
		 (unsized_format == GL_DEPTH_COMPONENT || unsized_format == GL_DEPTH_STENCIL) &&
		!(desc->binding_flags & R3_BindingFlag_ShaderResource) &&
		 (desc->binding_flags & R3_BindingFlag_DepthStencil) &&
		!desc->initial_data;

	if (can_be_renderbuffer)
	{
		ctx->api.glGenRenderbuffers(1, &out.gl_renderbuffer_id);
		ctx->api.glBindRenderbuffer(GL_RENDERBUFFER, out.gl_renderbuffer_id);
		ctx->api.glRenderbufferStorage(GL_RENDERBUFFER, format, desc->width, desc->height);
		ctx->api.glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	else
	{
		ctx->api.glGenTextures(1, &out.gl_id);
		ctx->api.glBindTexture(GL_TEXTURE_2D, out.gl_id);
		if (ctx->has_texstorage)
		{
			ctx->api.glTexStorage2D(GL_TEXTURE_2D, 1, format, desc->width, desc->height);
			if (desc->initial_data)
				ctx->api.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->width, desc->height, unsized_format, datatype, desc->initial_data);
		}
		else
			ctx->api.glTexImage2D(GL_TEXTURE_2D, 0, (int32)format, desc->width, desc->height, 0, unsized_format, datatype, desc->initial_data);
		ctx->api.glBindTexture(GL_TEXTURE_2D, 0);
	}

	out.format = desc->format;
	out.width = desc->width;
	out.height = desc->height;
	out.depth = desc->depth;

    return out;
}

API R3_Buffer
R3_MakeBuffer(R3_Context* ctx, R3_BufferDesc const* desc)
{
    Trace();
    R3_Buffer out = {};

	GLenum kind;
	if (desc->binding_flags & R3_BindingFlag_VertexBuffer)
		kind = GL_ARRAY_BUFFER;
	if (desc->binding_flags & R3_BindingFlag_IndexBuffer)
		kind = GL_ELEMENT_ARRAY_BUFFER;
	if (desc->binding_flags & R3_BindingFlag_UniformBuffer)
		kind = GL_UNIFORM_BUFFER;
	if (desc->binding_flags & R3_BindingFlag_StructuredBuffer)
		kind = GL_SHADER_STORAGE_BUFFER;
	GLenum usage;
	if (desc->usage == R3_Usage_Dynamic)
		usage = GL_STREAM_DRAW;
	else
		usage = GL_STATIC_DRAW;

	ctx->api.glGenBuffers(1, &out.gl_id);
	ctx->api.glBindBuffer(kind, out.gl_id);
	ctx->api.glBufferData(kind, desc->size, desc->initial_data, usage);
	ctx->api.glBindBuffer(kind, 0);

    return out;
}

API R3_RenderTarget
R3_MakeRenderTarget(R3_Context* ctx, R3_RenderTargetDesc const* desc)
{
    Trace();
    R3_RenderTarget out = {};

	ctx->api.glGenFramebuffers(1, &out.gl_id);
	ctx->api.glBindFramebuffer(GL_FRAMEBUFFER, out.gl_id);
	for (intsize i = 0; i < ArrayLength(desc->color_textures); ++i)
	{
		if (!desc->color_textures[i])
			continue;
		if (desc->color_textures[i]->gl_id)
			ctx->api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i, GL_TEXTURE_2D, desc->color_textures[i]->gl_id, 0);
		else if (desc->color_textures[i]->gl_renderbuffer_id)
			ctx->api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i, GL_RENDERBUFFER, desc->color_textures[i]->gl_renderbuffer_id);
	}
	if (desc->depth_stencil_texture)
	{
		if (desc->depth_stencil_texture->gl_id)
			ctx->api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, desc->depth_stencil_texture->gl_id, 0);
		else if (desc->depth_stencil_texture->gl_renderbuffer_id)
			ctx->api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, desc->depth_stencil_texture->gl_renderbuffer_id);
	}
	ctx->api.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return out;
}

API R3_Pipeline
R3_MakePipeline(R3_Context* ctx, R3_PipelineDesc const* desc)
{
    Trace();
    R3_Pipeline out = {};

	String vs = desc->glsl.vs;
	String fs = desc->glsl.fs;
	SafeAssert(vs.size > 0 && vs.data[vs.size-1] == 0);
	SafeAssert(fs.size > 0 && fs.data[fs.size-1] == 0);

	if (StringStartsWith(vs, Str("#version")))
	{
		uint8 const* first = MemoryFindByte(vs.data, '\n', vs.size);
		if (first)
		{
			uint8 const* old_data = vs.data;
			vs.data = first + 1;
			vs.size -= old_data - first - 1;
		}
	}
	if (StringStartsWith(fs, Str("#version")))
	{
		uint8 const* first = MemoryFindByte(fs.data, '\n', fs.size);
		if (first)
		{
			uint8 const* old_data = fs.data;
			fs.data = first + 1;
			fs.size -= old_data - first - 1;
		}
	}

	char const* vertex_shader_source = (char const*)vs.data;
	char const* fragment_shader_source = (char const*)fs.data;

	char const* vertex_lines[] = {
		"#version 140\n#extension GL_ARB_explicit_attrib_location : enable\n",
		vertex_shader_source,
	};
	char const* fragment_lines[] = {
		"#version 140\n#extension GL_ARB_explicit_attrib_location : enable\n",
		"\n",
		fragment_shader_source,
	};

	if (ctx->api.is_es)
	{
		vertex_lines[0] = "#version 300 es\n";
		fragment_lines[0] = "#version 300 es\n";
		fragment_lines[1] = "precision mediump float; precision highp int;\n";
	}

	int32 success;
	uint32 vertex_shader = ctx->api.glCreateShader(GL_VERTEX_SHADER);
	ctx->api.glShaderSource(vertex_shader, ArrayLength(vertex_lines), vertex_lines, NULL);
	ctx->api.glCompileShader(vertex_shader);
	ctx->api.glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	SafeAssert(success);

	uint32 fragment_shader = ctx->api.glCreateShader(GL_FRAGMENT_SHADER);
	ctx->api.glShaderSource(fragment_shader, ArrayLength(fragment_lines), fragment_lines, NULL);
	ctx->api.glCompileShader(fragment_shader);
	ctx->api.glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	SafeAssert(success);

	uint32 program = ctx->api.glCreateProgram();
	ctx->api.glAttachShader(program, vertex_shader);
	ctx->api.glAttachShader(program, fragment_shader);
	ctx->api.glLinkProgram(program);
	ctx->api.glGetProgramiv(program, GL_LINK_STATUS, &success);
	SafeAssert(success);

	static uint32 const functable[] = {
		[R3_BlendFunc_Zero] = GL_ZERO,
		[R3_BlendFunc_One] = GL_ONE,
		[R3_BlendFunc_SrcColor] = GL_SRC_COLOR,
		[R3_BlendFunc_InvSrcColor] = GL_ONE_MINUS_SRC_COLOR,
		[R3_BlendFunc_DstColor] = GL_DST_COLOR,
		[R3_BlendFunc_InvDstColor] = GL_ONE_MINUS_DST_COLOR,
		[R3_BlendFunc_SrcAlpha] = GL_SRC_ALPHA,
		[R3_BlendFunc_InvSrcAlpha] = GL_ONE_MINUS_SRC_ALPHA,
		[R3_BlendFunc_DstAlpha] = GL_DST_ALPHA,
		[R3_BlendFunc_InvDstAlpha] = GL_ONE_MINUS_DST_ALPHA,
	};
	static uint32 const optable[] = {
		[R3_BlendOp_Add] = GL_FUNC_ADD,
		[R3_BlendOp_Subtract] = GL_FUNC_SUBTRACT,
	};

	bool enable_blend = desc->rendertargets[0].enable_blend;
	R3_BlendFunc src = desc->rendertargets[0].src;
	R3_BlendFunc dst = desc->rendertargets[0].dst;
	R3_BlendFunc src_alpha = desc->rendertargets[0].src_alpha;
	R3_BlendFunc dst_alpha = desc->rendertargets[0].dst_alpha;
	R3_BlendOp op = desc->rendertargets[0].op;
	R3_BlendOp op_alpha = desc->rendertargets[0].op_alpha;
	SafeAssert(src       >= 0 && src       < ArrayLength(functable));
	SafeAssert(dst       >= 0 && dst       < ArrayLength(functable));
	SafeAssert(src_alpha >= 0 && src_alpha < ArrayLength(functable));
	SafeAssert(dst_alpha >= 0 && dst_alpha < ArrayLength(functable));
	SafeAssert(op       >= 0 && op       < ArrayLength(optable));
	SafeAssert(op_alpha >= 0 && op_alpha < ArrayLength(optable));

	static uint32 const culltable[] = {
		[R3_CullMode_None] = 0,
		[R3_CullMode_Front] = GL_FRONT,
		[R3_CullMode_Back] = GL_BACK,
	};
	SafeAssert(desc->cull_mode >= 0 && desc->cull_mode < ArrayLength(culltable));

	out.gl_cull_mode = culltable[desc->cull_mode];
	out.gl_cullface = (desc->cull_mode != R3_CullMode_None);
	out.gl_polygon_mode = 0;
	out.gl_depthtest = desc->flag_depth_test;
	out.gl_scissor = false;
	out.gl_blend = enable_blend;
	out.gl_program = program;
	out.gl_src = functable[src];
	out.gl_dst = functable[dst];
	out.gl_src_alpha = functable[src_alpha];
	out.gl_dst_alpha = functable[dst_alpha];
	out.gl_op = optable[op];
	out.gl_op_alpha = optable[op_alpha];
	out.gl_frontface = (desc->flag_cw_frontface) ? GL_CW : GL_CCW;
	MemoryCopy(out.gl_layout, desc->input_layout, sizeof(R3_LayoutDesc[16]));

    return out;
}

API R3_ComputePipeline
R3_MakeComputePipeline(R3_Context* ctx, R3_ComputePipelineDesc const* desc)
{
    Trace();
    R3_ComputePipeline out = {};

    return out;
}

API R3_Sampler
R3_MakeSampler(R3_Context* ctx, R3_SamplerDesc const* desc)
{
    Trace();
    R3_Sampler out = {};

	ctx->api.glGenSamplers(1, &out.gl_sampler);
	switch (desc->filtering)
	{
		case R3_TextureFiltering_Null: break;
		case R3_TextureFiltering_Nearest:
		{
			ctx->api.glSamplerParameteri(out.gl_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			ctx->api.glSamplerParameteri(out.gl_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		} break;
		case R3_TextureFiltering_Linear:
		{
			ctx->api.glSamplerParameteri(out.gl_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			ctx->api.glSamplerParameteri(out.gl_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		} break;
		case R3_TextureFiltering_Anisotropic:
		{
			ctx->api.glSamplerParameteri(out.gl_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			ctx->api.glSamplerParameteri(out.gl_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			ctx->api.glSamplerParameterf(out.gl_sampler, 0x84FE, desc->anisotropy);
		} break;
	}

    return out;
}

API void
R3_FreeTexture(R3_Context* ctx, R3_Texture* texture)
{
	Trace();
	if (texture->gl_id)
		ctx->api.glDeleteTextures(1, &texture->gl_id);
	if (texture->gl_renderbuffer_id)
		ctx->api.glDeleteRenderbuffers(1, &texture->gl_renderbuffer_id);

	*texture = (R3_Texture) {};
}

API void
R3_FreeBuffer(R3_Context* ctx, R3_Buffer* buffer)
{
	Trace();
	if (buffer->gl_id)
		ctx->api.glDeleteBuffers(1, &buffer->gl_id);

	*buffer = (R3_Buffer) {};
}

API void
R3_FreeRenderTarget(R3_Context* ctx, R3_RenderTarget* rendertarget)
{
	Trace();
	if (rendertarget->gl_id)
		ctx->api.glDeleteFramebuffers(1, &rendertarget->gl_id);

	*rendertarget = (R3_RenderTarget) {};
}

API void
R3_FreePipeline(R3_Context* ctx, R3_Pipeline* pipeline)
{
	Trace();
	if (pipeline->gl_program)
		ctx->api.glDeleteProgram(pipeline->gl_program);

	*pipeline = (R3_Pipeline) {};
}

API void
R3_FreeComputePipeline(R3_Context* ctx, R3_ComputePipeline* pipeline)
{
	Trace();
	if (pipeline->gl_program)
		ctx->api.glDeleteProgram(pipeline->gl_program);

	*pipeline = (R3_ComputePipeline) {};
}

API void
R3_FreeSampler(R3_Context* ctx, R3_Sampler* sampler)
{
	Trace();
	if (sampler->gl_sampler)
		ctx->api.glDeleteSamplers(1, &sampler->gl_sampler);

	*sampler = (R3_Sampler) {};
}

API void
R3_UpdateBuffer(R3_Context* ctx, R3_Buffer* buffer, void const* memory, uint32 size)
{
	Trace();

	ctx->api.glBindBuffer(GL_ARRAY_BUFFER, buffer->gl_id);
	ctx->api.glBufferData(GL_ARRAY_BUFFER, size, memory, GL_STREAM_DRAW);
	ctx->api.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

API void
R3_UpdateTexture(R3_Context* ctx, R3_Texture* texture, void const* memory, uint32 size, uint32 slice)
{
	Trace();
	GLenum unsized_format;
	GLenum type;
	OglFormatToGLEnum_(texture->format, &unsized_format, &type);

	ctx->api.glBindTexture(GL_TEXTURE_2D, texture->gl_id);
	ctx->api.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, unsized_format, type, memory);
	ctx->api.glBindTexture(GL_TEXTURE_2D, 0);
}

API void
R3_CopyBuffer(R3_Context* ctx, R3_Buffer* src, uint32 src_offset, R3_Buffer* dst, uint32 dst_offset, uint32 size)
{
	Trace();
	ctx->api.glBindBuffer(GL_COPY_READ_BUFFER, src->gl_id);
	ctx->api.glBindBuffer(GL_COPY_WRITE_BUFFER, dst->gl_id);
	ctx->api.glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset, dst_offset, size);
	ctx->api.glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	ctx->api.glBindBuffer(GL_COPY_READ_BUFFER, 0);
}

API void
R3_CopyTexture2D(R3_Context* ctx, R3_Texture* src, uint32 src_x, uint32 src_y, R3_Texture* dst, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height)
{
	Trace();
	ctx->api.glCopyImageSubData(src->gl_id, GL_TEXTURE_2D, 0, src_x, src_y, 0, dst->gl_id, GL_TEXTURE_2D, 0, dst_x, dst_y, 0, width, height, 1);
}

API void
R3_SetViewports(R3_Context* ctx, intsize count, R3_Viewport viewports[])
{
	Trace();
	if (count > 0)
	{
		int32 x = (int32)viewports[0].x;
		int32 y = (int32)viewports[0].y;
		int32 w = (int32)viewports[0].width;
		int32 h = (int32)viewports[0].height;
		float32 near = viewports[0].min_depth;
		float32 far = viewports[0].max_depth;
		ctx->api.glViewport(x, y, w, h);
		ctx->api.glDepthRangef(near, far);
	}
}

API void
R3_SetPipeline(R3_Context* ctx, R3_Pipeline* pipeline)
{
	ctx->api.glUseProgram(pipeline->gl_program);
	ctx->curr_program = pipeline->gl_program;
	for (intz i = 0; i < ArrayLength(ctx->ubo_indices); ++i)
	{
		char name[64] = {};
		String prefix = StrInit("type_UniformBuffer");
		SafeAssert(prefix.size+3 < sizeof(name));
		MemoryCopy(name, prefix.data, prefix.size);
		if (i == 0)
			name[prefix.size] = '0';
		else if (i < 10)
			name[prefix.size] = '0' + i;
		else
		{
			name[prefix.size+0] = '0' + i/10;
			name[prefix.size+1] = '0' + i%10;
		}

		uint32 block_id = ctx->api.glGetUniformBlockIndex(ctx->curr_program, name);
		SafeAssert(block_id < INT32_MAX || block_id == UINT32_MAX);
		ctx->ubo_indices[i] = (int32)block_id;
	}
	OglRebindTextures_(ctx);
	for (intz i = 0; i < ArrayLength(pipeline->gl_layout); ++i)
	{
		R3_LayoutDesc const* layout = &pipeline->gl_layout[i];
		if (!layout->format)
			ctx->curr_attribs[i].is_enabled = false;
		else
		{
			int32 elem_count = 0;
			bool is_normalized = false;
			bool is_float = false;
			GLenum datatype = 0;

			switch (layout->format)
			{
				case R3_Format_F32x1: elem_count = 1; datatype = GL_FLOAT; is_float = true; break;
				case R3_Format_F32x2: elem_count = 2; datatype = GL_FLOAT; is_float = true; break;
				case R3_Format_F32x3: elem_count = 3; datatype = GL_FLOAT; is_float = true; break;
				case R3_Format_F32x4: elem_count = 4; datatype = GL_FLOAT; is_float = true; break;
				case R3_Format_F16x2: elem_count = 2; datatype = GL_HALF_FLOAT; is_float = true; break;
				case R3_Format_F16x4: elem_count = 4; datatype = GL_HALF_FLOAT; is_float = true; break;
				case R3_Format_I16x2: elem_count = 2; datatype = GL_SHORT; break;
				case R3_Format_I16x4: elem_count = 4; datatype = GL_SHORT; break;

				default: SafeAssert(false);
			}

			ctx->curr_attribs[i] = (VertexAttrib_) {
				.is_enabled = true,
				.is_normalized = is_normalized,
				.is_float = is_float,
				.divisor = layout->divisor,
				.buffer_slot = layout->buffer_slot,
				.offset = layout->offset,
				.elem_count = elem_count,
				.type = datatype,
			};
		}
	}
	if (!pipeline->gl_blend)
		ctx->api.glDisable(GL_BLEND);
	else
	{
		ctx->api.glEnable(GL_BLEND);
		ctx->api.glBlendFuncSeparate(pipeline->gl_src, pipeline->gl_dst, pipeline->gl_src_alpha, pipeline->gl_dst_alpha);
		ctx->api.glBlendEquationSeparate(pipeline->gl_op, pipeline->gl_op_alpha);
	}
	ctx->api.glFrontFace(pipeline->gl_frontface);
	if (!pipeline->gl_cullface)
		ctx->api.glDisable(GL_CULL_FACE);
	else
	{
		ctx->api.glEnable(GL_CULL_FACE);
		ctx->api.glCullFace(pipeline->gl_cull_mode);
	}
	ctx->api.glFrontFace(pipeline->gl_frontface);
	if (!pipeline->gl_depthtest)
		ctx->api.glDisable(GL_DEPTH_TEST);
	else
		ctx->api.glEnable(GL_DEPTH_TEST);
}

API void
R3_SetRenderTarget(R3_Context* ctx, R3_RenderTarget* rendertarget)
{
	Trace();
	uint32 fbo = 0;
	if (rendertarget)
		fbo = rendertarget->gl_id;
	ctx->api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

API void
R3_SetVertexInputs(R3_Context* ctx, R3_VertexInputs const* desc)
{
	Trace();
	if (desc->ibuffer)
	{
		ctx->api.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, desc->ibuffer->gl_id);
		ctx->curr_index_type = (desc->index_format == R3_Format_U32x1) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	}
	else
		ctx->api.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	for (intz i = 0; i < ArrayLength(ctx->curr_attribs); ++i)
	{
		VertexAttrib_ const* attrib = &ctx->curr_attribs[i];
		if (!attrib->is_enabled)
		{
			ctx->api.glDisableVertexAttribArray(i);
			continue;
		}
		intz buffer_slot = attrib->buffer_slot;
		SafeAssert(buffer_slot < ArrayLength(desc->vbuffers));

		uint32 buffer_id = (desc->vbuffers[buffer_slot].buffer) ? desc->vbuffers[buffer_slot].buffer->gl_id : 0;
		intz stride = desc->vbuffers[buffer_slot].stride;
		uint32 base_offset = desc->vbuffers[buffer_slot].offset;
		ctx->api.glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
		ctx->api.glEnableVertexAttribArray(i);
		ctx->api.glVertexAttribDivisor(i, attrib->divisor);
		if (attrib->is_normalized || attrib->is_float)
			ctx->api.glVertexAttribPointer(i, attrib->elem_count, attrib->type, attrib->is_normalized, stride, (void*)(base_offset + attrib->offset));
		else
			ctx->api.glVertexAttribIPointer(i, attrib->elem_count, attrib->type, stride, (void*)(base_offset + attrib->offset));
	}
}

API void
R3_SetUniformBuffers(R3_Context* ctx, intsize count, R3_UniformBuffer buffers[])
{
	Trace();
	SafeAssert(count <= ArrayLength(ctx->ubo_indices));
	for (intz i = 0; i < count; ++i)
	{
		int32 block_id = ctx->ubo_indices[i];
		SafeAssert(block_id != -1);
		SafeAssert(block_id >= 0);
		if (!buffers[i].offset && !buffers[i].size)
			ctx->api.glBindBufferBase(GL_UNIFORM_BUFFER, i, buffers[i].buffer->gl_id);
		else
			ctx->api.glBindBufferRange(GL_UNIFORM_BUFFER, i, buffers[i].buffer->gl_id, buffers[i].offset, buffers[i].size);
		ctx->api.glUniformBlockBinding(ctx->curr_program, (uint32)block_id, i);
	}
}

API void
R3_SetResourceViews(R3_Context* ctx, intsize count, R3_ResourceView views[])
{
	Trace();
	for (intz i = 0; i < count; ++i)
	{
		if (views[i].buffer)
			ctx->api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, views[i].buffer->gl_id);
		else
		{
			ctx->api.glActiveTexture(GL_TEXTURE0 + i);
			ctx->api.glBindTexture(GL_TEXTURE_2D, views[i].texture->gl_id);
		}
	}
	ctx->api.glActiveTexture(GL_TEXTURE0);
}

API void
R3_SetSamplers(R3_Context* ctx, intsize count, R3_Sampler* samplers[])
{
	Trace();
	for (intz i = 0; i < count; ++i)
		ctx->api.glBindSampler(i, samplers[i] ? samplers[i]->gl_sampler : 0);
}

API void
R3_SetPrimitiveType(R3_Context* ctx, R3_PrimitiveType type)
{
	Trace();
	switch (type)
	{
		case R3_PrimitiveType_TriangleList: ctx->curr_prim = GL_TRIANGLES; break;
		case R3_PrimitiveType_TriangleStrip: ctx->curr_prim = GL_TRIANGLE_STRIP; break;
		case R3_PrimitiveType_TriangleFan: ctx->curr_prim = GL_TRIANGLE_FAN; break;
		case R3_PrimitiveType_LineList: ctx->curr_prim = GL_LINES; break;
		case R3_PrimitiveType_LineStrip: ctx->curr_prim = GL_LINE_STRIP; break;
		case R3_PrimitiveType_PointList: ctx->curr_prim = GL_POINTS; break;
		default: SafeAssert(false);
	}
}

API void
R3_Clear(R3_Context* ctx, R3_ClearDesc const* desc)
{
	Trace();
	GLenum flags = 0;
	if (desc->flag_color)
	{
		ctx->api.glClearColor(desc->color[0], desc->color[1], desc->color[2], desc->color[3]);
		flags |= GL_COLOR_BUFFER_BIT;
	}
	if (desc->flag_depth)
	{
		ctx->api.glClearDepthf(desc->depth);
		flags |= GL_DEPTH_BUFFER_BIT;
	}
	if (desc->flag_stencil)
	{
		SafeAssert(desc->stencil <= INT32_MAX);
		ctx->api.glClearStencil((int32)desc->stencil);
		flags |= GL_STENCIL_BUFFER_BIT;
	}

	if (flags)
		ctx->api.glClear(flags);
}

API void
R3_Draw(R3_Context* ctx, uint32 start_vertex, uint32 vertex_count, uint32 start_instance, uint32 instance_count)
{
	Trace();
	SafeAssert(start_instance == 0);

	if (instance_count)
		ctx->api.glDrawArraysInstanced(GL_TRIANGLES, start_vertex, vertex_count, instance_count);
	else
		ctx->api.glDrawArrays(GL_TRIANGLES, start_vertex, vertex_count);
}

API void
R3_DrawIndexed(R3_Context* ctx, uint32 start_index, uint32 index_count, uint32 start_instance, uint32 instance_count, int32 base_vertex)
{
	Trace();
	SafeAssert(start_instance == 0);
	GLenum type = ctx->curr_index_type;
	GLenum prim = ctx->curr_prim;
	uintptr offset = start_index * (type == GL_UNSIGNED_INT ? 4 : 2);

	if (instance_count)
	{
		if (base_vertex)
			ctx->api.glDrawElementsInstancedBaseVertex(prim, index_count, type, (void*)offset, instance_count, base_vertex);
		else
			ctx->api.glDrawElementsInstanced(prim, index_count, type, (void*)offset, instance_count);
	}
	else
	{
		if (base_vertex)
			ctx->api.glDrawElementsBaseVertex(prim, index_count, type, (void*)offset, base_vertex);
		else
			ctx->api.glDrawElements(prim, index_count, type, (void*)offset);
	}
}

// =============================================================================
API void
R3_SetComputePipeline(R3_Context* ctx, R3_ComputePipeline* pipeline)
{
	Trace();
	ctx->api.glUseProgram(pipeline->gl_program);
	ctx->curr_program = pipeline->gl_program;
	for (intz i = 0; i < ArrayLength(ctx->ubo_indices); ++i)
	{
		char name[64] = {};
		String prefix = StrInit("type_UniformBuffer");
		SafeAssert(prefix.size+3 < sizeof(name));
		MemoryCopy(name, prefix.data, prefix.size);
		if (i == 0)
			name[prefix.size] = '0';
		else if (i < 10)
			name[prefix.size] = '0' + i;
		else if (i < 100)
		{
			name[prefix.size+0] = '0' + i/10;
			name[prefix.size+1] = '0' + i%10;
		}
		else
			SafeAssert(!"are you really binding 100 uniform buffers?");

		int32 block_id = ctx->api.glGetUniformBlockIndex(ctx->curr_program, name);
		ctx->ubo_indices[i] = block_id;
	}
	OglRebindTextures_(ctx);
}

API void
R3_SetComputeUniformBuffers(R3_Context* ctx, intsize count, R3_UniformBuffer buffers[])
{
	Trace();
	R3_SetUniformBuffers(ctx, count, buffers);
}

API void
R3_SetComputeResourceViews(R3_Context* ctx, intsize count, R3_ResourceView views[])
{
	Trace();
	R3_SetResourceViews(ctx, count, views);
}

API void
R3_SetComputeUnorderedViews(R3_Context* ctx, intsize count, R3_UnorderedView views[])
{
	Trace();
	intz max_view_count = 16;
	SafeAssert(count < max_view_count);
	for (intz i = 0; i < count; ++i)
	{
		if (views[i].buffer)
			ctx->api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i+max_view_count, views[i].buffer->gl_id);
		else
			ctx->api.glBindImageTexture(i+max_view_count, views[i].texture->gl_id, 0, GL_FALSE, 0, GL_READ_WRITE, OglFormatToGLEnum_(views[i].texture->format, NULL, NULL));
	}
}

API void
R3_Dispatch(R3_Context* ctx, uint32 x, uint32 y, uint32 z)
{
	Trace();
	ctx->api.glDispatchCompute(x, y, z);
	ctx->api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
