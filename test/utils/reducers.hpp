#include <algorithm>
#include "ploop.hpp"
#include <iostream>

#ifndef _PARUTILS_REDUCERS_H_
#define _PARUTILS_REDUCERS_H_

namespace parutils {
namespace reducers {

#define BLOCK_SIZE 1024

template <template <class Item> class Array, class Item, class Multiply_fct>
Item reduce_serial(Array<Item>& items, size_t l, size_t r, const Item& zero, const Multiply_fct& multiplication) {
  Item result = zero;
  for (int i = l; i < r; ++i) {
    result = multiplication(result, items.at(i));
  }
  return result;
}

template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_exclusive_serial(Array<Item>& items, size_t l, size_t r, ResultArray<Item>& result, size_t result_offset, const Item& zero, const Multiply_fct& multiplication) {
  Item current = zero;
  for (int i = l; i < r; i++) {
//
    current = result.at(result_offset + i - l) = multiplication(current, items.at(i));  
  }
}

template <template <class Item> class Array, class Item, class Multiply_fct>
void scan_exclusive_serial(Array<Item>& items, size_t l, size_t r, const Item& zero, const Multiply_fct& multiplication) {
  scan_exclusive_serial(items, l, r, items, l, multiplication);
}

template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_inclusive_serial(Array<Item>& items, size_t l, size_t r, ResultArray<Item>& result, size_t result_offset, const Item& zero, const Multiply_fct& multiplication) {
  result.at(result_offset) = zero;
  scan_exclusive_serial(items, result, l + 1, r, result_offset + 1, multiplication);
}

/***
Reduce functions.
***/

template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
Item reduce(Array<Item>& items, size_t l, size_t r, TmpArray<Item>& tmp_array, size_t tmp_offset, const Item& zero, const Multiply_fct& multiplication) {
  Item result = zero;
  size_t blocks = (r - l + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    return reduce_serial(items, l, r, zero, multiplication);
  }
  pasl::pctl::parallel_for((size_t)0, blocks, [&] (size_t i) {
    tmp_array.at(i + tmp_offset) = reduce_serial(items, l + i * BLOCK_SIZE, l + std::min((i + 1) * BLOCK_SIZE, r - l), zero, multiplication);
  });

  return reduce(tmp_array, tmp_offset, tmp_offset + blocks, tmp_array, tmp_offset + blocks, zero, multiplication);
}

template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
Item reduce(Array<Item>& items, TmpArray<Item>& tmp_array, const Item& zero, const Multiply_fct& multiplication) {
  return reduce(items, items.size(), tmp_array, 0, zero, multiplication);
}

template <template <class Item> class Array, class Item, class Multiply_fct>
Item reduce(Array<Item>& items, const Item& zero, const Multiply_fct& multiplication) {
  size_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    return reduce_serial(items, 0, items.size(), zero, multiplication);
  }
  array::array<Item> tmp_array(2 * blocks);
  return reduce(items, 0, items.size(), tmp_array, 0, zero, multiplication);
}

/***
Scan exclusive functions.
***/

template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Multiply_fct>
void scan_exclusive(Array<Item>& items, size_t l, size_t r, ResultArray<Item>& result, size_t result_offset, TmpArray<Item>& tmp_array, size_t tmp_offset, const Item& zero, const Multiply_fct& multiplication) {
  size_t blocks = (r - l + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, 0, r - l, result, 0, zero, multiplication);
    return;
  }

  pasl::pctl::parallel_for((size_t)0, blocks, [&] (size_t i) {
    tmp_array.at(tmp_offset + i) = reduce_serial(items, l + i * BLOCK_SIZE, l + std::min((i + 1) * BLOCK_SIZE, r - l), zero, multiplication);
//    if (blocks == 2)
  });
  scan_exclusive(tmp_array, tmp_offset, tmp_offset + blocks, tmp_array, tmp_offset, tmp_array, tmp_offset + blocks, zero, multiplication);
  pasl::pctl::parallel_for((size_t)0, blocks, [&] (size_t i) {
    scan_exclusive_serial(items, l + i * BLOCK_SIZE, l + std::min((i + 1) * BLOCK_SIZE, r - l), result, result_offset + i * BLOCK_SIZE, i == 0 ? zero : tmp_array.at(tmp_offset + i - 1), multiplication);
  });
}

template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Multiply_fct>
void scan_exclusive(Array<Item>& items, ResultArray<Item>& result, TmpArray<Item>& tmp_array, const Item& zero, const Multiply_fct& multiplication) {
  size_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, result, 0, items.size(), 0, zero, multiplication);
    return;
  }
  scan_exclusive(items, 0, items.size(), result, 0, tmp_array, 0, zero, multiplication);
}

template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_exclusive_no_tmp(Array<Item>& items, ResultArray<Item>& result, const Item& zero, const Multiply_fct& multiplication) {
  size_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, 0, items.size(), result, 0, zero, multiplication);
    return;
  }
  array::array<Item> tmp_array(2 * blocks);
  scan_exclusive(items, 0, items.size(), result, 0, tmp_array, 0, zero, multiplication);
}

template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
array::array<Item> scan_exclusive_no_result(Array<Item>& items, TmpArray<Item>& tmp_array, const Item& zero, const Multiply_fct& multiplication) {
  array::array<Item> result(items.size());
  scan_exclusive(items, 0, items.size(), result, 0, tmp_array, 0, zero, multiplication);
  return result;
}

template <template <class Item> class Array, class Item, class Multiply_fct>
array::array<Item> scan_exclusive(Array<Item>& items, const Item& zero, const Multiply_fct& multiplication) {
  array::array<Item> result(items.size());
  scan_exclusive_no_tmp(items, result, zero, multiplication);
  return result;
}

/***
Scan inclusive functions.
***/

template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Multiply_fct>
void scan_inclusive(Array<Item>& items, ResultArray<Item>& result, TmpArray<Item>& tmp_array, const Item& zero, const Multiply_fct& multiplication) {
  result[0] = zero;
  scan_exclusive(items, 0, items.size(), result, 1, tmp_array, 0, zero, multiplication);
}

template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_inclusive_no_tmp(Array<Item>& items, ResultArray<Item>& result, const Item& zero, const Multiply_fct& multiplication) {
  size_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, 0, items.size(), result, 1, zero, multiplication);
    return;
  }
  array::array<Item> tmp_array(2 * blocks);
  result[0] = zero;
  scan_exclusive(items, 0, items.size(), result, 1, tmp_array, 0, zero, multiplication);
}

template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
array::array<Item> scan_inclusive_no_result(Array<Item>& items, TmpArray<Item>& tmp_array, const Item& zero, const Multiply_fct& multiplication) {
  array::array<Item> result(items.size() + 1);
  result[0] = zero;
  scan_exclusive(items, 0, items.size(), result, 1, tmp_array, 0, zero, multiplication);
  return result;
}

template <template <class Item> class Array, class Item, class Multiply_fct>
array::array<Item> scan_inclusive(Array<Item>& items, const Item& zero, const Multiply_fct& multiplication) {
  array::array<Item> result(items.size() + 1);
  scan_inclusive_no_tmp(items, result, zero, multiplication);
  return result;
}

} //end namespace
} //end namespace

#endif