### Const propagation for pointers

By using the keyword `const`, we can inform the compiler about which objects are immutable. The compiler can then check that we don't try to mutate objects that aren't intended to be changed. In other words, the compiler checks our code for **const-correctness**. 

A common mistake when writing const-correct code in C++ is that a const-initialized object can still manipulate the values that member pointers point at. The following example illustrates the problem:

```cpp
class Foo {
public:
  Foo(int* ptr) : ptr_{ptr} {} 
  auto set_ptr_val(int v) const { 
    *ptr_ = v; // Compiles despite function being declared const!
  }
private:
  int* ptr_{};
};
int main() {
  auto i = 0;
  const auto foo = Foo{&i};
  foo.set_ptr_val(42);
}
```

Although the function `set_ptr_val()` is mutating the int value, it's valid to declare it `const` since the pointer `ptr_` itself is not mutated, only the int object that the pointer is pointing at.

In order to prevent this in a readable way, a wrapper called `std::experimental::propagate_const` has been added to the standard library extensions (included in, at the time of writing, the latest versions of Clang and GCC). Using `propagate_const`, the function `set_ptr_val()` will not compile. Note that `propagate_const` only applies to pointers, and pointer-like classes such as `std::shared_ptr` and `std::unique_ptr`, but not `std::function`.

The following example demonstrates how propagate_const can be used to generate compilation errors when trying to mutate an object inside a const function:

```cpp
#include <experimental/propagate_const>
class Foo { 
public: 
  Foo(int* ptr) : ptr_{ptr} {}
  auto set_ptr(int* p) const { 
    ptr_ = p;  // Will not compile, as expected
  }
  auto set_val(int v) const { 
    val_ = v;  // Will not compile, as expected
  }
  auto set_ptr_val(int v) const { 
    *ptr_ = v; // Will not compile, const is propagated
  }
private:
  std::experimental::propagate_const<int*> ptr_ = nullptr; 
  int val_{}; 
};
```

*** A note on empty destructors

Writing an empty destructor can prevent the compiler from implementing certain optimizations. As you can see in the following snippets, copying an array of a trivial class with an empty destructor yields the same (non-optimized) assembler code as copying with a handcrafted `for`-loop. The first version uses an empty destructor with `std::copy()`:

```cpp
struct Point {
 int x_, y_;
 ~Point() {}     // Empty destructor, don't use!
};
auto copy(Point* src, Point* dst) {
  std::copy(src, src+64, dst);
}
```

The second version uses a `Point` class with no destructor but with a handcrafted `fo`r-loop:

```cpp
struct Point {
  int x_, y_;
};
auto copy(Point* src, Point* dst) {
  const auto end = src + 64;
  for (; src != end; ++src, ++dst) {
    *dst = *src;
  }
}
```

Both versions generate the following x86 assembler, which corresponds to a simple loop:

```asm
 xor eax, eax
.L2:
 mov rdx, QWORD PTR [rdi+rax]
 mov QWORD PTR [rsi+rax], rdx
 add rax, 8
 cmp rax, 512
 jne .L2
 rep ret
```

However, if we remove the `destructor` or declare the `destructor` `default`, the compiler optimizes `std::copy()` to utilize `memmove()` instead of a loop:

```cpp
struct Point { 
  int x_, y_; 
  ~Point() = default; // OK: Use default or no constructor at all
};
auto copy(Point* src, Point* dst) {
  std::copy(src, src+64, dst);
}
```

The preceding code generates the following x86 assembler, with the memmove() optimization:

```asm
 mov rax, rdi
 mov edx, 512
 mov rdi, rsi
 mov rsi, rax
 jmp memmove
```

The assembler was generated using GCC 7.1 in Compiler Explorer, which is available at https://godbolt.org/.

To summarize, use `default destructors` or `no destructors` at all in favor of empty destructors to squeeze a little bit more performance out of your application.

### Cases where pass-by-value is not applicable

Sometimes this pattern of accept-by-value-then-move is actually a **pessimization**. For example, consider the following class where the function `set_data()` will keep a copy of the argument passed to it:

```cpp
class Widget {
  std::vector<int> data_{};
  // ...
public:
  void set_data(std::vector<int> x) { 
    data_ = std::move(x);               
  }
};
```

Assume we call `set_data()` and pass it an lvalue, like this:

```cpp
auto v = std::vector<int>{1, 2, 3, 4};
widget.set_data(v);                  // Pass an lvalue
```

Since we are passing a named object, v, the code will copy-construct a new `std::vector` object, x, and then move-assign that object into the data_ member. Unless we pass an empty vector object to `set_data()`, the `std::vector` copy-constructor will perform a heap allocation for its internal buffer.

Now compare this with the following version of `set_data()` optimized for lvalues:

```cpp
void set_data(const std::vector<int>& x) { 
    data_ = x;  // Reuse internal buffer in data_ if possible
}
```

Here, there will only be a heap allocation inside the assignment operator if the capacity of the current vector, `data_`, is smaller than the size of the source object, `x`. In other words, the internal pre-allocated buffer of `data`_ can be reused in the assignment operator in many cases and save us from an extra heap allocation.

If we find it necessary to optimize set_data() for `lvalues` and `rvalues`, it's better, in this case, to provide two overloads:

```cpp
void set_data(const std::vector<int>& x) {
  data_ = x;
}
void set_data(std::vector<int>&& x) noexcept { 
  data_ = std::move(x);
}
```

### Moving constructor parameters

When initializing class members in a constructor, we can safely use the `pass-by-value-then-move` pattern. During the construction of a new object, there is no chance that there are pre-allocated buffers that could have been utilized to avoid heap allocations. What follows is an example of a class with one std::vector member and a constructor to demonstrate this pattern:

```cpp
class Widget {
  std::vector<int> data_;
public:
  Widget(std::vector<int> x)       // By value
      : data_{std::move(x)} {}     // Move-construct
  // ...
};
```


