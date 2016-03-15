/*!
  \file
  \brief File contains implementations of reduce function on array with different arguments.
*/

#include <algorithm>
#include "scan.hpp"
#include "map.hpp"
#include "granularity.hpp"
#include <iostream>

#ifndef _PARUTILS_WEIGHTED_REDUCE_H_
#define _PARUTILS_WEIGHTED_REDUCE_H_

namespace parutils {
namespace array {
namespace utils {

using cost_type = pasl::pctl::granularity::cost_type;

#define BLOCK_SIZE 1024

template <template <class Item> class Array, class Complexity_fct, class Item, class Multiply_fct>
Item weighted_reduce_binary_splitting(Array<Item>& items, int l, int r, const Complexity_fct& complexity, const Item& identity, const Multiply_fct& multiplication) {
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
      left = weighted_reduce_binary_splitting(items, l, mid, complexity, identity, multiplication);
    }, [&] {
      right = weighted_reduce_binary_splitting(items, mid, r, complexity, identity, multiplication);
    });
    value = multiplication(left, right);
  }, [&] {
    value = reduce_serial(items, l, r, identity, multiplication);
  });
  return value;
}

template <template <class Item> class Array, class Complexity_fct, class Item, class Multiply_fct>
Item weighted_reduce_binary_splitting(Array<Item>& items, const Complexity_fct& complexity, const Item& identity, const Multiply_fct& multiplication) {
  weighted_reduce_binary_splitting(items, 0, items.size(), complexity, identity, multiplication);
}

template <template <class Item> class Array, template <class Item> class TmpArray, class Item, class Multiply_fct, class Weight_fct>
Item weighted_reduce(Array<Item>& items, TmpArray<pasl::pctl::granularity::cost_type>& tmp_array, const Item& identity, const Multiply_fct& multiplication, const Weight_fct& weight) {
  map(items, 0, items.size(), tmp_array, 1, weight);
  tmp_array[0] = 0;
  scan_exclusive(tmp_array, 1, items.size() + 1, tmp_array, 1, tmp_array, items.size() + 1, (cost_type)0, [&] (cost_type a, cost_type b) { return a + b; });
  // Now prefix sum of weights contains in items[0,...,items.size()]
  auto complexity = [&] (int l, int r) { return tmp_array[r] - tmp_array[l]; };
  return weighted_reduce_binary_splitting(items, complexity, identity, multiplication);
}

template <template <class Item> class Array, class Item, class Multiply_fct, class Weight_fct>
Item weighted_reduce(Array<Item>& items, const Item& identity, const Multiply_fct& multiplication, const Weight_fct& weight) {
  array<pasl::pctl::granularity::cost_type> tmp_array(items.size() + 1 + 2 * ((items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE));
  return weighted_reduce(items, tmp_array, identity, multiplication, weight);
}

} //end namespace
} //end namespace
} //end namespace
#endif
