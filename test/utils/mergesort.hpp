/*!
  \file File contains implementation of merge function of two arrays in O(sqrt(n + m)) parallel time.
*/

#include <algorithm>
#include "ploop.hpp"
#include "defines.hpp"

#ifndef _PARUTILS_MERGESORT_H_
#define _PARUTILS_MERGESORT_H_

namespace parutils {
namespace array {
namespace utils {

constexpr char merge_sort_file[] = "mergesort";
                                                                                                           

/*!
  Sequentially sort certain range of array with classic mergesort algorithm and stores its result into given array (which could be the original one) using given temporary array.
  \param a array of elements
  \param left start position of range
  \param right end position of range (exclusive)
  \param result_array to contain result of sort
  \param result_offset position in result array from where start to write
  \param tmp_array temporary array to hold the temporary work (should be at least the same size as (right - left))
  \param tmp_offset starting from this offset the values of tmp_array could be used
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Compare_fct>
void merge_sort_seq(Array<Item>& a, int_t left, int_t right, ResultArray<Item>& result, int_t result_offset, TmpArray<Item>& tmp_array, int_t tmp_offset, const Compare_fct& compare) {
  if (left == right) {
    return;
  }
  if (left + 1 == right) {
    result.at(result_offset) = a.at(left);
    return;
  }
  int_t mid = (left + right) >> 1;
//  std::cerr << left << " " << mid << " " << right << std::endl;
  merge_sort_seq(a, left, mid, result, result_offset, tmp_array, tmp_offset, compare);
  merge_sort_seq(a, mid, right, result, result_offset + mid - left, tmp_array, tmp_offset + mid - left, compare);
  merge_two_parts(result, left, mid, result, mid, right, tmp_array, tmp_offset, compare);
  for (int_t i = 0; i < right - left; i++) {
    result.at(result_offset + i) = tmp_array.at(tmp_offset + i);
  }
}

/*!
  Sequentially sort certain range of array using classic mergesort and stores its result into given array (which could be the original one) using given temporary array.
  \param a array of elements
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
  \return sorted array
*/
template <template <class Item> class Array, class Item, class Compare_fct>
array<Item> merge_sort_seq(Array<Item>& a, const Compare_fct& compare) {
  array<Item> result(a.size());
  array<Item> tmp(a.size());
  merge_sort_seq(a, 0, a.size(), result, 0, tmp, 0, compare);
  return result;
}

/*!
  Sort certain range of array with classic mergesort algorithm and stores its result into given array (which could be the original one) using given temporary array.
  \param a array of elements
  \param left start position of range
  \param right end position of range (exclusive)
  \param result_array to contain result of sort
  \param result_offset position in result array from where start to write
  \param tmp_array temporary array to hold the temporary work (should be at least the same size as (right - left))
  \param tmp_offset starting from this offset the values of tmp_array could be used
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Compare_fct>
void merge_sort(Array<Item>& a, int_t left, int_t right, ResultArray<Item>& result, int_t result_offset, TmpArray<Item>& tmp_array, int_t tmp_offset, const Compare_fct& compare) {
  if (left == right) {
    return;
  }
  if (left + 1 == right) {
    result.at(result_offset) = a.at(left);
    return;
  }
  using controller_type = pasl::pctl::granularity::controller_holder<merge_sort_file, 1, Array<Item>, ResultArray<Item>, TmpArray<Item>, Compare_fct>;
  pasl::pctl::granularity::cstmt(controller_type::controller, [&] { return right - left; }, [&] {
    int_t mid = (left + right) >> 1;
    pasl::pctl::granularity::fork2([&] {
      merge_sort(a, left, mid, result, result_offset, tmp_array, tmp_offset, compare);
    }, [&] {
      merge_sort(a, mid, right, result, result_offset + mid - left, tmp_array, tmp_offset + mid - left, compare);
    });
    merge(result, left, mid, result, mid, right, tmp_array, tmp_offset, compare);
    pasl::pctl::parallel_for(0, right - left, [&] (int_t i) {
      result.at(result_offset + i) = tmp_array.at(tmp_offset + i);
    });
  }, [&] {
    merge_sort_seq(a, left, right, result, result_offset, tmp_array, tmp_offset, compare);
  });
}

/*!
  Sort array with classic mergesort algorithm and stores its result into given array (which could be the original one) using given temporary array.
  \param a array of elements
  \param result_array to contain result of sort
  \param tmp_array temporary array to hold the temporary work (should be at least the same size as (right - left))
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class Item, class Compare_fct>
void merge_sort(Array<Item>& a, ResultArray<Item>& result, TmpArray<Item>& tmp_array, const Compare_fct& compare) {
  merge_sort(a, 0, a.size(), result, 0, tmp_array, 0, compare);
}

/*!
  Sort array with classic mergesort algorithm and stores its result into given array (which could be the original one).
  \param a array of elements
  \param result_array to contain result of sort
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class Item, class Compare_fct>
void merge_sort_no_tmp(Array<Item>& a, ResultArray<Item>& result, const Compare_fct& compare) {
  array<Item> tmp(a.size());
  merge_sort(a, result, tmp, compare);
}

/*!
  Sort array with classic mergesort algorithm using given temporary array.
  \param a array of elements
  \param tmp_array temporary array to hold the temporary work (should be at least the same size as (right - left))
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
  \return sorted array
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Compare_fct>
array<Item> merge_sort_no_result(Array<Item>& a, TmpArray<Item>& tmp_array, const Compare_fct& compare) {
  array<Item> result(a.size());
  merge_sort(a, result, compare);
  return result;
}

/*!
  Sort array with classic mergesort algorithm.
  \param a array of elements
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
  \return sorted array
*/
template <template <class Item> class Array, class Item, class Compare_fct>
array<Item> merge_sort(Array<Item>& a, const Compare_fct& compare) {
  array<Item> result(a.size());
  merge_sort_no_tmp(a, result, compare);
  return result;
}

} //end namespace utils
} //end namespace array
} //end namespace utils

#endif