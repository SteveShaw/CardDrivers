#!/bin/sh
# Run from the folder containing order_book.cpp and order_book_tests.cpp.
# Requires a C++20 compiler and fmt headers (tested with fmt 11.1.4).
set -u
${CXX:-g++} -std=c++20 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer -fno-pie -no-pie -I. ${FMT_INCLUDE:+-I"$FMT_INCLUDE"} \
  order_book.cpp order_book_tests.cpp -o order_book_tests || exit 1
mkdir -p test_logs
./order_book_tests --list > test_logs/names.txt || exit 1
passed=0
failed=0
while IFS= read -r name; do
  if ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
      ./order_book_tests "$name" > "test_logs/$name.log" 2>&1; then
    echo "PASS $name"
    passed=$((passed+1))
  else
    echo "FAIL $name (see test_logs/$name.log)"
    failed=$((failed+1))
  fi
done < test_logs/names.txt
echo "$passed passed, $failed failed"
test "$failed" -eq 0
