```cpp
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

template<typename T>
class BlockingQueue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [this] {
            return !queue_.empty();
        });

        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
};
```

`wait()` atomically releases the mutex and blocks. Before returning, it reacquires the mutex. All access to the queue happens under that same mutex.

**1. Why use a predicate?**

A wakeup does not guarantee that an item is available. The predicate overload is equivalent to:

```
while (queue_.empty()) {
    cv_.wait(lock);
}
```

The loop handles spurious wakeups and cases where another consumer removes the item before this consumer reacquires the mutex. An explicit loop is also correct; a single `if` is insufficient.

**2. What is a spurious wakeup?**

A waiting thread can wake up even though no corresponding notification occurred. It must recheck the shared state before proceeding.

**3. Should `notify_one()` happen while holding the mutex?**

Both approaches are correct here. Usually, notify after unlocking, as shown above, so the awakened consumer can acquire the mutex immediately.

The queue state must change while holding the mutex. Notifications are not stored; the predicate checks the persistent state.

**4. How do you implement shutdown?**

Add a closed flag protected by the mutex, wake all waiters on shutdown, and define how `pop()` signals completion. This C++17 version drains queued items before returning `std::nullopt`:

```
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

template<typename T>
class BlockingQueue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) {
                throw std::runtime_error("Queue is closed");
            }
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return std::nullopt; // Closed and drained.
        }

        std::optional<T> value(std::in_place,
                               std::move(queue_.front()));
        queue_.pop();
        return value;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool closed_ = false;
};
```

If the original `T pop()` signature must remain, throw a dedicated exception when closed and drained instead.

Call `shutdown()`, then join threads using the queue before destroying it. Shutdown alone does not make destruction safe.

**5. What happens with multiple consumers?**

The mutex serializes removal, so each item is removed by only one consumer. Each push wakes one waiter; consumers recheck the predicate after acquiring the mutex.

Items are removed in FIFO order, but consumer fairness and processing completion order are not guaranteed. Shutdown uses `notify_all()` so every blocked consumer can exit.
