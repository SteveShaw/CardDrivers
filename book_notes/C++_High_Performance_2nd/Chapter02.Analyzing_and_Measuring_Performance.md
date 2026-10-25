## A microbenchmark example

There are various tools/libraries for benchmarking, but we will use **Google Benchmark**, https://github.com/google/benchmark, because of its widespread use, and as a bonus, it can also be easily tested online on the page http://quick-bench.com without the need for any installation.

Here is how a simple microbenchmark of `linear_search()` might look when using Google Benchmark:

```cpp
#include <benchmark/benchmark.h> // Non-standard header
#include <vector>
bool linear_search(const std::vector<int>& v, int key) { /* ... */ }
auto gen_vec(int n) { /* ... */ }
static void bm_linear_search(benchmark::State& state) {
  auto n = 1024;
  auto v = gen_vec(n);
  for (auto _ : state) {
    benchmark::DoNotOptimize(linear_search(v, n));
  }
}
BENCHMARK(bm_linear_search); // Register benchmarking function
BENCHMARK_MAIN();
```

Compiling and running this program will generate something like this:

```
-------------------------------------------------------------------
Benchmark                Time   CPU           Iterations
-------------------------------------------------------------------
bm_linear_search         361 ns 361 ns        1945664
```

The number of iterations reported in the rightmost column reports the number of times the loop needed to execute before a statistically stable result was achieved. The `state` object passed to our benchmarking function determines when to stop. The average time per iteration is reported in two columns: **Time** is the wall-clock time and **CPU** is the time spent on the CPU by the main thread. In this case they were the same, but if `linear_search()` had been blocked waiting for I/O (for example), the CPU time would have been lower than the wall-clock time.

The only code that is being measured is the code inside this loop:

```cpp
for (auto _ : state) {   // Only this loop is measured
  benchmark::DoNotOptimize(binary_search(v, n));
}
```

The `boolean` value returned from our search functions is wrapped inside `benchmark::DoNotOptimize()`. This is the mechanism used to ensure that the returned value is not optimized away, which could make the entire call to `linear_search()` disappear.

Now let's make this benchmark a little more interesting by varying the input size. We can do that by passing arguments to our benchmarking function using the state object. Here is how to do it:

```cpp
static void bm_linear_search(benchmark::State& state) {
  auto n = state.range(0);
  auto v = gen_vec(n);
  for (auto _ : state) {
    benchmark::DoNotOptimize(linear_search(v, n));
  }
}

BENCHMARK(bm_linear_search)->RangeMultiplier(2)->Range(64, 256);
```

This will start with an input size of 64 and double the size until it reaches 256. The test would generated the following output:

```cpp
-------------------------------------------------------------------
Benchmark                Time    CPU          Iterations
-------------------------------------------------------------------
bm_linear_search/64      17.9 ns 17.9 ns      38143169
bm_linear_search/128     44.3 ns 44.2 ns      15521161
bm_linear_search/256     74.8 ns 74.7 ns      8836955
```

As a final example, we will benchmark both the `linear_search()` and the `binary_search()` functions using a variable input size and also try to let the framework estimate the time complexity of our functions. This can be done by providing the input size to the state object using the `SetComplexityN()` function. The complete microbenchmark example looks like this:

```cpp
#include <benchmark/benchmark.h>
#include <vector>
bool linear_search(const std::vector<int>& v, int key) { /* ... */ }
bool binary_search(const std::vector<int>& v, int key) { /* ... */ }
auto gen_vec(int n) { /* ... */ }
static void bm_linear_search(benchmark::State& state) {
  auto n = state.range(0); 
  auto v = gen_vec(n);
  for (auto _ : state) { 
    benchmark::DoNotOptimize(linear_search(v, n)); 
  }
  state.SetComplexityN(n);
}
static void bm_binary_search(benchmark::State& state) {
  auto n = state.range(0); 
  auto v = gen_vec(n);
  for (auto _ : state) { 
    benchmark::DoNotOptimize(binary_search(v, n)); 
  }
  state.SetComplexityN(n);
}
BENCHMARK(bm_linear_search)->RangeMultiplier(2)->
  Range(64, 4096)->Complexity();
BENCHMARK(bm_binary_search)->RangeMultiplier(2)->
  Range(64, 4096)->Complexity();
BENCHMARK_MAIN();
```


