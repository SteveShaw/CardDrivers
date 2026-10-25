# Prefix 经典习题

## Prefix Sum

### 1314. Matrix Block Sum

Given a `m x n` matrix `mat` and an integer `k`, return *a matrix* `answer` *where each* `answer[i][j]` *is the sum of all elements* `mat[r][c]` *for*:

- `i - k <= r <= i + k,`
- `j - k <= c <= j + k`, and
- `(r, c)` is a valid position in the matrix.

**Example 1:**

**Input:** mat = [[1,2,3],[4,5,6],[7,8,9]], k = 1
**Output:** [[12,21,16],[27,45,33],[24,39,28]]

**Example 2:**

**Input:** mat = [[1,2,3],[4,5,6],[7,8,9]], k = 2
**Output:** [[45,45,45],[45,45,45],[45,45,45]]

#### 基本思路

解法和304类似，重点是通过 `min, max` 函数避免索引越界的技巧

```cpp
#include <vector>
#include <algorithm>
using namespace std;

class NumMatrix {
public:
    // 定义：preSum[i][j] 记录 matrix 中子矩阵 [0, 0, i-1, j-1] 的元素和
    vector<vector<int>> preSum;

    NumMatrix(vector<vector<int>>& matrix) {
        int m = matrix.size(), n = matrix[0].size();
        preSum.resize(m + 1, vector<int>(n + 1, 0));
        // 构造前缀和矩阵
        for (int i = 1; i <= m; i++) {
            for (int j = 1; j <= n; j++) {
                // 计算每个矩阵 [0, 0, i, j] 的元素和
                preSum[i][j] = preSum[i - 1][j] + preSum[i][j - 1] + matrix[i - 1][j - 1] - preSum[i - 1][j - 1];
            }
        }
    }

    // 计算子矩阵 [x1, y1, x2, y2] 的元素和
    int sumRegion(int x1, int y1, int x2, int y2) {
        // 目标矩阵之和由四个相邻矩阵运算获得
        return preSum[x2 + 1][y2 + 1] - preSum[x1][y2 + 1] - preSum[x2 + 1][y1] + preSum[x1][y1];
    }
};

class Solution {
public:
    vector<vector<int>> matrixBlockSum(vector<vector<int>>& mat, int k) {
        int m = mat.size(), n = mat[0].size();
        NumMatrix numMatrix(mat);
        vector<vector<int>> res(m, vector<int>(n, 0));
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                // 左上角的坐标
                int x1 = max(i - k, 0);
                int y1 = max(j - k, 0);
                // 右下角坐标
                int x2 = min(i + k, m - 1);
                int y2 = min(j + k, n - 1);

                res[i][j] = numMatrix.sumRegion(x1, y1, x2, y2);
            }
        }
        return res;
    }
};
```

### 724. Find Pivot Index

Given an array of integers `nums`, calculate the **pivot index** of this array.

The **pivot index** is the index where the sum of all the numbers **strictly** to the left of the index is equal to the sum of all the numbers **strictly** to the index's right.

If the index is on the left edge of the array, then the left sum is `0` because there are no elements to the left. This also applies to the right edge of the array.

Return *the **leftmost pivot index***. If no such index exists, return `-1`.

**Example 1:**

**Input:** nums = [1,7,3,6,5,6]
**Output:** 3
**Explanation:**
The pivot index is 3.
Left sum = nums[0] + nums[1] + nums[2] = 1 + 7 + 3 = 11
Right sum = nums[4] + nums[5] = 5 + 6 = 11

**Example 2:**

**Input:** nums = [1,2,3]
**Output:** -1
**Explanation:**
There is no index that satisfies the conditions in the problem statement.

**Example 3:**

**Input:** nums = [2,1,-1]
**Output:** 0
**Explanation:**
The pivot index is 0.
Left sum = 0 (no elements to the left of index 0)
Right sum = nums[1] + nums[2] = 1 + -1 = 0

#### 基本思路

根据 `prefix sum` 快速计算 `nums` 中任意位置的左侧元素和右侧元素之和。

```cpp
class Solution {
public:
    int pivotIndex(vector<int>& nums) {
        int n = nums.size();
        vector<int> preSum(n + 1, 0);
        preSum[0] = 0;
        // 计算 nums 的前缀和
        for (int i = 1; i <= n; i++) {
            preSum[i] = preSum[i - 1] + nums[i - 1];
        }
        // 根据前缀和判断左半边数组和右半边数组的元素和是否相同
        for (int i = 1; i < preSum.size(); i++) {
            // 计算 nums[i-1] 左侧和右侧的元素和
            int leftSum = preSum[i - 1] - preSum[0];
            int rightSum = preSum[n] - preSum[i];
            if (leftSum == rightSum) {
                // 相对 nums 数组，preSum 数组有一位索引偏移
                return i - 1;
            }
        }
        return -1;
    }
};
```

## Prefix Product

除了`Prefix Sum`，也可以构建`prefix product array`，两个元素相除的结果是子数组的乘积，但需要注意的是，要防止 0 作为除数。

### 238. Product of Array Except Self

Given an integer array `nums`, return *an array* `answer` *such that* `answer[i]` *is equal to the product of all the elements of* `nums` *except* `nums[i]`.

The product of any prefix or suffix of `nums` is **guaranteed** to fit in a **32-bit** integer.

You must write an algorithm that runs in `O(n)` time and without using the division operation.

**Example 1:**

**Input:** nums = [1,2,3,4]
**Output:** [24,12,8,6]

**Example 2:**

**Input:** nums = [-1,1,0,-3,3]
**Output:** [0,0,9,0,0]

#### 基本思路

构造一个 `prefix` 数组记录 `prefix product`，再用一个 `suffix` 记录 `suffix product`，根据`prefix product`和`suffix product`就能计算除了当前元素之外其他元素的积。

```cpp
class Solution {
public:
    vector<int> productExceptSelf(vector<int>& nums) {
        int n = nums.size();
        // 从左到右的前缀积，prefix[i] 是 nums[0..i] 的元素积
        vector<int> prefix(n);
        prefix[0] = nums[0];
        for (int i = 1; i < n; i++) {
            prefix[i] = prefix[i - 1] * nums[i];
        }
        // 从右到左的前缀积，suffix[i] 是 nums[i..n-1] 的元素积
        vector<int> suffix(n);
        suffix[n - 1] = nums[n - 1];
        for (int i = n - 2; i >= 0; i--) {
            suffix[i] = suffix[i + 1] * nums[i];
        }
        // 结果数组
        vector<int> res(n);
        res[0] = suffix[1];
        res[n - 1] = prefix[n - 2];
        for (int i = 1; i < n - 1; i++) {
            // 除了 nums[i] 自己的元素积就是 nums[i] 左侧和右侧所有元素之积
            res[i] = prefix[i - 1] * suffix[i + 1];
        }
        return res;
    }
};
```

### 1352. Product of the Last `K` Numbers

Design an algorithm that accepts a stream of integers and retrieves the product of the last `k` integers of the stream.

Implement the `ProductOfNumbers` class:

- `ProductOfNumbers()` Initializes the object with an empty stream.
- `void add(int num)` Appends the integer `num` to the stream.
- `int getProduct(int k)` Returns the product of the last `k` numbers in the current list. You can assume that always the current list has at least `k` numbers.

The test cases are generated so that, at any time, the product of any contiguous sequence of numbers will fit into a single 32-bit integer without overflowing.

**Example:**

**Input**
["ProductOfNumbers","add","add","add","add","add","getProduct","getProduct","getProduct","add","getProduct"]
[[],[3],[0],[2],[5],[4],[2],[3],[4],[8],[2]]
**Output**
[null,null,null,null,null,null,20,40,0,null,32]
**Explanation**
ProductOfNumbers productOfNumbers = new ProductOfNumbers();
productOfNumbers.add(3);        // [3]
productOfNumbers.add(0);        // [3,0]
productOfNumbers.add(2);        // [3,0,2]
productOfNumbers.add(5);        // [3,0,2,5]
productOfNumbers.add(4);        // [3,0,2,5,4]
productOfNumbers.getProduct(2); // return 20. The product of the last 2 numbers is 5 * 4 = 20
productOfNumbers.getProduct(3); // return 40. The product of the last 3 numbers is 2 * 5 * 4 = 40
productOfNumbers.getProduct(4); // return 0. The product of the last 4 numbers is 0 * 2 * 5 * 4 = 0
productOfNumbers.add(8);        // [3,0,2,5,4,8]
productOfNumbers.getProduct(2); // return 32. The product of the last 2 numbers is 4 * 8 = 32

**Follow-up:** Can you implement **both** `GetProduct` and `Add` to work in `O(1)` time complexity instead of `O(k)` time complexity?

#### 基本思路

利用`prefix product` 只不过乘积中如果有 0 需要特殊处理。

```cpp
#include <vector>

class ProductOfNumbers {
    // 前缀积数组
    // preProduct[i] / preProduct[j] 就是 [i, j] 之间的元素积
    vector<int> preProduct;

public:
    ProductOfNumbers() {
        // 初始化放一个 1，便于计算后续添加元素的乘积
        preProduct.push_back(1);
    }

    void add(int num) {
        if (num == 0) {
            // 如果添加的元素是 0，则前面的元素积都废了
            preProduct.clear();
            preProduct.push_back(1);
            return;
        }
        int n = preProduct.size();
        // 前缀积数组中每个元素
        preProduct.push_back(preProduct[n - 1] * num);
    }

    int getProduct(int k) {
        int n = preProduct.size();
        if (k > n - 1) {
            // 不足 k 个元素，是因为最后 k 个元素存在 0
            return 0;
        }
        // 计算最后 k 个元素积
        return preProduct[n - 1] / preProduct[n - k - 1];
    }
};
```

## Prefix Sum And Hash Table

如果需要在 `nums` 中寻找和为 `target` 的子数组，不仅需要 `prefix sum array`, 还需要`hash table` 来快速定位

```cpp
// 前缀和到索引的映射
HashMap<Integer, Integer> valToIndex;
// 一般我们让 preSum 比 nums 数组多一位，因为第一位要放 0
int[] preSum = new int[nums.length + 1];

for (int j = 0; j < preSum.length; j++) {
    valToIndex.put(preSum[j], j);
}

for (int i = 1; i < preSum.length; i++) {
    int need = preSum[i] - target;
    if (valToIndex.containsKey(need)) {
        // nums[valToIndex.get(need).. i-1] 就是和为 target 的子数组
        // 注意 preSum 和 nums 有一位索引偏移，所以是 i-1
    }
}
```

### 525. Contiguous Array

Given a binary array `nums`, return *the maximum length of a contiguous subarray with an equal number of* `0` *and* `1`.

**Example 1:**

**Input:** nums = [0,1]
**Output:** 2
**Explanation:** [0, 1] is the longest contiguous subarray with an equal number of 0 and 1.

**Example 2:**

**Input:** nums = [0,1,0]
**Output:** 2
**Explanation:** [0, 1] (or [1, 0]) is a longest contiguous subarray with equal number of 0 and 1.

**Example 3:**

**Input:** nums = [0,1,1,1,1,1,0,0,0]
**Output:** 6
**Explanation:** [1,1,1,0,0,0] is the longest contiguous subarray with equal number of 0 and 1.

#### 基本思路

首先，**如果把 `0` 视作 `-1`，就把题目转变成了：寻找和为 0 的最长子数组**。

涉及到和为 `xxx` 的子数组，就是要考察 `prefix sum` 和 `hash table`的组合使用了。

求和为 0 的最长子数组，相当于在 `preSum` 数组中找 `i, j`，使得 `preSum[i] - preSum[j] == 0`，其中 `i > j` 且 `i - j` 要尽可能大。

那么用一个`hash table` `valToIndex` 记录`prefix sum`到`index`的映射，给定任意 `preSum[i]`，都能通过 `valToIndex` 快速判断是否存在 `j`，使得 `preSum[i] - preSum[j] == 0`。

下面的代码实现中 `preSum` 数组可以进一步简化成单一变量。

```cpp
#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
public:
    int findMaxLength(vector<int>& nums) {
        int n = nums.size();
        vector<int> preSum(n + 1, 0);
        // 计算 nums 的前缀和
        for (int i = 0; i < n; i++) {
            preSum[i + 1] = preSum[i] + (nums[i] == 0 ? -1 : 1);
        }
        // 前缀和到索引的映射，方便快速查找所需的前缀和
        unordered_map<int, int> valToIndex;
        int res = 0;
        for (int i = 0; i < preSum.size(); i++) {
            // 如果这个前缀和还没有对应的索引，说明这个前缀和第一次出现，记录下来
            if (valToIndex.find(preSum[i]) == valToIndex.end()) {
                valToIndex[preSum[i]] = i;
            } else {
                // 这个前缀和已经出现过了，则找到一个和为 0 的子数组
                res = max(res, i - valToIndex[preSum[i]]);
            }
            // 因为题目想找长度最大的子数组，所以前缀和索引应尽可能小
        }
        return res;
    }
};
```

### 523. Continuous Subarray Sum

Given an integer array nums and an integer k, return `true` *if* `nums` *has a **good subarray** or* `false` *otherwise*.

A **good subarray** is a subarray where:

- its length is **at least two**, and
- the sum of the elements of the subarray is a multiple of `k`.

**Note** that:

- A **subarray** is a contiguous part of the array.
- An integer `x` is a multiple of `k` if there exists an integer `n` such that `x = n * k`. `0` is **always** a multiple of `k`.

**Example 1:**

**Input:** nums = [23,<u style="border: 0px solid rgba(0, 0, 0, 0.08); box-sizing: border-box; overflow-wrap: break-word; --tw-border-spacing-x: 0; --tw-border-spacing-y: 0; --tw-translate-x: 0; --tw-translate-y: 0; --tw-rotate: 0; --tw-skew-x: 0; --tw-skew-y: 0; --tw-scale-x: 1; --tw-scale-y: 1; --tw-pan-x: ; --tw-pan-y: ; --tw-pinch-zoom: ; --tw-scroll-snap-strictness: proximity; --tw-gradient-from-position: ; --tw-gradient-via-position: ; --tw-gradient-to-position: ; --tw-ordinal: ; --tw-slashed-zero: ; --tw-numeric-figure: ; --tw-numeric-spacing: ; --tw-numeric-fraction: ; --tw-ring-inset: ; --tw-ring-offset-width: 0px; --tw-ring-offset-color: #fff; --tw-ring-color: #3b82f680; --tw-ring-offset-shadow: 0 0 #0000; --tw-ring-shadow: 0 0 #0000; --tw-shadow: 0 0 #0000; --tw-shadow-colored: 0 0 #0000; --tw-blur: ; --tw-brightness: ; --tw-contrast: ; --tw-grayscale: ; --tw-hue-rotate: ; --tw-invert: ; --tw-saturate: ; --tw-sepia: ; --tw-drop-shadow: ; --tw-backdrop-blur: ; --tw-backdrop-brightness: ; --tw-backdrop-contrast: ; --tw-backdrop-grayscale: ; --tw-backdrop-hue-rotate: ; --tw-backdrop-invert: ; --tw-backdrop-opacity: ; --tw-backdrop-saturate: ; --tw-backdrop-sepia: ;">2,4</u>,6,7], k = 6
**Output:** true
**Explanation:** [2, 4] is a continuous subarray of size 2 whose elements sum up to 6.

**Example 2:**

**Input:** nums = [<u style="border: 0px solid rgba(0, 0, 0, 0.08); box-sizing: border-box; overflow-wrap: break-word; --tw-border-spacing-x: 0; --tw-border-spacing-y: 0; --tw-translate-x: 0; --tw-translate-y: 0; --tw-rotate: 0; --tw-skew-x: 0; --tw-skew-y: 0; --tw-scale-x: 1; --tw-scale-y: 1; --tw-pan-x: ; --tw-pan-y: ; --tw-pinch-zoom: ; --tw-scroll-snap-strictness: proximity; --tw-gradient-from-position: ; --tw-gradient-via-position: ; --tw-gradient-to-position: ; --tw-ordinal: ; --tw-slashed-zero: ; --tw-numeric-figure: ; --tw-numeric-spacing: ; --tw-numeric-fraction: ; --tw-ring-inset: ; --tw-ring-offset-width: 0px; --tw-ring-offset-color: #fff; --tw-ring-color: #3b82f680; --tw-ring-offset-shadow: 0 0 #0000; --tw-ring-shadow: 0 0 #0000; --tw-shadow: 0 0 #0000; --tw-shadow-colored: 0 0 #0000; --tw-blur: ; --tw-brightness: ; --tw-contrast: ; --tw-grayscale: ; --tw-hue-rotate: ; --tw-invert: ; --tw-saturate: ; --tw-sepia: ; --tw-drop-shadow: ; --tw-backdrop-blur: ; --tw-backdrop-brightness: ; --tw-backdrop-contrast: ; --tw-backdrop-grayscale: ; --tw-backdrop-hue-rotate: ; --tw-backdrop-invert: ; --tw-backdrop-opacity: ; --tw-backdrop-saturate: ; --tw-backdrop-sepia: ;">23,2,6,4,7</u>], k = 6
**Output:** true
**Explanation:** [23, 2, 6, 4, 7] is an continuous subarray of size 5 whose elements sum up to 42.
42 is a multiple of 6 because 42 = 7 * 6 and 7 is an integer.

**Example 3:**

**Input:** nums = [23,2,6,4,7], k = 13
**Output:** false

#### 基本思路

**寻找 `i, j` 使得 `(preSum[i] - preSum[j]) % k == 0` 且 `i - j >= 2`**。

另外，`(preSum[i] - preSum[j]) % k == 0` 其实就是 `preSum[i] % k == preSum[j] % k`。

利用`hash table`记录 `preSum[j] % k` 的值以及对应的索引，就可以迅速判断 `preSum[i]` 是否符合条件了。

```cpp
#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
public:
    bool checkSubarraySum(vector<int>& nums, int k) {
        int n = nums.size();
        // 计算 nums 的前缀和
        vector<int> preSum(n + 1, 0);
        for (int i = 1; i <= n; i++) {
            preSum[i] = preSum[i - 1] + nums[i - 1];
        }
        // 前缀和与 k 的余数到索引的映射，方便快速查找所需的前缀和
        unordered_map<int, int> valToIndex;
        for (int i = 0; i < preSum.size(); i++) {
            // 在哈希表中记录余数
            int val = preSum[i] % k;
            // 如果这个余数还没有对应的索引，则记录下来
            if (!valToIndex.count(val)) {
                valToIndex[val] = i;
            }
            // 如果这个前缀和已经有对应的索引了，则什么都不做
            // 因为题目想找长度最大的子数组，所以前缀和索引应尽可能小
        }
        for (int i = 1; i < preSum.size(); i++) {
            // 计算 need，使得 (preSum[i] - need) % k == 0
            int need = preSum[i] % k;
            if (valToIndex.count(need)) {
                if (i - valToIndex[need] >= 2) {
                    // 这个子数组的长度至少为 2
                    return true;
                }
            }
        }
        return false;
    }
};
```

### 560. Subarray Sum Equals `K`

Given an array of integers `nums` and an integer `k`, return *the total number of subarrays whose sum equals to* `k`.

A subarray is a contiguous **non-empty** sequence of elements within an array.

**Example 1:**

**Input:** nums = [1,1,1], k = 2
**Output:** 2

**Example 2:**

**Input:** nums = [1,2,3], k = 3
**Output:** 2

#### 基本思路

本题和*523*，*525*的最大区别在于，需要在维护 `prefix sum` 的同时动态维护 `count` 映射，而不能等到 `prefix sum` 计算完成后再处理 `count`，因为 `count[need]` 应该维护 `preSum[0..i]` 中值为 `need` 的元素个数。

```cpp
#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
public:
    int subarraySum(vector<int>& nums, int k) {
        int n = nums.size();
        // 前缀和数组
        vector<int> preSum(n + 1, 0);
        // 前缀和到该前缀和出现次数的映射，方便快速查找所需的前缀和
        unordered_map<int, int> count;
        count[0] = 1;
        // 记录和为 k 的子数组个数
        int res = 0;

        // 计算 nums 的前缀和
        for (int i = 1; i <= n; i++) {
            preSum[i] = preSum[i - 1] + nums[i - 1];
            // 如果之前存在值为 need 的前缀和
            // 说明存在以 nums[i-1] 结尾的子数组的和为 k
            int need = preSum[i] - k;
            if (count.find(need) != count.end()) {
                res += count[need];
            }
            // 将当前前缀和存入哈希表
            count[preSum[i]]++;
        }
        return res;
    }
};
```

#### Note

上述几个问题的代码中是保留 `prefix sum array`的，实际上整个算法流程一直在访问最新的 `preSum[i]`，根本不需要 `i` 之前的`prefix sum`，所以实际上根本不需要`prefix sum array`，只要维护一个变量记录最新的`prefix sum`就行了。

### 1124. Longest Well-Performing Interval

We are given `hours`, a list of the number of hours worked per day for a given employee.

A day is considered to be a *tiring day* if and only if the number of hours worked is (strictly) greater than `8`.

A *well-performing interval* is an interval of days for which the number of tiring days is strictly larger than the number of non-tiring days.

Return the length of the longest well-performing interval.

**Example 1:**

**Input:** hours = [9,9,6,0,6,6,9]
**Output:** 3
**Explanation:** The longest well-performing interval is [9,9,6].

**Example 2:**

**Input:** hours = [6,6,6]
**Output:** 0

#### 基本思路

题目说 `hours[i]` 以 8 作为分界线，**那么就要对数据进行「归一化」处理**，比如把所有大于 8 的元素视为 +1，把所有小于 8 的元素视为 -1，这样一来，这道题就变成了：计算数组中元素和大于 0 的子数组的最大长度。

同样借助`hash table`，快速寻找一个 `j` 使得 `preSum[i] - preSum[j] > 0` 。

```cpp
#include <vector>
#include <unordered_map>
using namespace std;

class Solution {
public:
    int longestWPI(vector<int>& hours) {
        int n = hours.size();
        vector<int> preSum(n + 1, 0);
        // 前缀和到索引的映射，方便快速查找所需的前缀和
        unordered_map<int, int> valToIndex;
        int res = 0;
        for (int i = 1; i <= n; i++) {
            // 计算 nums[0..i-1] 的前缀和
            preSum[i] = preSum[i - 1] + (hours[i - 1] > 8 ? 1 : -1);
            // 如果这个前缀和还没有对应的索引，说明这个前缀和第一次出现，记录下来
            if (!valToIndex.count(preSum[i])) {
                valToIndex[preSum[i]] = i;
            } else {
                // 因为题目想找长度最大的子数组，valToIndex 中的索引应尽可能小，
                // 所以这里什么都不做
            }

            // 现在我们想找 hours[0..i-1] 中元素和大于 0 的子数组
            // 这就要根据 preSum[i] 的正负分情况讨论了
            if (preSum[i] > 0) {
                // preSum[i] 为正，说明 hours[0..i-1] 都是「表现良好的时间段」
                res = max(res, i);
            } else {
                // preSum[i] 为负，需要寻找一个 j 使得 preSum[i] - preSum[j] > 0
                // 考虑到我们的 preSum 数组每两个相邻元素的差的绝对值都是 1 且 j 应该尽可能小，
                // 那么只要找到 preSum[j] == preSum[i] - 1，nums[j+1..i] 就是一个「表现良好的时间段」
                if (valToIndex.count(preSum[i] - 1)) {
                    int j = valToIndex[preSum[i] - 1];
                    res = max(res, i - j);
                }
            }
        }
        return res;
    }
};
```

### 974. Subarray Sums Divisible By `K`

Given an integer array `nums` and an integer `k`, return *the number of non-empty **subarrays** that have a sum divisible by* `k`.

A **subarray** is a **contiguous** part of an array.

**Example 1:**

**Input:** nums = [4,5,0,-2,-3,1], k = 5
**Output:** 7
**Explanation:** There are 7 subarrays with a sum divisible by k = 5:
[4, 5, 0, -2, -3, 1], [5], [5, 0], [5, 0, -2, -3], [0], [0, -2, -3], [-2, -3]

**Example 2:**

**Input:** nums = [5], k = 9
**Output:** 0

#### 基本思路

同样基于`prefix sum`和`hash table`，但是在这里哈希表中不是记录`prefix sum`到`index`的映射，而是记录`prefix sum`的余数到频率的映射。

假设子数组 `nums[i..j]` 中的元素之和能被 `k` 整除，即 `sum(nums[i..j]) % k == 0`，也可以理解为：

```cpp
   sum(nums[0..i]) % k == sum(nums[0..j]) % k
=> (sum(nums[0..i]) - sum(nums[0..j])) % k == 0
=> sum(nums[i..j]) % k == 0
```

```cpp
class Solution {
public:
    int subarraysDivByK(vector<int>& nums, int k) {
        int n = nums.size();
        // nums 的前缀和数组，注意索引偏移，preSum[i] 的值为 sum(nums[0..i-1])
        vector<int> preSum(n + 1);
        // 前缀和余数的计数器，方便快速查找所需的前缀和余数的数量
        unordered_map<int, int> remainderToCount;
        preSum[0] = 0;
        remainderToCount[0] = 1;
        // 计算 nums 的前缀和余数并更新计数器
        int res = 0;
        for (int i = 0; i < n; i++) {
            // 计算 nums[0..i] 的前缀和
            preSum[i + 1] = preSum[i] + nums[i];
            // nums[0..i] 的所有元素之和与 k 的余数
            int curRemainder = preSum[i + 1] % k;
            if (curRemainder < 0) {
                // 考虑到 preSum[i + 1] 可能是负数， 但我们实际想要正余数 1，所以这里需要对 curRemainder 进行调整
                curRemainder += k;
            }
            // 看看之前 nums[0..i-1] 中是否也存在前缀和余数为 curRemainder 的子数组
            if (remainderToCount.find(curRemainder) != remainderToCount.end()) {
                // 如果存在，则说明找到了可以整除 k 的子数组，累加子数组数量
                int count = remainderToCount[curRemainder];
                res += count;
                remainderToCount[curRemainder] = count + 1;
            } else {
                // 如果不存在，那么 nums[0..i] 是第一个前缀和余数为 curRemainder 的子数组
                remainderToCount[curRemainder] = 1;
            }
        }
        return res;
    }
};
```
