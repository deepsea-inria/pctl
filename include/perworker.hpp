/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file perworker.hpp
 * \brief Per-worker local storage
 *
 */

#include <assert.h>
#include <initializer_list>

#ifndef _PCTL_PERWORKER_H_
#define _PCTL_PERWORKER_H_

namespace pasl {
namespace pctl {
namespace perworker {

/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Cache-aligned fixed-capacity array */
  
template <class Item, int capacity>
class cache_aligned_fixed_capacity_array {
private:
  
  static constexpr int cache_align_szb = 128;
  static constexpr int item_szb = sizeof(Item);
  
  using aligned_item_type = typename std::aligned_storage<item_szb, cache_align_szb>::type;
  
  aligned_item_type items[capacity];
  
  Item& at(std::size_t i) {
    assert(i >= 0);
    assert(i < capacity);
    return *reinterpret_cast<Item*>(items + i);
  }
  
public:
  
  cache_aligned_fixed_capacity_array() { }
  
  cache_aligned_fixed_capacity_array(const Item& x) {
    init(x);
  }
  
  Item& operator[](std::size_t i) {
    return at(i);
  }
  
  std::size_t size() const {
    return capacity;
  }
  
  void init(const Item& x) {
    for (int i = 0; i < size(); i++) {
      at(i) = x;
    }
  }
  
};
  
static constexpr int default_max_nb_workers = 128;

template <class Item, class My_id, int max_nb_workers=default_max_nb_workers>
class array {
private:

  cache_aligned_fixed_capacity_array<Item, max_nb_workers> items;
  
  int get_my_id() {
    My_id my_id;
    int id = my_id();
    assert(id >= 0);
    assert(id < max_nb_workers);
    return id;
  }
  
public:
  
  array() { }

  array(const Item& x)
  : items(x) { }
  
  array(std::initializer_list<Item> l) {
    assert(l.size() == 1);
    items.init(*(l.begin()));
  }
  
  Item& mine() {
    return items[get_my_id()];
  }
  
  Item& operator[](std::size_t i) {
    assert(i >= 0);
    assert(i < max_nb_workers);
    return items[i];
  }
  
  void init(const Item& x) {
    items.init(x);
  }
  
};

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_PERWORKER_H_ */
