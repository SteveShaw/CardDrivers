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
