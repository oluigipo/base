#include "common.h"
#include "api_os.h"

#include <string>

#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

static void
PrintHelp_(char const* self)
{
	OS_LogOut("usage: %s path-to-input.hlsl path-to-output.glsl [vs|ps|cs] EntrypointName\n", self);
}

API int32
EntryPoint(int32 argc, char const* const* argv)
{
	OS_Init(0);
	OS_Error err = {};
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);

	if (argc != 5)
	{
		PrintHelp_(argv[0]);
		return 1;
	}

	String input_path = StringFromCString(argv[1]);
	String output_path = StringFromCString(argv[2]);
	String shader_profile = StringFromCString(argv[3]);
	String entrypoint_name = StringFromCString(argv[4]);

	if (!OS_GetFileInfoFromPath(input_path, &err).exists)
	{
		OS_LogOut("could not open input file (0x%x): %S\n", err.code, err.what);
		return 1;
	}

	if (!StringEquals(shader_profile, Str("vs")) &&
		!StringEquals(shader_profile, Str("ps")) &&
		!StringEquals(shader_profile, Str("cs")))
	{
		OS_LogOut("shader profile should be either 'vs', 'ps', or 'cs'\n");
		return 1;
	}

	void* spirv_data;
	uintsize spirv_size;
	for ArenaTempScope(scratch_arena)
	{
		String cmd = ArenaPrintf(
			scratch_arena,
			"dxc /nologo /O3 \"%S\" -spirv /Fo\"%S.spirv\" /T%S_6_0 /E%S",
			input_path, output_path, shader_profile, entrypoint_name);
		int32 result = OS_Exec(cmd, &err);
		if (!err.ok)
		{
			OS_LogOut("failed to run dxc (0x%x): %S\n", err.code, err.what);
			return 1;
		}
		if (result != 0)
			return 0;
		if (!OS_ReadEntireFile(ArenaPrintf(scratch_arena, "%S.spirv", output_path), scratch_arena, &spirv_data, &spirv_size, &err))
		{
			OS_LogOut("failed to read intermediate file (0x%x): %S\n", err.code, err.what);
			return 1;
		}
	}

	spirv_cross::CompilerGLSL glsl_compiler((uint32*)spirv_data, spirv_size / 4);

	spirv_cross::CompilerGLSL::Options options = glsl_compiler.get_common_options();
	options.version = StringEquals(shader_profile, Str("cs")) ? 310 : 300;
	options.es = true;
	options.enable_420pack_extension = false;
	options.relax_nan_checks = true;
	options.enable_row_major_load_workaround = false;
	glsl_compiler.set_common_options(options);
	glsl_compiler.build_dummy_sampler_for_combined_images();
	glsl_compiler.build_combined_image_samplers();

	if (StringEquals(shader_profile, Str("ps")))
	{
		spirv_cross::ShaderResources resources = glsl_compiler.get_shader_resources();
		for (spirv_cross::Resource& resource : resources.stage_inputs)
		{
			std::string name = resource.name;
			String str = StringFromCString(name.c_str());

			if (StringStartsWith(str, Str("in.var.")))
			{
				name.replace(0, 2, "out");
				glsl_compiler.set_name(resource.id, name);
			}
		}

		int32 sampler_count = 0;
		for (auto& sampler : glsl_compiler.get_combined_image_samplers())
			glsl_compiler.set_name(sampler.combined_id, "uTexture" + std::to_string(sampler_count++));
	}

	std::string final_glsl = glsl_compiler.compile();
	if (!OS_WriteEntireFile(output_path, final_glsl.c_str(), final_glsl.size(), &err))
	{
		OS_LogOut("failed to write output file (0x%x): %S\n", err.code, err.what);
		return 1;
	}

	return 0;
}
