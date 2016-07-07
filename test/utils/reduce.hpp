/*!
  \file
  \brief File contains implementations of reduce function on array with different arguments.
*/

#include <algorithm>
#include "ploop.hpp"
#include <iostream>
#include "defines.hpp"

#ifndef _PARUTILS_REDUCE_H_
#define _PARUTILS_REDUCE_H_

namespace parutils {
namespace array {
namespace utils {

template <template <class Item> class Array, class Item, class Multiply_fct>
Item reduce_serial(Array<Item>& items, int_t l, int_t r, const Item& identity, const Multiply_fct& multiplication) {
  Item result = identity;
  for (int i = l; i < r; ++i) {
    result = multiplication(result, items.at(i));
  }
//  std::cerr << "";
  return result;
}

/***
Reduce functions.
***/
/*!
  Calculates the value of given multiplication function on elements of the given array in certain range using given temporary array.
  \param items array of elements
  \param l start position of range
  \param r end position of range (exclusive)
  \param tmp_array temporary array to hold the temporary work (should be at least approximately 2 * (r - l) / BLOCK_SIZE length)
  \param tmp_offset starting from this offset the values of tmp_array could be used
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
Item reduce(Array<Item>& items, int_t l, int_t r, TmpArray<Item>& tmp_array, int_t tmp_offset, const Item& identity, const Multiply_fct& multiplication) {
  Item result = identity;
  int_t blocks = (r - l + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    return reduce_serial(items, l, r, identity, multiplication);
  }
  pasl::pctl::parallel_for((int_t)0, blocks, [&] (int_t i) {
    tmp_array.at(i + tmp_offset) = reduce_serial(items, l + i * BLOCK_SIZE, l + std::min((i + 1) * BLOCK_SIZE, r - l), identity, multiplication);
  });

  return reduce(tmp_array, tmp_offset, tmp_offset + blocks, tmp_array, tmp_offset + blocks, identity, multiplication);
}

/*!
  Calculates the value of given multiplication function on elements of the given array using given temporary array.
  \param items array of elements
  \param tmp_array temporary array to hold the temporary work (should be at least approximately 2 * items.size() / BLOCK_SIZE length)
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct>
Item reduce(Array<Item>& items, TmpArray<Item>& tmp_array, const Item& identity, const Multiply_fct& multiplication) {
  return reduce(items, items.size(), tmp_array, 0, identity, multiplication);
}

/*!
  Calculates the value of given multiplication function on elements of the given array.
  Uses additional 2 * items.size() / BLOCK_SIZE length temporary array.
  \param items array of elements
  \param identity identity element for mulitplication function
  \param multiplication multiplication function
*/
template <template <class Item> class Array, class Item, class Multiply_fct>
Item reduce(Array<Item>& items, const Item& identity, const Multiply_fct& multiplication) {
  int_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if (blocks == 1) {
    return reduce_serial(items, 0, items.size(), identity, multiplication);
  }
  array<Item> tmp_array(2 * blocks);
  return reduce(items, 0, items.size(), tmp_array, 0, identity, multiplication);
}

} //end namespace
} //end namespace
} //end namespace

#endif