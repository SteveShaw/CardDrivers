# Objects, Pointers, and References

## Representation of memory in C++

In C++, a byte is not necessarily an octet: a byte consists of at least eight bits but could be made of more (something that’s useful on some exotic hardware). The key idea here is that a byte is the smallest addressable unit of memory in a program.

### Objects, pointers, and references

One can compare the byte-per-byte representation of two objects, at least in some circumstances, to see whether they are equivalent:

```cpp
#include <iostream>
#include <type_traits>
using namespace std;
bool same_bytes(const char *p0, const char *p1,
                std::size_t n) {
    for(std::size_t i = 0; i != n; ++i)
        if(*(p0 + i) != *(p1 + i))
            return false;
    return true;
}
template <class T, class U>
   bool same_bytes(const T &a, const U &b) {
      using namespace std;
       static_assert(sizeof a == sizeof b);
       static_assert(has_unique_object_representations_v<
          T
       >);
       static_assert(has_unique_object_representations_v<
          U
       >);
       return same_bytes(reinterpret_cast<const char*>(&a),
                         reinterpret_cast<const char*>(&b),
                         sizeof a);
   }
struct X {
   int x {2}, y{3};
};
struct Y {
   int x {2}, y{3};
};
#include <cassert>
int main() {
    constexpr X x;
    constexpr Y y;
    assert(same_bytes(x, y));
}
```

The **has_unique_object_representations** trait is true for types uniquely defined by their values, that is, types exempt of padding bits.. That’s important as C++ does not say what happens to padding bits in an object, and performing a bit-per-bit comparison of two objects might yield surprising results. Note that objects of floating point types are not considered uniquely defined by their values as there are many distinct values that qualify as NaN, or `not a number`.

## Understanding the fundamental properties of objects

In C++, an object has a type and an address. 

Since C++20, if composition would be more appropriate than inheritance to describe the relation between two classes such as **X** and **Y**, a data member can be marked as **[[no_unique_address]]** to inform the compiler that this member, if it is an object of an **empty class**, does not have to occupy storage within the enclosing object. 

Compilers are not forced to comply, since attributes can be ignored:

```cpp
class X { };
class Y {
   char c;
   [[no_unique_address]] X x;
};
int main() {
   static_assert(sizeof(X) > 0);
   static_assert(sizeof(Y) == sizeof(char)); // <-- here
}
```

The alignment of an object (or of its type) tells us where that object can be placed in memory. The **char** type has an alignment of **1**, and as such one can place a **char** object literally anywhere (as long as one can access that memory). 

For an alignment of **2** (which is likely for type **short**), objects can only be placed at addresses that are a multiple of 2. More generally, if a type has an alignment of **n**, then objects of that type must be placed at an address that is a multiple of **n**. 

Note that alignment has to be a strictly positive power of 2; not respecting this rule incurs undefined behavior. 

The C++ language offers two operators related to alignment:

- The **alignof** operator, which yields the natural alignment of a type **T** or of an object of that type.
- The **alignas** operator, which lets programmers impose the alignment of an object. This can only reasonably increase the natural alignment of a type, not reduce it.

Sometimes, compiler add padding at the end of an object *because of arrays*. Elements of an array are contiguous in memory, and for that reason, it is important that each element of an array is properly aligned. In a case such as this, the trailing padding bytes in an object of class **X** ensure that if an element in an array of **X** objects is properly aligned, then the next element will be properly aligned too.


