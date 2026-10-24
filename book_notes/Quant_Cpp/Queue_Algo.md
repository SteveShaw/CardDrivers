# 622 Design Circular Queue

Design your implementation of the circular queue. The circular queue is a linear data structure in which the operations are performed based on FIFO (First In First Out) principle, and the last position is connected back to the first position to make a circle. It is also called "Ring Buffer".

One of the benefits of the circular queue is that we can make use of the spaces in front of the queue. In a normal queue, once the queue becomes full, we cannot insert the next element even if there is a space in front of the queue. But using the circular queue, we can use the space to store new values.

Implement the `MyCircularQueue` class:

- `MyCircularQueue(k)` Initializes the object with the size of the queue to be `k`.
- `int Front()` Gets the front item from the queue. If the queue is empty, return `-1`.
- `int Rear()` Gets the last item from the queue. If the queue is empty, return `-1`.
- `boolean enQueue(int value)` Inserts an element into the circular queue. Return `true` if the operation is successful.
- `boolean deQueue()` Deletes an element from the circular queue. Return `true` if the operation is successful.
- `boolean isEmpty()` Checks whether the circular queue is empty or not.
- `boolean isFull()` Checks whether the circular queue is full or not.

You must solve the problem without using the built-in queue data structure in your programming language. 

**Example 1:**

**Input**
["MyCircularQueue", "enQueue", "enQueue", "enQueue", "enQueue", "Rear", "isFull", "deQueue", "enQueue", "Rear"]
[[3], [1], [2], [3], [4], [], [], [], [4], []]
**Output**
[null, true, true, true, false, 3, true, true, true, 4]

**Explanation**
MyCircularQueue myCircularQueue = new MyCircularQueue(3);
myCircularQueue.enQueue(1); // return True
myCircularQueue.enQueue(2); // return True
myCircularQueue.enQueue(3); // return True
myCircularQueue.enQueue(4); // return False
myCircularQueue.Rear();     // return 3
myCircularQueue.isFull();   // return True
myCircularQueue.deQueue();  // return True
myCircularQueue.enQueue(4); // return True
myCircularQueue.Rear();     // return 4

**Constraints:**

- `1 <= k <= 1000`
- `0 <= value <= 1000`
- At most `3000` calls will be made to `enQueue`, `deQueue`, `Front`, `Rear`, `isEmpty`, and `isFull`.

## Solution

* using `head` and `tail` to track the position

* `enque` operation update `tail`

* `deque` operation update `head`

* The `queue` size must be `k+1` otherwise one element will miss

```cpp
class MyCircularQueue
{
private:
    vector<int> queue;
    std::size_t head;
    std::size_t tail;


public:
    MyCircularQueue(int k) : queue(k + 1)
    {}

    bool enQueue(int value)
    {
        auto next_tail = (tail + 1) % size(queue);
        if(next_tail == head)
        {
            //queue is full
            return false;
        }
    }

    bool deQueue()
    {
        if(isEmpty())
        {
            return false;
        }

        head = (head + 1) % size(queue);
    }

    int Front()
    {
        if (q.isEmpty())
        {
            return -1;
        }

        return queue[head];
    }

    int Rear()
    {
        if (queue.isEmpty())
        {
            return -1;
        }

        //tail is already incremented
        //so we need to get the position just before tail
        auto last = (tail + size(queue) - 1) % size(queue);
        return queue[last];
    }

    bool isEmpty()
    {
        return head == tail;
    }

    bool isFull()
    {
        auto next_tail = (tail + 1) % size(queue);
        return next_tail == head;
    }
};

```

# 641 Design Circular Deque

Design your implementation of the circular double-ended queue (deque).

Implement the `MyCircularDeque` class:

- `MyCircularDeque(int k)` Initializes the deque with a maximum size of `k`.
- `boolean insertFront()` Adds an item at the front of Deque. Returns `true` if the operation is successful, or `false` otherwise.
- `boolean insertLast()` Adds an item at the rear of Deque. Returns `true` if the operation is successful, or `false` otherwise.
- `boolean deleteFront()` Deletes an item from the front of Deque. Returns `true` if the operation is successful, or `false` otherwise.
- `boolean deleteLast()` Deletes an item from the rear of Deque. Returns `true` if the operation is successful, or `false` otherwise.
- `int getFront()` Returns the front item from the Deque. Returns `-1` if the deque is empty.
- `int getRear()` Returns the last item from Deque. Returns `-1` if the deque is empty.
- `boolean isEmpty()` Returns `true` if the deque is empty, or `false` otherwise.
- `boolean isFull()` Returns `true` if the deque is full, or `false` otherwise.

**Example 1:**

**Input**
["MyCircularDeque", "insertLast", "insertLast", "insertFront", "insertFront", "getRear", "isFull", "deleteLast", "insertFront", "getFront"]
[[3], [1], [2], [3], [4], [], [], [], [4], []]
**Output**
[null, true, true, true, false, 2, true, true, true, 4]

**Explanation**
MyCircularDeque myCircularDeque = new MyCircularDeque(3);
myCircularDeque.insertLast(1);  // return True
myCircularDeque.insertLast(2);  // return True
myCircularDeque.insertFront(3); // return True
myCircularDeque.insertFront(4); // return False, the queue is full.
myCircularDeque.getRear();      // return 2
myCircularDeque.isFull();       // return True
myCircularDeque.deleteLast();   // return True
myCircularDeque.insertFront(4); // return True
myCircularDeque.getFront();     // return 4

**Constraints:**

- `1 <= k <= 1000`
- `0 <= value <= 1000`
- At most `2000` calls will be made to `insertFront`, `insertLast`, `deleteFront`, `deleteLast`, `getFront`, `getRear`, `isEmpty`, `isFull`.

## Solution

* using `head` and `tail` to track the position

* `insertFront` and `deleteFront()` update `head`

* `insertLast` and `deleteLast()` update `tail`

* Use `n_elems` to track the number of elements in the queue, so that we don't need to increase the size of the queue to `k+1` as last example

```cpp
class MyCircularDeque {
private:
    size_t head {};
    size_t tail {};
    size_t n_elems {};
    vector<int> queue;

public:
    // Initialize your data structure here. Set the size of the deque to be k.
    MyCircularDeque(int k) : queue(k) {}

    // Adds an item at the front of Deque. Return true if the operation is successful.
    bool insertFront(int value) {
        if(isFull())
        {
            return false;
        }

        head = (head + size(queue) - 1) % size(queue);
        queue[head] = value;
        ++n_elems;
        return true;
    }

    // Adds an item at the rear of Deque. Return true if the operation is successful.
    bool insertLast(int value) {
        if(isFull())
        {
            return false;
        }

        queue[tail] = value;
        tail = (tail + 1) % size(queue);
        ++n_elems;
        return true;
    }

    // Deletes an item from the front of Deque. Return true if the operation is successful.
    bool deleteFront() {
        if (isEmpty()) {
            return false;
        }

        head = (head + 1) % size(queue);
        --n_elems;
        return true;
    }

    // Deletes an item from the rear of Deque. Return true if the operation is successful.
    bool deleteLast() {
        if (isEmpty()) {
            return false;
        }

        tail = (tail + size(queue) - 1) % size(queue);
        --n_elems;
        return true;
    }

    // Get the front item from the deque.
    int getFront() {
        if (isEmpty()) {
            return -1;
        }

        return queue[head];
    }

    // Get the last item from the deque.
    int getRear() {
        if (isEmpty()) {
            return -1;
        }
        
        auto last = (tail + size(queue) - 1) % size(queue);
        return queue[last];
    }

    // Checks whether the circular deque is empty or not.
    bool isEmpty() {
        return n_elems == 0;
    }

    // Checks whether the circular deque is full or not.
    bool isFull() {
        return n_elems == size(queue);
    }
};
```

# 1670 Design Front Middle Back Queue

Design a queue that supports `push` and `pop` operations in the front, middle, and back.

Implement the `FrontMiddleBack` class:

- `FrontMiddleBack()` Initializes the queue.
- `void pushFront(int val)` Adds `val` to the **front** of the queue.
- `void pushMiddle(int val)` Adds `val` to the **middle** of the queue.
- `void pushBack(int val)` Adds `val` to the **back** of the queue.
- `int popFront()` Removes the **front** element of the queue and returns it. If the queue is empty, return `-1`.
- `int popMiddle()` Removes the **middle** element of the queue and returns it. If the queue is empty, return `-1`.
- `int popBack()` Removes the **back** element of the queue and returns it. If the queue is empty, return `-1`.

**Notice** that when there are **two** middle position choices, the operation is performed on the **frontmost** middle position choice. For example:

- Pushing `6` into the middle of `[1, 2, 3, 4, 5]` results in `[1, 2, <u>6</u>, 3, 4, 5]`.
- Popping the middle from `[1, 2, <u>3</u>, 4, 5, 6]` returns `3` and results in `[1, 2, 4, 5, 6]`.

**Example 1:**

**Input:**
["FrontMiddleBackQueue", "pushFront", "pushBack", "pushMiddle", "pushMiddle", "popFront", "popMiddle", "popMiddle", "popBack", "popFront"]
[[], [1], [2], [3], [4], [], [], [], [], []]
**Output:**
[null, null, null, null, null, 1, 3, 4, 2, -1]

**Explanation:**
FrontMiddleBackQueue q = new FrontMiddleBackQueue();
q.pushFront(1);   // [<u>1</u>]
q.pushBack(2);    // [1, <u>2</u>]
q.pushMiddle(3);  // [1, <u>3</u>, 2]
q.pushMiddle(4);  // [1, <u>4</u>, 3, 2]
q.popFront();     // return 1 -> [4, 3, 2]
q.popMiddle();    // return 3 -> [4, 2]
q.popMiddle();    // return 4 -> [2]
q.popBack();      // return 2 -> []
q.popFront();     // return -1 -> [] (The queue is empty)

**Constraints:**

- `1 <= val <= 10^9`
- At most `1000` calls will be made to `pushFront`, `pushMiddle`, `pushBack`, `popFront`, `popMiddle`, and `popBack`.

```cpp

```
