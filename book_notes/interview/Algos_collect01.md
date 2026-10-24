# 981. Time Based Key-Value Store

Design a time-based key-value data structure that can store multiple values for the same key at different time stamps and retrieve the key's value at a certain timestamp.

Implement the `TimeMap` class:

- `TimeMap()` Initializes the object of the data structure.
- `void set(String key, String value, int timestamp)` Stores the key `key` with the value `value` at the given time `timestamp`.
- `String get(String key, int timestamp)` Returns a value such that `set` was called previously, with `timestamp_prev <= timestamp`. If there are multiple such values, it returns the value associated with the largest `timestamp_prev`. If there are no values, it returns `""`.

**Example 1:**

**Input**
["TimeMap", "set", "get", "get", "set", "get", "get"]
[[], ["foo", "bar", 1], ["foo", 1], ["foo", 3], ["foo", "bar2", 4], ["foo", 4], ["foo", 5]]
**Output**
[null, null, "bar", "bar", null, "bar2", "bar2"]

**Explanation**
TimeMap timeMap = new TimeMap();
timeMap.set("foo", "bar", 1);  // store the key "foo" and value "bar" along with timestamp = 1.
timeMap.get("foo", 1);         // return "bar"
timeMap.get("foo", 3);         // return "bar", since there is no value corresponding to foo at timestamp 3 and timestamp 2, then the only value is at timestamp 1 is "bar".
timeMap.set("foo", "bar2", 4); // store the key "foo" and value "bar2" along with timestamp = 4.
timeMap.get("foo", 4);         // return "bar2"
timeMap.get("foo", 5);         // return "bar2"

**Constraints:**

- `1 <= key.length, value.length <= 100`
- `key` and `value` consist of lowercase English letters and digits.
- `1 <= timestamp <= 10^7`
- All the timestamps `timestamp` of `set` are strictly increasing.
- At most `2 * 10^5` calls will be made to `set` and `get`.

## Solution

```cpp
string get(string key, int timestamp) {
    auto found = data.find(key);
    if (found == data.end()) return "";

    const auto& entries = found->second;

    auto it = upper_bound(
        entries.begin(), entries.end(), timestamp,
        [](int target, const pair<int, string>& entry) {
            return target < entry.first;
        }
    );

    if (it == entries.begin()) return "";
    return prev(it)->second;
}
```

For a sorted range:

| Function      | Returns the first element |
| ------------- | ------------------------- |
| `lower_bound` | **≥ target**              |
| `upper_bound` | **> target**              |

For example, with `[1, 4, 4, 7]` and target `4`:

- `lower_bound` points to the first `4` (index 1).
- `upper_bound` points to `7` (index 3).

Both return `end()` if no matching position exists.

For **TimeMap**, we want the last timestamp **≤ target**, so use `upper_bound` and move back one position:

```
string get(string key, int timestamp) {
    auto found = data.find(key);
    if (found == data.end()) return "";

    const auto& entries = found->second;

    auto it = upper_bound(
        entries.begin(), entries.end(), timestamp,
        [](int target, const pair<int, string>& entry) {
            return target < entry.first;
        }
    );

    if (it == entries.begin()) return "";
    return prev(it)->second;
}
```

Include `<algorithm>` for `upper_bound` and `<iterator>` for `prev`.

The comparator argument order differs:

```
// lower_bound: comparator(element, target)
entry.first < target

// upper_bound: comparator(target, element)
target < entry.first
```

The argument order differs because the two algorithms test different sides of the boundary using a **strict less-than comparison**.

**`lower_bound`** finds the first element **≥ target**. It skips elements while:

```
comp(element, target)  // element < target
```

**`upper_bound`** finds the first element **> target**. It looks for where this becomes true:

```
comp(target, element)  // target < element
```

Consider an element equal to the target:

- `element < target` is false → `lower_bound` can stop here.
- `target < element` is false → `upper_bound` must move past it.

Reversing the arguments lets both algorithms use the same strict ordering (`<`) while handling equality differently. It also matters when the element and target have different types, as in your `TimeMap`: the element is a pair, but the target is an integer.

### From cppreference

#### upper_bound

1) The order is determined by `operator<`(until C++20)`std::less{}`(since C++20).

2) If the elements `e` of the source range are not [partitioned](https://en.cppreference.com/cpp/algorithm#Requirements "cpp/algorithm") with respect to the expression `!bool(value < e)`(until C++20)`!bool(std::less{}(value, e))`(since C++20), the behavior is undefined.

3) The order is determined by `comp`.
   
   If the elements `e` of the source range are not partitioned with respect to the expression `!bool(comp(value, e))`, the behavior is undefined.

#### lower_bound

1) The order is determined by `operator<`(until C++20)`std::less{}`(since C++20).
   
   If the elements `e` of the source range are not [partitioned](https://en.cppreference.com/cpp/algorithm#Requirements "cpp/algorithm") with respect to the expression `bool(e < value)`(until C++20)`bool(std::less{}(e, value))`(since C++20), the behavior is undefined.

2) The order is determined by `comp`.
   
   If the elements `e` of the source range are not partitioned with respect to the expression `bool(comp(e, value))`, the behavior is undefined.

# 347. Top K Elements

Given an integer array `nums` and an integer `k`, return *the* `k` *most frequent elements*. You may return the answer in **any order**.

**Example 1:**

**Input:** nums = [1,1,1,2,2,3], k = 2

**Output:** [1,2]

**Example 2:**

**Input:** nums = [1], k = 1

**Output:** [1]

**Example 3:**

**Input:** nums = [1,2,1,2,1,2,3,1,3,2], k = 2

**Output:** [1,2]

**Constraints:**

- `1 <= nums.length <= 10^5`
- `-10^4 <= nums[i] <= 10^4`
- `k` is in the range `[1, the number of unique elements in the array]`.
- It is **guaranteed** that the answer is **unique**.

## Solution

```cpp
    vector<int> topKFrequent(vector<int>& nums, int k) {

    unordered_map<int, int> m;
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> q;
    vector<int> res;
    for( auto a : nums )
    {
        ++m[a];
    }
    //add to heap
    for( auto it : m )
    {
        q.push( {it.second, it.first} );
        if( ( int )q.size() > k )
        {
            q.pop();
        }
    }
    //output first k largest
    for( int i = 0; i < k; ++i )
    {
        res.push_back( q.top().second );
        q.pop();
    }
    reverse( begin( res ), end( res ) );
    return res;        
    }
};
```

# 146. LRU Cache

Design a data structure that follows the constraints of a **[Least Recently Used (LRU) cache](https://en.wikipedia.org/wiki/Cache_replacement_policies#LRU)**.

Implement the `LRUCache` class:

- `LRUCache(int capacity)` Initialize the LRU cache with **positive** size `capacity`.
- `int get(int key)` Return the value of the `key` if the key exists, otherwise return `-1`.
- `void put(int key, int value)` Update the value of the `key` if the `key` exists. Otherwise, add the `key-value` pair to the cache. If the number of keys exceeds the `capacity` from this operation, **evict** the least recently used key.

The functions `get` and `put` must each run in `O(1)` average time complexity.

**Example 1:**

**Input**
["LRUCache", "put", "put", "get", "put", "get", "put", "get", "get", "get"]
[[2], [1, 1], [2, 2], [1], [3, 3], [2], [4, 4], [1], [3], [4]]
**Output**
[null, null, null, 1, null, -1, null, -1, 3, 4]

**Explanation**
LRUCache lRUCache = new LRUCache(2);
lRUCache.put(1, 1); // cache is {1=1}
lRUCache.put(2, 2); // cache is {1=1, 2=2}
lRUCache.get(1);    // return 1
lRUCache.put(3, 3); // LRU key was 2, evicts key 2, cache is {1=1, 3=3}
lRUCache.get(2);    // returns -1 (not found)
lRUCache.put(4, 4); // LRU key was 1, evicts key 1, cache is {4=4, 3=3}
lRUCache.get(1);    // return -1 (not found)
lRUCache.get(3);    // return 3
lRUCache.get(4);    // return 4

**Constraints:**

* `1<= capacity <= 3000`
* $0\leq$ `key` $\leq 10^4$
* $0\leq$ `value` $\leq 10^5$
* At most $2 \times 10^5$ calls will be made to `get` and `put`.

## Solution

```cpp
#include <list>
#include <unordered_map>
#include <utility>

class LRUCache {
private:
    int capacity;

    // Front = most recently used, back = least recently used.
    std::list<std::pair<int, int>> items;
    std::unordered_map<
        int,
        std::list<std::pair<int, int>>::iterator
    > cache;

public:
    LRUCache(int capacity) : capacity(capacity) {}

    int get(int key) {
        auto it = cache.find(key);
        if (it == cache.end()) {
            return -1;
        }

        // Move the existing node to the front.
        items.splice(items.begin(), items, it->second);
        return it->second->second;
    }

    void put(int key, int value) {
        auto it = cache.find(key);

        if (it != cache.end()) {
            it->second->second = value;
            items.splice(items.begin(), items, it->second);
            return;
        }

        items.emplace_front(key, value);
        cache[key] = items.begin();

        if (items.size() > capacity) {
            cache.erase(items.back().first);
            items.pop_back();
        }
    }
};
```
