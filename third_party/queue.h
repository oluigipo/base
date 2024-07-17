// Originally from https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
// Licensed under Apache-v2 (https://www.apache.org/licenses/LICENSE-2.0.txt)
// Modified to compile as C code.

struct MpmcQueue
{
	// Probably having the sequences as an array induces false-sharing... But it would still
	// occur if the item_size was < 64-8 anyway, so...?
	size_t volatile* sequences;
	void* items;
	size_t item_size;
	size_t item_count;
	alignas(64) size_t volatile enqueue_pos;
	alignas(64) size_t volatile dequeue_pos;
}
typedef MpmcQueue;

static inline void
MpmcInitSequences(MpmcQueue* queue)
{
	for (size_t i = 0; i < queue->item_count; ++i)
		queue->sequences[i] = i;
	queue->enqueue_pos = 0;
	queue->dequeue_pos = 0;
}

static inline bool
MpmcEnqueue(MpmcQueue* queue, void const* item)
{
	size_t const buffer_mask = queue->item_count - 1;
	size_t index;
	size_t pos = __atomic_load_n(&queue->enqueue_pos, __ATOMIC_RELAXED);

	for (;;)
	{
		index = pos & buffer_mask;
		size_t seq = __atomic_load_n(&queue->sequences[index], __ATOMIC_ACQUIRE);
		intptr_t diff = (intptr_t)seq - (intptr_t)pos;

		if (diff < 0)
			return false;
		else if (diff > 0)
			pos = __atomic_load_n(&queue->enqueue_pos, __ATOMIC_RELAXED);
		else if (__atomic_compare_exchange_n(&queue->enqueue_pos, &pos, pos + 1, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
			break;
	}

	memcpy((uint8 *)queue->items + queue->item_size * index, item, queue->item_size);
	__atomic_store_n(&queue->sequences[index], pos + 1, __ATOMIC_RELEASE);
	return true;
}

static inline bool
MpmcDequeue(MpmcQueue* queue, void* out_item)
{
	size_t const buffer_mask = queue->item_count - 1;
	size_t index;
	size_t pos = __atomic_load_n(&queue->dequeue_pos, __ATOMIC_RELAXED);

	for (;;)
	{
		index = pos & buffer_mask;
		size_t seq = __atomic_load_n(&queue->sequences[index], __ATOMIC_ACQUIRE);
		intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

		if (diff < 0)
			return false;
		else if (diff > 0)
			pos = __atomic_load_n(&queue->dequeue_pos, __ATOMIC_RELAXED);
		else if (__atomic_compare_exchange_n(&queue->dequeue_pos, &pos, pos + 1, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
			break;
	}

	memcpy(out_item, (uint8 *)queue->items + queue->item_size * index, queue->item_size);
	__atomic_store_n(&queue->sequences[index], pos + buffer_mask + 1, __ATOMIC_RELEASE);
	return true;
}