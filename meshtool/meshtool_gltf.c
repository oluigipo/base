struct GltfAccessor_
{
	int32 buffer_view;
	int32 byte_offset;
	int32 component_type;
	bool normalized;
	int32 count;
	String type;
	String name;
	int32 max[16];
	int32 min[16];
	// TODO: .sparse
}
typedef GltfAccessor_;

struct GltfAnimationSampler_
{
	int32 input;
	int32 output;
	String interpolation;
}
typedef GltfAnimationSampler_;

struct GltfAnimationChannel_
{
	int32 sampler;
	struct
	{
		int32 node;
		String path;
	}
	target;
}
typedef GltfAnimationChannel_;

struct GltfBuffer_
{
	String uri;
	String name;
	int32 byte_length;
}
typedef GltfBuffer_;

struct GltfBufferView_
{
	int32 buffer;
	int32 byte_offset;
	int32 byte_length;
	int32 byte_stride;
	int32 target;
	String name;
}
typedef GltfBufferView_;

struct GltfCamera_
{
	struct
	{
		float32 xmag;
		float32 ymag;
		float32 zfar;
		float32 znear;
	}
	orthographic;
	struct
	{
		float32 aspect_ratio;
		float32 yfov;
		float32 zfar;
		float32 znear;
	}
	perspective;
	String type;
	String name;
}
typedef GltfCamera_;

struct GltfImage_
{
	String uri;
	String mime_type;
	int32 buffer_view;
	String name;
}
typedef GltfImage_;

struct GltfTextureInfo_
{
	int32 index;
	int32 tex_coord;
}
typedef GltfTextureInfo_;

struct GltfMaterial_
{
	String name;
	struct
	{
		float32 base_color_factor[4];
		GltfTextureInfo_ base_color_texture;
		float32 metallic_factor;
		float32 roughness_factor;
		GltfTextureInfo_ metallic_roughness_texture;
	}
	pbr_metallic_roughness;
	struct
	{
		int32 index;
		int32 tex_coord;
		float32 scale;
	}
	normal_texture;
	struct
	{
		int32 index;
		int32 tex_coord;
		float32 strength;
	}
	occlusion_texture;
	GltfTextureInfo_ emissive_texture;
	float32 emissive_factor[3];
	String alpha_mode;
	float32 alpha_cutoff;
	bool double_sided;
}
typedef GltfMaterial_;

struct GltfAnimation_
{
	String name;
	intsize channels_count;
	GltfAnimationChannel_* channels;
	intsize samplers_count;
	GltfAnimationSampler_* samplers;
}
typedef GltfAnimation_;

struct GltfMeshPrimitive_
{
	struct
	{
		String name;
		int32 index;
	}
	attributes[16];
	int32 indices;
	int32 material;
	int32 mode;
	// TODO: targets
}
typedef GltfMeshPrimitive_;

struct GltfMesh_
{
	intsize primitives_count;
	GltfMeshPrimitive_* primitives;
	intsize weights_count;
	float32* weights;
	String name;
}
typedef GltfMesh_;

struct GltfNode_
{
	int32 camera;
	intsize children_count;
	int32* children;
	int32 skin;
	float32 matrix[16];
	int32 mesh;
	float32 rotation[4];
	float32 scale[3];
	float32 translation[3];
	intsize weights_count;
	float32* weights;
	String name;
}
typedef GltfNode_;

struct GltfSampler_
{
	int32 mag_filter;
	int32 min_filter;
	int32 wrap_s;
	int32 wrap_t;
	String name;
}
typedef GltfSampler_;

struct GltfScene_
{
	intsize nodes_count;
	int32* nodes;
	String name;
}
typedef GltfScene_;

struct GltfSkin_
{
	int32 inverse_bind_matrices;
	int32 skeleton;
	intsize joints_count;
	int32* joints;
	String name;
}
typedef GltfSkin_;

struct GltfTexture_
{
	int32 sampler;
	int32 source;
	String name;
}
typedef GltfTexture_;

struct GltfRoot_
{
	int32 scene;
	struct
	{
		String copyright;
		String generator;
		String version;
		String min_version;
	}
	asset;
	
	intsize accessors_count;
	GltfAccessor_* accessors;
	intsize animations_count;
	GltfAnimation_* animations;
	intsize buffers_count;
	GltfBuffer_* buffers;
	intsize buffer_views_count;
	GltfBufferView_* buffer_views;
	intsize cameras_count;
	GltfCamera_* cameras;
	intsize images_count;
	GltfImage_* images;
	intsize materials_count;
	GltfMaterial_* materials;
	intsize meshes_count;
	GltfMesh_* meshes;
	intsize nodes_count;
	GltfNode_* nodes;
	intsize samplers_count;
	GltfSampler_* samplers;
	intsize scenes_count;
	GltfScene_* scenes;
	intsize skins_count;
	GltfSkin_* skins;
	intsize textures_count;
	GltfTexture_* textures;
	
	intsize extensions_used_count;
	String* extensions_used;
	intsize extensions_required_count;
	String* extensions_required;
}
typedef GltfRoot_;

static bool ParseGltfAccessors_(JsonValue_ object, Arena* output_arena, GltfAccessor_** out_array, intsize* out_count);
static bool ParseGltfAnimations_(JsonValue_ arr, Arena* output_arena, GltfAnimation_** out_array, intsize* out_count);
static bool ParseGltfAsset_(JsonValue_ object, GltfRoot_* root);
static bool ParseGltfBuffers_(JsonValue_ arr, Arena* output_arena, GltfBuffer_** out_array, intsize* out_count);
static bool ParseGltfBufferViews_(JsonValue_ arr, Arena* output_arena, GltfBufferView_** out_array, intsize* out_count);
static bool ParseGltfCameras_(JsonValue_ arr, Arena* output_arena, GltfCamera_** out_array, intsize* out_count);
static bool ParseGltfImages_(JsonValue_ arr, Arena* output_arena, GltfImage_** out_array, intsize* out_count);
static bool ParseGltfMaterials_(JsonValue_ arr, Arena* output_arena, GltfMaterial_** out_array, intsize* out_count);
static bool ParseGltfMeshes_(JsonValue_ arr, Arena* output_arena, GltfMesh_** out_array, intsize* out_count);
static bool ParseGltfNodes_(JsonValue_ arr, Arena* output_arena, GltfNode_** out_array, intsize* out_count);
static bool ParseGltfSamplers_(JsonValue_ arr, Arena* output_arena, GltfSampler_** out_array, intsize* out_count);
static bool ParseGltfScenes_(JsonValue_ arr, Arena* output_arena, GltfScene_** out_array, intsize* out_count);
static bool ParseGltfSkins_(JsonValue_ arr, Arena* output_arena, GltfSkin_** out_array, intsize* out_count);
static bool ParseGltfTextures_(JsonValue_ arr, Arena* output_arena, GltfTexture_** out_array, intsize* out_count);

static bool ParseGltfAnimationChannels_(JsonValue_ arr, Arena* output_arena, GltfAnimationChannel_** out_array, intsize* out_count);
static bool ParseGltfAnimationSamplers_(JsonValue_ arr, Arena* output_arena, GltfAnimationSampler_** out_array, intsize* out_count);
static bool ParseGltfCameraOrthographic_(JsonValue_ object, GltfCamera_* camera);
static bool ParseGltfCameraPerspective_(JsonValue_ object, GltfCamera_* camera);
static bool ParseGltfMaterialPbrMetallicRoughness_(JsonValue_ object, GltfMaterial_* material);
static bool ParseGltfMaterialNormalTextureInfo_(JsonValue_ object, GltfMaterial_* material);
static bool ParseGltfMaterialOcclusionTexture_(JsonValue_ object, GltfMaterial_* material);
static bool ParseGltfTextureInfo_(JsonValue_ object, GltfTextureInfo_* texture_info);
static bool ParseGltfMeshPrimitives_(JsonValue_ arr, Arena* output_arena, GltfMeshPrimitive_** out_array, intsize* out_count);

static bool
ParseGltfJson_(uint8 const* data, uint32 size, Arena* output_arena, GltfRoot_** out_root)
{
	Trace();
	JsonValue_ obj = JsonMakeValue_(data, size);
	if (obj.kind != JsonValueKind_Object)
		return false;
	
	GltfRoot_* root = ArenaPushStruct(output_arena, GltfRoot_);
	bool ok = true;
	
	for (JsonField_ field = { obj }; ok && JsonNextField_(&field); )
	{
		String name = JsonGetFieldName_(field);
		JsonValue_ value = JsonGetFieldValue_(field);
		
		if (StringEquals(name, Str("accessors")))
			ok &= ParseGltfAccessors_(value, output_arena, &root->accessors, &root->accessors_count);
		else if (StringEquals(name, Str("animations")))
			ok &= ParseGltfAnimations_(value, output_arena, &root->animations, &root->animations_count);
		else if (StringEquals(name, Str("asset")))
			ok &= ParseGltfAsset_(value, root);
		else if (StringEquals(name, Str("buffers")))
			ok &= ParseGltfBuffers_(value, output_arena, &root->buffers, &root->buffers_count);
		else if (StringEquals(name, Str("bufferViews")))
			ok &= ParseGltfBufferViews_(value, output_arena, &root->buffer_views, &root->buffer_views_count);
		else if (StringEquals(name, Str("cameras")))
			ok &= ParseGltfCameras_(value, output_arena, &root->cameras, &root->cameras_count);
		else if (StringEquals(name, Str("images")))
			ok &= ParseGltfImages_(value, output_arena, &root->images, &root->images_count);
		else if (StringEquals(name, Str("materials")))
			ok &= ParseGltfMaterials_(value, output_arena, &root->materials, &root->materials_count);
		else if (StringEquals(name, Str("meshes")))
			ok &= ParseGltfMeshes_(value, output_arena, &root->meshes, &root->meshes_count);
		else if (StringEquals(name, Str("nodes")))
			ok &= ParseGltfNodes_(value, output_arena, &root->nodes, &root->nodes_count);
		else if (StringEquals(name, Str("samplers")))
			ok &= ParseGltfSamplers_(value, output_arena, &root->samplers, &root->samplers_count);
		else if (StringEquals(name, Str("scenes")))
			ok &= ParseGltfScenes_(value, output_arena, &root->scenes, &root->scenes_count);
		else if (StringEquals(name, Str("skins")))
			ok &= ParseGltfSkins_(value, output_arena, &root->skins, &root->skins_count);
		else if (StringEquals(name, Str("textures")))
			ok &= ParseGltfTextures_(value, output_arena, &root->textures, &root->textures_count);
		else if (StringEquals(name, Str("scene")))
		{
			if (value.kind != JsonValueKind_Number)
				ok = false;
			else
				root->scene = JsonGetValueInt32_(value);
		}
	}
	
	*out_root = root;
	return ok;
}

static bool
ParseGltfAccessors_(JsonValue_ arr, Arena* output_arena, GltfAccessor_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfAccessor_* array = ArenaPushArray(output_arena, GltfAccessor_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; ok && JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfAccessor_ accessor = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			String field_name = JsonGetFieldName_(field);
			JsonValue_ field_value = JsonGetFieldValue_(field);
			
			if (StringEquals(field_name, Str("bufferView")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					accessor.buffer_view = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("byteOffset")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					accessor.byte_offset = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("componentType")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					accessor.component_type = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("normalized")))
			{
				if (field_value.kind != JsonValueKind_Boolean)
					ok = false;
				else
					accessor.normalized = JsonGetValueBool_(field_value);
			}
			else if (StringEquals(field_name, Str("count")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					accessor.count = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("type")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					accessor.type = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("max")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= ArrayLength(accessor.max))
						{
							ok = false;
							break;
						}
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						accessor.max[i] = JsonGetValueInt32_(value);
					}
				}
			}
			else if (StringEquals(field_name, Str("min")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= ArrayLength(accessor.min))
						{
							ok = false;
							break;
						}
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						accessor.min[i] = JsonGetValueInt32_(value);
					}
				}
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					accessor.name = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("sparse")))
			{
				SafeAssert(!"TODO");
			}
		}
		
		array[i] = accessor;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfAnimations_(JsonValue_ arr, Arena* output_arena, GltfAnimation_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfAnimation_* array = ArenaPushArray(output_arena, GltfAnimation_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfAnimation_ animation = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("channels")))
				ok &= ParseGltfAnimationChannels_(field_value, output_arena, &animation.channels, &animation.channels_count);
			else if (StringEquals(field_name, Str("samplers")))
				ok &= ParseGltfAnimationSamplers_(field_value, output_arena, &animation.samplers, &animation.samplers_count);
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					animation.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = animation;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfAsset_(JsonValue_ object, GltfRoot_* root)
{
	if (object.kind != JsonValueKind_Object)
		return false;
	
	bool ok = true;
	for (JsonField_ field = { object }; ok && JsonNextField_(&field); )
	{
		String name = JsonGetFieldName_(field);
		JsonValue_ value = JsonGetFieldValue_(field);
		
		if (StringEquals(name, Str("copyright")))
		{
			if (value.kind != JsonValueKind_String)
				ok = false;
			else
				root->asset.copyright = JsonGetValueString_(value);
		}
		else if (StringEquals(name, Str("generator")))
		{
			if (value.kind != JsonValueKind_String)
				ok = false;
			else
				root->asset.generator = JsonGetValueString_(value);
		}
		else if (StringEquals(name, Str("version")))
		{
			if (value.kind != JsonValueKind_String)
				ok = false;
			else
				root->asset.version = JsonGetValueString_(value);
		}
		else if (StringEquals(name, Str("minVersion")))
		{
			if (value.kind != JsonValueKind_String)
				ok = false;
			else
				root->asset.min_version = JsonGetValueString_(value);
		}
	}
	
	return ok;
}

static bool
ParseGltfBuffers_(JsonValue_ arr, Arena* output_arena, GltfBuffer_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfBuffer_* array = ArenaPushArray(output_arena, GltfBuffer_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfBuffer_ buffer = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("uri")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					buffer.uri = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("byteLength")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					buffer.byte_length = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					buffer.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = buffer;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfBufferViews_(JsonValue_ arr, Arena* output_arena, GltfBufferView_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfBufferView_* array = ArenaPushArray(output_arena, GltfBufferView_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfBufferView_ buffer_view = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("buffer")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					buffer_view.buffer = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("byteOffset")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					buffer_view.byte_offset = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("byteLength")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					buffer_view.byte_length = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("byteStride")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					buffer_view.byte_stride = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("target")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					buffer_view.target = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					buffer_view.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = buffer_view;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfCameras_(JsonValue_ arr, Arena* output_arena, GltfCamera_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfCamera_* array = ArenaPushArray(output_arena, GltfCamera_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfCamera_ camera = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("orthographic")))
				ok &= ParseGltfCameraOrthographic_(field_value, &camera);
			else if (StringEquals(field_name, Str("perspective")))
				ok &= ParseGltfCameraPerspective_(field_value, &camera);
			else if (StringEquals(field_name, Str("type")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					camera.type = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					camera.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = camera;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfImages_(JsonValue_ arr, Arena* output_arena, GltfImage_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfImage_* array = ArenaPushArray(output_arena, GltfImage_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfImage_ image = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("uri")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					image.uri = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("mimeType")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					image.mime_type = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("bufferView")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					image.buffer_view = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					image.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = image;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfMaterials_(JsonValue_ arr, Arena* output_arena, GltfMaterial_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfMaterial_* array = ArenaPushArray(output_arena, GltfMaterial_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfMaterial_ material = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("pbrMetallicRoughness")))
				ok &= ParseGltfMaterialPbrMetallicRoughness_(field_value, &material);
			else if (StringEquals(field_name, Str("normalTexture")))
				ok &= ParseGltfMaterialNormalTextureInfo_(field_value, &material);
			else if (StringEquals(field_name, Str("occlusionTexture")))
				ok &= ParseGltfMaterialOcclusionTexture_(field_value, &material);
			else if (StringEquals(field_name, Str("emissiveTexture")))
				ok &= ParseGltfTextureInfo_(field_value, &material.emissive_texture);
			else if (StringEquals(field_name, Str("emissiveFactor")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= ArrayLength(material.emissive_factor))
						{
							ok = false;
							break;
						}
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						material.emissive_factor[i] = JsonGetValueFloat32_(value);
					}
				}
			}
			else if (StringEquals(field_name, Str("alphaMode")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					material.alpha_mode = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("alphaCutoff")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					material.alpha_cutoff = JsonGetValueFloat32_(field_value);
			}
			else if (StringEquals(field_name, Str("doubleSided")))
			{
				if (field_value.kind != JsonValueKind_Boolean)
					ok = false;
				else
					material.double_sided = JsonGetValueBool_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					material.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = material;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfMeshes_(JsonValue_ arr, Arena* output_arena, GltfMesh_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfMesh_* array = ArenaPushArray(output_arena, GltfMesh_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfMesh_ mesh = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("primitives")))
				ok &= ParseGltfMeshPrimitives_(field_value, output_arena, &mesh.primitives, &mesh.primitives_count);
			else if (StringEquals(field_name, Str("weights")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					intsize count = 0;
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
						++count;
					
					mesh.weights_count = count;
					mesh.weights = ArenaPushArray(output_arena, float32, count);
					
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= count)
							Unreachable();
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						mesh.weights[i] = JsonGetValueFloat32_(value);
					}
				}
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					mesh.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = mesh;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfNodes_(JsonValue_ arr, Arena* output_arena, GltfNode_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfNode_* array = ArenaPushArray(output_arena, GltfNode_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfNode_ node = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("camera")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					node.camera = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("children")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					intsize count = 0;
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
						++count;
					
					node.children_count = count;
					node.children = ArenaPushArray(output_arena, int32, count);
					
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= count)
							Unreachable();
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						node.children[i] = JsonGetValueInt32_(value);
					}
				}
			}
			else if (StringEquals(field_name, Str("skin")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					node.skin = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("mesh")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					node.mesh = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					node.name = JsonGetValueString_(field_value);
			}
			else if (StringEquals(field_name, Str("weights")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					intsize count = 0;
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
						++count;
					
					node.weights_count = count;
					node.weights = ArenaPushArray(output_arena, float32, count);
					
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= count)
							Unreachable();
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						node.weights[i] = JsonGetValueFloat32_(value);
					}
				}
			}
			else
			{
				float32* float_array = NULL;
				intsize float_count = 0;
				
				if      (StringEquals(field_name, Str("matrix")))
					float_array = node.matrix,      float_count = ArrayLength(node.matrix);
				else if (StringEquals(field_name, Str("rotation")))
					float_array = node.rotation,    float_count = ArrayLength(node.rotation);
				else if (StringEquals(field_name, Str("scale")))
					float_array = node.scale,       float_count = ArrayLength(node.scale);
				else if (StringEquals(field_name, Str("translation")))
					float_array = node.translation, float_count = ArrayLength(node.translation);
				
				if (float_array && float_count)
				{
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= float_count)
						{
							ok = false;
							break;
						}
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						float_array[i] = JsonGetValueFloat32_(value);
					}
				}
			}
		}
		
		array[i] = node;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfSamplers_(JsonValue_ arr, Arena* output_arena, GltfSampler_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfSampler_* array = ArenaPushArray(output_arena, GltfSampler_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfSampler_ sampler = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("magFilter")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					sampler.mag_filter = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("minFilter")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					sampler.min_filter = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("wrapS")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					sampler.wrap_s = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("wrapT")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					sampler.wrap_t = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					sampler.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = sampler;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfScenes_(JsonValue_ arr, Arena* output_arena, GltfScene_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfScene_* array = ArenaPushArray(output_arena, GltfScene_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfScene_ scene = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("nodes")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					intsize count = 0;
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
						++count;
					
					scene.nodes_count = count;
					scene.nodes = ArenaPushArray(output_arena, int32, count);
					
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= count)
							Unreachable();
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						scene.nodes[i] = JsonGetValueInt32_(value);
					}
				}
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					scene.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = scene;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfSkins_(JsonValue_ arr, Arena* output_arena, GltfSkin_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfSkin_* array = ArenaPushArray(output_arena, GltfSkin_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfSkin_ skin = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("inverseBindMatrices")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					skin.inverse_bind_matrices = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("skeleton")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					skin.skeleton = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("joints")))
			{
				if (field_value.kind != JsonValueKind_Array)
					ok = false;
				else
				{
					intsize count = 0;
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
						++count;
					
					skin.joints_count = count;
					skin.joints = ArenaPushArray(output_arena, int32, count);
					
					for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
					{
						intsize i = JsonGetIndexI_(index);
						if (i >= count)
							Unreachable();
						JsonValue_ value = JsonGetIndexValue_(index);
						if (value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}
						skin.joints[i] = JsonGetValueInt32_(value);
					}
				}
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					skin.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = skin;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfTextures_(JsonValue_ arr, Arena* output_arena, GltfTexture_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfTexture_* array = ArenaPushArray(output_arena, GltfTexture_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfTexture_ texture = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("sampler")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					texture.sampler = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("source")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					texture.source = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("name")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					texture.name = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = texture;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfAnimationChannels_(JsonValue_ arr, Arena* output_arena, GltfAnimationChannel_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfAnimationChannel_* array = ArenaPushArray(output_arena, GltfAnimationChannel_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfAnimationChannel_ entry = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("sampler")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					entry.sampler = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("target")))
			{
				if (field_value.kind != JsonValueKind_Object)
					ok = false;
				else
				{
					for (JsonField_ field = { value }; ok && JsonNextField_(&field);)
					{
						JsonValue_ field_value = JsonGetFieldValue_(field);
						String field_name = JsonGetFieldName_(field);

						if (StringEquals(field_name, Str("node")))
						{
							if (field_value.kind != JsonValueKind_Number)
								ok = false;
							else
								entry.target.node = JsonGetValueInt32_(field_value);
						}
						else if (StringEquals(field_name, Str("path")))
						{
							if (field_value.kind != JsonValueKind_String)
								ok = false;
							else
								entry.target.path = JsonGetValueString_(field_value);
						}
					}
				}
			}
		}
		
		array[i] = entry;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfAnimationSamplers_(JsonValue_ arr, Arena* output_arena, GltfAnimationSampler_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfAnimationSampler_* array = ArenaPushArray(output_arena, GltfAnimationSampler_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfAnimationSampler_ entry = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("input")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					entry.input = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("output")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					entry.output = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("interpolation")))
			{
				if (field_value.kind != JsonValueKind_String)
					ok = false;
				else
					entry.interpolation = JsonGetValueString_(field_value);
			}
		}
		
		array[i] = entry;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}

static bool
ParseGltfCameraOrthographic_(JsonValue_ object, GltfCamera_* camera)
{
	if (object.kind != JsonValueKind_Object)
		return false;
	
	bool ok = true;
	for (JsonField_ field = { object }; ok && JsonNextField_(&field);)
	{
		String field_name = JsonGetFieldName_(field);
		JsonValue_ field_value = JsonGetFieldValue_(field);

		if (StringEquals(field_name, Str("xmag")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->orthographic.xmag = JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("ymag")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->orthographic.ymag = JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("zfar")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->orthographic.zfar= JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("znear")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->orthographic.znear = JsonGetValueFloat32_(field_value);
		}
	}

	return ok;
}

static bool
ParseGltfCameraPerspective_(JsonValue_ object, GltfCamera_* camera)
{
	if (object.kind != JsonValueKind_Object)
		return false;
	
	bool ok = true;
	for (JsonField_ field = { object }; ok && JsonNextField_(&field);)
	{
		String field_name = JsonGetFieldName_(field);
		JsonValue_ field_value = JsonGetFieldValue_(field);

		if (StringEquals(field_name, Str("aspectRatio")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->perspective.aspect_ratio = JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("yfov")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->perspective.yfov = JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("zfar")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->perspective.zfar= JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("znear")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				camera->perspective.znear = JsonGetValueFloat32_(field_value);
		}
	}

	return ok;
}

static bool
ParseGltfMaterialPbrMetallicRoughness_(JsonValue_ object, GltfMaterial_* material)
{
	if (object.kind != JsonValueKind_Object)
		return false;
	
	bool ok = true;
	for (JsonField_ field = { object }; ok && JsonNextField_(&field);)
	{
		String field_name = JsonGetFieldName_(field);
		JsonValue_ field_value = JsonGetFieldValue_(field);

		if (StringEquals(field_name, Str("baseColorFactor")))
		{
			if (field_value.kind != JsonValueKind_Array)
				ok = false;
			else
			{
				for (JsonIndex_ index = { field_value }; JsonNextIndex_(&index); )
				{
					intsize i = JsonGetIndexI_(index);
					if (i >= ArrayLength(material->pbr_metallic_roughness.base_color_factor))
					{
						ok = false;
						break;
					}
					JsonValue_ value = JsonGetIndexValue_(index);
					if (value.kind != JsonValueKind_Number)
					{
						ok = false;
						break;
					}
					material->pbr_metallic_roughness.base_color_factor[i] = JsonGetValueFloat32_(value);
				}
			}
		}
		else if (StringEquals(field_name, Str("metallicFactor")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->pbr_metallic_roughness.metallic_factor = JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("roughnessFactor")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->pbr_metallic_roughness.roughness_factor = JsonGetValueFloat32_(field_value);
		}
		else if (StringEquals(field_name, Str("baseColorTexture")))
			ok &= ParseGltfTextureInfo_(field_value, &material->pbr_metallic_roughness.base_color_texture);
		else if (StringEquals(field_name, Str("metallicRoughnessTexture")))
			ok &= ParseGltfTextureInfo_(field_value, &material->pbr_metallic_roughness.metallic_roughness_texture);
	}

	return ok;
}

static bool
ParseGltfMaterialNormalTextureInfo_(JsonValue_ object, GltfMaterial_* material)
{
	if (object.kind != JsonValueKind_Object)
		return false;
	
	material->normal_texture.index = -1;
	material->pbr_metallic_roughness.base_color_texture.index = -1;

	bool ok = true;
	for (JsonField_ field = { object }; ok && JsonNextField_(&field);)
	{
		String field_name = JsonGetFieldName_(field);
		JsonValue_ field_value = JsonGetFieldValue_(field);

		if (StringEquals(field_name, Str("index")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->normal_texture.index = JsonGetValueInt32_(field_value);
		}
		else if (StringEquals(field_name, Str("texCoord")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->normal_texture.tex_coord = JsonGetValueInt32_(field_value);
		}
		else if (StringEquals(field_name, Str("scale")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->normal_texture.scale = JsonGetValueFloat32_(field_value);
		}
	}

	return ok;
}

static bool
ParseGltfMaterialOcclusionTexture_(JsonValue_ object, GltfMaterial_* material)
{
	if (object.kind != JsonValueKind_Object)
		return false;
	
	bool ok = true;
	for (JsonField_ field = { object }; ok && JsonNextField_(&field);)
	{
		String field_name = JsonGetFieldName_(field);
		JsonValue_ field_value = JsonGetFieldValue_(field);

		if (StringEquals(field_name, Str("index")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->occlusion_texture.index = JsonGetValueInt32_(field_value);
		}
		else if (StringEquals(field_name, Str("texCoord")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->occlusion_texture.tex_coord = JsonGetValueInt32_(field_value);
		}
		else if (StringEquals(field_name, Str("strength")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				material->occlusion_texture.strength = JsonGetValueFloat32_(field_value);
		}
	}

	return ok;
}

static bool
ParseGltfTextureInfo_(JsonValue_ object, GltfTextureInfo_* texture_info)
{
	if (object.kind != JsonValueKind_Object)
		return false;
	
	bool ok = true;
	for (JsonField_ field = { object }; ok && JsonNextField_(&field);)
	{
		String field_name = JsonGetFieldName_(field);
		JsonValue_ field_value = JsonGetFieldValue_(field);

		if (StringEquals(field_name, Str("index")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				texture_info->index = JsonGetValueInt32_(field_value);
		}
		else if (StringEquals(field_name, Str("texCoord")))
		{
			if (field_value.kind != JsonValueKind_Number)
				ok = false;
			else
				texture_info->tex_coord = JsonGetValueInt32_(field_value);
		}
	}

	return ok;
}

static bool
ParseGltfMeshPrimitives_(JsonValue_ arr, Arena* output_arena, GltfMeshPrimitive_** out_array, intsize* out_count)
{
	if (arr.kind != JsonValueKind_Array)
		return false;
	
	intsize count = 0;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
		++count;
	GltfMeshPrimitive_* array = ArenaPushArray(output_arena, GltfMeshPrimitive_, count);
	
	bool ok = true;
	for (JsonIndex_ index = { arr }; JsonNextIndex_(&index); )
	{
		JsonValue_ value = JsonGetIndexValue_(index);
		intsize i = JsonGetIndexI_(index);
		
		if (value.kind != JsonValueKind_Object)
		{
			ok = false;
			break;
		}
		
		GltfMeshPrimitive_ entry = { 0 };
		for (JsonField_ field = { value }; ok && JsonNextField_(&field); )
		{
			JsonValue_ field_value = JsonGetFieldValue_(field);
			String field_name = JsonGetFieldName_(field);
			
			if (StringEquals(field_name, Str("attributes")))
			{
				if (field_value.kind != JsonValueKind_Object)
					ok = false;
				else
				{
					intsize attrib_index = 0;
					for (JsonField_ attrib_field = { field_value }; ok && JsonNextField_(&attrib_field); ++attrib_index)
					{
						String attrib_name = JsonGetFieldName_(attrib_field);
						JsonValue_ attrib_value = JsonGetFieldValue_(attrib_field);
						if (attrib_index >= ArrayLength(entry.attributes))
						{
							ok = false;
							break;
						}
						if (attrib_value.kind != JsonValueKind_Number)
						{
							ok = false;
							break;
						}

						entry.attributes[attrib_index].name = attrib_name;
						entry.attributes[attrib_index].index = JsonGetValueInt32_(attrib_value);
					}
				}
			}
			else if (StringEquals(field_name, Str("indices")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					entry.indices = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("material")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					entry.material = JsonGetValueInt32_(field_value);
			}
			else if (StringEquals(field_name, Str("mode")))
			{
				if (field_value.kind != JsonValueKind_Number)
					ok = false;
				else
					entry.mode = JsonGetValueInt32_(field_value);
			}
		}
		
		array[i] = entry;
	}
	
	*out_array = array;
	*out_count = count;
	return ok;
}
