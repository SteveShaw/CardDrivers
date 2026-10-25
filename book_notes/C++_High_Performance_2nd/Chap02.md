## Data structures that can be used by generic algorithms

```cpp
struct Grid {
  Grid(std::size_t w, std::size_t h) : w_{w}, h_{h} {    data_.resize(w * h); 
  }
  auto get_row(std::size_t y); // Returns iterators or a range

  std::vector<int> data_{};
  std::size_t w_{};
  std::size_t h_{};
};
```

A possible implementation of `get_row()` would return a `std::pair` holding iterators that represent the beginning and the end of the row:

```cpp
auto Grid::get_row(std::size_t y) {
  auto left = data_.begin() + w_ * y;
  auto right = left + w_;
  return std::make_pair(left, right);
}
```

The iterator pair representing a row can then be utilized by standard library algorithms. 

```cpp
auto grid = Grid{10, 10};
auto y = 3;
auto row = grid.get_row(y);
std::generate(row.first, row.second, std::rand);
auto num_fives = std::count(row.first, row.second, 5);
```

While this works, it is a bit clumsy to work with `std::pair`, and it also requires the client to know how to handle the iterator pair. There is nothing that explicitly says that the first and second members actually denote a half-open range.

Fortunately, the Ranges library provides us with a view type called `std::ranges::subrange`. Now, the `get_row()` function could be implemented like this:

```cpp
auto Grid::get_row(std::size_t y) {
  auto first = data_.begin() + w_ * y;
  auto sentinel = first + w_;
  return std::ranges::subrange{first, sentinel};
}
```

We could use the handy view that is tailor-made for this scenario, called `std::views::counted()`

```cpp
auto Grid::get_row(std::size_t y) {
  auto first = data_.begin() + w_ * y;
  return std::views::counted(first, w_);
}
```

A row returned from the Grid class could now be used with any of the constrained algorithms that accept ranges instead of iterator pairs:

```cpp
auto row = grid.get_row(y);
std::ranges::generate(row, std::rand);
auto num_fives = std::ranges::count(row, 5);
```

## Sorting only for the data you need to retrieve

`partial_sort()` and `nth_element()` could be thought of as algorithms for inspecting parts of that sorted range. In many cases, you are only interested in a certain part of the sorted range, for example:

* If you want to calculate the median of a range, you require the value in the middle of the sorted range.
* If you want to create a body scanner that can be used by the mean 80% by height of a population, you require two values in the sorted range: the value located 10% from the tallest person, and the value located 10% from the shortest person.

```cpp
auto v = std::vector{6, 3, 2, 7,
                     4, 1, 5};
auto it = v.begin() + v.size()/2;
```

![](C:\Users\Chi-O\AppData\Roaming\marktext\images\2026-10-11-17-08-46-image.png)

```cpp
std::nth_element(v.begin(), it, v.end());
```

![](C:\Users\Chi-O\AppData\Roaming\marktext\images\2026-10-11-17-09-30-image.png)

```cpp
std::partial_sort(v.begin(), it,v.end());
```

![](C:\Users\Chi-O\AppData\Roaming\marktext\images\2026-10-11-17-10-01-image.png)

## Use standard algorithms over raw for-loops

Let's say we need a function that moves the first n elements from the front of a container to the back, like this:

![](C:\Users\Chi-O\AppData\Roaming\marktext\images\2026-10-11-17-13-51-image.png)

A naive approach would be to copy the first $n$ elements to the back while iterating over them and then erasing the first $n$ elements:

![](C:\Users\Chi-O\AppData\Roaming\marktext\images\2026-10-11-17-14-08-image.png)

Here's the corresponding implementation:

```cpp
template <typename Container>
auto move_n_elements_to_back(Container& c, std::size_t n) {
  // Copy the first n elements to the end of the container
  for (auto it = c.begin(); it != std::next(c.begin(), n); ++it) {
    c.emplace_back(std::move(*it));
  }
  // Erase the copied elements from front of container
  c.erase(c.begin(), std::next(c.begin(), n));
}
```

At first glance, it might look plausible, but inspecting it reveals a severe problem: if the container reallocates during the iteration due to `emplace_back()`, the iterator it will no longer be valid. As the algorithm tries to access an invalid iterator, the algorithm will go into undefined behavior and, in the best case, crash. 

As undefined behaviors are an obvious problem, we'll have to rewrite the algorithm. We are still using a handcrafted for-loop, but we'll utilize the index instead of the iterator:

```cpp
template <typename Container>
auto move_n_elements_to_back(Container& c, std::size_t n) {
  for (size_t i = 0; i < n; ++i) {
    auto value = *std::next(c.begin(), i);
    c.emplace_back(std::move(value));
  }
  c.erase(c.begin(), std::next(c.begin(), n));
}
```

The solution works; it doesn't crash anymore. But now, it has a subtle performance problem. The algorithm is significantly slower on `std::list` than on `std::vector`. The reason is that `std::next(it, n)` used with `std::list::iterator` is *O(n)*, and *O(1)* on a `std::vector::iterator`. As `std::next(it, n)` is invoked in every step of the `for`-loop, this algorithm will have a time complexity of $O(n^2)$ on containers such as `std::list`. Apart from this performance limitation, the preceding code also has the following limitations:

- It doesn't work with containers of a static size, such as `std::array`, due to `emplace_back()`
- It might throw an exception, since `emplace_back()` may allocate memory and fail (this is probably rare though)

When we have reached this stage, we should browse through the standard library and see whether it contains a suitable algorithm to be used as a building block. Conveniently, the <algorithm> header provides an algorithm called `std::rotate()`, which does exactly what we are looking for while avoiding all the disadvantages mentioned before. Here is the final version using the `std::rotate()` algorithm:

```cpp
template <typename Container>
auto move_n_elements_to_back(Container& c, std::size_t n) {
  auto new_begin = std::next(c.begin(), n);
  std::rotate(c.begin(), new_begin, c.end());
}
```

Let's have a look at the advantages of using `std::rotate()`:

* The algorithm does not throw exceptions, as it does not allocate memory (the contained object might throw exceptions though)
* It works with containers whose size cannot be changed, such as std::array
* Performance is $O(n)$ regardless of the container it operates on
  The implementation may very well be optimized with specific hardware in mind
