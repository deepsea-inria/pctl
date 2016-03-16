/*!
  \file
  \brief File contains implementations of map function on array with different arguments.
*/

#include <algorithm>
#include "array.hpp"
#include <iostream>
#include <functional>

#ifndef _PARUTILS_MAP_H_
#define _PARUTILS_MAP_H_

namespace parutils {
namespace array {
namespace utils {

template <template <class Item> class Array, template <class Item> class ResultArray, class ItemIn, class ItemOut, class Map_fct>
void map_serial(Array<ItemIn>& items, int l, int r, ResultArray<ItemOut>& result, int result_offset, const Map_fct& map_fct) {
 for (int i = l; i < r; ++i) {
   result.at(i - l + result_offset) = map_fct(items.at(i));
 }
}

/*!
  Maps elements of given array in certain range into specified array.
  \param items array of elements
  \param l start position of range
  \param r end position of range (exclusive)
  \param result array to contain result of map
  \param result_offset position in result array from where start to write
  \param map_fct function to map with
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class ItemIn, class ItemOut, class Map_fct>
void map(Array<ItemIn>& items, int l, int r, ResultArray<ItemOut>& result, int result_offset, const Map_fct& map_fct) {
  pasl::pctl::parallel_for(l, r, [&] (int i) {
    result.at(i - l + result_offset) = map_fct(items.at(i));
  });
}

/*!
  Maps elements of given array using given function in specified array.
  \param items array of elements
  \param result array to contain result of map
  \param map function to map with
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class ItemIn, class ItemOut, class Map_fct>
void map(Array<ItemIn>& items, ResultArray<ItemOut>& result, const Map_fct& map_fct) {
  map(items, 0, items.size(), result, 0, map_fct);
}

/*!
  Maps elements of given array using given function.
  \param items array of elements
  \param map function to map with
  \result array, which contains result of map
*/
template <template <class Item> class Array, class ItemIn, class Map_fct, typename ItemOut = typename std::result_of<Map_fct&(ItemIn)>::type>
array<ItemOut> map(Array<ItemIn>& items, const Map_fct& map_fct) {
  array<ItemOut> result(items.size());
  map(items, result, map_fct);
  return result;
}

/*!
  Maps elements of given array using given function into the same array.
  \param items array of elements
  \param map function to map with
*/
template <template <class Item> class Array, class ItemIn, class ItemOut>
void inplace_map(Array<ItemIn>& items, std::function<ItemOut(ItemIn const &)>& map_fct) {
  map(items, items, map_fct);
}

} //end namespace
} //end namespace
} //end namespace

#endif