#ifndef API_MODELSPEC_H
#define API_MODELSPEC_H

#include "common.h"

enum MDL_HeaderFlags
{
	MDL_HeaderFlags_AttribPosition = 0x0001,  // float32[3]
	MDL_HeaderFlags_AttribNormal = 0x0002,    // float32[3]
	MDL_HeaderFlags_AttribTangent = 0x0004,   // float32[4]
	MDL_HeaderFlags_AttribTexcoords = 0x0008, // int16[2]

	MDL_HeaderFlags_MeshletPipeline = 0x0100,
	MDL_HeaderFlags_16BitIndices = 0x0200,
}
typedef MDL_HeaderFlags;

struct MDL_Header
{
	uint8 magic[8]; // "MODEL00\n"
	uint8 description[128];
	uint32 flags;

	// NOTE(ljre): This must be the sum of all specified attributes in '.flags'!
	uint32 vertex_size;
	uint32 _pad;
	
	uint32 block_count;
	alignas(8) uint8 blocks[];
}
typedef MDL_Header;

struct MDL_AnyBlock
{
	uint8 magic[12];
	uint32 block_size;
}
typedef MDL_AnyBlock;

//------------------------------------------------------------------------
enum MDL_BasicMeshFilter
{
	MDL_BasicMeshFilter_Nearest,
	MDL_BasicMeshFilter_Linear,
	MDL_BasicMeshFilter_NearestMipmapNearest,
	MDL_BasicMeshFilter_NearestMipmapLinear,
	MDL_BasicMeshFilter_LinearMipmapNearest,
	MDL_BasicMeshFilter_LinearMipmapLinear,
}
typedef MDL_BasicMeshFilter;

struct MDL_BasicMeshBlock
{
	uint8 magic[12]; // "\nBasicMesh"
	uint32 block_size;
	
	uint32 vbuffer_size;
	uint32 vbuffer_offset;
	uint32 ibuffer_size;
	uint32 ibuffer_offset;
	uint16 texid_base_color;
	uint16 texid_normal;
	
	alignas(8) uint8 data[];
}
typedef MDL_BasicMeshBlock;

//------------------------------------------------------------------------
enum MDL_TextureFormat
{
	MDL_TextureFormat_RgbaU8Norm,
	MDL_TextureFormat_RgU8Norm,
	MDL_TextureFormat_RU8Norm,
	MDL_TextureFormat_RgbaF16,
}
typedef MDL_TextureFormat;

enum MDL_TextureEncoding
{
	MDL_TextureEncoding_Uncompressed,
	MDL_TextureEncoding_Png,
	MDL_TextureEncoding_Jpeg,
	MDL_TextureEncoding_Zstd,
}
typedef MDL_TextureEncoding;

struct MDL_TextureBlock
{
	uint8 magic[12]; // "\nTexture"
	uint32 block_size;
	
	uint32 size;
	uint32 width;
	uint32 height;
	MDL_TextureFormat format;
	MDL_TextureEncoding encoding;
	
	alignas(8) uint8 data[];
}
typedef MDL_TextureBlock;

//------------------------------------------------------------------------
struct MDL_SingleMeshlet
{
	uint32 vertex_offset;
	uint32 vertex_count;
	uint32 prim_offset;
	uint32 prim_count;
}
typedef MDL_SingleMeshlet;

struct MDL_MeshletBlock
{
	uint8 magic[12]; // "\nMeshlet\0\0\0\0"
	uint32 block_size;
	
	uint32 prim_buffer_offset;
	uint32 prim_buffer_size;
	uint32 meshlet_buffer_offset;
	uint32 meshlet_buffer_size;

	alignas(8) uint8 data[];
}
typedef MDL_MeshletBlock;

#endif //API_MODELSPEC_H
