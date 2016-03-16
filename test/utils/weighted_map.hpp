/*!
  \file
  \brief File contains implementations of weighted map function with different arguments.
*/

#include <algorithm>
#include "scan.hpp"
#include "map.hpp"
#include "granularity.hpp"
#include <iostream>

#ifndef _PARUTILS_WEIGHTED_MAP_H_
#define _PARUTILS_WEIGHTED_MAP_H_

namespace parutils {
namespace array {
namespace utils {

using cost_type = pasl::pctl::granularity::cost_type;

#define BLOCK_SIZE 1024

/*!
  Maps elements of given array in certain range into specified array balancing the workload depending on the complexity of
  map on subarrays.

  \param items array of elements
  \param l start position of range
  \param r end position of range (exclusive)
  \param result array to contain result of map
  \param result_offset position in result array from where start to write
  \param complexity function which returns the complexity of map function application on subarray
  \param map_fct function to map with
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class ItemIn, class ItemOut, class Complexity_fct, class Map_fct>
void weighted_map(Array<ItemIn>& items, int l, int r, ResultArray<ItemOut>& result, int result_offset, const Complexity_fct& complexity, const Map_fct& map_fct) {
  using controller_type = pasl::pctl::granularity::controller_holder<1, Array<ItemIn>, ResultArray<ItemOut>, Complexity_fct, Map_fct>;
  pasl::pctl::granularity::cstmt(controller_type::controller, [&] { return complexity(l, r); }, [&] {
    if (r - l == 1) {
      result.at(result_offset) = map_fct(items.at(l));
      return;
    }
    int mid = (r + l) >> 1;
    pasl::pctl::granularity::fork2([&] {
      weighted_map(items, l, mid, result, result_offset, complexity, map_fct);
    }, [&] {
      weighted_map(items, mid, r, result, result_offset + mid - l, complexity, map_fct);
    });
  }, [&] {
    map_serial(items, l, r, result, result_offset, map_fct);
  });
}

/*!
  Maps elements of given array into specified array balancing the workload depending on the complexity of map on subarrays.

  \param items array of elements
  \param result array to contain result of map
  \param complexity function which returns the complexity of map function application on subarray
  \param map_fct function to map with
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class ItemIn, class ItemOut, class Complexity_fct, class Map_fct>
void weighted_map(Array<ItemIn>& items, ResultArray<ItemOut>& result, const Complexity_fct& complexity, const Map_fct& map_fct) {
  weighted_map(items, 0, items.size(), result, 0, complexity, map_fct);
}

/*!
  Maps elements of given array into specified array balancing the workload depending on the complexity of map on each element using given temporary array for scan of weights.

  \param items array of elements
  \param result array to contain result of map
  \param tmp_array temporary array to contain scan of weights of elements
  \param map_fct function to map with
  \param weight function which returns the weight (complexity) of map on element with type pasl::granularity::cost_type
*/
template <template <class Item> class Array, template <class Item> class ResultArray, template <class Item> class TmpArray, class ItemIn, class ItemOut, class Map_fct, class Weight_fct>
void weighted_map(Array<ItemIn>& items, ResultArray<ItemOut>& result, TmpArray<pasl::pctl::granularity::cost_type>& tmp_array, const Map_fct& map_fct, const Weight_fct& weight) {
  map(items, 0, items.size(), tmp_array, 1, weight);
  tmp_array[0] = 0;
  scan_exclusive(tmp_array, 1, items.size() + 1, tmp_array, 1, tmp_array, items.size() + 1, (cost_type)0, [&] (cost_type a, cost_type b) { return a + b; });
  // Now prefix sum of weights contains in items[0,...,items.size()]
  auto complexity = [&] (int l, int r) { return tmp_array[r] - tmp_array[l]; };
  weighted_map(items, result, complexity, map_fct);
}

/*!
  Maps elements of given array into specified array balancing the workload depending on the complexity of map on each element.

  \param items array of elements
  \param result array to contain result of map
  \param map_fct function to map with
  \param weight function which returns the weight (complexity) of map_fct on element with type pasl::granularity::cost_type
*/
template <template <class Item> class Array, template <class Item> class ResultArray, class ItemIn, class ItemOut, class Map_fct, class Weight_fct>
void weighted_map_no_tmp(Array<ItemIn>& items, ResultArray<ItemOut>& result, const Map_fct& map_fct, const Weight_fct& weight) {
  array<pasl::pctl::granularity::cost_type> tmp_array(items.size() + 1 + 2 * ((items.size() + BLOCK_SIZE - 1) / BLOCK_SIZE));
  weighted_map(items, result, tmp_array, map_fct, weight);
}

/*!
  Maps elements of given array balancing the workload depending on the complexity of map on each element using given temporary array for scan of weights.

  \param items array of elements
  \param tmp_array temporary array to contain scan of weights of elements
  \param map_fct function to map with
  \param weight function which returns the weight (complexity) of map_fct on element with type pasl::granularity::cost_type
*/
template <template <class Item> class Array, template <class Item> class TmpArray, class ItemIn, class Map_fct, class Weight_fct, typename ItemOut = typename std::result_of<Map_fct&(ItemIn)>::type>
array<ItemOut> weighted_map_no_result(Array<ItemIn>& items, TmpArray<pasl::pctl::granularity::cost_type>& tmp_array, const Map_fct& map_fct, const Weight_fct& weight) {
  array<ItemOut> result(items.size());
  weighted_map(items, result, tmp_array, map_fct, weight);
  return result;
}

/*!
  Maps elements of given array balancing the workload depending on the complexity of map on each element.

  \param items array of elements
  \param map_fct function to map with
  \param weight function which returns the weight (complexity) of element with type pasl::granularity::cost_type
*/
template <template <class Item> class Array, class ItemIn, class Map_fct, class Weight_fct, typename ItemOut = typename std::result_of<Map_fct&(ItemIn)>::type>
array<ItemOut> weighted_map(Array<ItemIn>& items, const Map_fct& map_fct, const Weight_fct& weight) {
  array<ItemOut> result(items.size());
  weighted_map_no_tmp(items, result, map_fct, weight);
  return result;
}

} //end namespace
} //end namespace
} //end namespace
#endif
