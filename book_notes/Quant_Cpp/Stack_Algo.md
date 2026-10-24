You are given an *absolute* path for a Unix-style file system, which always begins with a slash `'/'`. Your task is to transform this absolute path into its **simplified canonical path**.

The *rules* of a Unix-style file system are as follows:

- A single period `'.'` represents the current directory.
- A double period `'..'` represents the previous/parent directory.
- Multiple consecutive slashes such as `'//'` and `'///'` are treated as a single slash `'/'`.
- Any sequence of periods that does **not match** the rules above should be treated as a **valid directory or** **file** **name**. For example, `'...'` and `'....'` are valid directory or file names.

The simplified canonical path should follow these *rules*:

- The path must start with a single slash `'/'`.
- Directories within the path must be separated by exactly one slash `'/'`.
- The path must not end with a slash `'/'`, unless it is the root directory.
- The path must not have any single or double periods (`'.'` and `'..'`) used to denote current or parent directories.

Return the **simplified canonical path**.

**Example 1:**

**Input:** path = "/home/"

**Output:** "/home"

**Explanation:**

The trailing slash should be removed.

**Example 2:**

**Input:** path = "/home//foo/"

**Output:** "/home/foo"

**Explanation:**

Multiple consecutive slashes are replaced by a single one.

**Example 3:**

**Input:** path = "/home/user/Documents/../Pictures"

**Output:** "/home/user/Pictures"

**Explanation:**

A double period `".."` refers to the directory up a level (the parent directory).

**Example 4:**

**Input:** path = "/../"

**Output:** "/"

**Explanation:**

Going one level up from the root directory is not possible.

**Example 5:**

**Input:** path = "/.../a/../b/c/../d/./"

**Output:** "/.../b/d"

**Explanation:**

`"..."` is a valid name for a directory in this problem.

**Constraints:**

- `1 <= path.length <= 3000`
- `path` consists of English letters, digits, period `'.'`, slash `'/'` or `'_'`.
- `path` is a valid absolute Unix path.

## Solution

```cpp
#include <string>
#include <vector>
#include <sstream>

class Solution {
public:
    string simplifyPath(string path) {
        vector<string> parts;
        istringstream ss(path);
        string part;

        // 借助栈计算最终的文件夹路径
        while (getline(ss, part, '/')) {
            if (part.empty() || part == ".") {
                // Skip empty parts and current directory symbol.
                continue;
            }
            if (part == "..") {
                // Go up one directory (pop from the stack) unless the stack is empty.
                if (!parts.empty()) parts.pop_back();
            } else {
                // Add the non-empty and non-".." part to the stack.
                parts.push_back(part);
            }
        }

        // 栈中存储的文件夹组成路径
        string res;
        for (const auto& p : parts) {
            res += "/" + p;
        }

        // If the result is empty, it means the path is root directory.
        return res.empty() ? "/" : res;
    }
};
```

# 143 Reorder List

You are given the head of a singly linked-list. The list can be represented as:

$L_0 \rightarrow L_1 \rightarrow ... \rightarrow L_n - 1 \rightarrow L_n$

*Reorder the list to be on the following form:*

L0 → Ln → L1 → Ln - 1 → L2 → Ln - 2 → …

You may not modify the values in the list's nodes. Only nodes themselves may be changed.

**Example 1:**

![](https://assets.leetcode.com/uploads/2021/03/04/reorder1linked-list.jpg)

**Input:** head = [1,2,3,4]
**Output:** [1,4,2,3]

**Example 2:**

![](https://assets.leetcode.com/uploads/2021/03/09/reorder2-linked-list.jpg)

**Input:** head = [1,2,3,4,5]
**Output:** [1,5,2,4,3]

**Constraints:**

- The number of nodes in the list is in the range `[1, 5 * 104]`.
- `1 <= Node.val <= 1000`

## Solution

```cpp
#include <stack>

class Solution {
public:
    void reorderList(ListNode* head) {
        stack<ListNode*> stk;
        // 先把所有节点装进栈里，得到倒序结果
        ListNode* p = head;
        while (p != nullptr) {
            stk.push(p);
            p = p->next;
        }

        p = head;
        while (!stk.empty()) {
            // 链表尾部的节点
            ListNode* lastNode = stk.top();
            stk.pop();
            ListNode* next = p->next;
            if (lastNode == next || lastNode->next == next) {
                // 结束条件，链表节点数为奇数或偶数时均适用
                lastNode->next = nullptr;
                break;
            }
            p->next = lastNode;
            lastNode->next = next;
            p = next;
        }
    }
};
```

# 225 Implement Stack Using Queue(s)

Implement a last-in-first-out (LIFO) stack using only two queues. The implemented stack should support all the functions of a normal stack (`push`, `top`, `pop`, and `empty`).

Implement the `MyStack` class:

- `void push(int x)` Pushes element x to the top of the stack.
- `int pop()` Removes the element on the top of the stack and returns it.
- `int top()` Returns the element on the top of the stack.
- `boolean empty()` Returns `true` if the stack is empty, `false` otherwise.

**Notes:**

- You must use **only** standard operations of a queue, which means that only `push to back`, `peek/pop from front`, `size` and `is empty` operations are valid.
- Depending on your language, the queue may not be supported natively. You may simulate a queue using a list or deque (double-ended queue) as long as you use only a queue's standard operations.

**Example 1:**

**Input**
["MyStack", "push", "push", "top", "pop", "empty"]
[[], [1], [2], [], [], []]
**Output**
[null, null, null, 2, 2, false]

**Explanation**
MyStack myStack = new MyStack();
myStack.push(1);
myStack.push(2);
myStack.top(); // return 2
myStack.pop(); // return 2
myStack.empty(); // return False

**Constraints:**

- `1 <= x <= 9`
- At most `100` calls will be made to `push`, `pop`, `top`, and `empty`.
- All the calls to `pop` and `top` are valid.

**Follow-up:** Can you implement the stack using only one queue?

## Solution

```cpp
//using one queue
class MyStack {
    queue<int> q;
    int top_elem = 0;

public:
    // 将元素 x 压入栈顶
    void push(int x) {
        // x 是队列的队尾，是栈的栈顶
        q.push(x);
        top_elem = x;
    }

    // 返回栈顶元素
    int top() {
        return top_elem;
    }

    // 删除栈顶的元素并返回
    int pop() {
        int size = q.size();
        // 留下队尾 2 个元素
        while (size > 2) {
            q.push(q.front());
            q.pop();
            size--;
        }
        // 记录新的队尾元素
        top_elem = q.front();
        q.push(q.front());
        q.pop();
        // 删除之前的队尾元素
        int result = q.front();
        q.pop();
        return result;
    }

    // 判断栈是否为空
    bool empty() {
        return q.empty();
    }
};
```

# 388 Longest Absolute File Path

Suppose we have a file system that stores both files and directories. An example of one system is represented in the following picture:

![](https://assets.leetcode.com/uploads/2020/08/28/mdir.jpg)

Here, we have `dir` as the only directory in the root. `dir` contains two subdirectories, `subdir1` and `subdir2`. `subdir1` contains a file `file1.ext` and subdirectory `subsubdir1`. `subdir2` contains a subdirectory `subsubdir2`, which contains a file `file2.ext`.

In text form, it looks like this (with ⟶ representing the tab character):

dir
⟶ subdir1
⟶ ⟶ file1.ext
⟶ ⟶ subsubdir1
⟶ subdir2
⟶ ⟶ subsubdir2
⟶ ⟶ ⟶ file2.ext

If we were to write this representation in code, it will look like this: `"dir\n\tsubdir1\n\t\tfile1.ext\n\t\tsubsubdir1\n\tsubdir2\n\t\tsubsubdir2\n\t\t\tfile2.ext"`. Note that the `'\n'` and `'\t'` are the new-line and tab characters.

Every file and directory has a unique **absolute path** in the file system, which is the order of directories that must be opened to reach the file/directory itself, all concatenated by `'/'s`. Using the above example, the **absolute path** to `file2.ext` is `"dir/subdir2/subsubdir2/file2.ext"`. Each directory name consists of letters, digits, and/or spaces. Each file name is of the form `name.extension`, where `name` and `extension` consist of letters, digits, and/or spaces.

Given a string `input` representing the file system in the explained format, return *the length of the **longest absolute path** to a **file** in the abstracted file system*. If there is no file in the system, return `0`.

**Note** that the testcases are generated such that the file system is valid and no file or directory name has length 0.

**Example 1:**

![](https://assets.leetcode.com/uploads/2020/08/28/dir1.jpg)

**Input:** input = "dir\n\tsubdir1\n\tsubdir2\n\t\tfile.ext"
**Output:** 20
**Explanation:** We have only one file, and the absolute path is "dir/subdir2/file.ext" of length 20.

**Example 2:**

![](https://assets.leetcode.com/uploads/2020/08/28/dir2.jpg)

**Input:** input = "dir\n\tsubdir1\n\t\tfile1.ext\n\t\tsubsubdir1\n\tsubdir2\n\t\tsubsubdir2\n\t\t\tfile2.ext"
**Output:** 32
**Explanation:** We have two files:
"dir/subdir1/file1.ext" of length 21
"dir/subdir2/subsubdir2/file2.ext" of length 32.
We return 32 since it is the longest absolute path to a file.

**Example 3:**

**Input:** input = "a"
**Output:** 0
**Explanation:** We do not have any files, just a single directory named "a".

**Constraints:**

- `1 <= input.length <= 104`
- `input` may contain lowercase or uppercase English letters, a new line character `'\n'`, a tab character `'\t'`, a dot `'.'`, a space `' '`, and digits.
- All file and directory names have **positive** length.

## Solution

```cpp
 int longestPath(string path)
    {
        vector<int> stk;
        string_view sv_path(path);
        int ans {};

        for(auto part: std::views::split(sv_path, '\n'))
        {
            auto sv_part = string_view(part);
            int level = 0;

            auto pos = sv_part.rfind('\t');
            if(pos != string::npos)
            {
                level = pos + 1;
            }

            while(cmp_less(level, stk.size()))
            {
                //remove those parts that are not in current directory structure
                stk.pop_back();
            }

            stk.push_back(sv_part.size() - level);

            if(sv_part.find('.') != string::npos)
            {
                // this is the file
                auto len = ranges::fold_left(stk, 0, plus<int>());
                // also need to add the path separator 
                len += static_cast<int>(stk.size()) - 1;
                ans = max(len, ans);
            }
        }

        return ans;
    }
};
```

# 394 Decode String

Given an encoded string, return its decoded string.

The encoding rule is: `k[encoded_string]`, where the `encoded_string` inside the square brackets is being repeated exactly `k` times. Note that `k` is guaranteed to be a positive integer.

You may assume that the input string is always valid; there are no extra white spaces, square brackets are well-formed, etc. Furthermore, you may assume that the original data does not contain any digits and that digits are only for those repeat numbers, `k`. For example, there will not be input like `3a` or `2[4]`.

The test cases are generated so that the length of the output will never exceed `105`.

**Example 1:**

**Input:** s = "3[a]2[bc]"
**Output:** "aaabcbc"

**Example 2:**

**Input:** s = "3[a2[c]]"
**Output:** "accaccacc"

**Example 3:**

**Input:** s = "2[abc]3[cd]ef"
**Output:** "abcabccdcdcdef"

**Constraints:**

- `1 <= s.length <= 30`
- `s` consists of lowercase English letters, digits, and square brackets `'[]'`.
- `s` is guaranteed to be **a valid** input.
- All the integers in `s` are in the range `[1, 300]`.

## Solution

核心思路：用一个栈存储「当前字符串」和「重复次数」的配对。遍历编码串时：

- 遇到数字：累积重复次数（注意可能是多位数，如 `12[a]`）
- 遇到 `[`：把当前积累的字符串和重复次数**压栈**，然后重置两者
- 遇到 `]`：从栈中**弹出**之前的字符串和重复次数，把当前字符串重复若干次后拼接到弹出的字符串后面
- 遇到字母：拼接到当前字符串

以 `"3[a2[c]]"` 为例演示：进入 `3[` 时，把 `("", 3)` 压栈；`a` 追加到当前串变成 `"a"`；进入 `2[` 时，把 `("a", 2)` 压栈；`c` 追加到当前串变成 `"c"`；遇到第一个 `]`，弹出 `("a", 2)`，当前串 = `"a" + "c" × 2 = "acc"`；遇到第二个 `]`，弹出 `("", 3)`，当前串 = `"" + "acc" × 3 = "accaccacc"`，即为答案。

```cpp
class Solution {
public:
    string decodeString(string s) {
        // 栈存储 (之前的字符串, 重复次数) 的配对
        stack<string> strStack;
        stack<int> cntStack;
        // 当前正在构建的字符串
        string cur = "";
        // 当前正在构建的数字（可能是多位数）
        int k = 0;
        for (char c : s) {
            if (isdigit(c)) {
                // 累积多位数字
                k = k * 10 + (c - '0');
            } else if (c == '[') {
                // 压栈，重置当前状态
                strStack.push(cur);
                cntStack.push(k);
                cur = "";
                k = 0;
            } else if (c == ']') {
                // 弹栈，把当前串重复 k 次后拼接到之前的串
                int times = cntStack.top(); cntStack.pop();
                string prev = strStack.top(); strStack.pop();
                string repeated = "";
                for (int i = 0; i < times; i++) {
                    repeated += cur;
                }
                cur = prev + repeated;
            } else {
                // 普通字符直接追加
                cur += c;
            }
        }
        return cur;
    }
};
```
