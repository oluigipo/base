
#include "common.h"
#include "api_os.h"
#include "api.h"
#pragma comment(lib, "vulkan-1.lib")

#include "third_party/stb_image.h"

static R4_Image
BumpAllocateImage_(R4_Context* ctx, int64* bump_offset_ptr, R4_PlacedImageDesc* desc)
{
	int64 bump_offset = *bump_offset_ptr;

	R4_MemoryRequirements requirements = R4_GetMemoryRequirementsFromImageDesc(ctx, &desc->image_desc);
	SafeAssert(IsPowerOf2(requirements.alignment));
	bump_offset = AlignUp(bump_offset, requirements.alignment-1);
	desc->heap_offset = bump_offset;
	R4_Image image = R4_MakePlacedImage(ctx, NULL, desc);
	bump_offset += requirements.size;
	
	*bump_offset_ptr = bump_offset;
	return image;
}

static R4_Buffer
BumpAllocateBuffer_(R4_Context* ctx, int64* bump_offset_ptr, R4_PlacedBufferDesc* desc)
{
	int64 bump_offset = *bump_offset_ptr;

	R4_MemoryRequirements requirements = R4_GetMemoryRequirementsFromBufferDesc(ctx, &desc->buffer_desc);
	SafeAssert(IsPowerOf2(requirements.alignment));
	bump_offset = AlignUp(bump_offset, requirements.alignment-1);
	desc->heap_offset = bump_offset;
	R4_Buffer buffer = R4_MakePlacedBuffer(ctx, NULL, desc);
	bump_offset += requirements.size;
	
	*bump_offset_ptr = bump_offset;
	return buffer;
}

static R4_Buffer
BumpAllocateUploadBuffer_(R4_Context* ctx, int64* bump_offset_ptr, R4_UploadBufferDesc* desc)
{
	int64 bump_offset = *bump_offset_ptr;

	R4_MemoryRequirements requirements = R4_GetMemoryRequirementsFromBufferDesc(ctx, &desc->buffer_desc);
	SafeAssert(IsPowerOf2(requirements.alignment));
	bump_offset = AlignUp(bump_offset, requirements.alignment-1);
	desc->heap_offset = bump_offset;
	R4_Buffer buffer = R4_MakeUploadBuffer(ctx, NULL, desc);
	bump_offset += requirements.size;
	
	*bump_offset_ptr = bump_offset;
	return buffer;
}

static void
Render4ErrorCallback_(void* user_data, R4_Result r, R4_Result* output_r)
{
	Trace();
	OS_LogErr("[ERROR] render4 callback: 0x%x", (uint32)r);
	if (!output_r)
		SafeAssert(!"unhandled render4 error");
}

IncludeBinary(g_shader_dxvs, "test_shader_vs.dxil");
IncludeBinary(g_shader_dxps, "test_shader_ps.dxil");
IncludeBinary(g_shader_spvs, "test_shader_vs.spirv");
IncludeBinary(g_shader_spps, "test_shader_ps.spirv");

struct Vertex
{
	float32 pos[3];
	float32 color[3];
	float32 texcoord[2];
};

API int32
EntryPoint(int32 argc, const char* const* argv)
{
	OS_Init(OS_Init_WindowAndGraphics);
	OS_Window window = OS_CreateWindow(&(OS_WindowDesc) {
		.width = 1280,
		.height = 720,
		.title = StrInit("test_render4"),
	});

	enum { BACKBUFFER_COUNT = 3 };

	Arena arena = OS_VirtualAllocArena(256<<20);
	R4_Queue graphics_queue = {}, compute_queue = {};
	R4_Image swapchain_images[BACKBUFFER_COUNT] = {};
	R4_Context* r4 = R4_MakeContext(AllocatorFromArena(&arena), NULL, &(R4_ContextDesc) {
		.os_window = &window,
		.debug_mode = true,
		.backbuffer_count = BACKBUFFER_COUNT,
		.backbuffer_format = R4_Format_R8G8B8A8_UNorm,
		.on_error = Render4ErrorCallback_,
		.out_graphics_queue = &graphics_queue,
		.out_compute_queue = &compute_queue,
		.out_backbuffer_images = swapchain_images,
	});
	if (!r4)
	{
		OS_MessageBox(Str("Error"), Str("Failed to create context"));
		return 1;
	}

	R4_CommandAllocator allocators[BACKBUFFER_COUNT] = {
		R4_MakeCommandAllocator(r4, NULL, &graphics_queue),
		R4_MakeCommandAllocator(r4, NULL, &graphics_queue),
		R4_MakeCommandAllocator(r4, NULL, &graphics_queue),
	};
	R4_CommandList cmdlists[BACKBUFFER_COUNT] = {
		R4_MakeCommandList(r4, NULL, R4_CommandListType_Graphics, &allocators[0]),
		R4_MakeCommandList(r4, NULL, R4_CommandListType_Graphics, &allocators[1]),
		R4_MakeCommandList(r4, NULL, R4_CommandListType_Graphics, &allocators[2]),
	};

	R4_RenderTargetView swapchain_views[BACKBUFFER_COUNT] = {};
	for (intz i = 0; i < ArrayLength(swapchain_images); ++i)
	{
		swapchain_views[i] = R4_MakeRenderTargetView(r4, NULL, &(R4_RenderTargetViewDesc) {
			.image = &swapchain_images[i],
			.format = R4_Format_R8G8B8A8_UNorm,
		});
	}

	int64 upload_heap_bump_offset = 0;
	R4_Heap upload_heap = R4_MakeHeap(r4, NULL, R4_HeapType_Upload, 32<<20);
	int64 default_heap_bump_offset = 0;
	R4_Heap default_heap = R4_MakeHeap(r4, NULL, R4_HeapType_Default, 128 << 20);

	struct Vertex vertices[3] = {
		{ 0.0, 0.5, 0.0,  1.0, 0.0, 0.0,  0.5, 0.0 },
		{ 0.5,-0.5, 0.0,  0.0, 1.0, 0.0,  1.0, 1.0 },
		{-0.5,-0.5, 0.0,  0.0, 0.0, 1.0,  0.0, 1.0 },
	};

	R4_Buffer vbuf_upload = BumpAllocateUploadBuffer_(r4, &upload_heap_bump_offset, &(R4_UploadBufferDesc) {
		.heap = &upload_heap,
		.buffer_desc = {
			.size = sizeof(vertices),
			.usage_flags = R4_ResourceUsageFlag_TransferSrc,
		},
	});
	R4_Buffer vbuf = BumpAllocateBuffer_(r4, &default_heap_bump_offset, &(R4_PlacedBufferDesc) {
		.heap = &default_heap,
		.initial_state = R4_ResourceState_TransferDst,
		.buffer_desc = {
			.size = sizeof(vertices),
			.usage_flags = R4_ResourceUsageFlag_VertexBuffer | R4_ResourceUsageFlag_TransferDst,
		},
	});

	intz texture_size = 512*512*4;
	R4_Buffer texture_upload = BumpAllocateUploadBuffer_(r4, &upload_heap_bump_offset, &(R4_UploadBufferDesc) {
		.heap = &upload_heap,
		.buffer_desc = {
			.size = texture_size,
			.usage_flags = R4_ResourceUsageFlag_TransferSrc,
		},
	});
	R4_Image texture = BumpAllocateImage_(r4, &default_heap_bump_offset, &(R4_PlacedImageDesc) {
		.heap = &default_heap,
		.initial_state = R4_ResourceState_Null,
		.image_desc = {
			.width = 512,
			.height = 512,
			.dimension = R4_ImageDimension_Texture2D,
			.format = R4_Format_R8G8B8A8_SRgb,
			.usage_flags = R4_ResourceUsageFlag_TransferDst | R4_ResourceUsageFlag_SampledImage,
		},
	});
	R4_ImageView texture_view = R4_MakeImageView(r4, NULL, &(R4_ImageViewDesc) {
		.image = &texture,
		.format = R4_Format_R8G8B8A8_SRgb,
		.type = R4_DescriptorType_ImageSampled,
	});

	R4_Sampler sampler = R4_MakeSampler(r4, NULL, &(R4_SamplerDesc) {});

	void* mem = NULL;
	R4_MapResource(r4, NULL, R4_BufferResource(&vbuf_upload), 0, sizeof(vertices), &mem);
	memcpy(mem, vertices, sizeof(vertices));
	R4_UnmapResource(r4, NULL, R4_BufferResource(&vbuf_upload), 0);

	mem = NULL;
	{
		int32 width, height, _;
		void* pixels = stbi_load("render4/test_image.png", &width, &height, &_, 4);
		SafeAssert(pixels);
		SafeAssert(width == 512 && height == 512);
		R4_MapResource(r4, NULL, R4_BufferResource(&texture_upload), 0, texture_size, &mem);
		MemoryCopy(mem, pixels, width*height*4);
		R4_UnmapResource(r4, NULL, R4_BufferResource(&texture_upload), 0);
	}

	R4_BindLayout bind_layout = R4_MakeBindLayout(r4, NULL, &(R4_BindLayoutDesc) {
		.shader_visibility = R4_ShaderVisibility_All,
		.entries = {
			[0] = {
				.descriptor_type = R4_DescriptorType_ImageSampled,
				.descriptor_count = 1,
				.start_shader_slot = 0,
			},
			[1] = {
				.descriptor_type = R4_DescriptorType_Sampler,
				.descriptor_count = 1,
				.start_shader_slot = 0,
			},
		},
	});
	R4_PipelineLayout pipeline_layout = R4_MakePipelineLayout(r4, NULL, &(R4_PipelineLayoutDesc) {
		.push_constants = {
			[0] = {
				.shader_visibility = R4_ShaderVisibility_Vertex,
				.start_index = 0,
				.count = 1,
			},
		},
		.bind_layouts = {
			[0] = &bind_layout,
		},
	});
	R4_Pipeline pipeline = R4_MakeGraphicsPipeline(r4, NULL, &(R4_GraphicsPipelineDesc) {
		.pipeline_layout = &pipeline_layout,
		.primitive = R4_PrimitiveType_TriangleList,
		.rendertarget_count = 1,
		.rendertarget_formats[0] = R4_Format_R8G8B8A8_UNorm,
		.shaders_dxil = {
			.vertex = BufInitRange(g_shader_dxvs_begin, g_shader_dxvs_end),
			.fragment = BufInitRange(g_shader_dxps_begin, g_shader_dxps_end),
		},
		.shaders_spirv = {
			.vertex = BufInitRange(g_shader_spvs_begin, g_shader_spvs_end),
			.fragment = BufInitRange(g_shader_spps_begin, g_shader_spps_end),
		},
		.vertex_inputs = {
			[0] = {
				.input_slot = 0,
				.byte_offset = offsetof(struct Vertex, pos),
				.format = R4_Format_R32G32B32_Float,
			},
			[1] = {
				.input_slot = 0,
				.byte_offset = offsetof(struct Vertex, color),
				.format = R4_Format_R32G32B32_Float,
			},
			[2] = {
				.input_slot = 0,
				.byte_offset = offsetof(struct Vertex, texcoord),
				.format = R4_Format_R32G32_Float,
			},
		},
		.sample_count = 1,
		.sample_mask = UINT32_MAX,
		.rast_enable_depth_clip = true,
		.blend_rendertargets[0] = {
			.write_mask = 0b1111,
		},
	});

	R4_DescriptorHeap descriptor_heap = R4_MakeDescriptorHeap(r4, NULL, &(R4_DescriptorHeapDesc) {
		.buffering_count = BACKBUFFER_COUNT,
		.max_set_count = 1,
		.pools = {
			[0] = {
				.type = R4_DescriptorType_ImageSampled,
				.count = 1,
			},
			[1] = {
				.type = R4_DescriptorType_Sampler,
				.count = 1,
			},
		},
	});
	R4_DescriptorSet descriptor_sets[BACKBUFFER_COUNT] = {
		R4_MakeDescriptorSet(r4, NULL, &(R4_DescriptorSetDesc) { &bind_layout, &descriptor_heap, 0 }),
		R4_MakeDescriptorSet(r4, NULL, &(R4_DescriptorSetDesc) { &bind_layout, &descriptor_heap, 1 }),
		R4_MakeDescriptorSet(r4, NULL, &(R4_DescriptorSetDesc) { &bind_layout, &descriptor_heap, 2 }),
	};

	intsize image_index = 0;
	bool running = true;
	bool first_frame = true;
	while (running)
	{
		ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
		intsize event_count;
		OS_Event* events = OS_PollEvents(false, scratch.arena, &event_count);
		for (intsize i = 0; i < event_count; ++i)
		{
			if (events[i].kind == OS_EventKind_WindowClose)
				running = false;
		}
		ArenaRestore(scratch);

		image_index = R4_AcquireNextBackbuffer(r4, NULL);
		// OS_LogErr("image_index = %I\n", image_index);

		R4_CommandAllocator* allocator = &allocators[image_index];
		R4_CommandList* cmdlist = &cmdlists[image_index];
		R4_RenderTargetView* backbuffer_view = &swapchain_views[image_index];
		R4_Image* backbuffer = &swapchain_images[image_index];
		R4_DescriptorSet* descriptor_set = &descriptor_sets[image_index];
		R4_ResetCommandAllocator(r4, NULL, allocator);
		R4_BeginCommandList(r4, NULL, cmdlist, allocator);

		R4_DescriptorSetWrite descriptor_set_writes[2] = {
			[0] = {
				.dst_set = descriptor_set,
				.dst_binding = 0,
				.type = R4_DescriptorType_ImageSampled,
				.count = 1,
				.images = (R4_DescriptorImage[1]) {
					[0] = {
						.image_view = &texture_view,
						.state = R4_ResourceState_SampledImage,
					},
				},
			},
			[1] = {
				.dst_set = descriptor_set,
				.dst_binding = 1,
				.type = R4_DescriptorType_Sampler,
				.count = 1,
				.samplers = (R4_Sampler*[1]) { &sampler },
			},
		};
		R4_UpdateDescriptorSets(r4, ArrayLength(descriptor_set_writes), descriptor_set_writes, 0, NULL);

		if (first_frame)
		{
			first_frame = false;
			R4_CmdCopyBuffer(cmdlist, &vbuf, &vbuf_upload, 1, (R4_BufferCopyRegion[1]) {
				[0] = {
					.dst_offset = 0,
					.src_offset = 0,
					.size = sizeof(vertices),
				},
			});
			R4_CmdResourceBarrier(cmdlist, &(R4_ResourceBarriers) {
				.buffer_barrier_count = 1,
				.buffer_barriers = (R4_BufferBarrier[1]) {
					[0] = {
						.buffer = &texture_upload,
						.from_state = R4_ResourceState_Preinitialized,
						.to_state = R4_ResourceState_TransferSrc,
						.to_access = R4_AccessMask_TransferSrc,
						.to_stage = R4_StageMask_Transfer,
					},
				},
				.image_barrier_count = 1,
				.image_barriers = (R4_ImageBarrier[1]) {
					[0] = {
						.image = &texture,
						.from_state = R4_ResourceState_Null,
						.to_state = R4_ResourceState_TransferDst,
						.to_access = R4_AccessMask_TransferDst,
						.to_stage = R4_StageMask_Transfer,
					},
				},
			});
			R4_CmdCopyBufferToImage(cmdlist, &texture, &texture_upload, 1, &(R4_BufferImageCopyRegion) {
				.buffer_width_in_pixels = 512,
				.buffer_height_in_pixels = 512,
				.image_width = 512,
				.image_height = 512,
				.image_format = R4_Format_R8G8B8A8_SRgb,
			});
			R4_CmdResourceBarrier(cmdlist, &(R4_ResourceBarriers) {
				.buffer_barrier_count = 1,
				.buffer_barriers = (R4_BufferBarrier[1]) {
					[0] = {
						.buffer = &vbuf,
						.from_state = R4_ResourceState_TransferDst,
						.from_access = R4_AccessMask_TransferDst,
						.from_stage = R4_StageMask_Transfer,
						.to_state = R4_ResourceState_VertexBuffer,
						.to_access = R4_AccessMask_VertexBuffer,
						.to_stage = R4_StageMask_VertexInput,
					},
				},
				.image_barrier_count = 1,
				.image_barriers = (R4_ImageBarrier[1]) {
					[0] = {
						.image = &texture,
						.from_state = R4_ResourceState_TransferDst,
						.from_access = R4_AccessMask_TransferDst,
						.from_stage = R4_StageMask_Transfer,
						.to_state = R4_ResourceState_SampledImage,
						.to_access = R4_AccessMask_SampledImage,
						.to_stage = R4_StageMask_PixelShader,
					},
				},
			});
		}

		R4_CmdResourceBarrier(cmdlist, &(R4_ResourceBarriers) {
			.image_barrier_count = 1,
			.image_barriers = (R4_ImageBarrier[1]) {
				[0] = {
					.image = backbuffer,
					.from_state = R4_ResourceState_Null,
					.from_stage = R4_StageMask_All,
					.to_state = R4_ResourceState_RenderTarget,
					.to_access = R4_AccessMask_RenderTarget,
					.to_stage = R4_StageMask_RenderTarget,
				},
			},
		});
		R4_CmdBeginRenderpass(cmdlist, &(R4_Renderpass) {
			.render_area = { 0, 0, 1280, 720 },
			.render_targets[0] = {
				.render_target_view = backbuffer_view,
				.load = R4_AttachmentLoadOp_Clear,
				.clear_color = { 0.2f, 0.0f, 0.3f, 1.0f },
				.store = R4_AttachmentStoreOp_Store,
			},
		});

		R4_CmdSetViewports(cmdlist, 1, (R4_Viewport[]) {
			[0] = { 0, 0, 1280, 720, 0.0f, 1.0f },
		});
		R4_CmdSetScissors(cmdlist, 1, (R4_Rect[]) {
			[0] = { 0, 0, 1280, 720 },
		});
		R4_CmdSetPrimitiveType(cmdlist, R4_PrimitiveType_TriangleList);
		R4_CmdSetPipelineLayout(cmdlist, &pipeline_layout);
		R4_CmdSetPipeline(cmdlist, &pipeline);
		R4_CmdSetDescriptorHeap(cmdlist, &descriptor_heap);
		R4_CmdSetDescriptorSets(cmdlist, &(R4_DescriptorSets) {
			.pipeline_layout = &pipeline_layout,
			.count = 1,
			.sets = &descriptor_set,
		});
		R4_CmdSetVertexBuffers(cmdlist, 0, 1, &(R4_VertexBuffer) {
			.buffer = &vbuf,
			.size = sizeof(vertices),
			.stride = sizeof(struct Vertex),
		});
		R4_CmdPushConstants(cmdlist, &(R4_PushConstant) {
			.pipeline_layout = &pipeline_layout,
			.shader_visibility = R4_ShaderVisibility_Vertex,
			.count = 1,
			.u32_constants = &(float32) { OS_TimeInSeconds() },
		});
		R4_CmdDraw(cmdlist, 0, 3, 0, 1);

		R4_CmdEndRenderpass(cmdlist);
		R4_CmdResourceBarrier(cmdlist, &(R4_ResourceBarriers) {
			.image_barrier_count = 1,
			.image_barriers = (R4_ImageBarrier[1]) {
				[0] = {
					.image = backbuffer,
					.from_state = R4_ResourceState_RenderTarget,
					.from_access = R4_AccessMask_RenderTarget,
					.from_stage = R4_StageMask_RenderTarget,
					.to_state = R4_ResourceState_Present,
					.to_access = R4_AccessMask_Present,
					.to_stage = R4_StageMask_All,
				},
			},
		});

		R4_EndCommandList(r4, NULL, cmdlist);
		R4_ExecuteCommandLists(r4, NULL, &graphics_queue, true, 1, &cmdlist);
		R4_PresentAndSync(r4, NULL, 1);
	}

	R4_WaitRemainingWorkOnQueue(r4, NULL, &graphics_queue);
	R4_FreeCommandList(r4, &allocators[0], &cmdlists[0]);
	R4_FreeCommandList(r4, &allocators[1], &cmdlists[1]);
	R4_FreeCommandList(r4, &allocators[2], &cmdlists[2]);
	R4_FreeCommandAllocator(r4, &allocators[0]);
	R4_FreeCommandAllocator(r4, &allocators[1]);
	R4_FreeCommandAllocator(r4, &allocators[2]);
	R4_FreeDescriptorHeap(r4, &descriptor_heap);
	R4_FreeBuffer(r4, &texture_upload);
	R4_FreeImage(r4, &texture);
	R4_FreeBuffer(r4, &vbuf_upload);
	R4_FreeBuffer(r4, &vbuf);
	R4_FreeHeap(r4, &default_heap);
	R4_FreeHeap(r4, &upload_heap);
	R4_FreeRenderTargetView(r4, &swapchain_views[0]);
	R4_FreeRenderTargetView(r4, &swapchain_views[1]);
	R4_FreeRenderTargetView(r4, &swapchain_views[2]);
	// R4_FreeSampler(r4, &sampler);
	R4_DestroyContext(r4, NULL);
	OS_DestroyWindow(window);
	return 0;
}

DisableWarnings();
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
ReenableWarnings();