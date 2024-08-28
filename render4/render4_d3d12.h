#ifndef RENDER4_D3D12_H
#define RENDER4_D3D12_H

#include "common_basic.h"

struct R4_D3D12_Queue            typedef R4_D3D12_Queue;
struct R4_D3D12_Buffer           typedef R4_D3D12_Buffer;
struct R4_D3D12_Image            typedef R4_D3D12_Image;
struct R4_D3D12_Heap             typedef R4_D3D12_Heap;
struct R4_D3D12_CommandAllocator typedef R4_D3D12_CommandAllocator;
struct R4_D3D12_CommandList      typedef R4_D3D12_CommandList;
struct R4_D3D12_Pipeline         typedef R4_D3D12_Pipeline;
struct R4_D3D12_PipelineLayout   typedef R4_D3D12_PipelineLayout;
struct R4_D3D12_BindLayout       typedef R4_D3D12_BindLayout;
struct R4_D3D12_DescriptorHeap   typedef R4_D3D12_DescriptorHeap;
struct R4_D3D12_DescriptorSet    typedef R4_D3D12_DescriptorSet;
struct R4_D3D12_BufferView       typedef R4_D3D12_BufferView;
struct R4_D3D12_ImageView        typedef R4_D3D12_ImageView;
struct R4_D3D12_Sampler          typedef R4_D3D12_Sampler;
struct R4_D3D12_RenderTargetView typedef R4_D3D12_RenderTargetView;
struct R4_D3D12_DepthStencilView typedef R4_D3D12_DepthStencilView;

struct R4_D3D12_Queue
{
	struct ID3D12CommandQueue* queue;
};

struct R4_D3D12_Buffer
{
	struct ID3D12Resource* resource;
};

struct R4_D3D12_Image
{
	struct ID3D12Resource* resource;
};

struct R4_D3D12_Heap
{
	struct ID3D12Heap* heap;
};

struct R4_D3D12_CommandAllocator
{
	struct ID3D12CommandAllocator* allocator;
};

struct R4_D3D12_CommandList
{
	struct ID3D12CommandList* list;
};

struct R4_D3D12_Pipeline
{
	struct ID3D12PipelineState* pipeline;
};

struct R4_D3D12_PipelineLayout
{
	struct ID3D12RootSignature* rootsig;
	int32 rest_table_count;
	int32 sampler_table_count;
	int32 const_count;
};

struct R4_D3D12_BindLayout
{

};

struct R4_D3D12_DescriptorHeap
{
	struct ID3D12DescriptorHeap* sampler_heap;
	struct ID3D12DescriptorHeap* view_heap;

	int64 sampler_heap_buffer_size;
	int64 view_heap_buffer_size;
	int64 buffering_offsets[4];
	int32 buffering_count;
};

struct R4_D3D12_DescriptorSet
{
	uint64 cpu_ptr;
	uint64 gpu_ptr;
};

struct R4_D3D12_BufferView
{
	uint64 cbv_srv_uav_ptr;
};

struct R4_D3D12_ImageView
{
	uint64 srv_uav_ptr;
};

struct R4_D3D12_Sampler
{
	uint64 sampler_ptr;
};

struct R4_D3D12_RenderTargetView
{
	uint64 rtv_ptr;
};

struct R4_D3D12_DepthStencilView
{
	uint64 dsv_ptr;
};

#endif