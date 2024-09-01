#ifndef RENDER4_VULKAN_H
#define RENDER4_VULKAN_H

#include "common_basic.h"

struct R4_VK_Queue            typedef R4_VK_Queue;
struct R4_VK_Buffer           typedef R4_VK_Buffer;
struct R4_VK_Image            typedef R4_VK_Image;
struct R4_VK_Heap             typedef R4_VK_Heap;
struct R4_VK_CommandAllocator typedef R4_VK_CommandAllocator;
struct R4_VK_CommandList      typedef R4_VK_CommandList;
struct R4_VK_Pipeline         typedef R4_VK_Pipeline;
struct R4_VK_PipelineLayout   typedef R4_VK_PipelineLayout;
struct R4_VK_BindLayout       typedef R4_VK_BindLayout;
struct R4_VK_DescriptorHeap   typedef R4_VK_DescriptorHeap;
struct R4_VK_DescriptorSet    typedef R4_VK_DescriptorSet;
struct R4_VK_BufferView       typedef R4_VK_BufferView;
struct R4_VK_ImageView        typedef R4_VK_ImageView;
struct R4_VK_Sampler          typedef R4_VK_Sampler;
struct R4_VK_RenderTargetView typedef R4_VK_RenderTargetView;
struct R4_VK_DepthStencilView typedef R4_VK_DepthStencilView;

struct R4_VK_Queue
{
	struct VkQueue_T* queue;
	uint32 family_index;
};

struct R4_VK_Buffer
{
	struct VkBuffer_T* buffer;
	struct VkDeviceMemory_T* memory;
	uint64 offset;
};

struct R4_VK_Image
{
	struct VkImage_T* image;
	struct VkDeviceMemory_T* memory;
	uint64 offset;
};

struct R4_VK_Heap
{
	struct VkDeviceMemory_T* memory;
};

struct R4_VK_CommandAllocator
{
	struct VkCommandPool_T* pool;
};

struct R4_VK_CommandList
{
	struct VkCommandBuffer_T* buffer;
};

struct R4_VK_Pipeline
{
	struct VkPipeline_T* pso;
	int32 bind_point;
};

struct R4_VK_PipelineLayout
{
	struct VkPipelineLayout_T* layout;
};

struct R4_VK_BindLayout
{
	struct VkDescriptorSetLayout_T* layout;
};

struct R4_VK_DescriptorHeap
{
	struct VkDescriptorPool_T* pool;
};

struct R4_VK_DescriptorSet
{
	struct VkDescriptorSet_T* set;
};

struct R4_VK_BufferView
{
	struct VkBufferView_T* view;
	struct VkBuffer_T* buffer;
};

struct R4_VK_ImageView
{
	struct VkImageView_T* view;
};

struct R4_VK_Sampler
{
	struct VkSampler_T* sampler;
};

struct R4_VK_RenderTargetView
{
	struct VkImageView_T* image_view;
};

struct R4_VK_DepthStencilView
{
	struct VkImageView_T* image_view;
};

#endif //RENDER4_VULKAN_H