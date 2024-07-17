
#include "third_party/opengl46.h"

#include "common.h"
#include "api_render3.h"

struct R3_Context
{
	R4_OpenGLPresentProc* present_proc;

	#define X(Type, name) Type name;
	OPENGL_HEADER_GL10(X)
	OPENGL_HEADER_GL11(X)
	OPENGL_HEADER_GL12(X)
	OPENGL_HEADER_GL13(X)
	OPENGL_HEADER_GL14(X)
	OPENGL_HEADER_GL15(X)
	OPENGL_HEADER_GL20(X)
	OPENGL_HEADER_GL21(X)
	OPENGL_HEADER_GL30(X)
	OPENGL_HEADER_GL31(X)
	OPENGL_HEADER_GL32(X)
	OPENGL_HEADER_GL33(X)
	OPENGL_HEADER_GL40(X)
	OPENGL_HEADER_GL41(X)
	OPENGL_HEADER_GL42(X)
	OPENGL_HEADER_GL43(X)
	OPENGL_HEADER_GL44(X)
	OPENGL_HEADER_GL45(X)
	OPENGL_HEADER_GL46(X)
	#undef X
};

API R3_Context*
R4_MGL_MakeContext(Arena* arena, R4_OpenGLLoaderProc* loader_proc, R4_OpenGLPresentProc* present_proc)
{
	PFNGLGETSTRINGPROC get_string = loader_proc("glGetString");
	if (!get_string || !get_string(GL_VERSION))
		return NULL;
	PFNGLGETINTEGERVPROC get_integer_v = loader_proc("glGetIntegerv");
	if (!get_integer_v)
		return NULL;
	int32 major = 0;
	int32 minor = 0;
	get_integer_v(GL_MAJOR_VERSION, &major);
	get_integer_v(GL_MINOR_VERSION, &minor);
	int32 version = major*10 + minor;
	if (!version || version < 43)
		return NULL;

	R3_Context* ctx = ArenaPushStruct(arena, R3_Context);

	#define X(Type, name) ctx->name = (Type)loader_proc(#name);
	OPENGL_HEADER_GL10(X);
	OPENGL_HEADER_GL11(X);
	OPENGL_HEADER_GL12(X);
	OPENGL_HEADER_GL13(X);
	OPENGL_HEADER_GL14(X);
	OPENGL_HEADER_GL15(X);
	OPENGL_HEADER_GL20(X);
	OPENGL_HEADER_GL21(X);
	OPENGL_HEADER_GL30(X);
	OPENGL_HEADER_GL31(X);
	OPENGL_HEADER_GL32(X);
	OPENGL_HEADER_GL33(X);
	OPENGL_HEADER_GL40(X);
	OPENGL_HEADER_GL41(X);
	OPENGL_HEADER_GL42(X);
	OPENGL_HEADER_GL43(X);
	if (version >= 44) { OPENGL_HEADER_GL44(X); }
	if (version >= 45) { OPENGL_HEADER_GL45(X); }
	if (version >= 46) { OPENGL_HEADER_GL46(X); }
	#undef X

	return ctx;
}

API R3_ContextInfo
R4_QueryInfo(R3_Context* ctx)
{
	Trace();
	return (R3_ContextInfo) {};
}
