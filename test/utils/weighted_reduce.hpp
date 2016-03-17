/*!
  \file
  \brief File contains implementations of weighted reduce function on array with different arguments and different approaches.
*/

#include <algorithm>
#include <utility>
#include "scan.hpp"
#include "map.hpp"
#include "granularity.hpp"
#include "defines.hpp"
#include <iostream>

#ifndef _PARUTILS_WEIGHTED_REDUCE_H_
#define _PARUTILS_WEIGHTED_REDUCE_H_

namespace parutils {
namespace array {
namespace utils {

using cost_type = pasl::pctl::granularity::cost_type;
using complexity_type = std::function<cost_type(int_t, int_t)>;
using splitting_type = std::function<std::pair<int_t, int_t>(int_t, int_t, int_t, const complexity_type&)>;

namespace splitting {
std::pair<int_t, int_t> binary_splitting(int_t depth, int_t l, int_t r, const complexity_type& complexity) {
  return std::make_pair((l + r) >> 1, (l + r) >> 1);
}

std::pair<int_t, int_t> binary_search_splitting(int_t depth, int_t left, int_t right, const complexity_type& complexity) {
  int_t l = left;
  int_t r = right;
  cost_type total = complexity(left, right);
  while (l < r - 1) {
    int_t m = (l + r) >> 1;
    if (2 * complexity(left, m) > total) {
      r = m;
    } else {
      l = m;
    }
  }
  cost_type left_total = complexity(left, l);
//  return std::make_pair((left + right) >> 1, (left + right) >> 1);
  if (4 * left_total < total) {
    return std::make_pair(l, l + 1);
  } else {
    return std::make_pair(l, l);
  }
}

std::pair<int_t, int_t> hybrid_splitting(int_t depth, int_t l, int_t r, const complexity_type& complexity) {
  if (depth & 1 == 0) {
    return binary_splitting(depth, l, r, complexity);
  } else {
    return binary_search_splitting(depth, l, r, complexity);
  }
}
}

// split_fct : l, r, complexity -> std::pair<int_t, int_t>
// Split by (l, m, r) or (l, m, m + 1, r)
template <template <class Item> class Array, class Item, class Complexity_fct, class Multiply_fct, class Split_fct>
Item weighted_reduce(Array<Item>& items, int_t l, int_t r, const Complexity_fct& complexity, const Item& identity, const Multiply_fct& multiplication, const Split_fct& split_fct, int_t depth = 0) {
  Item value;
  using controller_type = pasl::pctl::granularity::controller_holder<1, Array<Item>, complexity_type, Multiply_fct, Split_fct>;
  pasl::pctl::granularity::cstmt(controller_type::controller, [&] { return complexity(l, r); }, [&] {
    if (r - l == 1) {
      value = items[l];
      return;
    }
    std::pair<int_t, int_t> mid = split_fct(depth, l, r, complexity);
    Item left, right;
    pasl::pctl::granularity::fork2([&] {
      left = weighted_reduce(items, l, mid.first, complexity, identity, multiplication, split_fct, depth + 1);
    }, [&] {
      right = weighted_reduce(items, mid.second, r, complexity, identity, multiplication, split_fct, depth + 1);
    });
    value = multiplication(left, right);
    if (mid.first != mid.second) {
      value = multiplication(value, items[mid.first]);
    }
  }, [&] {
    value = reduce_serial(items, l, r, identity, multiplication);
  });
  return value;
}


/*!
  Calculates the value of given multiplication function on elements of the given array in certain range using binary splitting technique
  where complexity function of reduce on subarray is given.

  \param items array of elements
  \param l start position of range
  \param r end position of range (exclusive)
  \param complexity function which returns the complexity of reduce function application on subarray
  \param identity identity element of multiplication
  \param multiplication multiplication function
*/
/*template <template <class Item> class Array, class Complexity_fct, class Item, class Multiply_fct>
Item weighted_reduce(Array<Item>& items, int_t l, int_t r, const Complexity_fct& complexity, const Item& identity, const Multiply_fct& multiplication) {
  Item value;
  using controller_type = pasl::pctl::granularity::controller_holder<1, Array<Item>, Complexity_fct, Multiply_fct>;
  pasl::pctl::granularity::cstmt(controller_type::controller, [&] { return complexity(l, r); }, [&] {
    if (r - l == 1) {
      value = items[l];
      return;
    }
    int mid = (r + l) >> 1;
    Item left, right;
    pasl::pctl::granularity::fork2([&] {
      left = weighted_reduce(items, l, mid, complexity, identity, multiplication);
    }, [&] {
      right = weighted_reduce(items, mid, r, complexity, identity, multiplication);
    });
    value = multiplication(left, right);
  }, [&] {
    value = reduce_serial(items, l, r, identity, multiplication);
  });
  return value;
}*/

/*!
  Calculates the value of given multiplication function on elements of the given array using binary splitting technique
  where complexity function of reduce on subarray is given.

  \param items array of elements
  \param complexity function which returns the complexity of reduce function application on subarray
  \param identity identity element of multiplication
  \param multiplication multiplication function
*/
template <template <class Item> class Array, class Complexity_fct, class Item, class Multiply_fct, class Split_fct>
Item weighted_reduce(Array<Item>& items, const Complexity_fct& complexity, const Item& identity, const Multiply_fct& multiplication, const Split_fct& split_fct) {
  return weighted_reduce(items, 0, items.size(), complexity, identity, multiplication, split_fct);
}

/*!
  Calculates the value of given multiplication function on elements of the given array assuming that the reduce function
  on subarray works in time proportional to the sum of weights of each element using given temporary array for scan of weights.

  \param items array of elements
  \param tmp_array temporary array to contain scan of weights of elements
  \param identity identity element of multiplication
  \param multiplication multiplication function
  \param weight function which returns the weight (complexity) of element with type pasl::granularity::cost_type
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Weight_fct, class Multiply_fct, class Split_fct>
Item weighted_sequence_reduce(Array<Item>& items, TmpArray<cost_type>& tmp_array, const Item& identity, const Multiply_fct& multiplication, const Weight_fct& weight, const Split_fct& split_fct) {
  map(items, 0, items.size(), tmp_array, 1, weight);
//  std::cerr << tmp_array.at(20) << std::endl;
  tmp_array[0] = 0;
  scan_exclusive(tmp_array, 1, items.size() + 1, tmp_array, 1, tmp_array, items.size() + 1, (cost_type)0, [&] (cost_type a, cost_type b) { return a + b; });
  // Now prefix sum of weights contains in items[0,...,items.size()]
  auto complexity = [&] (int l, int r) { return tmp_array[r] - tmp_array[l]; };
  return weighted_reduce(items, complexity, identity, multiplication, split_fct);
}

/*!
  Calculates the value of given multiplication function on elements of the given array assuming that the reduce function
  on subarray works in time proportional to the sum of weight of each element.

  \param items array of elements
  \param identity identity element of multiplication
  \param multiplication multiplication function
  \param weight function which returns the weight (complexity) of element with type pasl::granularity::cost_type
*/
template <template <class Item> class Array, class Item, class Weight_fct, class Multiply_fct, class Split_fct>
Item weighted_sequence_reduce(Array<Item>& items, const Item& identity, const Multiply_fct& multiplication, const Weight_fct& weight, const Split_fct& split) {
  array<pasl::pctl::granularity::cost_type> tmp_array(items.size() + 1 + 2 * ((items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE));
  return weighted_sequence_reduce(items, tmp_array, identity, multiplication, weight, split);
}

} //end namespace
} //end namespace
} //end namespace
#endif
