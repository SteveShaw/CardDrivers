Use a hash table to find entries and a doubly linked list to track recency. The front is most recently used; the back is least recently used.

```cpp
#include <cstddef>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>

class LRUCache {
    using Entry = std::pair<int, int>; // key, value
    using List = std::list<Entry>;

    std::size_t capacity_;
    List entries_;
    std::unordered_map<int, List::iterator> index_;

public:
    explicit LRUCache(std::size_t capacity)
        : capacity_(capacity) {}

    // Copying would leave the copied map pointing into the original list.
    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;

    int get(int key) {
        auto found = index_.find(key);
        if (found == index_.end())
            return -1; // Assumed missing-key result.

        entries_.splice(entries_.begin(), entries_, found->second);
        return found->second->second;
    }

    void put(int key, int value) {
        if (capacity_ == 0)
            return;

        auto found = index_.find(key);
        if (found != index_.end()) {
            found->second->second = value;
            entries_.splice(entries_.begin(), entries_, found->second);
            return;
        }

        entries_.emplace_front(key, value);
        try {
            index_.emplace(key, entries_.begin());
        } catch (...) {
            entries_.pop_front();
            throw;
        }

        if (entries_.size() > capacity_) {
            auto oldest = std::prev(entries_.end());
            index_.erase(oldest->first);
            entries_.pop_back();
        }
    }
};
```

Both operations take **average O(1)** time; space is **O(capacity)**. Hash collisions can make individual operations O(n). `splice` moves an existing list node in constant time without allocating or invalidating its iterator.

Your example produces these recency orders, most recent first:

```
put(1, 10)  → [1]
put(2, 20)  → [2, 1]
put(3, 30)  → [3, 2, 1]
get(1)      → [1, 3, 2]
put(4, 40)  → [4, 1, 3]   // evicts 2
```

**1. Why combine a hash table with a linked list?**

The hash table finds a key in average O(1). The doubly linked list moves a known entry to the front and removes the oldest entry in O(1). A list alone requires O(n) lookup; a hash table alone does not maintain recency.

**2. Why might `std::list` be undesirable for low latency?**

Its nodes are typically allocated individually and scattered in memory. This creates allocator overhead, pointer chasing, poor cache locality, and variable latency. The hash table also typically allocates nodes and occasionally rehashes.

**3. Could you implement an intrusive list?**

Yes. Embed the recency links directly in each cache entry:

```
struct Node {
    int key;
    int value;
    Node* prev = nullptr;
    Node* next = nullptr;
};

// head and tail are sentinel nodes.
void unlink(Node* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

void insertAfter(Node* position, Node* node) {
    node->prev = position;
    node->next = position->next;
    position->next->prev = node;
    position->next = node;
}
```

The map stores `Node*`. Touching an entry becomes `unlink(node)` followed by `insertAfter(&head, node)`. Eviction removes `tail.prev`. Nodes must stay at stable addresses, and their storage must outlive their list membership.

**4. How could you eliminate allocations after initialization?**

Preallocate all storage:

- Allocate `capacity` nodes in a fixed pool and maintain a free list.
- Use intrusive recency links, optionally represented as array indices.
- When full, remove the victim’s old hash mapping and reuse its node.
- Use a fixed-size hash table with preallocated buckets. For example, embed a separate hash-chain link in each pooled node.

That last design avoids separate hash-node allocations and retains average O(1) operations with a suitable hash function and bounded load factor. Calling `unordered_map::reserve()` alone is insufficient: it reserves buckets, but new entries can still allocate nodes.
