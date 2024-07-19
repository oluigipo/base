
// #define USE_VULKAN

#include "common.h"
#include "api_os.h"
#include "api_os_win32.h"
#ifndef USE_VULKAN
#include "api_render4.h"
#else
#	pragma comment(lib, "vulkan-1.lib")
#	undef API
#	define API static inline
#	include "api_render4.h"
#   include "render4_vulkan.c"
#	undef API
#	define API
#endif

IncludeBinary(g_shader_dxvs, "shader_vs.dxil");
IncludeBinary(g_shader_dxps, "shader_ps.dxil");
IncludeBinary(g_shader_spvs, "shader_vs.spirv");
IncludeBinary(g_shader_spps, "shader_ps.spirv");

API int32
EntryPoint(int32 argc, const char* const* argv)
{
	OS_Init(OS_Init_WindowAndGraphics);
	OS_Window window = OS_CreateWindow(&(OS_WindowDesc) {
		.width = 1280,
		.height = 720,
		.title = StrInit("test_render4"),
	});

	Arena arena = OS_VirtualAllocArena(256<<20);
	R4_Queue graphics_queue = {}, compute_queue = {};
	R4_Swapchain swapchain = {};
	R4_ContextDesc ctx_desc = {
		.swapchains[0] = {
			.window = &window,
			.buffer_count = 3,
			.format = R4_Format_R8G8B8A8_UNorm,
			.out_graphics_queue = &graphics_queue,
			.out_swapchain = &swapchain,
		},
		.queues[0] = {
			.kind = R4_CommandListKind_Compute,
			.out_queue = &compute_queue,
		},
	};
#ifdef USE_VULKAN
	R4_Context* r4 = R4_VK_MakeContext(&arena, &ctx_desc);
#else
	R4_Context* r4 = R4_D3D12_MakeContext(&arena, &ctx_desc);
#endif
	if (!r4)
	{
		OS_MessageBox(Str("Error"), Str("Failed to create context"));
		return 1;
	}

	R4_Fence image_fences[3] = {
		R4_MakeFence(r4),
		R4_MakeFence(r4),
		R4_MakeFence(r4),
	};
	R4_CommandAllocator allocators[3] = {
		R4_MakeCommandAllocator(r4, &graphics_queue),
		R4_MakeCommandAllocator(r4, &graphics_queue),
		R4_MakeCommandAllocator(r4, &graphics_queue),
	};
	R4_CommandList cmdlists[3] = {
		R4_MakeCommandList(r4, R4_CommandListKind_Graphics, &allocators[0]),
		R4_MakeCommandList(r4, R4_CommandListKind_Graphics, &allocators[1]),
		R4_MakeCommandList(r4, R4_CommandListKind_Graphics, &allocators[2]),
	};
	R4_Resource swapchain_images[3] = {};
	R4_GetSwapchainBuffers(r4, &swapchain, ArrayLength(swapchain_images), swapchain_images);
	R4_RenderTargetView rtvs[3] = {};
	R4_CreateRenderTargetViewsFromResources(r4, R4_Format_R8G8B8A8_UNorm, ArrayLength(rtvs), swapchain_images, rtvs);

	R4_Heap upload_heap = R4_MakeHeap(r4, &(R4_HeapDesc) {
		.kind = R4_HeapKind_Upload,
		.size = 32 << 20,
	});
	R4_Heap default_heap = R4_MakeHeap(r4, &(R4_HeapDesc) {
		.kind = R4_HeapKind_Default,
		.size = 128 << 20,
	});

	struct Vertex
	{
		float32 pos[3];
		float32 color[3];
	};
	struct Vertex vertices[3] = {
		{ 0.0, 0.5, 0.0, 1.0, 0.0, 0.0 },
		{ 0.5,-0.5, 0.0, 0.0, 1.0, 0.0 },
		{-0.5,-0.5, 0.0, 0.0, 0.0, 1.0 },
	};

	R4_Resource vbuf_upload = R4_MakePlacedResource(r4, &(R4_PlacedResourceDesc) {
		.heap = &upload_heap,
		.initial_state = R4_ResourceState_GenericRead,
		.heap_offset = 0,
		.resource_desc = {
			.kind = R4_ResourceKind_Buffer,
			.width = sizeof(vertices),
			.usage = R4_ResourceUsageFlag_TransferSrc,
		},
	});
	R4_Resource vbuf = R4_MakePlacedResource(r4, &(R4_PlacedResourceDesc) {
		.heap = &default_heap,
		.initial_state = R4_ResourceState_CopyDest,
		.heap_offset = 0,
		.resource_desc = {
			.kind = R4_ResourceKind_Buffer,
			.width = sizeof(vertices),
			.usage = R4_ResourceUsageFlag_VertexBuffer | R4_ResourceUsageFlag_TransferDst,
		},
	});

	void* mem = NULL;
	R4_MapResource(r4, &vbuf_upload, 0, sizeof(vertices), &mem);
	memcpy(mem, vertices, sizeof(vertices));
	R4_UnmapResource(r4, &vbuf_upload, 0);

	// R4_DescriptorHeap descriptor_heap = R4_MakeDescriptorHeap(r4, &(R4_DescriptorHeapDesc) {
	// 	.type = R4_DescriptorHeapType_CbvSrvUav,
	// 	.flags = R4_DescriptorHeapFlag_ShaderVisible,
	// 	.descriptor_count = 2,
	// });

	R4_RootSignature root_signature = R4_MakeRootSignature(r4, &(R4_RootSignatureDesc) {
		.params[0] = {
			.type = R4_RootParameterType_Constants,
			.visibility = R4_ShaderVisibility_Vertex,
			.constants = {
				.offset = 0,
				.count = 1,
			},
		},
	});
	R4_Pipeline pipeline = R4_MakeGraphicsPipeline(r4, &(R4_GraphicsPipelineDesc) {
		.rootsig = &root_signature,
		.primitive_topology = R4_PrimitiveTopology_TriangleList,
		.rendertarget_count = 1,
		.rendertarget_formats[0] = R4_Format_R8G8B8A8_UNorm,
		.vs_dxil = {
			g_shader_dxvs_begin,
			g_shader_dxvs_end - g_shader_dxvs_begin,
		},
		.ps_dxil = {
			g_shader_dxps_begin,
			g_shader_dxps_end - g_shader_dxps_begin,
		},
		.vs_spirv = {
			g_shader_spvs_begin,
			g_shader_spvs_end - g_shader_spvs_begin,
		},
		.ps_spirv = {
			g_shader_spps_begin,
			g_shader_spps_end - g_shader_spps_begin,
		},
		.input_layout = {
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
		},
		.sample_count = 1,
		.sample_mask = UINT32_MAX,
		.rast_enable_depth_clip = true,
		.blend_rendertargets[0] = {
			.rendertarget_write_mask = 15,
		},
	});

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

		image_index = R4_GetCurrentBackBufferIndex(r4, &swapchain);
		R4_WaitFence(r4, &image_fences[image_index], UINT32_MAX);
		// OS_LogErr("image_index = %I\n", image_index);

		R4_CommandAllocator* allocator = &allocators[image_index];
		R4_CommandList* cmdlist = &cmdlists[image_index];
		R4_RenderTargetView* rtv = &rtvs[image_index];
		R4_Resource* image = &swapchain_images[image_index];
		R4_ResetCommandAllocator(r4, allocator);
		R4_BeginCommandList(r4, cmdlist, &allocators[image_index]);

		if (first_frame)
		{
			R4_CmdCopyBuffer(r4, cmdlist, &vbuf, 0, &vbuf_upload, 0, sizeof(vertices));
			R4_CmdBarrier(r4, cmdlist, 1, (R4_ResourceBarrier[]) {
				[0] = {
					.resource = &vbuf,
					.type = R4_BarrierType_Transition,
					.transition = {
						.subresource = 0,
						.from = R4_ResourceState_CopyDest,
						.to = R4_ResourceState_VertexBuffer,
					},
				},
			});
			first_frame = false;
		}

		R4_CmdBarrier(r4, cmdlist, 1, &(R4_ResourceBarrier) {
			.resource = image,
			.type = R4_BarrierType_Transition,
			.transition = {
				.subresource = 0,
				.from = R4_ResourceState_Null,
				.to = R4_ResourceState_RenderTarget,
			},
		});
		R4_CmdBeginRenderpass(r4, cmdlist, &(R4_BeginRenderpassDesc) {
			.render_area = { .width = 1280, .height = 720 },
			.color_attachments[0] = {
				.rendertarget = rtv,
				.initial_layout = R4_ResourceState_Common,
				.target_layout = R4_ResourceState_RenderTarget,
				.load = R4_AttachmentLoadOp_Clear,
				.clear_color = { 0.2f, 0.0f, 0.3f, 1.0f },
				.store = R4_AttachmentStoreOp_Store,
			},
		});

		R4_CmdSetViewports(r4, cmdlist, 1, (R4_Viewport[]) {
			[0] = { 0, 0, 1280, 720, 0.0f, 1.0f },
		});
		R4_CmdSetScissors(r4, cmdlist, 1, (R4_Rect[]) {
			[0] = { 0, 0, 1280, 720 },
		});
		R4_CmdSetPrimitiveTopology(r4, cmdlist, R4_PrimitiveTopology_TriangleList);
		R4_CmdSetRootSignature(r4, cmdlist, &root_signature);
		R4_CmdSetPipeline(r4, cmdlist, &pipeline);
		R4_CmdSetVertexBuffers(r4, cmdlist, 0, 1, &(R4_VertexBuffer) {
			.resource = &vbuf,
			.size = sizeof(vertices),
			.stride = sizeof(struct Vertex),
		});
		R4_CmdSetGraphicsRootConstantsU32(r4, cmdlist, &(R4_RootArgument) {
			.rootsig = &root_signature,
			.visibility = R4_ShaderVisibility_Vertex,
			.count = 1,
			.u32_args = &(float32) { OS_TimeInSeconds() },
		});
		R4_CmdDraw(r4, cmdlist, 0, 3, 0, 1);

		R4_CmdEndRenderpass(r4, cmdlist);
		R4_CmdBarrier(r4, cmdlist, 1, &(R4_ResourceBarrier) {
			.resource = image,
			.type = R4_BarrierType_Transition,
			.transition = {
				.subresource = 0,
				.from = R4_ResourceState_RenderTarget,
				.to = R4_ResourceState_Present,
			},
		});

		R4_EndCommandList(r4, cmdlist);
		R4_ExecuteCommandLists(r4, &graphics_queue, &image_fences[image_index], &swapchain, 1, &cmdlist);
		R4_Present(r4, &graphics_queue, &swapchain, 1);
	}
	
	R4_WaitFence(r4, &image_fences[image_index], UINT32_MAX);
	R4_FreeFence(r4, &image_fences[0]);
	R4_FreeFence(r4, &image_fences[1]);
	R4_FreeFence(r4, &image_fences[2]);
	R4_FreeCommandList(r4, &allocators[0], &cmdlists[0]);
	R4_FreeCommandList(r4, &allocators[1], &cmdlists[1]);
	R4_FreeCommandList(r4, &allocators[2], &cmdlists[2]);
	R4_FreeCommandAllocator(r4, &allocators[0]);
	R4_FreeCommandAllocator(r4, &allocators[1]);
	R4_FreeCommandAllocator(r4, &allocators[2]);
	// R4_FreeDescriptorHeap(r4, &descriptor_heap);
	R4_FreeResource(r4, &vbuf_upload);
	R4_FreeResource(r4, &vbuf);
	R4_FreeHeap(r4, &default_heap);
	R4_FreeHeap(r4, &upload_heap);
	R4_DestroyContext(r4);
	OS_DestroyWindow(window);
	return 0;
}
