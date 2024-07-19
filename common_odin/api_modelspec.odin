package common_odin

MDL_HeaderFlags :: enum i32{
	AttribPosition = 0x0001,
	AttribNormal = 0x0002,
	AttribTangent = 0x0004,
	AttribTexcoords = 0x0008,
	MeshletPipeline = 0x0100,
	_16BitIndices = 0x0200,
}
MDL_Header :: struct
{
	magic: [8]u8,
	description: [128]u8,
	flags: u32,
	vertex_size: u32,
	_pad: u32,
	block_count: u32,
	_: struct #align(8) {},
	blocks: [0]u8,
}
MDL_AnyBlock :: struct
{
	magic: [12]u8,
	block_size: u32,
}
MDL_BasicMeshFilter :: enum i32{
	Nearest,
	Linear,
	NearestMipmapNearest,
	NearestMipmapLinear,
	LinearMipmapNearest,
	LinearMipmapLinear,
}
MDL_BasicMeshBlock :: struct
{
	magic: [12]u8,
	block_size: u32,
	vbuffer_size: u32,
	vbuffer_offset: u32,
	ibuffer_size: u32,
	ibuffer_offset: u32,
	texid_base_color: u16,
	texid_normal: u16,
	_: struct #align(8) {},
	data: [0]u8,
}
MDL_TextureFormat :: enum i32{
	RgbaU8Norm,
	RgU8Norm,
	RU8Norm,
	RgbaF16,
}
MDL_TextureEncoding :: enum i32{
	Uncompressed,
	Png,
	Jpeg,
	Zstd,
}
MDL_TextureBlock :: struct
{
	magic: [12]u8,
	block_size: u32,
	size: u32,
	width: u32,
	height: u32,
	format: MDL_TextureFormat,
	encoding: MDL_TextureEncoding,
	_: struct #align(8) {},
	data: [0]u8,
}
MDL_SingleMeshlet :: struct
{
	vertex_offset: u32,
	vertex_count: u32,
	prim_offset: u32,
	prim_count: u32,
}
MDL_MeshletBlock :: struct
{
	magic: [12]u8,
	block_size: u32,
	prim_buffer_offset: u32,
	prim_buffer_size: u32,
	meshlet_buffer_offset: u32,
	meshlet_buffer_size: u32,
	_: struct #align(8) {},
	data: [0]u8,
}
