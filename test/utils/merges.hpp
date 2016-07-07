/*!
  \file File contains implementation of merge function of two arrays in O(sqrt(n + m)) parallel time.
*/

#include <algorithm>
#include "ploop.hpp"
#include "defines.hpp"

#ifndef _PARUTILS_MERGE_H_
#define _PARUTILS_MERGE_H_

namespace parutils {
namespace array {
namespace utils {

constexpr char merge_file[] = "merge";

/*!
  Finds the largest element bigger than given in certain range of the given sorted array.
  \param a array in which to search
  \param left start position of range
  \param right end position of range (exclusive)
  \param x item to look for
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class Array, class Item, class Compare_fct>
int_t lower_bound(Array<Item>& a, int_t left, int_t right, Item& x, const Compare_fct& compare) {
  int_t l = left - 1;
  int_t r = right;
  while (l < r - 1) {
    int_t m = (l + r) >> 1;
    if (compare(a.at(m), x) <= 0) {
      l = m;
    } else {
      r = m;
    }
  }
  return l;
}

/*!
  Finds where to split certain ranges of two given sorted arrays, such that elements in left part are smaller than elements
  in right part and the number of elements in left part is exactly size.
  \param a first array to split
  \param a_l start position of range of array a
  \param a_r end position of range of array a (exclusive)
  \param b second array to split
  \param b_l start position of range of array b
  \param b_r end position of range of array b (exclusive)
  \param size the size of the left part of split
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, class Item, class Compare_fct>
std::pair<int_t, int_t> find(ArrayA<Item>& a, int_t a_l, int_t a_r, ArrayB<Item>& b, int_t b_l, int_t b_r, int_t size, const Compare_fct& compare) {
  int_t l = a_l - 1;
  int_t r = a_r;
  while (l < r - 1) {
    int_t m = (l + r) >> 1;
    if ((m - a_l + 1) + (lower_bound(b, b_l, b_r, a.at(m), compare) - b_l + 1) <= size) {
      l = m;
    } else {
      r = m;
    }
  }
  r = b_l - 1 + (size - (l - a_l + 1));
  return std::make_pair(l, r);
}

/*!
  Sequentially merges certain ranges of two given sorted arrays into specified array.
  \param a first array to merge
  \param a_l start position of range of array a
  \param a_r end position of range of array a (exclusive)
  \param b second array to megre
  \param b_l start position of range of array b
  \param b_r end position of range of array b (exclusive)
  \param result array to contain result of merge
  \param result_offset position in result array from where start to write
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, template <class Item> class ResultArray, class Item, class Compare_fct>
void merge_two_parts(ArrayA<Item>& a, int_t a_l, int_t a_r, ArrayB<Item>& b, int_t b_l, int_t b_r, ResultArray<Item>& result, int_t result_offset, const Compare_fct& compare) {
  while (a_l + b_l < a_r + b_r) {
    if (b_l == b_r) {
      result.at(result_offset++) = a.at(a_l++);
      continue;
    }
    if (a_l == a_r) {
      result.at(result_offset++) = b.at(b_l++);
      continue;
    }
    if (compare(a.at(a_l), b.at(b_l)) < 0) {
      result.at(result_offset++) = a.at(a_l++);
    } else {
      result.at(result_offset++) = b.at(b_l++);
    }
  }
}

/*!
  Merges certain ranges of two given sorted arrays into specified array. Uses algorithm with blocks.
  \param a first array to merge
  \param a_l start position of range of array a
  \param a_r end position of range of array a (exclusive)
  \param b second array to merge
  \param b_l start position of range of array b
  \param b_r end position of range of array b (exclusive)
  \param result array to contain result of merge
  \param result_offset position in result array from where start to write
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, template <class Item> class ResultArray, class Item, class Compare_fct>
void merge(ArrayA<Item>& a, int_t a_l, int_t a_r, ArrayB<Item>& b, int_t b_l, int_t b_r, ResultArray<Item>& result, int_t result_offset, const Compare_fct& compare) {
  using controller_type = pasl::pctl::granularity::controller_holder<merge_file, 1, ArrayA<Item>, ArrayB<Item>, ResultArray<Item>, Compare_fct>;
  pasl::pctl::granularity::cstmt(controller_type::controller, [&] { return (a_r - a_l) + (b_r - b_l); }, [&] {
    int_t block_size = std::max((int_t)std::ceil(std::sqrt((a_r - a_l) + (b_r - b_l))), BLOCK_SIZE);
    int_t blocks = ((a_r - a_l) + (b_r - b_l) + block_size - 1) / block_size;
    pasl::pctl::parallel_for(0, blocks, [&] (int i) {
      std::pair<int_t, int_t> l = find(a, a_l, a_r, b, b_l, b_r, i * block_size, compare);
      std::pair<int_t, int_t> r = find(a, a_l, a_r, b, b_l, b_r, std::min((i + 1) * block_size, (a_r - a_l) + (b_r - b_l)), compare);
      merge_two_parts(a, l.first + 1, r.first + 1, b, l.second + 1, r.second + 1, result, result_offset + i * block_size, compare);
    });
  }, [&] {
    merge_two_parts(a, a_l, a_r, b, b_l, b_r, result, result_offset, compare);
  });
}

/*!
  Merges certain ranges of two given sorted arrays into specified array. Uses algorithm with blocks.
  \param a first array to merge
  \param b second array to merge
  \param result array to contain result of merge
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, template <class Item> class ResultArray, class Item, class Compare_fct>
void merge(ArrayA<Item>& a, ArrayB<Item>& b, ResultArray<Item>& result, const Compare_fct& compare) { 
  merge(a, 0, a.size(), b, 0, b.size(), result, 0, compare);
}

/*!
  Merges certain ranges of two given sorted arrays into specified array. Uses algorithm with blocks.
  \param a first array to merge
  \param b second array to merge
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
  \return result of merge of two given arrays
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, class Item, class Compare_fct>
array<Item> merge(ArrayA<Item>& a, ArrayB<Item>& b, const Compare_fct& compare) {
  array<Item> result(a.size() + b.size());
  merge(a, b, result, compare);
  return result;
}

/*!
  Merges certain ranges of two given sorted arrays into specified array. Uses binary splitting algorithm.
  \param a first array to merge
  \param a_l start position of range of array a
  \param a_r end position of range of array a (exclusive)
  \param b second array to merge
  \param b_l start position of range of array b
  \param b_r end position of range of array b (exclusive)
  \param result array to contain result of merge
  \param result_offset position in result array from where start to write
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, template <class Item> class ResultArray, class Item, class Compare_fct>
void merge_bs(ArrayA<Item>& a, int_t a_l, int_t a_r, ArrayB<Item>& b, int_t b_l, int_t b_r, ResultArray<Item>& result, int_t result_offset, const Compare_fct& compare) {
//  std::cerr << a_r << " " << a_l << " " << b_l << " " << b_r << "\n";
  if (a_r - a_l < b_r - b_l) {
    merge_bs(b, b_l, b_r, a, a_l, a_r, result, result_offset, compare);
    return;
  }
  using controller_type = pasl::pctl::granularity::controller_holder<merge_file, 2, ArrayA<Item>, ArrayB<Item>, ResultArray<Item>, Compare_fct>;
  int_t size = (a_r - a_l) + (b_r - b_l);
  if (size <= 2) {
    merge_two_parts(a, a_l, a_r, b, b_l, b_r, result, result_offset, compare);
    return;
  }
//  std::cerr << (controller_type::controller).get_estimator().get_name() << std::endl;
  pasl::pctl::granularity::cstmt(controller_type::controller, [&] { return size; }, [&] {
    int_t m = (a_l + a_r) >> 1;
    int_t pos = lower_bound(b, b_l, b_r, a.at(m), compare) + 1;
    pasl::pctl::granularity::fork2(
      [&] { merge_bs(a, a_l, m, b, b_l, pos, result, result_offset, compare); },
      [&] { merge_bs(a, m, a_r, b, pos, b_r, result, result_offset + (m - a_l) + (pos - b_l), compare); }
    );
  }, [&] {
//    std::cerr << a_l << " " << a_r << " " << b_l << " " << b_r << "\n";
    merge_two_parts(a, a_l, a_r, b, b_l, b_r, result, result_offset, compare);
  });
}

/*!
  Merges certain ranges of two given sorted arrays into specified array. Uses binary splitting algorithm.
  \param a first array to merge
  \param b second array to merge
  \param result array to contain result of merge
  \param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, template <class Item> class ResultArray, class Item, class Compare_fct>
void merge_bs(ArrayA<Item>& a, ArrayB<Item>& b, ResultArray<Item>& result, const Compare_fct& compare) {
  merge_bs(a, 0, a.size(), b, 0, b.size(), result, 0, compare);
}

/*!
  Merges certain ranges of two given sorted arrays into specified array. Uses binary splitting algorithm.
  \param a first array to merge
  \param b second array to merge
  param compare comparator function between elements (compare(a, b) < 0 if a < b, = 0 if a = b, > 0 if a > b)
  \return result of merge of two given arrays
*/
template <template <class Item> class ArrayA, template <class Item> class ArrayB, class Item, class Compare_fct>
array<Item> merge_bs(ArrayA<Item>& a, ArrayB<Item>& b, const Compare_fct& compare) {
  array<Item> result(a.size() + b.size());
  merge_bs(a, b, result, compare);
  return result;
}

} //end namespace utils
} //end namespace array
} //end namespace parutils

#endif
