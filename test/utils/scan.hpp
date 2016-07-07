/*!
  \file
  \brief File contains implementations of inclusive and exclusive scan functions on array with diffrerent arguments.
*/

#include <algorithm>
#include "reduce.hpp"
#include <iostream>

#ifndef _PARUTILS_SCAN_H_
#define _PARUTILS_SCAN_H_

namespace parutils {
namespace array {
namespace utils {

#define BLOCK_SIZE 1024

template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_exclusive_serial(Array<Item>& items, size_t l, size_t r, ResultArray<Item>& result, size_t result_offset, const Item& identity, const Multiply_fct& multiplication) {
  Item current = identity;
  for (int i = l; i < r; i++) {
//
    current = result.at(result_offset + i - l) = multiplication(current, items.at(i));  
  }
}

template <template <class Item> class Array, class Item, class Multiply_fct>
void scan_exclusive_serial(Array<Item>& items, size_t l, size_t r, const Item& identity, const Multiply_fct& multiplication) {
  scan_exclusive_serial(items, l, r, items, l, multiplication);
}

template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_inclusive_serial(Array<Item>& items, size_t l, size_t r, ResultArray<Item>& result, size_t result_offset, const Item& identity, const Multiply_fct& multiplication) {
  result.at(result_offset) = identity;
  scan_exclusive_serial(items, result, l + 1, r, result_offset + 1, multiplication);
}

/***
Scan exclusive functions.
***/

/*!
  Calculates the exclusive scan array (i.e. array of prefix sums without identity element first) of given multiplication function on elements
  of the given array in certain range using given temporary array and into specified array.
  \param items array of elements
  \param l start position of range
  \param r end position of range (exclusive)
  \param result array to contain result of scan
  \param result_offset position in result array from where start to write
  \param tmp_array temporary array to hold the temporary work (should be at least approximately 2 * (r - l) / BLOCK_SIZE length)
  \param tmp_offset starting from this offset the values of tmp_array could be used
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Multiply_fct>
void scan_exclusive(Array<Item>& items, size_t l, size_t r, ResultArray<Item>& result, size_t result_offset, TmpArray<Item>& tmp_array, size_t tmp_offset, const Item& identity, const Multiply_fct& multiplication) {
  size_t blocks = (r - l + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, l, r, result, result_offset, identity, multiplication);
    return;
  }

  pasl::pctl::parallel_for((size_t)0, blocks, [&] (size_t i) {
    tmp_array.at(tmp_offset + i) = reduce_serial(items, l + i * BLOCK_SIZE, l + std::min((i + 1) * BLOCK_SIZE, r - l), identity, multiplication);
//    if (blocks == 2)
//      std::cerr << tmp_array.at(tmp_offset + i) << std::endl;
  });
  scan_exclusive(tmp_array, tmp_offset, tmp_offset + blocks, tmp_array, tmp_offset, tmp_array, tmp_offset + blocks, identity, multiplication);
  pasl::pctl::parallel_for((size_t)0, blocks, [&] (size_t i) {
//    if (blocks == 2)
//      std::cerr << tmp_array.at(tmp_offset + i) << std::endl;
    scan_exclusive_serial(items, l + i * BLOCK_SIZE, l + std::min((i + 1) * BLOCK_SIZE, r - l), result, result_offset + i * BLOCK_SIZE, i == 0 ? identity : tmp_array.at(tmp_offset + i - 1), multiplication);
  });
}

/*!
  Calculates the exclusive scan array (i.e. array of prefix sums without identity element first) of given multiplication function on elements
  of the given array using given temporary array and into specified array.
  \param items array of elements
  \param result array to contain result of scan
  \param tmp_array temporary array to hold the temporary work (should be at least approximately 2 * items.size() / BLOCK_SIZE length)
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Multiply_fct>
void scan_exclusive(Array<Item>& items, ResultArray<Item>& result, TmpArray<Item>& tmp_array, const Item& identity, const Multiply_fct& multiplication) {
  size_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, result, 0, items.size(), 0, identity, multiplication);
    return;
  }
  scan_exclusive(items, 0, items.size(), result, 0, tmp_array, 0, identity, multiplication);
}

/*!
  Calculates the exclusive scan array (i.e. array of prefix sums without identity element first) of given multiplication function on elements
  of the given array into specified array. Uses additional 2 * items.size() / BLOCK_SIZE length temporary array.
  \param items array of elements
  \param result array to contain result of scan
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_exclusive_no_tmp(Array<Item>& items, ResultArray<Item>& result, const Item& identity, const Multiply_fct& multiplication) {
  size_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, 0, items.size(), result, 0, identity, multiplication);
    return;
  }
  array<Item> tmp_array(2 * blocks);
  scan_exclusive(items, 0, items.size(), result, 0, tmp_array, 0, identity, multiplication);
}

/*!
  Calculates the exclusive scan array (i.e. array of prefix sums without identity element first) of given multiplication function on elements
  of the given array using given temporary array.
  \param items array of elements
  \param tmp_array temporary array to hold the temporary work (should be at least approximately 2 * items.size() / BLOCK_SIZE length)
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
  \return array with results of scan
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
array<Item> scan_exclusive_no_result(Array<Item>& items, TmpArray<Item>& tmp_array, const Item& identity, const Multiply_fct& multiplication) {
  array<Item> result(items.size());
  scan_exclusive(items, 0, items.size(), result, 0, tmp_array, 0, identity, multiplication);
  return result;
}

/*!
  Calculates the exclusive scan array (i.e. array of prefix sums without identity element first) of given multiplication function on elements
  of the given array. Uses additional 2 * items.size() / BLOCK_SIZE length temporary array.
  \param items array of elements
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
  \return array with results of scan
*/
template <template <class Item> class Array, class Item, class Multiply_fct>
array<Item> scan_exclusive(Array<Item>& items, const Item& identity, const Multiply_fct& multiplication) {
  array<Item> result(items.size());
  scan_exclusive_no_tmp(items, result, identity, multiplication);
  return result;
}

/***
Scan inclusive functions.
***/

/*!
  Calculates the inclusive scan array (i.e. array of prefix sums with identity element first) of given multiplication function on elements
  of the given array using given temporary array and into specified array.
  \param items array of elements
  \param result array to contain result of scan
  \param tmp_array temporary array to hold the temporary work (should be at least approximately 2 * items.size() / BLOCK_SIZE length)
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Multiply_fct>
void scan_inclusive(Array<Item>& items, ResultArray<Item>& result, TmpArray<Item>& tmp_array, const Item& identity, const Multiply_fct& multiplication) {
  result[0] = identity;
  scan_exclusive(items, 0, items.size(), result, 1, tmp_array, 0, identity, multiplication);
}

/*!
  Calculates the inclusive scan array (i.e. array of prefix sums with identity element first) of given multiplication function on elements
  of the given array into specified array. Uses additional 2 * items.size() / BLOCK_SIZE length temporary array.
  \param items array of elements
  \param result array to contain result of scan
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Multiply_fct>
void scan_inclusive_no_tmp(Array<Item>& items, ResultArray<Item>& result, const Item& identity, const Multiply_fct& multiplication) {
  size_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    scan_exclusive_serial(items, 0, items.size(), result, 1, identity, multiplication);
    return;
  }
  array<Item> tmp_array(2 * blocks);
  result[0] = identity;
  scan_exclusive(items, 0, items.size(), result, 1, tmp_array, 0, identity, multiplication);
}

/*!
  Calculates the inclusive scan array (i.e. array of prefix sums with identity element first) of given multiplication function on elements
  of the given array using given temporary array.
  \param items array of elements
  \param tmp_array temporary array to hold the temporary work (should be at least approximately 2 * items.size() / BLOCK_SIZE length)
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
  \return array with results of scan
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
array<Item> scan_inclusive_no_result(Array<Item>& items, TmpArray<Item>& tmp_array, const Item& identity, const Multiply_fct& multiplication) {
  array<Item> result(items.size() + 1);
  result[0] = identity;
  scan_exclusive(items, 0, items.size(), result, 1, tmp_array, 0, identity, multiplication);
  return result;
}

/*!
  Calculates the inclusive scan array (i.e. array of prefix sums with identity element first) of given multiplication function on elements
  of the given array. Uses additional 2 * items.size() / BLOCK_SIZE length temporary array.
  \param items array of elements
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
  \return array with results of scan
*/
template <template <class Item> class Array, class Item, class Multiply_fct>
array<Item> scan_inclusive(Array<Item>& items, const Item& identity, const Multiply_fct& multiplication) {
  array<Item> result(items.size() + 1);
  scan_inclusive_no_tmp(items, result, identity, multiplication);
  return result;
}

} //end namespace
} //end namespace
} //end namespace

#endif