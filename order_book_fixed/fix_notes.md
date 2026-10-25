# Fixed order book

Final validation: **59 passed, 0 failed**, with no compiler warnings or sanitizer errors. This includes the original 53 cases and six added regression cases.

The original 53 tests passed after these fixes. Six additional regression cases cover replacement capacity validation on both sides: an increase overflowing the current level, repricing into a full level, and a valid increase that must exclude the old order quantity from capacity calculations.

Changes:

- Subtract level quantity before deleting an empty level, avoiding use-after-free.
- Reduce aggregate level depth for partial fills.
- Execute at each resting order's price.
- Reject invalid sides, negative quantities, nonpositive limit prices, and duplicate live IDs before executing trades.
- Preserve FIFO for same-price reductions and unchanged modifications; increases and repricing lose priority.
- Preflight quantity/count capacity before adds or replacements; reject overflow without changing orders or depth. Capacity uses the post-match remainder and excludes the order being replaced.
- Use unsigned 64-bit dictionary keys to match the public order-ID type.
- Remove duplicate includes and the unused level-helper parameter.

Public semantics follow the existing test contract. Invalid add inputs and duplicate positive-quantity adds throw `std::invalid_argument`; capacity failures throw `std::overflow_error`. A zero-quantity add is a no-op after side/price validation. A zero-quantity modify cancels an existing order regardless of its requested price. Missing modify/cancel IDs are no-ops. Prices must be positive. Trades contain the resting order ID. Same-price unchanged modifications preserve FIFO. The API remains a limit-order API.

Build/run from this folder with C++20 and fmt headers (verified using GCC 13.3.0 and fmt 11.1.4 in WSL):

```sh
FMT_INCLUDE=/path/to/fmt/include sh run_tests.sh
```

The runner enables AddressSanitizer and UndefinedBehaviorSanitizer and runs each case separately. See `fixed_test_results.txt`, `fixed_build.log`, and `test_logs` for final validation. The original review and failing results are retained in the earlier `order_book_review_and_tests.zip`; that archive represents the pre-fix implementation.
