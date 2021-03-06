/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file pmem.hpp
 * \brief Basic allocation and memory transfer operations
 *
 */

#include <limits.h>
#include <memory>
#include <utility>
#ifndef TARGET_MAC_OS
#include <malloc.h>
#endif
#include <algorithm>
#include <type_traits>

#include "ploop.hpp"

#ifndef _PCTL_PMEM_H_
#define _PCTL_PMEM_H_

namespace pasl {
namespace pctl {
namespace pmem {
  
/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* Primitive memory operations */

#define PMEM_THRESHOLD 2000

template <class Iter, class Item>
void fill(Iter lo, Iter hi, const Item& val) {
  if (std::is_trivially_copyable<Item>::value) {
    // later: can we relax the above constraint without breaking gcc compatibility?
#ifdef MANUAL_CONTROL
    blocked_for(lo, hi, PMEM_THRESHOLD, [&] (Iter l, Iter r) {
      std::fill(l, r, val);
    });
#else
    range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
      std::fill(i, i+1, val);
    }, [&] (Iter lo, Iter hi) {
      std::fill(lo, hi, val);
    });
#endif
  } else {
    range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
      new (i) Item();
    }, [&] (Iter lo, Iter hi) {
      for (Iter i = lo; i != hi; i++) {
        new (i) Item();
      }
    });
  }
}
  
template <class Iter, class Output_iterator>
void copy(Iter lo, Iter hi, Output_iterator dst) {
#ifdef MANUAL_CONTROL
  blocked_for(lo, hi, PMEM_THRESHOLD, [&] (Iter l, Iter r) {
    std::copy(l, r, dst + (l - lo));
  });
#else
  range::parallel_for(lo, hi, [&] (Iter lo, Iter hi) { return hi - lo; }, [&] (Iter i) {
    std::copy(i, i + 1, dst + (i - lo));
  }, [&] (Iter lo2, Iter hi2) {
    std::copy(lo2, hi2, dst + (lo2 - lo));
  });
#endif
}

template <class Item, class Alloc>
void pdelete(Item* lo, Item* hi) { //trivially_destructible
  if (!std::is_trivially_destructible<Item>()) {
    parallel_for(lo, hi, [&] (Item* p) {
      Alloc alloc;
      alloc.destroy(p);
    });
 }
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_PMEM_H_ */
