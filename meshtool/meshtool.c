#include "common.h"
#include "api_os.h"
#include "api_modelspec.h"
#include "third_party/meshoptimizer/meshoptimizer.h"
#include "third_party/stb_image.h"

#include <zstd.h>
#include <lz4.h>
#include <stdlib.h>

#include "meshtool_json.c"
#include "meshtool_gltf.c"

struct
{
	bool f_meshlet;
	bool f_lz4;
	bool f_zstd;
	bool f_copy_textures;
	bool f_buffer_compression;
	bool f_texture_compression;
	bool f_choose_texture_compression;
	
	int32 zstd_level;
}
static g_meshtool = {
	.f_zstd = true,
	.f_buffer_compression = true,
	.zstd_level = 15,
};

struct BasicMesh_
{
	uint32 vbuffer_size;
	uint32 ibuffer_size;
	int32 texid_base_color;
	int32 texid_normal;
	void* vbuffer;
	void* ibuffer;
}
typedef BasicMesh_;

struct Texture_
{
	uint32 width, height;
	MDL_TextureFormat format;
	MDL_TextureEncoding encoding;
	uintsize data_size;
	void const* data;
}
typedef Texture_;

API int32
EntryPoint(int32 argc, char const* const* argv)
{
	Trace();
	OS_Init(0);
	OS_Error err = { .ok = true };
	Arena arena = OS_VirtualAllocArena(8ull << 30);
	Arena scratch_arena = OS_VirtualAllocArena(8ull << 30);
	Arena scratch_arena2 = OS_VirtualAllocArena(8ull << 30);
	
	if (argc < 3)
	{
		OS_LogErr(
			"usage: %s INPUTFILE OUTPUTFILE [OPTIONS...]\n"
			"options:\n"
			"\t-meshlet                     Generate meshlets information\n"
			"\t-lz4                         Use LZ4 for compression\n"
			"\t-zstd                        (default)\n"
			"\t-zstd=N                      (default=15) Use Zstd for compression\n"
			"\t-copy-textures               Always copy texture data from source instead of re-encoding\n"
			"\t-no-buffer-comp              Dont allow compressed buffer data\n"
			"\t-no-texture-comp             Dont allow compressed texture data\n"
			"\t-buffer-comp                 (default) Always compress buffer data\n"
			"\t-texture-comp                Always re-encode texture data\n"
			"\t-gpu-ready                   Dont compress anything\n"
			"\t-choose-texture-comp         (not supported yet) Re-encode texture if X compression is better\n"
			"",
			argv[0]);
		return 1;
	}
	
	String input_file_path = StringFromCString(argv[1]);
	String output_file_path = StringFromCString(argv[2]);
	if (StringEquals(input_file_path, output_file_path))
	{
		OS_LogErr("you surely dont want input file to be the same as the output file...\n");
		return 1;
	}
	
	// {
	// 	void* buffer;
	// 	uintsize size;
	// 	SafeAssert(OS_ReadEntireFile(input_file_path, &arena, &buffer, &size, NULL));
	// 	int32 width, height, channels;
	// 	uint8* pixels = stbi_load_from_memory(buffer, size, &width, &height, &channels, 0);
	// 	SafeAssert(pixels);
	// 	uintsize imgsize = width*height*channels;
	// 	OS_WriteEntireFile(output_file_path, pixels, imgsize, NULL);

	// 	uintsize zstd_size = ZSTD_compressBound(imgsize);
	// 	void* zstd = ArenaPushDirty(&arena, zstd_size);
	// 	zstd_size = ZSTD_compress(zstd, zstd_size, pixels, imgsize, 19);
	// 	OS_WriteEntireFile(StringPrintfLocal(256, "%S.zstd", output_file_path), zstd, zstd_size, NULL);

	// 	uint8* r = ArenaPushDirty(&arena, width*height*4);
	// 	uint8* g = r + width*height*1;
	// 	uint8* b = r + width*height*2;
	// 	uint8* a = r + width*height*3;

	// 	for (intsize y = 0; y < height; ++y)
	// 	{
	// 		for (intsize x = 0; x < width; ++x)
	// 		{
	// 			r[x + y*width] = pixels[0 + 4*(x + y*width)];
	// 			g[x + y*width] = pixels[1 + 4*(x + y*width)];
	// 			b[x + y*width] = pixels[2 + 4*(x + y*width)];
	// 			a[x + y*width] = pixels[3 + 4*(x + y*width)];
	// 		}
	// 	}

	// 	zstd_size = ZSTD_compressBound(width*height*4);
	// 	zstd = ArenaPushDirty(&arena, zstd_size);
	// 	zstd_size = ZSTD_compress(zstd, zstd_size, r, width*height*4, 19);
	// 	OS_WriteEntireFile(StringPrintfLocal(256, "%S.swizzled.zstd", output_file_path), zstd, zstd_size, NULL);
	// 	return 0;
	// }

	//------------------------------------------------------------------------
	// flags
	for (int32 i = 3; i < argc; ++i)
	{
		String arg = StringFromCString(argv[i]);
		
		if (StringEquals(arg, Str("-meshlet")))
			g_meshtool.f_meshlet = true;
		else if (StringEquals(arg, Str("-zstd")))
			g_meshtool.f_zstd = true;
		else if (StringEquals(StringSubstr(arg, 0, 6), Str("-zstd=")))
		{
			String num = StringSubstr(arg, 6, -1);
			if (num.size == 0)
			{
				OS_LogErr("expected number argument for flag '-zstd='\n");
				return 1;
			}
			
			int64 parsed = 0;
			while (num.size > 0)
			{
				uint8 ch = num.data[0];
				if (ch < '0' || ch > '9')
				{
					OS_LogErr("invalid number in argument '%s'\n", argv[i]);
					return 1;
				}
				
				parsed *= 10;
				parsed += ch - '0';
			}
			
			if (parsed <= 0 || parsed > INT32_MAX)
			{
				OS_LogErr("number is outside of safe range 1..<INT32_MAX in argument '%s'\n", argv[i]);
				return 1;
			}
			
			g_meshtool.zstd_level = (int32)parsed;
		}
		else if (StringEquals(arg, Str("-copy-textures")))
			g_meshtool.f_copy_textures = true;
		else if (StringEquals(arg, Str("-no-buffer-comp")))
			g_meshtool.f_buffer_compression = false;
		else if (StringEquals(arg, Str("-no-texture-comp")))
			g_meshtool.f_texture_compression = false;
		else if (StringEquals(arg, Str("-buffer-comp")))
			g_meshtool.f_buffer_compression = true;
		else if (StringEquals(arg, Str("-texture-comp")))
			g_meshtool.f_texture_compression = true;
		else if (StringEquals(arg, Str("-gpu-ready")))
		{
			g_meshtool.f_buffer_compression = false;
			g_meshtool.f_texture_compression = false;
		}
		else if (StringEquals(arg, Str("-choose-texture-comp")))
			g_meshtool.f_choose_texture_compression = true;
	}
	
	//------------------------------------------------------------------------
	//- Open input and output files
	OS_File input_file = OS_OpenFile(input_file_path, OS_OpenFileFlags_Read | OS_OpenFileFlags_FailIfDoesntExist, &err);
	if (!err.ok)
	{
		OS_LogErr("error when opening input file: (code 0x%x) %S\n", err.code, err.what);
		return 1;
	}
	
	uint8 const* input_buffer;
	uintsize input_size;
	OS_FileMapping input_file_mapping = OS_MapFileForReading(input_file, (void const**)&input_buffer, &input_size, &err);
	if (!err.ok)
	{
		OS_LogErr("error when mapping input file: (code 0x%x) %S\n", err.code, err.what);
		OS_CloseFile(input_file);
		return 1;
	}
	
	OS_File output_file = OS_OpenFile(output_file_path, OS_OpenFileFlags_Write, &err);
	if (!err.ok)
	{
		OS_LogErr("error when opening output file: (code 0x%x) %S\n", err.code, err.what);
		OS_UnmapFile(input_file_mapping);
		OS_CloseFile(input_file);
		return 1;
	}
	
	//------------------------------------------------------------------------
	// Determine type of input file
	enum { NONE, GLTF, MODELSPEC, FBX } kind = 0;
	
	if (!kind)
	{
		// check for GLB
		if (input_size > 12)
		{
			uint32 magic = *(uint32*)(input_buffer+0);
			uint32 version = *(uint32*)(input_buffer+4);
			uint32 length = *(uint32*)(input_buffer+8);
			
			if (magic == 0x46546C67 && version == 2 && length == input_size)
				kind = GLTF;
		}
	}
	
	if (!kind)
	{
		// check for modelspec
		if (input_size > sizeof(MDL_Header))
		{
			MDL_Header const* header = (void*)input_buffer;
			if (MemoryCompare(header->magic, "MODEL00\n", 8) == 0)
				kind = MODELSPEC;
		}
	}
	
	BasicMesh_ o_basic_meshes[64] = { 0 };
	Texture_ o_textures[64] = { 0 };
	intsize o_basic_meshes_count = 0;
	intsize o_textures_count = 0;
	uint32 vertex_size = 0;
	int32 result = 1;
	switch (kind)
	{
		case NONE:
		{
			OS_LogErr("could not identify the type of the input file\n");
		} break;
		case GLTF:
		{
			//------------------------------------------------------------------------
			// Parse headers
			if (input_size < 20)
			{
				OS_LogErr("input GLB does not have a json chunk\n");
				break;
			}
			
			uint32 json_chunk_length = *(uint32*)(input_buffer+12);
			uint32 json_chunk_type = *(uint32*)(input_buffer+16);
			uint8 const* json_chunk_data = input_buffer+20;
			if (json_chunk_type != 0x4E4F534A)
			{
				OS_LogErr("first chunk of GLB should be json\n");
				break;
			}
			if (input_size < 28 + json_chunk_length)
			{
				OS_LogErr("input GLB does not have a binary chunk\n");
				break;
			}
			
			uint32 bin_chunk_length = *(uint32*)(input_buffer+20+json_chunk_length+0);
			uint32 bin_chunk_type = *(uint32*)(input_buffer+20+json_chunk_length+4);
			uint8 const* bin_chunk_data = input_buffer+20+json_chunk_length+8;
			if (bin_chunk_data + bin_chunk_length != input_buffer+input_size)
			{
				OS_LogErr("input GLB has some sizes mismatch\n");
				break;
			}
			if (bin_chunk_type != 0x004E4942)
			{
				OS_LogErr("second chunk of GLB should be binary\n");
				break;
			}
			
			GltfRoot_* root;
			bool ok = ParseGltfJson_(json_chunk_data, json_chunk_length, &scratch_arena, &root);
			if (!ok)
			{
				ill_json:;
				OS_LogErr("input GLB has ill formed json\n");
				break;
			}
			
			//------------------------------------------------------------------------
			// Parse json structure
			int32 scene_index = root->scene;
			if (scene_index < 0 || scene_index >= root->scenes_count)
				goto ill_json;
			GltfScene_* scene = &root->scenes[scene_index];
			if (scene->nodes_count != 1 || scene->nodes[0] < 0 || scene->nodes[0] >= root->nodes_count)
				goto ill_json;
			GltfNode_* node = &root->nodes[scene->nodes[0]];
			if (node->mesh < 0 || node->mesh >= root->meshes_count)
				goto ill_json;
			GltfMesh_* mesh = &root->meshes[node->mesh];
			if (mesh->primitives_count != 1)
				goto ill_json;
			GltfMeshPrimitive_* primitive = &mesh->primitives[0];

			//------------------------------------------------------------------------
			// Vertex data
			GltfMaterial_* material = &root->materials[primitive->material]; //UNCHECKED
			GltfAccessor_* index_accessor = &root->accessors[primitive->indices]; //UNCHECKED
			GltfBufferView_* index_view = &root->buffer_views[index_accessor->buffer_view]; //UNCHECKED
			GltfAccessor_* accessors[16] = { 0 };
			GltfBufferView_* views[16] = { 0 };
			intsize accessors_count = 0;
			for (intsize i = 0; i < ArrayLength(primitive->attributes); ++i)
			{
				if (!primitive->attributes[i].name.size)
					break;
				if (primitive->attributes[i].index < 0 || primitive->attributes[i].index >= root->accessors_count)
					goto ill_json;

				accessors[i] = &root->accessors[primitive->attributes[i].index];
				views[i] = &root->buffer_views[accessors[i]->buffer_view]; //UNCHECKED
				accessors_count = i+1;
			}
			if (accessors_count < 1)
				goto ill_json;
			
			uintsize component_size[16] = { 0 };
			uintsize total_size = 0;
			for (intsize i = 0; i < accessors_count; ++i)
			{
				total_size += views[i]->byte_length;
				uintsize n = 1;
				uintsize size = 4;
				
				if (StringEquals(accessors[i]->type, Str("SCALAR")))
					n = 1;
				else if (StringEquals(accessors[i]->type, Str("VEC2")))
					n = 2;
				else if (StringEquals(accessors[i]->type, Str("VEC3")))
					n = 3;
				else if (StringEquals(accessors[i]->type, Str("VEC4")))
					n = 4;

				switch (accessors[i]->component_type)
				{
					case 5120: case 5121: size = 1; break;
					case 5122: case 5123: size = 2; break;
					case 5125: case 5126: size = 4; break;
				}
				
				if (StringEquals(primitive->attributes[i].name, Str("TEXCOORD_0")))
					size = 2;

				component_size[i] = n * size;
				vertex_size += n * size;
			}

			// Interleave vertex data, because gltf doesnt do it for some reason
			uint8* new_vbuffer = ArenaPush(&arena, total_size);
			uint32 vertex_count = accessors[0]->count;
			uint32 curr_stride = 0;
			for (intsize i = 0; i < accessors_count; ++i)
			{
				if (StringEquals(primitive->attributes[i].name, Str("TEXCOORD_0")))
				{
					for (intsize j = 0; j < vertex_count; ++j)
					{
						uint8* dst = new_vbuffer + j*vertex_size + curr_stride;
						uint8 const* src = bin_chunk_data + views[i]->byte_offset + j*8;
						
						((int16*)dst)[0] = (int16)(((float32 const*)src)[0] * INT16_MAX);
						((int16*)dst)[1] = (int16)(((float32 const*)src)[1] * INT16_MAX);
					}
				}
				else
				{
					for (intsize j = 0; j < vertex_count; ++j)
					{
						uint8* dst = new_vbuffer + j*vertex_size + curr_stride;
						uint8 const* src = bin_chunk_data + views[i]->byte_offset + j*component_size[i];
						MemoryCopy(dst, src, component_size[i]);
					}
				}
				curr_stride += component_size[i];
			}

			uint32* ibuffer;
			uint32 ibuffer_size;
			if (index_accessor->component_type == 5125/*GL_UNSIGNED_INT*/)
			{
				ibuffer = (uint32*)(bin_chunk_data + index_view->byte_offset);
				ibuffer_size = index_view->byte_length;
			}
			else
			{
				ibuffer_size = index_view->byte_length * 2;
				ibuffer = ArenaPushArray(&arena, uint32, ibuffer_size/4);
				for (intsize i = 0; i < ibuffer_size/4; ++i)
					ibuffer[i] = *(uint16*)(bin_chunk_data + index_view->byte_offset + i*2);
			}

			o_basic_meshes[o_basic_meshes_count++] = (BasicMesh_) {
				.vbuffer_size = vertex_count * vertex_size,
				.vbuffer = new_vbuffer,
				.ibuffer = ibuffer,
				.ibuffer_size = ibuffer_size,
				.texid_base_color = material->pbr_metallic_roughness.base_color_texture.index,
				.texid_normal = material->normal_texture.index,
			};

			//------------------------------------------------------------------------
			// Texture data
			for (intsize i = 0; i < root->textures_count; ++i)
			{
				GltfTexture_* texture = &root->textures[i];
				GltfImage_* image = &root->images[texture->source]; //UNCHECKED
				GltfBufferView_* image_view = &root->buffer_views[image->buffer_view]; //UNCHECKED

				void const* image_data = bin_chunk_data + image_view->byte_offset;
				uintsize image_data_size = image_view->byte_length;
				MDL_TextureEncoding encoding = -1;
				MDL_TextureFormat format = MDL_TextureFormat_RgbaU8Norm;

				if (StringEquals(image->mime_type, Str("image/png")))
					encoding = MDL_TextureEncoding_Png;
				else if (StringEquals(image->mime_type, Str("image/jpeg")))
					encoding = MDL_TextureEncoding_Jpeg;

				SafeAssert(encoding != -1);
				SafeAssert(image_data_size <= INT32_MAX);
				int32 width, height, comp;
				int32 r = stbi_info_from_memory(image_data, (int32)image_data_size, &width, &height, &comp);
				SafeAssert(r == 1);

				o_textures[o_textures_count++] = (Texture_) {
					.width = (uint32)width,
					.height = (uint32)height,
					.format = format,
					.encoding = encoding,
					.data_size = image_data_size,
					.data = image_data,
				};
			}

			//------------------------------------------------------------------------
			// ok
			result = 0;
		} break;
		case MODELSPEC:
		{
			OS_LogErr("cannot use compiled model as input\n");
		} break;
	}
	
	if (result == 0)
	{
		MDL_Header* out_header = ArenaPushStruct(&arena, MDL_Header);
		MemoryCopy(out_header->magic, "MODEL00\n", 8);
		String description = StringPrintfLocal(128, "input file '%S'", input_file_path);
		MemoryCopy(out_header->description, description.data, description.size);
		out_header->block_count = o_basic_meshes_count + o_textures_count;
		if (g_meshtool.f_meshlet)
			out_header->flags |= MDL_HeaderFlags_MeshletPipeline;
		
		out_header->flags |= MDL_HeaderFlags_AttribPosition;
		out_header->flags |= MDL_HeaderFlags_AttribNormal;
		out_header->flags |= MDL_HeaderFlags_AttribTangent;
		out_header->flags |= MDL_HeaderFlags_AttribTexcoords;
		out_header->vertex_size = vertex_size;

		for (intsize i = 0; i < o_basic_meshes_count; ++i)
		{
			for ArenaTempScope(&scratch_arena2)
			{
				BasicMesh_* mesh = &o_basic_meshes[i];
				uint32* index_buffer = mesh->ibuffer;
				uintsize index_count = mesh->ibuffer_size/4;
				uint8* vertex_buffer = mesh->vbuffer;
				uintsize vertex_count = mesh->vbuffer_size / vertex_size;

				for ArenaTempScope(&scratch_arena)
				{
					uint32* remap = ArenaPushArray(&scratch_arena, uint32, index_count);
					uintsize new_vertex_count = meshopt_generateVertexRemap(remap, index_buffer, index_count, vertex_buffer, vertex_count, vertex_size);
		
					uint32* new_index_buffer = ArenaPushArray(&scratch_arena, uint32, index_count);
					uint8* new_vertex_buffer = ArenaPushArray(&scratch_arena, uint8, new_vertex_count * vertex_size);
					meshopt_remapIndexBuffer(new_index_buffer, index_buffer, index_count, remap);
					meshopt_remapVertexBuffer(new_vertex_buffer, vertex_buffer, vertex_count, vertex_size, remap);
				
					index_buffer = ArenaPushArrayData(&scratch_arena2, uint32, new_index_buffer, index_count);
					vertex_buffer = ArenaPushArrayData(&scratch_arena2, uint8, new_vertex_buffer, new_vertex_count * vertex_size);
					vertex_count = new_vertex_count;
				}
				
				meshopt_optimizeVertexCache(index_buffer, index_buffer, index_count, vertex_count);
				meshopt_optimizeOverdraw(index_buffer, index_buffer, index_count, (float32*)vertex_buffer, vertex_count, vertex_size, 1.05f);
				meshopt_optimizeVertexFetch(vertex_buffer, index_buffer, index_count, vertex_buffer, vertex_count, vertex_size);

				MDL_BasicMeshBlock* out_block = ArenaPushStruct(&arena, MDL_BasicMeshBlock);
				MemoryCopy(out_block->magic, "\nBasicMesh\0\0", 12);
				out_block->block_size = AlignUp(sizeof(MDL_BasicMeshBlock) + vertex_size * vertex_count + index_count * 4, 7);
				out_block->vbuffer_size = vertex_size * vertex_count;
				out_block->vbuffer_offset = 0;
				out_block->ibuffer_size = index_count * 4;
				out_block->ibuffer_offset = out_block->vbuffer_size;
				out_block->texid_base_color = mesh->texid_base_color + 1;
				out_block->texid_normal = mesh->texid_normal + 1;
				ArenaPushMemory(&arena, vertex_buffer, vertex_size * vertex_count);
				ArenaPushMemory(&arena, index_buffer, index_count * 4);
				ArenaEndAligned(&arena, 8);
			}
		}

		for (intsize i = 0; i < o_textures_count; ++i)
		{
			for ArenaTempScope(&scratch_arena2)
			{
				Texture_* texture = &o_textures[i];
				uintsize data_size = texture->data_size;
				uint8 const* data = texture->data;
				MDL_TextureEncoding encoding = texture->encoding;

				if (!g_meshtool.f_copy_textures)
				{
					if (g_meshtool.f_texture_compression && encoding == MDL_TextureEncoding_Uncompressed)
					{
						uintsize max_size = ZSTD_compressBound(data_size);
						uint8* new_data = ArenaPush(&scratch_arena2, max_size);
						uintsize new_size = ZSTD_compress(new_data, max_size, data, data_size, g_meshtool.zstd_level);
						SafeAssert(new_size);

						data = new_data;
						data_size = new_size;
						encoding = MDL_TextureEncoding_Zstd;
					}
				}

				MDL_TextureBlock* out_block = ArenaPushStruct(&arena, MDL_TextureBlock);
				MemoryCopy(out_block->magic, "\nTexture\0\0\0\0", 12);
				out_block->block_size = AlignUp(sizeof(MDL_TextureBlock) + data_size, 7);
				out_block->size = data_size;
				out_block->width = texture->width;
				out_block->height = texture->height;
				out_block->format = texture->format;
				out_block->encoding = encoding;
				ArenaPushMemory(&arena, data, data_size);
				ArenaEndAligned(&arena, 8);
			}
		}

		uint8* output_buffer = (uint8*)out_header;
		uintsize output_size = (uint8*)ArenaEnd(&arena) - output_buffer;
		OS_WriteFile(output_file, (intsize)output_size, output_buffer, &err);
		if (!err.ok)
		{
			OS_LogErr("error when writing to output file: (code 0x%x) %S\n", err.code, err.what);
			result = 1;
		}
	}

	OS_CloseFile(output_file);
	OS_UnmapFile(input_file_mapping);
	OS_CloseFile(input_file);
	return result;
}

#if 0
struct OutputMesh_
{
	uint32 prim_buffer_offset;
	uint32 prim_buffer_size;
	uint32 vertex_buffer_offset;
	uint32 vertex_buffer_size;
	uint32 meshlet_count;
	uint8 data[];
}
typedef OutputMesh_;

struct OutputMeshlet_
{
	uint32 vertex_offset;
	uint32 prim_offset;
	uint32 vertex_count;
	uint32 prim_count;
}
typedef OutputMeshlet_;

{
	uint32* index_buffer;
	uintsize index_count;
	uint8* vertex_buffer;
	uintsize vertex_count;
	uintsize vertex_size;
	
	{
		// TODO: read model
	}
	
	for ArenaTempScope(&scratch_arena)
	{
		uint32* remap = ArenaPushArray(&scratch_arena, uint32, index_count);
		uintsize new_vertex_count = meshopt_generateVertexRemap(remap, index_buffer, index_count, vertex_buffer, vertex_count, vertex_size);
		
		uint32* new_index_buffer = ArenaPushArray(&scratch_arena, uint32, index_count);
		uint8* new_vertex_buffer = ArenaPushArray(&scratch_arena, uint8, new_vertex_count*vertex_size);
		meshopt_remapIndexBuffer(new_index_buffer, NULL, index_count, remap);
		meshopt_remapVertexBuffer(new_vertex_buffer, vertex_buffer, vertex_count, vertex_size, remap);
		ArenaClear(&arena1);
		
		index_buffer = ArenaPushArrayData(&arena, uint32, new_index_buffer, index_count);
		vertex_buffer = ArenaPushArrayData(&arena, uint8, new_vertex_buffer, new_vertex_count*vertex_size);
		vertex_count = new_vertex_count;
	}
	
	meshopt_optimizeVertexCache(index_buffer, index_buffer, index_count, vertex_count);
	meshopt_optimizeOverdraw(index_buffer, index_buffer, index_count, (float32*)vertex_buffer, vertex_count, vertex_size, 1.05f);
	meshopt_optimizeVertexFetch(vertex_buffer, index_buffer, index_count, vertex_buffer, vertex_count, vertex_size);
	
	float32 const cone_weight = 0.0f;
	uintsize const max_vertices = 64;
	uintsize const max_triangles = 124;
	uintsize max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, max_triangles);
	
	meshopt_Meshlet* meshlets = ArenaPushArray(&arena, meshopt_Meshlet, max_meshlets);
	uint32* meshlet_vertices = ArenaPushArray(&arena, uint32, max_meshlets * max_vertices);
	uint8* meshlet_triangles = ArenaPushArray(&arena, uint8, max_meshlets * max_triangles * 3);
	uintsize meshlet_count = meshopt_buildMeshlets(meshlets, meshlet_vertices, meshlet_triangles, index_buffer, index_count, (float32*)vertex_buffer, vertex_count, vertex_size, max_vertices, max_triangles, cone_weight);
	
	meshopt_Meshlet* last_meshlet = &meshlets[meshlet_count - 1];
	uintsize meshlet_vertex_count = last_meshlet->vertex_offset + last_meshlet->vertex_count;
	uintsize meshlet_triangle_count = last_meshlet->triangle_offset + last_meshlet->triangle_count;
	
	for ArenaTempScope(&scratch_arena)
	{
		OutputMesh_* output_mesh = ArenaPushStruct(&scratch_arena, OutputMesh_);
		output_mesh->prim_buffer_offset;
		output_mesh->prim_buffer_size;
		output_mesh->vertex_buffer_offset;
		output_mesh->vertex_buffer_size;
		
		OutputMeshlet_* output_meshlets = ArenaPushArray(&scratch_arena, OutputMeshlet_, meshlet_count);
		for (uintsize i = 0; i < meshlet_count; ++i)
		{
			output_meshlets[i].vertex_offset = meshlets[i].vertex_offset;
			output_meshlets[i].prim_offset = meshlets[i].triangle_offset;
			output_meshlets[i].vertex_count = meshlets[i].vertex_count;
			output_meshlets[i].prim_count = meshlets[i].triangle_count;
		}
		
		uint8* vertex_
	}
}
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
