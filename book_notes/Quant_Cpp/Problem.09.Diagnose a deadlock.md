# Problem 9 Diagnose a deadlock

Given:

```cpp
void transfer(Account& from,
              Account& to,
              double amount)
{
    std::lock_guard l1(from.mutex);
    std::lock_guard l2(to.mutex);

    from.balance -= amount;
    to.balance += amount;
}
```

Two threads execute:

```cpp
transfer(A, B, 100)
transfer(B, A, 50)
```

Explain what can happen and fix it.

**Follow-ups:** 

1. Explain the four Coffman conditions. 

2. How does `std::scoped_lock` avoid the issue?

3. Could lock ordering solve it? 

4. What happens for `transfer(A, A, 100)`?

**Tests:** deadlocks, lock ordering, edge cases.

### Solution

the issue is classic lock-order deadlock.

```cpp
void transfer(Account& from,
              Account& to,
              double amount)
{
    std::lock_guard l1(from.mutex);
    std::lock_guard l2(to.mutex);

    from.balance -= amount;
    to.balance += amount;
}
```

Suppose two threads run concurrently:

```cpp
// Thread 1
transfer(A, B, 100);

// Thread 2
transfer(B, A, 50);
```

Thread 1 may lock `A.mutex` first, while Thread 2 locks `B.mutex` first. Then Thread 1 waits for `B.mutex`, while Thread 2 waits for `A.mutex`. Neither can proceed.

That is a **deadlock**.

A clean C++17 fix is:

```cpp
void transfer(Account& from,
              Account& to,
              double amount)
{
    if (&from == &to)
        return;

    std::scoped_lock lock(from.mutex, to.mutex);

    from.balance -= amount;
    to.balance += amount;
}
```

`std::scoped_lock` can lock multiple mutexes using a deadlock-avoidance algorithm.

The `&from == &to` check matters because trying to lock the same non-recursive mutex twice is invalid/deadlocking behavior.

#### Follow-up 1: Explain the four Coffman conditions

A deadlock can occur when all four conditions are present.

1. **Mutual exclusion**  
   A resource can only be owned by one thread at a time.
   
   Here, a mutex can only be locked by one thread.

2. **Hold and wait**  
   A thread holds one resource while waiting for another.
   
   Thread 1 holds `A.mutex` while waiting for `B.mutex`.

3. **No preemption**  
   A resource cannot simply be forcibly taken away.
   
   Thread 2 cannot forcibly unlock `A.mutex` from Thread 1.

4. **Circular wait**  
   There is a dependency cycle.

```textile
Thread 1 holds A -> waits for B
Thread 2 holds B -> waits for A
```

This creates:

```cpp
T1 -> B -> T2 -> A -> T1
```

Breaking any one of these conditions prevents deadlock.

In practice, the most common strategy is to eliminate **circular wait** through consistent lock ordering.

#### Follow-up 2: How does `std::scoped_lock` avoid deadlock?

With:

```cpp
std::scoped_lock lock(from.mutex, to.mutex);
```

C++ uses deadlock-avoidance semantics similar to `std::lock`.

Conceptually, instead of doing:

```cpp
mutex1.lock();
mutex2.lock();
```

which can deadlock, it coordinates acquisition of all mutexes so that it does not simply hold one forever while blocking on another.

You normally don't need to know the exact library algorithm because the implementation is allowed to vary.

The important interview answer is:

* `std::scoped_lock` with multiple mutexes uses `std::lock`-style deadlock avoidance and acquires the mutexes as a group, rather than naively locking them sequentially.

Example:

```cpp
std::scoped_lock lock(a.mutex, b.mutex);
```

is preferable to:

```cpp
std::lock_guard lock1(a.mutex);
std::lock_guard lock2(b.mutex);
```

when lock ordering is not otherwise guaranteed.

#### Follow-up 3: Can lock ordering solve the problem?

Yes.

Define a global ordering for locks, for example by account ID:

```cpp
struct Account {
    int id;
    double balance;
    std::mutex mutex;
};
```

Then always acquire the lower-ID account first:

```cpp
void transfer(Account& from,
              Account& to,
              double amount)
{
    if (&from == &to)
        return;

    Account* first;
    Account* second;

    if (from.id < to.id) {
        first = &from;
        second = &to;
    } else {
        first = &to;
        second = &from;
    }

    std::lock_guard<std::mutex> lock1(first->mutex);
    std::lock_guard<std::mutex> lock2(second->mutex);

    from.balance -= amount;
    to.balance += amount;
}
```

Now both:

```cpp
transfer(A, B, 100);
transfer(B, A, 50);
```

acquire locks in the same order.

For example, if:

```cpp
A.id = 1
B.id = 2
```

both threads lock:

```cpp
A
then B
```

so circular wait cannot occur.

This technique is known as establishing a **global lock hierarchy**.

One subtle point: don't normally order unrelated objects using raw pointer comparison such as:

```cpp
if (&from < &to)
```

because relational comparison between pointers to unrelated objects isn't generally the best portable way to establish object ordering. A stable account ID or another explicit key is better.

#### Follow-up 4: What happens with `transfer(A, A, 100)`?

The naive version:

```cpp
std::lock_guard l1(from.mutex);
std::lock_guard l2(to.mutex);
```

becomes effectively:

```cpp
A.mutex.lock();
A.mutex.lock();
```

A normal `std::mutex` is non-recursive, so the second lock attempt blocks forever or otherwise results in undefined behavior depending on how the preconditions are violated.

Therefore handle this case explicitly:

```cpp
if (&from == &to)
    return;
```

### Another valid solution using `std::lock`

Before C++17, a common pattern was:

```cpp
void transfer(Account& from,
              Account& to,
              double amount)
{
    if (&from == &to)
        return;

    std::unique_lock<std::mutex> lock1(
        from.mutex, std::defer_lock);

    std::unique_lock<std::mutex> lock2(
        to.mutex, std::defer_lock);

    std::lock(lock1, lock2);

    from.balance -= amount;
    to.balance += amount;
}
```

`std::defer_lock` constructs the `unique_lock`s without locking immediately.

Then:

```cpp
std::lock(lock1, lock2);
```

locks both with deadlock avoidance.

This is useful to know because an interviewer may ask:

* What did people do before `std::scoped_lock`?

### Why not use `std::recursive_mutex`?

You could try:

```cpp
std::recursive_mutex mutex;
```

so the same thread can lock a mutex multiple times.

But that does **not** solve the original deadlock between two different accounts.

Thread 1 could still hold `A` and wait for `B`, while Thread 2 holds `B` and waits for `A`.

So `recursive_mutex` is not the right solution.

It can also hide poor locking design and has additional overhead.

### Another likely interview follow-up: is the transfer atomic?

From the perspective of other code that correctly takes both account locks, yes.

Inside:

```cpp
std::scoped_lock lock(from.mutex, to.mutex);

from.balance -= amount;
to.balance += amount;
```

no other correctly synchronized operation can observe the transfer halfway through.

But imagine another function:

```cpp
double getBalance(Account& a)
{
    return a.balance;
}
```

This is not safe if transfers are happening concurrently.

It must also synchronize:

```cpp
double getBalance(Account& a)
{
    std::lock_guard lock(a.mutex);
    return a.balance;
}
```

A key concurrency principle is:

> Every access to shared mutable state must follow the same synchronization discipline.

### Another likely follow-up: validate before modifying

A realistic transfer needs to check funds:

```cpp
bool transfer(Account& from,
              Account& to,
              double amount)
{
    if (&from == &to)
        return true;

    std::scoped_lock lock(from.mutex, to.mutex);

    if (from.balance < amount)
        return false;

    from.balance -= amount;
    to.balance += amount;

    return true;
}
```

The balance check must happen **after acquiring the locks**.

This would be incorrect:

```cpp
if (from.balance >= amount) {
    std::scoped_lock lock(...);
    ...
}
```

because another thread could change `from.balance` between the check and acquiring the lock. That's a classic **TOCTOU**—time-of-check to time-of-use—race.

## What I would say in an interview

A concise strong answer would be:

> "The code can deadlock because two transfers in opposite directions can acquire the account mutexes in opposite orders. One thread can hold A and wait for B while another holds B and waits for A. I would either establish a global lock ordering, such as account ID order, or more simply use `std::scoped_lock(from.mutex, to.mutex)`, which performs deadlock-avoiding multi-mutex acquisition. I'd also explicitly handle the case where `from` and `to` are the same account, because a normal `std::mutex` cannot be recursively locked."

If the interviewer asks about theory:

> "This satisfies all four Coffman conditions: mutual exclusion, hold-and-wait, no preemption, and circular wait. Consistent lock ordering breaks circular wait."

That is usually the level of answer they're looking for.