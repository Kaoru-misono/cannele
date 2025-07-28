#pragma once

#include "idiom.hpp"

#include <new>
#include <atomic>
#include <memory>
#include <expected>

namespace cannele::inline core
{
    template<typename T>
	struct HeapAllocator
	{
        CNE_MOVE_ONLY(HeapAllocator);

		alignas(std::hardware_destructive_interference_size) std::atomic<int64_t> counter{0};

		using ValueType = T;

        HeapAllocator() = default;
		~HeapAllocator()
		{
			assert(counter.load(std::memory_order_relaxed) == 0);
		}

        static auto trace_malloc(size_t size) -> void*
        {
            if (size == 0) {
                // avoid std::malloc(0) which may return nullptr on success
                ++size;
            }

            if (auto ptr = std::malloc(size)) {
                // TODO: Trace.
                return ptr;
            }
            throw std::bad_alloc{};
        }

        static auto trace_free(void* ptr, size_t size) -> void
        {
            // TODO: Trace.
            std::free(ptr);
        }

		inline auto allocate() -> void* // new allocate() T;
		{
			counter.fetch_add(1, std::memory_order_acq_rel);
			return reinterpret_cast<T*>(trace_malloc(sizeof(T)));
		}

		inline auto free(T* node) -> void // ptr->~T(); free(ptr);
		{
			trace_free(reinterpret_cast<void*>(node), sizeof(T));
			counter.fetch_sub(1, std::memory_order_acq_rel);
		}

	};

    template<typename T>
    struct MPSCQueueNode
    {
        T data{}; // Temp store data, use move semantic to dequeue/enqueue.
        std::atomic<MPSCQueueNode*> next{};
    };

    template<typename T>
    using MPSCQueueHeapAllocator = HeapAllocator<MPSCQueueNode<T>>;

    #define CPU_CACHELINE_SIZE_ALIGNAS alignas(std::hardware_destructive_interference_size)
    // http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
	template<typename T, typename Allocator>
	struct MPSCQueue
	{
        CNE_MOVE_ONLY(MPSCQueue);

        using Node = MPSCQueueNode<T>;
        static_assert(sizeof(Node) == sizeof(typename Allocator::ValueType));

        CPU_CACHELINE_SIZE_ALIGNAS std::atomic<Node*> enqueue_pos{};
        CPU_CACHELINE_SIZE_ALIGNAS Node* dequeue_pos{};
        CPU_CACHELINE_SIZE_ALIGNAS std::atomic<int64_t> node_count{0};
        CPU_CACHELINE_SIZE_ALIGNAS std::unique_ptr<Allocator> allocator{};

        auto free_node(Node* node) -> void
        {
            node->~Node();
            allocator->free(node);
        }

        auto allocate_node() -> Node*
        {
            return new (allocator->allocate()) Node;
        }

		MPSCQueue()  // Consumer thread.
        {
            allocator = std::make_unique<Allocator>();

            dequeue_pos = allocate_node();
            dequeue_pos->next.store(nullptr, std::memory_order_relaxed);

            enqueue_pos.store(dequeue_pos, std::memory_order_relaxed);
        }

        ~MPSCQueue()
        {
            auto temp = T{};
            while (dequeue(temp)) {}

            assert(enqueue_pos.load(std::memory_order_relaxed) == dequeue_pos);
            free_node(dequeue_pos);
        }

        auto enqueue(T&& input) -> void // Producer threads.
        {
            // Multi thread may modify m_enqueuePos, so we need to use acq_rel semantic.
            node_count++;
            auto node = allocate_node();

            node->data = std::move(input); // Move semantic.
            node->next.store(nullptr, std::memory_order_release);

			// memory_order_seq_cst ensure node data flush before exchange.
			auto prev = enqueue_pos.exchange(node, std::memory_order_seq_cst);

            // Update store.
            prev->next.store(node, std::memory_order_release);
        }

        auto empty() const -> bool
        {
            return node_count == 0;
        }

		// Get dequeue node data but not dequeue it.
        auto dequeue_data(T& output) const -> bool
        {
            auto dequeueNode = dequeue_pos;
            auto next = dequeueNode->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                // No update yet or empty.
                return false;
            }

            output = next->data;
            return true;
        }

        auto dequeue(T& output) -> bool // Consumer thread.
        {
            auto dequeueNode = dequeue_pos;
            auto next = dequeueNode->next.load(std::memory_order_acquire);
            if (next == nullptr)     {
                // No update yet or empty.
                return false;
            }

            node_count--;

            // Move semantic.
            output = std::move(next->data);

			// dequeue node update for next time dequeue.
            dequeue_pos = next;

            // Clean this.
            free_node(dequeueNode);

            return true;
        }
	};
    #undef CPU_CACHELINE_SIZE_ALIGNAS
}
