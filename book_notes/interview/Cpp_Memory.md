# Vector Implementation

```cpp
// also available live: https://wandbox.org/permlink/ZTWbNWzauMqhfXXQ

#include <cstddef>
#include <algorithm>
#include <utility>
#include <initializer_list>
#include <iterator>
#include <cstdlib> // HERE
#include <memory> // HERE

template <class T>
class Vector {
public:
   using value_type = T;
   using size_type = std::size_t;
   using pointer = T*;
   using const_pointer = const T*;
   using reference = T&;
   using const_reference = const T&;
private:
   pointer elems{};
   size_type nelems{},
      cap{};
   // ...
public:
   size_type size() const { return nelems; }
   size_type capacity() const { return cap; }
   bool empty() const { return size() == 0; }
private:
   bool full() const { return size() == capacity(); }
   // ...
public:
   using iterator = pointer;
   using const_iterator = const_pointer;
   iterator begin() { return elems; }
   const_iterator begin() const { return elems; }
   const_iterator cbegin() const { return begin(); }
   iterator end() { return begin() + size(); }
   const_iterator end() const { return begin() + size(); }
   const_iterator cend() const { return end(); }
   // ...
   Vector() = default;
   // HERE
   Vector(size_type n, const_reference init)
      : elems{ static_cast<pointer>(std::malloc(n * sizeof(value_type))) },
        nelems{ n }, cap{ n } {

      try {
         std::uninitialized_fill(begin(), end(), init);
      } catch (...) {
         std::free(elems);
         throw;
      }
   }
   // HERE
   Vector(const Vector& other)
      : elems{ static_cast<pointer>(std::malloc(other.size() * sizeof(value_type))) },
        nelems{ other.size() }, cap{ other.size() } {
      try {
         std::uninitialized_copy(other.begin(), other.end(), begin());
      } catch (...) {
         std::free(elems);
         throw;
      }
   }
   // ...
   Vector(Vector&& other) noexcept
      : elems{ std::exchange(other.elems, nullptr) },
        nelems{ std::exchange(other.nelems, 0) },
        cap{ std::exchange(other.cap, 0) } {
   }
   // ...
   // HERE
   Vector(std::initializer_list<T> src)
      : elems{ static_cast<pointer>(std::malloc(src.size() * sizeof(value_type))) },
        nelems{ src.size() }, cap{ src.size() } {
      try {
         std::uninitialized_copy(src.begin(), src.end(), begin());
      } catch (...) {
         std::free(elems);
         throw;
      }
   }
   // HERE
   ~Vector() {
      std::destroy(begin(), end());
      std::free(elems);
   }
   // ...
   void swap(Vector& other) noexcept {
      using std::swap;
      swap(elems, other.elems);
      swap(nelems, other.nelems);
      swap(cap, other.cap);
   }
   Vector& operator=(const Vector& other) {
      Vector{ other }.swap(*this);
      return *this;
   }
   Vector& operator=(Vector&& other) {
      Vector{ std::move(other) }.swap(*this);
      return *this;
   }
   // ...
   reference operator[](size_type n) { return elems[n]; }
   const_reference operator[](size_type n) const { return elems[n]; }
   // precondition: !empty()
   reference front() { return (*this)[0]; }
   const_reference front() const { return (*this)[0]; }
   reference back() { return (*this)[size() - 1]; }
   const_reference back() const { return (*this)[size() - 1]; }
   // ...
   bool operator==(const Vector& other) const {
      return size() == other.size() &&
         std::equal(begin(), end(), other.begin());
   }
   // can be omitted since C++20
   bool operator!=(const Vector& other) const {
      return !(*this == other);
   }
   // ...
   void push_back(const_reference val) {
      if (full())
         grow();
      // HERE
      std::construct_at(end(), val);
      ++nelems;
   }
   void push_back(T&& val) {
      if (full())
         grow();
      // HERE
      std::construct_at(end(), std::move(val));
      ++nelems;
   }
   template <class ... Args>
      reference emplace_back(Args &&...args) {
         if (full())
            grow();
         // HERE
         std::construct_at(end(), std::forward<Args>(args)...);
         ++nelems;
         return back();
      }
private:
   // HERE
   void grow() {
      reserve(capacity()? capacity() * 2 : 16);
   }
public:
   // HERE
   void reserve(size_type new_cap) {
      if(new_cap <= capacity()) return;
      auto p = static_cast<pointer>(std::malloc(new_cap * sizeof(T)));
      if constexpr(std::is_nothrow_move_assignable_v<T>) {
         std::uninitialized_move(begin(), end(), p);         
      } else try {
         std::uninitialized_copy(begin(), end(), p);
      } catch (...) {
         std::free(p);
         throw;
      }
      std::destroy(begin(), end());
      std::free(elems);
      elems = p;
      cap = new_cap;
   }
   // HERE
   void resize(size_type new_cap) {
      if(new_cap <= capacity()) return;
      auto p = static_cast<pointer>(std::malloc(new_cap * sizeof(T)));
      if constexpr(std::is_nothrow_move_assignable_v<T>) {
         std::uninitialized_move(begin(), end(), p);         
      } else try {
         std::uninitialized_copy(begin(), end(), p);
      } catch (...) {
         std::free(p);
         throw;
      }
      std::uninitialized_fill(p + size(), p + capacity(), value_type{});
      std::destroy(begin(), end());
      std::free(elems);
      elems = p;
      nelems = new_cap;
      cap = new_cap;
   }
   // etc.
   // two small examples, one that inserts elements
   // at a given position in the container and one
   // that erases an element at a given position
   // in the container
   template <class It>
   iterator insert(const_iterator pos, It first, It last) {
      iterator pos_ = const_cast<iterator>(pos);
      // deliberate usage of unsigned integrals
      const std::size_t remaining = capacity() - size();
      const std::size_t n = std::distance(first, last);
      if (remaining < n) {
         auto index = std::distance(begin(), pos_);
         reserve(capacity() + n - remaining);
         pos_ = std::next(begin(), index);
      }
      if (auto m = std::distance(std::next(pos_, n), end()); m > 0) {
         std::uninitialized_copy(pos_ + n, end(), end() + n - m);
         std::uninitialized_copy(pos_ + m, pos_ + n, end());
         std::copy_backward(pos_, pos_ + m, pos_ + n + m);
         std::copy(first, last, pos_);
      } else {
         std::uninitialized_copy(pos_, end(), end() + n - (n + m));
         std::uninitialized_copy(first + n + m, last, end());
         std::copy(first, first + n + m, pos_);
      }
      nelems += n;
      return pos_;
   }
   iterator erase(const_iterator pos) {
      iterator pos_ = const_cast<iterator>(pos);
      if (pos_ == end()) return pos_;
      std::copy(std::next(pos_), end(), pos_);
      std::destroy_at(std::prev(end()));
      --nelems;
      return pos_;
   }
};

#include <iostream>

template <class T>
std::ostream& operator<<(std::ostream& os, const Vector<T>& v) {
   if (v.empty()) return os;
   os << v.front();
   for (auto p = std::next(v.begin()); p != v.end(); ++p)
      os << ',' << *p;
   return os;
}

int main() {
   Vector v0{ 2,3,5,7,11 };
   Vector v1 = v0; // copy ctor
   std::cout << v1 << '\n'; // 2,3,5,7,11
   for (int n : { 13, 17, 19, 23, 29, 31, 37, 41, 43, 47 })
      v0.push_back(n); // will call grow() at some point
   // Size: 15, capacity: 20
   // 2,3,5,7,11,13,17,19,23,29,31,37,41,43,47
   std::cout << "Size: " << v0.size() << ", capacity: " << v0.capacity() << '\n'
      << v0 << '\n';
   int arr[]{ -2, -3, -4 };
   v1.insert(v1.begin(), std::begin(arr), std::end(arr));
   // Size: 8, capacity: 8
   // -2,-3,-4,2,3,5,7,11
   std::cout << "Size: " << v1.size() << ", capacity: " << v1.capacity() << '\n'
      << v1 << '\n';
   v1.insert(v1.end(), std::begin(arr), std::end(arr));
   // Size: 11, capacity: 11
   // -2,-3,-4,2,3,5,7,11,-2,-3-,4
   std::cout << "Size: " << v1.size() << ", capacity: " << v1.capacity() << '\n'
      << v1 << '\n';
   v1.erase(std::next(v1.begin(), 2));
   // Size: 10, capacity: 11
   // -2,-3,2,3,5,7,11,-2,-3,-4
   std::cout << "Size: " << v1.size() << ", capacity: " << v1.capacity() << '\n'
      << v1 << '\n';
}
```

We can use a C++ 23 feature to simplify `operator []`, `front` and `back`

```cpp
 template <class S>
         decltype(auto) operator[](this S && self, size_type n) {
            return self.elems[n];
         }
      // precondition: !empty()
      template <class S>
         decltype(auto) front(this S &&self) { return self[0]; }
      template <class S>
         decltype(auto) back(this S &&self) { return self[self.size()-1]; }
```

# Memory Allocators

## Placement new

The most important version of **operator new()** and friends is not one you can replace, but even if you could… well, let’s just state that it would be difficult to achieve something more efficient:

```cpp
// note: these exist, you can use them but you cannot
// replace them
void *operator new(std::size_t, **void *p**) { **return p;** }
void *operator new[](std::size_t, **void *p**) { **return p;** }
void operator delete(void*, **void***) noexcept { }
void operator delete[](void*, **void***) noexcept { }
```

Example of using `placement new`

```cpp
auto string_length(const char *p) {
   using std::string;
   // A) make a local buffer of the right size and
   // alignment for a string object
   alignas(string) char buf[sizeof(string)];
   // B) "paint" a string object in that buffer
   // (note: that object might allocate its
   // own data externally, but that's not
   // our concern here)
   string *s = new (static_cast<void*>(buf)) string{ p };
   // C) use that object to compute the size
   const auto sz = s->size();
   // D) destroy the object without releasing the memory
   // for the buffer (it's not dynamically allocated,
   // it's just local storage)
   s->~string(); // yes, you can do this
   return sz;
}
```

- Step **A)** makes sure that the location where the object will be constructed is of the right size and shape: it’s a buffer of bytes (type **char**), aligned in memory as a **std::string** object should be, and of sufficient size to hold a **std::string** object.
- Step **B)** paints a **std::string** object in that buffer. That’s what a constructor does, really: it (conceptually) transforms raw memory into an object and initializes the state of that object. If the **std::string** constructor throws an exception, then the object has never been constructed and our **string_length()** function concludes without satisfying its postconditions. There is no memory allocation involved here unless the constructor itself allocates, but that’s fair (the object does what it has to do).
- Step **C)** uses the newly constructed object; in our case, it’s just a matter of querying the size of that character string, but we could do whatever we want here. Do note, however, that (a) the object’s lifetime is tied to the buffer in which it is located, and (b) since we explicitly called the constructor, we will need to explicitly destroy it, which means that if an exception is thrown when we use the object, we will need to make sure the object’s destructor is called somehow.
- Step **D)** destroys the object before we leave the function, as not doing so would lead to a possible leak of resources. If the buffer’s lifetime ends at a point where the object is not yet destroyed, things will be very wrong: either the destructor of the object we put in that buffer will never be called and code will leak, or someone might try to use the object even though the storage for that object is not ours anymore, leading to UB. Note the syntax, **s->~string()**, which calls the destructor but does not deallocate the storage for ***s**.

## Member Version Of the allocation Pointers

Suppose class **X** would somehow benefit from a per-class specialization of these mechanisms, and show that client code will call these specializations when we call **new X** but not when we call **new int**:

```cpp
#include <iostream>
#include <new>
class X {
   // ...
public:
   X() { std::cout << "X::X()\n"; }
   ~X() { std::cout << "X::~X()\n"; }
   void *operator new(std::size_t);
   void *operator new[](std::size_t);
   void operator delete(void*);
   void operator delete[](void*);
   // ...
};
// ...
void* X::operator new(std::size_t n) {
   std::cout << "Some X::operator new() magic\n";
   return ::operator new(n);
}
void* X::operator new[](std::size_t n) {
   std::cout << "Some X::operator new[]() magic\n";
   return ::operator new[](n);
}
void X::operator delete(void *p) {
   std::cout << "Some X::operator delete() magic\n";
   return ::operator delete(p);
}
void X::operator delete[](void *p) {
   std::cout << "Some X::operator delete[]() magic\n";
   return ::operator delete[](p);
}
int main() {
   std::cout << "p = new int{3}\n";
   int *p = new int{ 3 }; // global operator new
   std::cout << "q = new X\n";
   X *q = new X; // X::operator new
   std::cout << "delete p\n";
   delete p; // global operator delete
   std::cout << "delete q\n";
   delete q; // X::operator delete
}
```

## Alignment-aware versions of the allocation operators

When designing C++17, a fundamental problem with the memory allocation process was fixed with respect to what we call **overaligned types**. The idea is that there are types for which we will want alignment constraints that are stricter than the alignment of the strictest natural alignment on a machine as modeled by type **std::max_align_t**.

Since C++17, we can specify **overaligned type requirements** when calling either **operator new()** or **operator new[]()** by passing an additional argument of type **std::align_val_t**, an integral type. This has to be done explicitly at the call site, as the following example shows:

```cpp
#include <iostream>
#include <new>
#include <cstdlib>
#include <type_traits>
void* operator new(std::size_t n, std::align_val_t al) {
   std::cout << "new(" << n << ", align: "
             << static_cast<std::underlying_type_t<
                   std::align_val_t
                >>(al) << ")\n";
   return std::aligned_alloc(
      static_cast<std::size_t>(al), n
   );
}
// (others omitted for brevity)
struct alignas(16) Float4 { float vals[4]; };
int main() {
   auto p = new Float4; // calls operator new(size_t)
   // calls operator new(size_t, align_val_t)
   auto q = new(std::align_val_t{ 16 }) Float4;
   // leaks, of course, but that's beside the point
}
```

The memory block allocated for **p** in this example will be aligned on a boundary of **std::max_align_t**, whereas the memory block allocated for **q** will be aligned on a 16-byte boundary.

## Destroying Delete

C++20 brings a novel and highly specialized feature called destroying **delete**. The use case targeted here is a member function overload that benefits from specific knowledge of the type of object being destroyed in order to better perform the destruction process. When that member function is defined for some type **T**, it is preferred over other options when **delete** is invoked on a **T***, even if **T** exposes another overload of **operator delete()**. To use destroying **delete** for some type **X**, one must implement the following member function:

```cpp
class X {
   // ...
public:
   void operator delete(X*, std::destroying_delete_t);
   // ...
};
```

Here, **std::destroying_delete_t** is a tag type like **std::nothrow_t**. Note that the first argument of the destroying **delete** for class **X** is an **X***, not a **void***, as the destroying **delete** has the double role of destroying the object and deallocating memory… hence its name!

Let’s look at a concrete example with the following **Wrapper** class. In this example, an object of type **Wrapper** hides one of two implementations, modeled by **Wrapper::ImplA** and **Wrapper::ImplB**. The implementation is selected at construction time based on an enumerated value of type **Wrapper::Kind**. The intent is to remove the need for **virtual** functions from this class, replacing them with **if** statements based on the kind of implementation that was chosen. Of course, in this (admittedly) small example, there’s still only one **virtual** function (**Impl::f()**) as we aim to minimize the example’s complexity. There is also a wish to keep the destructor of class **Wrapper** trivial, a property that can be useful on occasion.

First, let’s examine the basic structure of **Wrapper** including **Wrapper::Kind**, **Wrapper::Impl**, and its derived classes:

```cpp
#include <new>
#include <iostream>
class Wrapper {
public:
   enum class Kind { A, B };
private:
   struct Impl {
      virtual int f() const = 0;
   };
   struct ImplA final : Impl {
      int f() const override { return 3; }
      ~ImplA() { std::cout << "Kind A\n"; }
   };
   struct ImplB final : Impl {
      int f() const override { return 4; }
      ~ImplB() { std::cout << "Kind B\n"; }
   };
   Impl *p;
   Kind kind;
   // ...
```

Visibly, **Wrapper::Impl** does not have a **virtual** destructor, yet **Wrapper** keeps as a data member an **Impl*** named **p**, which means that simply calling **delete p** might not call the appropriate destructor for the pointed-to object.

The **Wrapper** class exposes a constructor that takes a **Kind** as argument, then calls **Wrapper::create()** to construct the appropriate implementation, modeled by a type derived from **Impl**:

```cpp
 // ...
   static Impl *create(Kind kind) {
      switch(kind) {
      using enum Kind;
      case A: return new ImplA;
      case B: return new ImplB;
      }
      throw 0;
   }
public:
   Wrapper(Kind kind)
      : p{ create(kind) }, kind{ kind } {
   }
   // ...
```

Now comes the destroying **delete**. Since we know by construction that the only possible implementations would be **ImplA** and **ImplB**, we test **p->kind** to know which one was chosen for **p**, then directly call the appropriate destructor. Once that is done, the **Wrapper** object itself is finalized and memory is freed through a direct call to **operator delete()**:

```cpp
 // ...
   void operator delete(Wrapper *p,
                        std::destroying_delete_t) {
      if(p->kind == Kind::A) {
         delete static_cast<ImplA*>(p->p);
      } else {
         delete static_cast<ImplB*>(p->p);
      }
      p->~Wrapper();
      ::operator delete(p);
   }
   int f() const { return p->f(); }
};
```

For client code, the fact that we decided to use a destroying **delete** is completely transparent:

```cpp
int main() {
   using namespace std;
   auto p = new Wrapper{ Wrapper::Kind::A };
   cout << p->f() << endl;
   delete p;
   p = new Wrapper{ Wrapper::Kind::B };
   cout << p->f() << endl;
   delete p;
}
```

# Forward List Implementation

```cpp
// also available live: https://wandbox.org/permlink/vQczKJu2kH2CTXcZ

#include <cstddef>
#include <algorithm>
#include <utility>
#include <iterator>
#include <initializer_list>
#include <concepts>
#include <memory> // <--

template <class T>
class ForwardList {
public:
   using value_type = T;
   using size_type = std::size_t;
   using pointer = T*;
   using const_pointer = const T*;
   using reference = T&;
   using const_reference = const T&;
private:
   struct Node {
      value_type value;
      Node *next = nullptr;
      Node(const_reference value) : value { value } {
      }
      Node(value_type &&value) : value { std::move(value) } {
      }
   };
   struct deleter { // <--
      void operator()(Node *p) const {
         while(p) {
            Node *q = p->next;
            delete p;
            p = q;
         }
      }
   };
   std::unique_ptr<Node, deleter> head;
   size_type nelems {};
   // ...
public:
   size_type size() const { return nelems; }
   bool empty() const { return !head; }
private:
   template <class U> class Iterator {
   public:
      using value_type = typename ForwardList<T>::value_type;
      using pointer = typename ForwardList<T>::pointer;
      using reference = typename ForwardList<T>::reference;
      using difference_type = std::ptrdiff_t;
      using iterator_category = std::forward_iterator_tag;
      friend class Iterator<T>;
      friend class Iterator<const T>;
   private:
      Node *cur {};
   public:
      Iterator() = default;
      Iterator(Node *p) : cur { p } {
      }
      Iterator& operator++() {
         cur = cur->next;
         return *this;
      }
      Iterator operator++(int) {
         auto temp = *this;
         operator++();
         return temp;
      }
      bool operator==(const Iterator &other) const {
         return cur == other.cur;
      }
      // not needed since C++20
      bool operator!=(const Iterator &other) const {
         return !(*this == other);
      }
      U& operator*() { return cur->value; }
      const U& operator*() const { return cur->value; }
      U* operator->() { return cur->value; }
      const U* operator->() const { return cur->value; }
      friend class ForwardList<U>;
   };
public:
   using iterator = Iterator<T>;
   using const_iterator = Iterator<const T>;
   iterator begin() { return { head.get() }; } // <--
   const_iterator begin() const { return { head.get() }; } // <--
   const_iterator cbegin() const { return begin(); }
   iterator end() { return {}; }
   const_iterator end() const { return {}; }
   const_iterator cend() const { return end(); }
   void clear() noexcept {
      for(auto p = head.get(); p; ) { // <--
         auto q = p->next;
         delete p;
         p = q;
      }
      nelems = 0;
   }
   // ...
   ForwardList() = default;
   template <std::forward_iterator It>
      ForwardList(It b, It e) {
         if(b == e) return;
         head.reset(new Node{ *b }); // <--
         auto q = head.get(); // <--
         ++nelems;
         for(++b; b != e; ++b) {
            q->next = new Node{ *b };
            q = q->next;
            ++nelems;
         }
      }
   ForwardList(const ForwardList& other)
      : ForwardList(other.begin(), other.end()) {
   }
   ForwardList(std::initializer_list<T> other)
      : ForwardList(other.begin(), other.end()) {
   }
   ForwardList(ForwardList&& other) noexcept
      : head{ std::exchange(other.head, nullptr) },
        nelems{ std::exchange(other.nelems, 0) } {
   }
   // ...
   // ...
   ~ForwardList() = default;
   // ...
   void swap(ForwardList& other) noexcept {
      using std::swap;
      swap(head, other.head);
      swap(nelems, other.nelems);
   }
   ForwardList& operator=(const ForwardList& other) {
      ForwardList{ other }.swap(*this);
      return *this;
   }
   ForwardList& operator=(ForwardList&& other) {
      ForwardList{ std::move(other) }.swap(*this);
      return *this;
   }
   // ...
   reference front() { return head->value; }
   const_reference front() const { return head->value; }
   bool operator==(const ForwardList &other) const {
      return size() == other.size() &&
             std::equal(begin(), end(), other.begin());
   }
   // can be omitted since C++20
   bool operator!=(const ForwardList &other) const {
      return !(*this == other);
   }
   // ...
   void push_front(const_reference val) {
      auto p = new Node{ val };
      p->next = head.get(); // <--
      head.release(); // <--
      head.reset(p); // <--
      ++nelems;
   }
   void push_front(T&& val) {
      auto p = new Node{ std::move(val) };
      p->next = head.get(); // <--
      head.release(); // <--
      head.reset(p); // <--
      ++nelems;
   }
   // ...
   iterator insert_after(iterator pos, const_reference value) {
      auto p = new Node{ value };
      p->next = pos.cur->next;
      pos.cur->next = p;
      ++nelems;
      return { p };
   }
   template <std::input_iterator It>
      iterator insert_after(iterator pos, It b, It e) {
         for(; b != e; ++b)
            pos = insert_after(pos, *b);
         return pos;
      }
};

#include <iostream>

template <class T>
std::ostream& operator<<(std::ostream& os, const ForwardList<T>& lst) {
   if (lst.empty()) return os;
   os << lst.front();
   for (auto p = std::next(lst.begin()); p != lst.end(); ++p)
      os << ',' << *p;
   return os;
}

int main() {
   ForwardList lst0{ 2,3,5,7,11 };
   ForwardList lst1 = lst0; // copy ctor
   std::cout << lst1 << '\n'; // 2,3,5,7,11
   for (int n : { 13, 17, 19, 23, 29, 31, 37, 41, 43, 47 })
      lst0.push_front(n);
   // Size: 15
   // 47,43,41,37,31,29,23,18,17,13,2,3,5,7,11
   std::cout << "Size: " << lst0.size() << '\n'
             << lst0 << '\n';
   // Size: 5
   // 2,3,5,7,11
   std::cout << "Size: " << lst1.size() << '\n'
             << lst1 << '\n';
   auto pos = lst1.insert_after(lst1.begin(), -1);
   // Size: 6
   // 2,-1,3,5,7,11
   std::cout << "Size: " << lst1.size() << '\n'
             << lst1 << '\n';
   int arr[]{ -2, -3, -4 };
   lst1.insert_after(pos, std::begin(arr), std::end(arr));
   // Size: 9
   // 2,-1,-2,-3,-4,3,5,7,11
   std::cout << "Size: " << lst1.size() << '\n'
             << lst1 << '\n';
}
```
