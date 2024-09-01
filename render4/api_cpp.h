#ifndef RENDER4_API_CPP_H
#define RENDER4_API_CPP_H

#include "common_basic.h"
#include "api.h"

struct R4_TmInterface;

struct R4_CppCommandList
{
	R4_TmInterface* vtbl;
	R4_CommandList cmdlist;

	void BeginRenderpass         (R4_Renderpass const* renderpass);
	void EndRenderpass           ();
	void ResourceBarrier         (R4_ResourceBarriers const* barriers);
	void SetDescriptorHeap       (R4_DescriptorHeap* heap);
	void SetPrimitiveType        (R4_PrimitiveType type);
	void SetViewports            (intz count, R4_Viewport const viewports[]);
	void SetScissors             (intz count, R4_Rect const rects[]);
	void SetPipeline             (R4_Pipeline* pipeline);
	void SetPipelineLayout       (R4_PipelineLayout* pipeline_layout);
	void SetVertexBuffers        (intz first_slot, intz count, R4_VertexBuffer const buffers[]);
	void SetIndexBuffer          (R4_IndexBuffer const* buffer);
	void PushConstants           (R4_PushConstant const* push_constant);
	void SetDescriptorSets       (R4_DescriptorSets const* sets);
	void ComputeSetPipelineLayout(R4_PipelineLayout* pipeline_layout);
	void ComputePushConstants    (R4_PushConstant const* push_constant);
	void ComputeSetDescriptorSet (R4_DescriptorSet* descriptor_set);
	void Draw                    (int64 start_vertex, int64 vertex_count, int64 start_instance, int64 instance_count);
	void DrawIndexed             (int64 start_index, int64 index_count, int64 start_instance, int64 instance_count, int64 base_vertex);
	void Dispatch                (int64 x, int64 y, int64 z);
	void DispatchMesh            (int64 x, int64 y, int64 z);
	void CopyBuffer              (R4_Buffer* dest, R4_Buffer* source, intz region_count, R4_BufferCopyRegion const regions[]);
	void CopyImage               (R4_Image* dest, R4_Image* source, intz region_count, R4_ImageCopyRegion const regions[]);
	void CopyBufferToImage       (R4_Image* dest, R4_Buffer* source, intz region_count, R4_BufferImageCopyRegion const regions[]);
};

struct R4_CppContext
{
	R4_TmInterface* vtbl;
	R4_Context* ctx;

	R4_ContextInfo GetContextInfo          ();
	void           PresentAndSync          (R4_Result* r, int32 sync_interval);
	intz           AcquireNextBackbuffer   (R4_Result* r);
	void           ExecuteCommandLists     (R4_Result* r, R4_Queue* queue, bool last_submission, intz cmdlist_count, R4_CppCommandList* const cmdlists[]);
	void           WaitRemainingWorkOnqueue(R4_Result* r, R4_Queue* queue);

	R4_Heap             MakeHeap            (R4_Result* r, R4_HeapType type, int64 size);
	R4_Buffer           MakeUploadBuffer    (R4_Result* r, R4_UploadBufferDesc const* desc);
	R4_Buffer           MakePlacedBuffer    (R4_Result* r, R4_PlacedBufferDesc const* desc);
	R4_Image            MakePlacedImage     (R4_Result* r, R4_PlacedImageDesc const* desc);
	R4_ImageView        MakeImageView       (R4_Result* r, R4_ImageViewDesc const* desc);
	R4_CommandAllocator MakeCommandAllocator(R4_Result* r, R4_Queue* target_queue);
	R4_CommandList      MakeCommandList     (R4_Result* r, R4_CommandListType type, R4_CommandAllocator* allocator);
	R4_DescriptorSet    MakeDescriptorSet   (R4_Result* r, R4_DescriptorSetDesc const* desc);
	R4_DescriptorHeap   MakeDescriptorHeap  (R4_Result* r, R4_DescriptorHeapDesc const* desc);
	R4_RenderTargetView MakeRenderTargetView(R4_Result* r, R4_RenderTargetViewDesc const* desc);
	R4_DepthStencilView MakeDepthStencilView(R4_Result* r, R4_DepthStencilViewDesc const* desc);
	R4_Sampler          MakeSampler         (R4_Result* r, R4_SamplerDesc const* desc);
	R4_BindLayout       MakeBindLayout      (R4_Result* r, R4_BindLayoutDesc const* desc);
	R4_PipelineLayout   MakePipelineLayout  (R4_Result* r, R4_PipelineLayoutDesc const* desc);
	R4_Pipeline         MakeGraphicsPipeline(R4_Result* r, R4_GraphicsPipelineDesc const* desc);
	R4_Pipeline         MakeComputePipeline (R4_Result* r, R4_ComputePipelineDesc  const* desc);
	R4_CppCommandList   MakeCppCommandList  (R4_Result* r, R4_CommandListType type, R4_CommandAllocator* allocator);
	void FreeHeap            (R4_Heap* heap);
	void FreeBuffer          (R4_Buffer* buffer);
	void FreeImage           (R4_Image* image);
	void FreeImageView       (R4_ImageView* image_view);
	void FreeCommandAllocator(R4_CommandAllocator* cmd_allocator);
	void FreeCommandList     (R4_CommandAllocator* cmdalloc, R4_CommandList* cmd_list);
	void FreeDescriptorSet   (R4_DescriptorSet* descriptor_set);
	void FreeDescriptorHeap  (R4_DescriptorHeap* descriptor_heap);
	void FreeRenderTargetView(R4_RenderTargetView* render_target_view);
	void FreeDepthStencilView(R4_DepthStencilView* depth_stencil_view);
	void FreeSampler         (R4_Sampler* sampler);
	void FreeBindLayout      (R4_BindLayout* bind_layout);
	void FreePipelineLayout  (R4_PipelineLayout* pipeline_layout);
	void FreePipeline        (R4_Pipeline* pipeline);
	void FreeCppCommandList  (R4_CppCommandList* cmdlist);

	R4_MemoryRequirements GetMemoryRequirementsFromBufferDesc(R4_BufferDesc const* desc);
	R4_MemoryRequirements GetMemoryRequirementsFromBuffer    (R4_Buffer* buffer);
	R4_MemoryRequirements GetMemoryRequirementsFromImageDesc (R4_ImageDesc const* desc);
	R4_MemoryRequirements GetMemoryRequirementsFromImage     (R4_Image* image);
	void MapResource                 (R4_Result* r, R4_Resource resource, int32 subresource, intz size, void** out_memory);
	void UnmapResource               (R4_Result* r, R4_Resource resource, int32 subresource);
	void UpdateDescriptorSets        (intz write_count, R4_DescriptorSetWrite const writes[], intz copy_count, R4_DescriptorSetCopy const copies[]);
	void ResetDescriptorHeapBuffering(R4_DescriptorHeap* descriptor_heap, int32 buffer_index);
	void ResetCommandAllocator       (R4_Result* r, R4_CommandAllocator* cmd_allocator);
	void BeginCommandList            (R4_Result* r, R4_CommandList* cmdlist, R4_CommandAllocator* cmd_allocator);
	void EndCommandList              (R4_Result* r, R4_CommandList* cmdlist);
};

API R4_CppContext R4_MakeCppContext   (Allocator allocator, R4_Result* r, R4_ContextDesc const* desc);
API void          R4_DestroyCppContext(R4_CppContext* cppi, R4_Result* r);

#endif //RENDER4_API_CPP_H