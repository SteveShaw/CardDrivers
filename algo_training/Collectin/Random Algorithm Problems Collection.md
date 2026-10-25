# Random Algorithm Problems Collection

## 470. Implement rand10() using rand7()

Given the **API** `rand7()` that generates a uniform random integer in the range `[1, 7]`, write a function `rand10()` that generates a uniform random integer in the range `[1, 10]`. You can only call the API `rand7()`, and you shouldn't call any other API. Please **do not** use a language's built-in random API.

Each test case will have one **internal** argument `n`, the number of times that your implemented function `rand10()` will be called while testing. Note that this is **not an argument** passed to `rand10()`.

**Example 1:**

**Input:** n = 1
**Output:** [2]

**Example 2:**

**Input:** n = 2
**Output:** [2,8]

**Example 3:**

**Input:** n = 3
**Output:** [3,8,10]

## 380. Insert Delete GetRandom O(1)

Implement the `RandomizedSet` class:

- `RandomizedSet()` Initializes the `RandomizedSet` object.
- `bool insert(int val)` Inserts an item `val` into the set if not present. Returns `true` if the item was not present, `false` otherwise.
- `bool remove(int val)` Removes an item `val` from the set if present. Returns `true` if the item was present, `false` otherwise.
- `int getRandom()` Returns a random element from the current set of elements (it's guaranteed that at least one element exists when this method is called). Each element must have the **same probability** of being returned.

You must implement the functions of the class such that each function works in **average** `O(1)` time complexity.

**Example 1:**

**Input**
["RandomizedSet", "insert", "remove", "insert", "getRandom", "remove", "insert", "getRandom"]
[[], [1], [2], [2], [], [1], [2], []]
**Output**
[null, true, false, true, 2, true, false, 2]
**Explanation**
RandomizedSet randomizedSet = new RandomizedSet();
randomizedSet.insert(1); // Inserts 1 to the set. Returns true as 1 was inserted successfully.
randomizedSet.remove(2); // Returns false as 2 does not exist in the set.
randomizedSet.insert(2); // Inserts 2 to the set, returns true. Set now contains [1,2].
randomizedSet.getRandom(); // getRandom() should return either 1 or 2 randomly.
randomizedSet.remove(1); // Removes 1 from the set, returns true. Set now contains [2].
randomizedSet.insert(2); // 2 was already in the set, so return false.
randomizedSet.getRandom(); // Since 2 is the only number in the set, getRandom() will always return 2.

**Constraints:**

* $-2^{31} \leq$ `val` $\leq 2^{31} - 1$

* At most $2 \ast 10^5$ calls will be made to `insert`, `remove`, and `getRandom`.

* There will be **at least one** element in the data structure when `getRandom` is called.

## 381. Insert Delete GetRandom O(1) - Duplicates Allowed

`RandomizedCollection` is a data structure that contains a collection of numbers, possibly duplicates (i.e., a multiset). It should support inserting and removing specific elements and also reporting a random element.

Implement the `RandomizedCollection` class:

- `RandomizedCollection()` Initializes the empty `RandomizedCollection` object.
- `bool insert(int val)` Inserts an item `val` into the multiset, even if the item is already present. Returns `true` if the item is not present, `false` otherwise.
- `bool remove(int val)` Removes an item `val` from the multiset if present. Returns `true` if the item is present, `false` otherwise. Note that if `val` has multiple occurrences in the multiset, we only remove one of them.
- `int getRandom()` Returns a random element from the current multiset of elements. The probability of each element being returned is **linearly related** to the number of the same values the multiset contains.

You must implement the functions of the class such that each function works on **average** `O(1)` time complexity.

**Note:** The test cases are generated such that `getRandom` will only be called if there is **at least one** item in the `RandomizedCollection`.

**Example 1:**

**Input**
["RandomizedCollection", "insert", "insert", "insert", "getRandom", "remove", "getRandom"]
[[], [1], [1], [2], [], [1], []]
**Output**
[null, true, false, true, 2, true, 1]
**Explanation**
RandomizedCollection randomizedCollection = new RandomizedCollection();
randomizedCollection.insert(1);   // return true since the collection does not contain 1.
                                  // Inserts 1 into the collection.
randomizedCollection.insert(1);   // return false since the collection contains 1.
                                  // Inserts another 1 into the collection. Collection now contains [1,1].
randomizedCollection.insert(2);   // return true since the collection does not contain 2.
                                  // Inserts 2 into the collection. Collection now contains [1,1,2].
randomizedCollection.getRandom(); // getRandom should:
                                  // - return 1 with probability 2/3, or
                                  // - return 2 with probability 1/3.
randomizedCollection.remove(1);   // return true since the collection contains 1.
                                  // Removes 1 from the collection. Collection now contains [1,2].
randomizedCollection.getRandom(); // getRandom should return 1 or 2, both equally likely.

**Constraints:**

* $-2^{31} \leq$ `val` $\leq 2^{31} - 1$
* At most $2 \star 10^5$ calls will be made to `insert`, `remove`, and `getRandom`.
* There will be **at least one** element in the data structure when `getRandom` is called.

## 382. Linked List Random Node

Given a singly linked list, return a random node's value from the linked list. Each node must have the **same probability** of being chosen.

Implement the `Solution` class:

- `Solution(ListNode head)` Initializes the object with the head of the singly-linked list `head`.
- `int getRandom()` Chooses a node randomly from the list and returns its value. All the nodes of the list should be equally likely to be chosen.

**Example 1:**

![](C:\algo_training\Collectin\images\getrand-linked-list.jpg)

**Input**
["Solution", "getRandom", "getRandom", "getRandom", "getRandom", "getRandom"]
[[[1, 2, 3]], [], [], [], [], []]
**Output**
[null, 1, 3, 2, 2, 3]
**Explanation**
Solution solution = new Solution([1, 2, 3]);
solution.getRandom(); // return 1
solution.getRandom(); // return 3
solution.getRandom(); // return 2
solution.getRandom(); // return 2
solution.getRandom(); // return 3
// getRandom() should return either 1, 2, or 3 randomly. Each element should have equal probability of returning.

**Constraints:**

* The number of nodes in the linked list will be in the range $[1, 10^4]$.
* $-10^4 \leq$ `Node.val` $\leq 10^4$
* At most $10^4$ calls will be made to `getRandom`.

**Follow up:**

- What if the linked list is extremely large and its length is unknown to you?
- Could you solve this efficiently without using extra space?

### Solution

Use **reservoir sampling**: when visiting the `i`th node, select it with probability `1/i`. This uses constant extra space and works without knowing the list’s length.

```cpp
#include <random>

class Solution {
private:
    ListNode* head;
    std::mt19937 rng{std::random_device{}()};

public:
    Solution(ListNode* head) : head(head) {}

    int getRandom() {
        int chosen = head->val;
        int count = 0;

        for (ListNode* node = head; node; node = node->next) {
            ++count;
            std::uniform_int_distribution<int> dist(1, count);

            if (dist(rng) == 1) {
                chosen = node->val;
            }
        }

        return chosen;
    }
};
```

## 384. Shuffle an Array

Given an integer array `nums`, design an algorithm to randomly shuffle the array. All permutations of the array should be **equally likely** as a result of the shuffling.

Implement the `Solution` class:

- `Solution(int[] nums)` Initializes the object with the integer array `nums`.
- `int[] reset()` Resets the array to its original configuration and returns it.
- `int[] shuffle()` Returns a random shuffling of the array.

**Example 1:**

**Input**
["Solution", "shuffle", "reset", "shuffle"]
[[[1, 2, 3]], [], [], []]
**Output**
[null, [3, 1, 2], [1, 2, 3], [1, 3, 2]]
**Explanation**
Solution solution = new Solution([1, 2, 3]);
solution.shuffle();    // Shuffle the array [1,2,3] and return its result.
                       // Any permutation of [1,2,3] must be equally likely to be returned.
                       // Example: return [3, 1, 2]
solution.reset();      // Resets the array back to its original configuration [1,2,3]. Return [1, 2, 3]
solution.shuffle();    // Returns the random shuffling of array [1,2,3]. Example: return [1, 3, 2]

**Constraints:**

* `1 <= nums.length <= 50`
* $-10^6 \leq$ `nums[i]` $\leq 10^6$
* All the elements of nums are **unique**.
* At most $10^4$ calls in total will be made to `reset` and `shuffle`.

## 398. Random Pick Index

Given an integer array `nums` with possible **duplicates**, randomly output the index of a given `target` number. You can assume that the given target number must exist in the array.

Implement the `Solution` class:

- `Solution(int[] nums)` Initializes the object with the array `nums`.
- `int pick(int target)` Picks a random index `i` from `nums` where `nums[i] == target`. If there are multiple valid i's, then each index should have an equal probability of returning.

**Example 1:**

**Input**
["Solution", "pick", "pick", "pick"]
[[[1, 2, 3, 3, 3]], [3], [1], [3]]
**Output**
[null, 4, 0, 2]
**Explanation**
Solution solution = new Solution([1, 2, 3, 3, 3]);
solution.pick(3); // It should return either index 2, 3, or 4 randomly. Each index should have equal probability of returning.
solution.pick(1); // It should return 0. Since in the array only nums[0] is equal to 1.
solution.pick(3); // It should return either index 2, 3, or 4 randomly. Each index should have equal probability of returning.

**Constraints:**

* $1$ `<= nums.length <=` $2 \ast 10^4$
* $-2^{31}$ `<= nums[i] <=` $2^{31} - 1$
* `target` is an integer from `nums`.
* At most $10^4$ calls will be made to `pick`.

### Solution

Use **reservoir sampling**: when encountering the `count`-th match, select its index with probability `1 / count`

```cpp
#include <vector>
#include <random>
using namespace std;

class Solution {
    vector<int> nums;
    mt19937 rng{random_device{}()};

public:
    Solution(vector<int>& nums) : nums(nums) {}

    int pick(int target) {
        int selected = -1;
        int count = 0;

        for (int i = 0; i < static_cast<int>(nums.size()); ++i) {
            if (nums[i] == target) {
                ++count;
                uniform_int_distribution<int> dist(1, count);
                if (dist(rng) == 1) {
                    selected = i;
                }
            }
        }

        return selected;
    }
};
```

## 497. Random Point in Non-overlapping Rectangles

You are given an array of non-overlapping axis-aligned rectangles rects where `rects[i] = [a_i, b_i, x_i, y_i]` indicates that `(a_i, b_i)` is the bottom-left corner point of the `i`-th rectangle and `(x_i, y_i)` is the top-right corner point of the ith rectangle. Design an algorithm to pick a random integer point inside the space covered by one of the given rectangles. A point on the perimeter of a rectangle is included in the space covered by the rectangle.

Any integer point inside the space covered by one of the given rectangles should be equally likely to be returned.

**Note** that an integer point is a point that has integer coordinates.

Implement the Solution class:

* `Solution(int[][] rects)` Initializes the object with the given rectangles `rects`.

* `int[] pick()` Returns a random integer point `[u, v]` inside the space covered by one of the given rectangles.

**Example 1:**

![](E:\github\CardDrivers\algo_training\Collectin\images\lc-pickrandomrec.jpg)

**Input**
["Solution", "pick", "pick", "pick", "pick", "pick"]
[[[[-2, -2, 1, 1], [2, 2, 4, 6]]], [], [], [], [], []]
**Output**
[null, [1, -2], [1, -1], [-1, -2], [-2, -2], [0, 0]]
**Explanation**
Solution solution = new Solution([[-2, -2, 1, 1], [2, 2, 4, 6]]);
solution.pick(); // return [1, -2]
solution.pick(); // return [1, -1]
solution.pick(); // return [-1, -2]
solution.pick(); // return [-2, -2]
solution.pick(); // return [0, 0]

**Constraints:**

* `1 <= rects.length <= 100`
* `rects[i].length == 4`
* $-10^9$ `<= a_i < x_i <=` $10^9$
* $-10^9$ `<= b_i < y_i <=` $10^9$
* `x_i - a_i <= 2000`
* `y_i - b_i <= 2000`
* All the rectangles do not overlap.
* At most $10^4$ calls will be made to pick.

### Solution

```cpp
class Solution {
public:
    vector<int64_t> rct_pts;
    random_device seeder;
    mt19937 rng{seeder()};
    
    vector<vector<int>> rects_;
    Solution(vector<vector<int>>& rects) {
        
        rct_pts.reserve(rects.size());

        uint64_t total = 0;
        for(auto i: views::iota(0uz, rects.size()))
        {
            const auto& rect = rects[i];
            auto w = static_cast<int64_t>(rect[2] - rect[0] + 1);
            auto h = static_cast<int64_t>(rect[3] - rect[1] + 1);
            total += w * h;
            rct_pts.push_back(total);
        }

        rects_ = std::move(rects);
    }

    ssize_t leftmost(uint64_t target)
    {
        ssize_t left = 0;
        ssize_t right = ssize(rct_pts);

        while(left < right)
        {
            auto mid = left + (right - left) / 2;
            if(rct_pts[mid] < target)
            {
                left = mid + 1;
            }
            else
            {
                right = mid;
            }
        }

        return left;
    }
    
    vector<int> pick() {
        
//we need random value between [0, total points - 1];
//total points = total area
        uniform_int_distribution<> dist(0, rct_pts.back() - 1);
        auto val = dist(rng);

        //find first index so that rct_pts[0] + ... + rct_pts[index] >= val + 1
        //because the points are starting from zero
       auto predecessor = leftmost(val + 1);

        const auto& rect = rects_[predecessor];

        auto w = rect[2] - rect[0] + 1;

        //offset means how many points are in the current rectangle `rect`
        //target value minus the total points in the previous index
        auto offset = val - (predecessor >0 ? rct_pts[predecessor-1] : 0);

        return {rect[0] + static_cast<int>(offset % w), rect[1] + static_cast<int>(offset / w)};

    }
};

/**
 * Your Solution object will be instantiated and called as such:
 * Solution* obj = new Solution(rects);
 * vector<int> param_1 = obj->pick();
 */
```

## 519. Random Flip Matrix

There is an `m x n` binary grid matrix with all the values set `0` initially. Design an algorithm to randomly pick an `index (i, j)` where `matrix[i][j] == 0` and flips it to `1`. All the `indices (i, j)` where `matrix[i][j] == 0` should be equally likely to be returned.

Optimize your algorithm to minimize the number of calls made to the **built-in** random function of your language and optimize the time and space complexity.

Implement the `Solution` class:

* `Solution(int m, int n)` Initializes the object with the size of the binary matrix `m` and `n`.

* `int[] flip()` Returns a random index `[i, j]` of the matrix where `matrix[i][j] == 0` and flips it to `1`.

* `void reset()` Resets all the values of the matrix to be 0.

**Example 1:**

**Input**
["Solution", "flip", "flip", "flip", "reset", "flip"]
[[3, 1], [], [], [], [], []]
**Output**
[null, [1, 0], [2, 0], [0, 0], null, [2, 0]]
**Explanation**
Solution solution = new Solution(3, 1);
solution.flip();  // return [1, 0], [0,0], [1,0], and [2,0] should be equally likely to be returned.
solution.flip();  // return [2, 0], Since [1,0] was returned, [2,0] and [0,0]
solution.flip();  // return [0, 0], Based on the previously returned indices, only [0,0] can be returned.
solution.reset(); // All the values are reset to 0 and can be returned.
solution.flip();  // return [2, 0], [0,0], [1,0], and [2,0] should be equally likely to be returned.

**Constraints:**

* $1$ `<= m, n <=` $10^4$
* There will be at least one free cell for each call to flip.
* At most 1000 calls will be made to flip and reset.

### Solution

Use a **lazy Fisher–Yates shuffle**: flatten the matrix into indices `[0, m*n)` and randomly remove one index per `flip()`. Instead of storing every index, use a hash map to record only positions whose values have changed.

```cpp
class Solution {
public:
int64_t total {};
int64_t cols {};
int64_t rows {};
unordered_map<int64_t, int64_t> mapping;

random_device seeder;
mt19937 rng{seeder()};

    Solution(int m, int n) {
        cols = n;
        rows = m;
        total = cols * rows;
    }
    
    vector<int> flip() {
        
        uniform_int_distribution<int64_t> dist(0, total - 1);
        auto random_index = dist(rng);

        auto sel = index;

        auto [it_index, succ] = mapping.try_emplace(random_index);

        if(!succ)
        {
            sel = it_index->second;
        }
        
        --total;

        //find if we already have mapping for total
        if(auto it = mapping.find(total); it != end(mapping))
        {
            it_index->second = it->second;
        }
        else
        {
            it_index->second = total;
        }
        return {static_cast<int>(sel / cols), static_cast<int>(sel % cols) };
    }
    
    void reset() {
        
        mapping.clear();
        total = rows * cols;
    }
};

/**
 * Your Solution object will be instantiated and called as such:
 * Solution* obj = new Solution(m, n);
 * vector<int> param_1 = obj->flip();
 * obj->reset();
 */
```

For example, the virtual array initially contains:

```
[0, 1, 2, 3, 4, 5]
```

If position `2` is selected, return index `2`, replace it with the last active value `5`, and shrink the active range:

```
[0, 1, 5, 3, 4]
```

The map only needs to store `{2 → 5}`.

**Why it is uniform:** The active range contains every unflipped index exactly once. Selecting a uniformly random position therefore selects every remaining zero with probability `1 / remaining_`.

## 528. Random Pick with Weight

You are given a 0-indexed array of positive integers `w` where `w[i]` describes the weight of the `i`-th index.

You need to implement the function `pickIndex()`, which randomly picks an index in the range `[0, w.length - 1]` (inclusive) and returns it. The probability of picking an index `i` is `w[i] / sum(w)`.

For example, if `w = [1, 3]`, the probability of picking index `0` is `1 / (1 + 3) = 0.25` (i.e., `25%`), and the probability of picking index 1 is `3 / (1 + 3) = 0.75` (i.e., `75%`).

**Example 1:**

**Input**
["Solution","pickIndex"]
[[[1]],[]]
**Output**
[null,0]
**Explanation**

```java
Solution solution = new Solution([1]); 

solution.pickIndex(); // return 0. The only option is to 
                      // return 0 since there is 
                      // only one element in w.

```

**Example 2:**

**Input**
`["Solution","pickIndex","pickIndex","pickIndex","pickIndex","pickIndex"] `

`[[[1,3]],[],[],[],[],[]]`

 **Output**
`[null,1,1,1,1,0]`

 **Explanation**

```java
Solution solution = new Solution([1, 3]);
solution.pickIndex(); // return 1. 
                      // It is returning 
                      // the second element (index = 1) 
                      // that has a probability of 3/4.
solution.pickIndex(); // return 1
solution.pickIndex(); // return 1
solution.pickIndex(); // return 1
solution.pickIndex(); // return 0. 
                      // It is returning the first element (index = 0) 
                      // that has a probability of 1/4.
                      
Since this is a randomization problem, multiple answers are allowed.
All of the following outputs can be considered correct:

[null,1,1,1,1,0]

[null,1,1,1,1,1]

[null,1,1,1,0,0]

[null,1,1,1,0,1] 

[null,1,0,1,0,0]

 ......

and so on.

```

## Solution

Similar to problem *497*

```cpp
class Solution {
public:
vector<int> acc;
random_device seeder;
mt19937 rng{seeder()};
    Solution(vector<int>& w) {
        acc.resize(size(w));
        acc[0] = w[0];
        for(auto i : std::views::iota(0uz, size(w)) | std::views::drop(1))
        {
            acc[i] = acc[i-1] + w[i];
        }
    }

    auto leftmost(int weight)
    {
        size_t L = 0;
        size_t R = size(acc);
        while(L < R)
        {
            auto mid = L + (R-L)/2;
            if(acc[mid] < weight)
            {
                L = mid + 1;
            }
            else
            {
                R = mid;
            }
        }

        return L;
    }
    
    int pickIndex() {
        
        uniform_int_distribution<int> dist(1, acc.back());
        auto random_weight = dist(rng);

        auto pre = leftmost(random_weight);
        if(cmp_equal(pre, size(acc)))
         {
            --pre;
         }

         return pre;
    }
};

/**
 * Your Solution object will be instantiated and called as such:
 * Solution* obj = new Solution(w);
 * int param_1 = obj->pickIndex();
 */
```

## 710. Random Pick with Blacklist

You are given an integer `n` and an array of **unique** integers `blacklist`. Design an algorithm to pick a random integer in the range `[0, n - 1]` that is **not** in blacklist. Any integer that is in the mentioned range and not in `blacklist` should be equally likely to be returned.

Optimize your algorithm such that it minimizes the number of calls to the **built-in** random function of your language.

Implement the Solution class:

* `Solution(int n, int[] blacklist)` Initializes the object with the integer `n` and the blacklisted integers `blacklist`.

* `int pick()` Returns a random integer in the range `[0, n - 1]` and not in `blacklist`.

**Example 1:**

**Input**
`["Solution", "pick", "pick", "pick", "pick", "pick", "pick", "pick"]`
`[[7, [2, 3, 5]], [], [], [], [], [], [], []]`
**Output**
`[null, 0, 4, 1, 6, 1, 0, 4]`
**Explanation**

```
Solution solution = new Solution(7, [2, 3, 5]);
solution.pick(); // return 0, any integer from [0,1,4,6] should be ok. Note that for every call of pick,
                 // 0, 1, 4, and 6 must be equally likely to be returned (i.e., with probability 1/4).
solution.pick(); // return 4
solution.pick(); // return 1
solution.pick(); // return 6
solution.pick(); // return 1
solution.pick(); // return 0
solution.pick(); // return 4
```

### Solution

If the `blacklist` has $m$ values, there are `bound = n - m` valid values. The algorithm should randomly chooses only from `[0, bound)` and remaps any blacklisted values in that range to valid values in `[bound, n)`

For example

```
n = 7
blacklist = [2, 3, 5]
bound = 4

Random range: [0,1,2,3]
Valid values: [0,1,4,6]
```

The mapping becomes

```
2 -> 4
3 -> 6
```

Use a `hash set` say `tail` records *balcklisted* numbers already in `[bound, n)`. These cannot be used as replacement values

Then set 

```cpp
auto w = bound
```

here, `w` searches for available replacement values starting at `bound`

For each `blacklisted` value below `bound`

```cpp
while(tail.count(w))
{
    ++w;
}
mapping[b] = w;
++w;
```

This skips blacklisted tail values and maps the low blacklisted value to tnext valid tail value

Take the above example

```
tail = [3, 5] and leaves only 2 to be mapped
so starting from bound = 4 to try
we can see 4 is not in tail
so we can directly mapping 2 to 4, i.e. dict[2] = 4
```

```cpp
class Solution {
public:
    
    unordered_map<int,int> dict;
    random_device seeder;
    int bound {};
    mt19937 rng{seeder()};
    Solution(int n, vector<int>& blacklist)
     {
        int m = static_cast<int>(ssize(blacklist));
        bound = n - m; // [0, bound) are non-blacklist values
        unordered_set<int> tail;
        for(auto b: blacklist)
        {
            if(b >= bound)
            {
                //put all backlisted values are in range [bound, n-1]
                //into tail, those values can be obtained directly
                //without mapping
                tail.insert(b);
            }
        }

        auto next_valid_value = bound;
        for(auto b: blacklist)
        {
            //check which value in [bound, n-1] that b can be mapped to
            if(b < bound)
            {
                while(tail.count(next_valid_value))
                {
                    ++next_valid_value;
                }
                
                dict.try_emplace(b, next_valid_value);
                ++next_valid_value;
            }
        }
    }
    
    int pick() {
        uniform_int_distribution<int> dist(0, bound - 1);
        auto rand_val = dist(rng);

        if(auto it = dict.find(rand_val); it != end(dict))
        {
            return it->second;
        }

        return rand_val;
    }
};

/**
 * Your Solution object will be instantiated and called as such:
 * Solution* obj = new Solution(n, blacklist);
 * int param_1 = obj->pick();
 */
```
