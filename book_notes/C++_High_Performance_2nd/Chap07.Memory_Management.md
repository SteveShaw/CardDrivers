# Chapter 7 Memory Management

## The virtual address space

Addresses in the virtual address space that programmers see are mapped to physical addresses by the operating system and the **memory management unit** (MMU), which is a part of the processor. This mapping or translation happens each time we access a memory address.

This extra layer of indirection makes it possible for the operating system to use physical memory for the parts of a process that are currently being used, and back up the rest of the virtual memory on disk. In this sense, we can look at the physical main memory as a cache for the virtual memory space, which resides on secondary storage. The areas of the secondary storage that are used for backing up memory pages are usually called **swap** space, **swap file**, or simply **pagefile**, depending on the operating system.

Virtual memory makes it possible for processes to have a virtual address space bigger than the physical address space, since virtual memory that is not in use does not have to occupy physical memory.

## Memory pages

The most common way to implement virtual memory today is to divide the address space into fixed-size blocks called memory pages. When a process accesses memory at a virtual address, the operating system checks whether the memory page is backed by physical memory (a page frame). If the memory page is not mapped in the main memory, a hardware exception occurs, and the page is loaded from disk into memory. This type of hardware exception is called a page fault. This is not an error but a necessary interrupt in order to load data from disk to memory. This is very slow compared to reading data that is already resident in memory.

When there are no more available page frames in the main memory, a page frame has to be evicted. If the page to be evicted is dirty, that is, it has been modified since it was last loaded from disk, it needs to be written to disk before it can be replaced. This mechanism is called **paging**. If the memory page has not been modified, the memory page is simply evicted.

Not all operating systems that support virtual memory support paging. iOS, for example, does have virtual memory but dirty pages are never stored on disk; only clean pages can be evicted from memory. If the main memory is full, iOS will start terminating processes until there is enough free memory again. Android uses a similar strategy. One reason for not writing memory pages back to the flash storage of the mobile devices is that it drains the battery, and it also shortens the lifespan of the flash storage itself.

## Thrashing

**Thrashing** can happen when a system runs low on physical memory and is, therefore, constantly paging. Whenever a process gets time scheduled on the CPU, it tries to access memory that has been paged out. Loading new memory pages means that the other pages first have to be stored on disk. Moving data back and forth between disk and memory is usually very slow; in some cases, this more or less stalls the computer since the system spends all its time paging. Looking at the system's page fault frequency is a good way to determine whether the program has started thrashing.

## Placement new

C++17 introduces a set of utility functions in `<memory>` for constructing and destroying objects without allocating or deallocating memory. So, instead of calling **placement new**, it is now possible to use some of the functions from `<memory>` whose names begin with `std::uninitialized_` for constructing, copying, and moving objects to an uninitialized memory area. And instead of calling the destructor explicitly, we can now use `std::destroy_at()` to destruct an object at a specific memory address without deallocating the memory.

```cpp
auto* memory = std::malloc(sizeof(User));
auto* user_ptr = reinterpret_cast<User*>(memory);
std::uninitialized_fill_n(user_ptr, 1, User{"john"});
std::destroy_at(user_ptr);
std::free(memory);
```

`C++20` also introduces `std::construct_at()`, which makes it possible to replace the `std::uninitialized_fill_n()` call with:

```cpp
std::construct_at(user_ptr, User{"john"});        // C++20
```

## The new and delete operators

The function `operator new` is responsible for allocating memory when a new expression is invoked. The `new` operator can be either a globally defined function or a static member function of a class. It is possible to overload the global operators `new` and `delete`.

The class-specific version of `new` will be used when we create new dynamically allocated Document objects:

```cpp
auto* p = new Document{}; // Uses class-specific operator new
delete p;
```

If we instead want to use `global new` and `delete`, it is still possible by using the global scope (`::`):

```cpp
auto* p = ::new Document{}; // Uses global operator new
::delete p;
```

## Memory Alignment

Let's say that we have a type with an alignment requirement of 2. The C++ standard doesn't say whether the valid addresses are `1, 3, 5, 7...` or `0, 2, 4, 6...`. All platforms start counting addresses at 0, so, in practice we could check if an object is correctly aligned by checking if its address is a multiple of the alignment using the modulo operator (`%`).

However, to write fully portable C++ code, we need to use `std::align()` and not `modulo` to check the alignment of an object. `std::align()` is a function from <memory> that will adjust a pointer according to an alignment that is passed as an argument. If the memory address is passed to it is already aligned, the pointer will not be adjusted. 

Here shows how to use `std::align()` to implement a small utility function called is_aligned():

```cpp
bool is_aligned(void* ptr, std::size_t alignment) {
  assert(ptr != nullptr);
  assert(std::has_single_bit(alignment)); // Power of 2
  auto s = std::numeric_limits<std::size_t>::max();
  auto aligned_ptr = ptr;
  std::align(alignment, 1, aligned_ptr, s);
  return ptr == aligned_ptr;
}
```

At first, check if `ptr` argument isn't null and that alignment is a power of 2, which is stated as a requirement in the C++ standard. We are using C++20 std::has_single_bit() from the <bit> header to check this. 

Next, calling `std::align()`. The typical use case for `std::align() ` is when we have a memory buffer of some size in which we want to store an object with some alignment requirement. In this case, we don't have a buffer, and we don't care about the size of the objects, so we say that the object is of size 1 and the buffer is the maximum value of a `std::size_t`. Then, we can compare the original `ptr` and the adjusted `aligned_ptr` to see if the original pointer was already aligned
