/*!
  \file
  \brief File contains implementation of filter functions on array with different arguments.
*/

#include <algorithm>
#include <iostream>
#include "defines.hpp"
#include "scan.hpp"

#ifndef _PARUTILS_FILTER_H_
#define _PARUTILS_FILTER_H_

namespace parutils {
namespace array {
namespace utils {

/*!
  Calculates the filter array (i.e. array of elements which suits given filter function) of elements
  of the given array in certain range using given temporary array and into specified array.
  \param items array of elements
  \param l start position of range
  \param r end position of range (exclusive)
  \param result array to contain result of scan
  \param result_offset position in result array from where start to write
  \param tmp_array temporary array to hold the temporary work (should be at least approximately (r - l) * (1 + 2 / BLOCK_SIZE) length)
  \param tmp_offset starting from this offset the values of tmp_array could be used
  \param filter function to choose elements with (given element, returns boolean)
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Filter_fct>
int_t filter(Array<Item>& items, int_t l, int_t r, ResultArray<Item>& result, int_t result_offset, TmpArray<int_t>& tmp_array, int_t tmp_offset, const Filter_fct& filter_fct) {
  pasl::pctl::parallel_for(l, r, [&] (int i) {
    tmp_array.at(i - l + tmp_offset) = filter_fct(items.at(i));
  });

  scan_exclusive(tmp_array, tmp_offset, tmp_offset + r - l, tmp_array, tmp_offset, tmp_array, tmp_offset + r - l, 0, [&] (int a, int b) { return a + b; });

  pasl::pctl::parallel_for(0, r - l, [&] (int i) {
    int_t pos = tmp_array.at(i + tmp_offset);
    if (pos == (i == 0 ? 0 : tmp_array.at(i - 1 + tmp_offset))) {
      return;
    }

    result.at(result_offset + pos - 1) = items.at(l + i);
  });

  return tmp_array.at(r - l - 1 + tmp_offset);
}

/*!
  Calculates the filter array (i.e. array of elements chosen by filter function) of elements
  of the given array using given temporary array and into specified array.
  \param items array of elements
  \param result array to contain result of scan
  \param tmp_array temporary array to hold the temporary work (should be at least approximately items.size() * (1 + 2 / BLOCK_SIZE) length)
  \param identity identity element for mulitplication function
  \param filter function to choose elements with (given element, returns boolean)
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Filter_fct>
int_t filter(Array<Item>& items, ResultArray<Item>& result, TmpArray<int_t>& tmp_array, const Filter_fct& filter_fct) {
  return filter(items, 0, items.size(), result, 0, tmp_array, 0, filter_fct);
}

/*!
  Calculates the filter array (i.e. array of elements chosen by filter function) of elements
  of the given array into specified array. Uses additional items.size() * (1 + 2 / BLOCK_SIZE) length temporary array.
  \param items array of elements
  \param result array to contain result of scan
  \param identity identity element for mulitplication function
  \param filter function to choose elements with (given element, returns boolean)
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Filter_fct>
int_t filter_no_tmp(Array<Item>& items, ResultArray<Item>& result, const Filter_fct& filter_fct) {
  int_t blocks = (items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  array<int_t> tmp_array(items.size() + 2 * blocks);
  return filter(items, result, tmp_array, filter_fct);
}

/*!
  Calculates the filter array (i.e. array of elements chosen by filter function) of elements
  of the given array using given temporary array.
  \param items array of elements
  \param tmp_array temporary array to hold the temporary work (should be at least approximately items.size() * (1 + 2 / BLOCK_SIZE) length)
  \param filter function to choose elements with (given element, returns boolean)
  \return array with results of filter
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Filter_fct>
array<Item> filter_no_result(Array<Item>& items, TmpArray<int_t>& tmp_array, const Filter_fct& filter_fct) {
  array<Item> result(items.size());
  int_t size = filter(items, result, tmp_array, filter_fct);
  return array<Item>(result, 0, size);
}

/*!
  Calculates the filter array (i.e. array of elements chosen by filter function) of elements
  of the given array. Uses additional items.size() * (1 + 2 / BLOCK_SIZE) length temporary array.
  \param items array of elements
  \param filter function to choose elements with (given element, returns boolean)
  \return array with results of scan
*/
template <template <class Item> class Array, class Item, class Filter_fct>
array<Item> filter(Array<Item>& items, const Filter_fct& filter_fct) {
  array<Item> result(items.size());
  int_t size = filter_no_tmp(items, result, filter_fct);
  return array<Item>(result, 0, size);
}
} //end namespace
} //end namespace
} //end namespace

#endif