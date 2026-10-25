# Order-book review and test instructions

## Observed validation results

Compiled the unmodified implementation and the generated suite with GCC 13.3.0 in WSL, C++20, fmt 11.1.4, AddressSanitizer and UndefinedBehaviorSanitizer. All 53 cases ran in separate processes: **15 passed, 38 failed**. The compiler emitted only the implementation's unused `qty` parameter warning at line 97.

Sanitizers directly confirmed heap-use-after-free at line 54, null Level access at line 82, and signed quantity overflow at line 94. Assertion failures directly confirmed partial-fill depth errors, duplicate-ID executions, incoming-limit execution prices, negative same-price modifications, and no-op FIFO loss. Some cases aborted on the lifetime defect before reaching their remaining assertions; 38 failing cases do not mean 38 distinct bugs. Nonpositive-price and no-op-priority expectations depend on the contract below.

The supplied files were retrieved from the referenced conversation's attachments. These copies are unmodified; line references below refer to `order_book.cpp` in this folder.

## Definite correctness defects

1. **Use-after-free when a level becomes empty — lines 42, 46, 54.** Erasing the map entry destroys its `unique_ptr<Level>`. Line 54 subsequently writes through the dangling `order->ptr_level`. This affects cancellation, complete fills, zero-quantity modifications, and repricing/removing the sole order at a level. Subtract the quantity before erasing the level, and never access the level after erasure. Reproductions: `bid_cancel_only_level`, `ask_full_fill_last_order`.

2. **Partial fills corrupt aggregate depth — lines 138 and 172.** These branches reduce `Order::qty` but leave `Level::total_qty` unchanged. Example: a resting quantity of 10 filled for 4 still displays depth 10 instead of 6. Cancelling that remainder subtracts only 6, leaving phantom quantity if another order remains at the level. Subtract `filled_qty` from the level aggregate in each partial-fill branch. Reproductions: `bid_partial_fill_depth`, `ask_partial_then_cancel`.

3. **Duplicate live IDs are checked after trades — lines 186–197 and 61–64.** `add_order` matches first; only residual insertion checks uniqueness. A duplicate can consume opposite-side liquidity, and any positive remainder is silently discarded. Example: ID 1 rests as bid 90 x 5 and ID 2 as ask 100 x 4; adding ID 1 as bid 100 x 7 executes ID 2 and discards the residual 3. Reject duplicates before matching, preserving both sides. Reproductions: `bid_duplicate_crossing`, `bid_duplicate_crossing_other_order`.

4. **Invalid side can dereference null or execute trades — lines 186–192 and 70–82.** Any non-Buy side takes the sell matching path. If a remainder exists, the insertion switch never initializes `ptr_level`, and line 82 dereferences null. A fully executed invalid-side order can instead return trades without reaching the null dereference. Validate the enum before matching. Reproductions: `invalid_side_empty`, `invalid_side_crossing`.

5. **Negative modification creates invalid resting quantities or deletes orders — lines 207–224.** At the same price a negative quantity enters the reduction branch, making the resting quantity and aggregate negative. Subsequent matching can produce negative fills and increase incoming quantity. With a changed price, the existing order is removed and the negative replacement is silently dropped. Validate quantity before mutation; keep zero as cancellation. Reproductions: both `negative_modify_*` cases for each side.

6. **Signed aggregate overflow — line 94 (also possible at 217).** Adding `INT64_MAX` and then 1 at one level overflows `int64_t`, which is undefined behavior. Modification subtraction can also overflow for extreme negative input. Validate input and check aggregate arithmetic before insertion/modification. Reproductions: `bid_quantity_overflow`, `ask_quantity_overflow`.

## Findings dependent on the intended contract

7. **Execution price is the incoming limit — lines 128 and 164.** Under the standard resting-order-price convention, use `cur_bid` or `cur_ask`. An incoming buy limit 110 against ask 100 currently reports 110. Sweeping multiple levels reports the same incoming limit for every trade. The tests assume resting-order prices; confirm this convention if the book intentionally uses a different pricing rule.

8. **An unchanged modification loses FIFO — lines 215–224.** Only a strict quantity reduction retains priority. Repeating the current price and quantity removes/reinserts the order behind peers. Tests assume a no-op retains priority; this is a policy question if every replace intentionally resets priority.

9. **Nonpositive prices are accepted — lines 182–198, 203–224.** Tests assume positive limit prices and atomic rejection of nonpositive prices. The API has no documented price domain; instruments supporting zero or negative prices should change these test expectations. Negative adds currently silently do nothing; tests permit a no-op or a standard validation exception, but never mutation.

## Test suite

`order_book_tests.cpp` contains 53 independently selectable black-box cases. It uses no test framework and checks public trade/depth results. Buy and sell cases cover noncrossing orders, exact and price-improved crossing, FIFO, partial/full fills, price sweeps and limit stops, residual resting orders, removal from all linked-list positions, cancellation/repeated cancellation, missing IDs, all modification priority cases, repricing/matching, duplicate IDs, invalid side/quantity/price, aggregate overflow, extreme prices/quantities/IDs, ID reuse, depth ordering/counts/default five levels, and zero/negative depth limits.

Rejected operations may throw `invalid_argument`, `domain_error`, or `overflow_error`, or return an empty trade vector without changing public depth. Other exceptions fail the test. The suite expects one Trade per resting fill, with the resting ID. Market-order tests are omitted because the public API accepts only an integer limit price; the private optional-price helpers are not directly accessible.

Use C++20: aggregate parenthesized initialization and `std::cmp_less`/`std::ssize` require it. Supply fmt headers supporting `fmt::println` (this review used fmt 11.1.4). From this folder on Linux/WSL:

```sh
FMT_INCLUDE=/path/to/fmt/include sh run_tests.sh
```

If fmt is already on the compiler's header search path, omit `FMT_INCLUDE`. The runner builds with AddressSanitizer and UndefinedBehaviorSanitizer, lists the cases, and runs each in a separate process. A sanitizer abort therefore does not suppress later tests. See `test_results.txt`, `build.log`, and the per-case files in `test_logs` for actual results. The runner returns nonzero on any failure. Run `./order_book_tests CASE_NAME` to reproduce one case, or `--list` to list names. Running the entire suite in one process can abort at the first memory bug.

Fix the lifetime and input/overflow defects before interpreting later behavioral failures; sanitizer aborts can mask a test's subsequent assertions. No implementation fixes were made as part of this review.
