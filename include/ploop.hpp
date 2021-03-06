/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file ploop.hpp
 * \brief Parallel loops
 *
 */

#include "granularity.hpp"
#ifndef _PCTL_PLOOP_H_
#define _PCTL_PLOOP_H_

namespace pasl {
namespace pctl {
  
/***********************************************************************/
  
/*---------------------------------------------------------------------*/
/* pctl global configuration */

namespace par = granularity;
  
#if defined(CONTROL_BY_FORCE_SEQUENTIAL)
  using controller_type = par::control_by_force_sequential;
#elif defined(CONTROL_BY_FORCE_PARALLEL)
  using controller_type = par::control_by_force_parallel;
#else
  using controller_type = par::control_by_prediction;
#endif

template <class T>
std::string string_of_template_arg() {
  return std::string(typeid(T).name());
}

template <class T>
std::string sota() {
  return string_of_template_arg<T>();
}
  
template <class Iter>
using value_type_of = typename std::iterator_traits<Iter>::value_type;

template <class Iter>
using reference_of = typename std::iterator_traits<Iter>::reference;

template <class Iter>
using pointer_of = typename std::iterator_traits<Iter>::pointer;
  
/*---------------------------------------------------------------------*/
/* Parallel-for loops */

namespace range {
  
namespace contr {
  
template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
class parallel_for {
public:
  static controller_type contr;
};

template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
controller_type parallel_for<Iter,Body,Comp_rng,Seq_body_rng>::contr(                                                                                       "parallel_for"+sota<Iter>()+sota<Body>()+sota<Comp_rng>()+sota<Seq_body_rng>());
    
} // end namespace

double multiplier = 20.0;
int cacheline = 64;

template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
void parallel_for(Iter lo,
                  Iter hi,
                  const Comp_rng& comp_rng,
                  const Body& body,
                  const Seq_body_rng& seq_body_rng,
                  par::complexity_type whole_range_comp) {
#if defined(MANUAL_CONTROL) && defined(USE_CILK_PLUS_RUNTIME)
//  if (std::is_fundamental<Iter>::value) {
   { cilk_for (Iter i = lo; i < hi; i++) {
      body(i);
    }}
    return;
//  }
#endif
  
  
  using controller_type = contr::parallel_for<Iter, Body, Comp_rng, Seq_body_rng>;
  double comp = comp_rng(lo, hi);
#if defined(EASYOPTIMISTIC) && !defined(SMART_ESTIMATOR)
//  std::cerr << controller_type::contr.get_estimator().privates.mine() << " " << controller_type::contr.get_estimator().shared << " " << comp << " " << whole_range_comp << std::endl;
  if (comp * multiplier * par::nb_proc < whole_range_comp) {
    par::cstmt_sequential_with_reporting(comp, [&] { seq_body_rng(lo, hi); }, controller_type::contr.get_estimator());
    return;
  }
#endif
  par::cstmt(controller_type::contr, [&] { return comp; }, [&] {
    long n = hi - lo;
    if (n <= 0) {
      
    } else if (n == 1) {
      body(lo);
    } else {
      Iter mid = lo + (n / 2);

      par::fork2([&] {
        parallel_for(lo, mid, comp_rng, body, seq_body_rng, whole_range_comp);
      }, [&] {
        parallel_for(mid, hi, comp_rng, body, seq_body_rng, whole_range_comp);
      });
    }
  }, [&] {
    seq_body_rng(lo, hi);
  });
}

template <
  class Iter,
  class Body,
  class Comp_rng,
  class Seq_body_rng
>
void parallel_for(Iter lo,
                  Iter hi,
                  const Comp_rng& comp_rng,
                  const Body& body,
                  const Seq_body_rng& seq_body_rng) {
  parallel_for(lo, hi, comp_rng, body, seq_body_rng, comp_rng(lo, hi));
}

template <class Iter, class Body, class Comp_rng>
void parallel_for(Iter lo, Iter hi, const Comp_rng& comp_rng, const Body& body) {
  auto seq_body_rng = [&] (Iter lo, Iter hi) {
    for (Iter i = lo; i != hi; i++) {
      body(i);
    }
  };
  parallel_for(lo, hi, comp_rng, body, seq_body_rng);
}

} // end namespace

template <class Iter, class Body, class Comp>
void parallel_for(Iter lo, Iter hi, const Comp& comp, const Body& body);
  
template <class Iter, class Body>
void parallel_for(Iter lo, Iter hi, const Body& body) {
  auto comp_rng = [&] (Iter lo, Iter hi) {
    return (long)(hi - lo);
  };
  range::parallel_for(lo, hi, comp_rng, body);
}
  
template <class Iter, class Body>
void blocked_for(Iter l, Iter r, int bsize, const Body& body) {
  int n = ((int)(r - l) + bsize - 1) / bsize;	
  range::parallel_for(0, n, [&] (int l, int r) { return r - l; }, [&] (int b) {
    Iter ll = l + b * bsize;
    Iter rr = std::min(l + (b + 1) * bsize, r);
    body(ll, rr);
  }, [&] (int left, int right) {
    Iter ll = l + left * bsize;
    Iter rr = std::min(l + right * bsize, r);
    body(ll, rr);
  });
}

/***********************************************************************/

} // end namespace
} // end namespace

#endif /*! _PCTL_PLOOP_H_ */
