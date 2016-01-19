/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file granularity.hpp
 * \brief Granularity-controlled parallel primitives
 *
 */

#include <atomic>

#include "perworker.hpp"

#ifndef _PCTL_GRANULARITY_H_
#define _PCTL_GRANULARITY_H_

namespace pasl {
namespace pctl {
namespace granularity {

/***********************************************************************/

namespace {
  
/*---------------------------------------------------------------------*/
/* Cycle counter */
  
using cycles_type = uint64_t;
  
static inline
cycles_type rdtsc() {
  unsigned int hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return  ((cycles_type) lo) | (((cycles_type) hi) << 32);
}

static inline
void rdtsc_wait(cycles_type n) {
  const cycles_type start = rdtsc();
  while (rdtsc() < (start + n)) {
    __asm__("PAUSE");
  }
}

static inline
cycles_type now() {
  return rdtsc();
}

static inline
double elapsed(cycles_type time_start, cycles_type time_end) {
  return (double)time_end - (double)time_start;
}
  
static inline
double since(cycles_type time_start) {
  return elapsed(time_start, now());
}

/*---------------------------------------------------------------------*/
/* */
  
template <class Body_fct1, class Body_fct2>
void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(PCTL_CILK_PLUS)
  cilk_spawn f1();
  f2();
  cilk_sync;
#else
  f1();
  f2();
#endif
}

} // end namespace
  
/*---------------------------------------------------------------------*/
/* Dynamic scope */

template <class Item>
class dynidentifier {
private:
  Item bk;
public:
  dynidentifier() {};
  dynidentifier(Item& bk_) : bk(bk_) {};
  
  Item& back() {
    return bk;
  }
  
  template <class Block_fct>
  void block(Item x, const Block_fct& f) {
    Item tmp = bk;
    bk = x;
    f();
    bk = tmp;
  }
};
  
/*---------------------------------------------------------------------*/
/* Execution mode */
  
// names of configurations supported by the granularity controller
using execmode_type = enum {
  Force_parallel,
  Force_sequential,
  Sequential,
  Parallel
};

// `p` configuration of caller; `c` callee
static inline
execmode_type execmode_combine(execmode_type p, execmode_type c) {
  // callee gives priority to Force*
  if (c == Force_parallel || c == Force_sequential) {
    return c;
  }
  // callee gives priority to caller when caller is Sequential
  if (p == Sequential) {
    return Sequential;
  }
  // otherwise, callee takes priority
  return c;
}
  
namespace {
  
std::atomic<int> counter(0);
  
__thread int my_id = -1;
  
class get_my_id {
public:
  
  int operator()() {
    while (my_id == -1) {
      my_id = counter++;
    }
    return my_id;
  }
  
};
  
template <class Item>
using perworker_type = perworker::array<Item, get_my_id>;
  
perworker_type<dynidentifier<execmode_type>> execmode;
  
static inline
execmode_type& my_execmode() {
  return execmode.mine().back();
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Constant-estimator data structure */
  
using complexity_type = long;
using cost_type = double;

namespace complexity {

// A `tiny` complexity forces sequential execution
static constexpr complexity_type tiny = (complexity_type) (-1l);

// An `undefined` complexity indicates that the value hasn't been computed yet
static constexpr complexity_type undefined = (complexity_type) (-2l);
  
} // end namespace

namespace cost {

//! an `undefined` execution time indicates that the value hasn't been computed yet
static constexpr cost_type undefined = -1.0;

//! an `unknown` execution time forces parallel execution
static constexpr cost_type unknown = -2.0;

//! a `tiny` execution time forces sequential execution, and skips time measures
static constexpr cost_type tiny = -3.0;

//! a `pessimistic` cost is 1 microsecond per unit of complexity
static constexpr cost_type pessimistic = 1.0;
  
} // end namespace
  
namespace {
  
double cpu_frequency_ghz = 2.1;
double local_ticks_per_microsecond = cpu_frequency_ghz * 1000.0;
  
class estimator {
private:
  
  constexpr static const double min_report_shared_factor = 2.0;
  constexpr static const double weighted_average_factor = 8.0;
  
  std::string name;
  
  cost_type shared;

  perworker_type<cost_type> privates;
  
  cost_type get_constant() {
    cost_type cst = privates.mine();
    // if local constant is undefined, use shared cst
    if (cst == cost::undefined) {
      return shared;
    }
    // else return local constant
    return cst;
  }
  
  cost_type get_constant_or_pessimistic() {
    cost_type cst = get_constant();
    assert (cst != 0.);
    if (cst == cost::undefined) {
      return cost::pessimistic;
    } else {
      return cst;
    }
  }
  
  void update_shared(cost_type new_cst) {
    shared = new_cst;
  }
  
  void update(cost_type new_cst) {
    // if decrease is significant, report it to the shared constant;
    // (note that the shared constant never increases)
    cost_type s = shared;
    if (s == cost::undefined) {
      update_shared(new_cst);
    } else {
      cost_type min_shared_cst = shared / min_report_shared_factor;
      if (new_cst < min_shared_cst) {
        update_shared(min_shared_cst);
      }
    }
    // store the new constant locally in any case
    privates.mine() = new_cst;
  }
  
  void init() {
    shared = cost::undefined;
    privates.init(cost::undefined);
  }
  
public:
  
  estimator() {
    init();
  }

  estimator(std::string name)
  : name(name) {
    init();
  }
  
  void report(complexity_type complexity, cost_type elapsed) {
    double elapsed_time = elapsed / local_ticks_per_microsecond;
    cost_type measured_cst = elapsed_time / complexity;
    cost_type cst = get_constant();
    if (cst == cost::undefined) {
      // handle the first measure without average
      update(measured_cst);
    } else {
      // compute weighted average
      update(((weighted_average_factor * cst) + measured_cst)
             / (weighted_average_factor + 1.0));
    }
  }
  
  cost_type predict(complexity_type complexity) {
    // tiny complexity leads to tiny cost
    if (complexity == complexity::tiny) {
      return cost::tiny;
    }
    // complexity shouldn't be undefined
    assert (complexity >= 0);
    // compute the constant multiplied by the complexity
    cost_type cst = get_constant_or_pessimistic();
    return cst * ((double) complexity);
  }
  
};
  
cost_type kappa = 300.0;

} // end namespace
  
/*---------------------------------------------------------------------*/
/* Granularity-control policies */

class control {};

class control_by_force_parallel : public control {
public:
  control_by_force_parallel(std::string) { }
};

class control_by_force_sequential : public control {
public:
  control_by_force_sequential(std::string) { }
};

class control_by_prediction : public control {
public:
  estimator e;
  
  control_by_prediction(std::string name = ""): e(name) { }
  
  estimator& get_estimator() {
    return e;
  }
  
};
  
/*---------------------------------------------------------------------*/
/* Controlled statements */
  
template <class Body_fct>
void cstmt_sequential(execmode_type c, const Body_fct& body_fct) {
  execmode_type p = my_execmode();
  execmode_type e = execmode_combine(p, c);
  execmode.mine().block(e, body_fct);
}

template <class Body_fct>
void cstmt_parallel(execmode_type c, const Body_fct& body_fct) {
  execmode.mine().block(c, body_fct);
}

template <class Seq_body_fct>
void cstmt_sequential_with_reporting(complexity_type m,
                                     Seq_body_fct& seq_body_fct,
                                     estimator& estimator) {
  cost_type start = now();
  execmode.mine().block(Sequential, seq_body_fct);
  cost_type elapsed = since(start);
  estimator.report(std::max((complexity_type)1, m), elapsed);
}
  
template <
class Complexity_measure_fct,
class Par_body_fct
>
void cstmt(control& contr,
           const Complexity_measure_fct&,
           const Par_body_fct& par_body_fct) {
  cstmt_sequential(Force_parallel, par_body_fct);
}

template <class Par_body_fct>
void cstmt(control_by_force_parallel&, const Par_body_fct& par_body_fct) {
  cstmt_parallel(Force_parallel, par_body_fct);
}

// same as above but accepts all arguments to support general case
template <
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(control_by_force_parallel& contr,
           const Complexity_measure_fct&,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct&) {
  cstmt(contr, par_body_fct);
}

template <class Seq_body_fct>
void cstmt(control_by_force_sequential&, const Seq_body_fct& seq_body_fct) {
  cstmt_sequential(Force_sequential, seq_body_fct);
}

// same as above but accepts all arguments to support general case
template <
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(control_by_force_sequential& contr,
           const Complexity_measure_fct&,
           const Par_body_fct&,
           const Seq_body_fct& seq_body_fct) {
  cstmt(contr, seq_body_fct);
}

template <
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(control_by_prediction& contr,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
#ifdef PCTL_SEQUENTIAL_BASELINE
  seq_body_fct();
  return;
#endif
#ifdef PCTL_SEQUENTIAL_ELISION
  par_body_fct();
  return;
#endif
  estimator& estimator = contr.get_estimator();
  complexity_type m = complexity_measure_fct();
  execmode_type c;
  if (m == complexity::tiny) {
    c = Sequential;
  } else if (m == complexity::undefined) {
    c = Parallel;
  } else {
    if (estimator.predict(std::max((complexity_type)1, m)) <= kappa) {
      c = Sequential;
    } else {
      c = Parallel;
    }
  }
  if (c == Sequential) {
    cstmt_sequential_with_reporting(m, seq_body_fct, estimator);
  } else {
    cstmt_parallel(c, par_body_fct);
  }
}

template <
class Complexity_measure_fct,
class Par_body_fct
>
void cstmt(control_by_prediction& contr,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct) {
  cstmt(contr, complexity_measure_fct, par_body_fct, par_body_fct);
}

// same as above but accepts all arguments to support general case
template <
class Cutoff_fct,
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(control_by_prediction& contr,
           const Cutoff_fct&,
           const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
  cstmt(contr, complexity_measure_fct, par_body_fct, seq_body_fct);
}
  
/*---------------------------------------------------------------------*/
/* Granularity-control enriched fork join */

template <class Body_fct1, class Body_fct2>
void fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(PCTL_SEQUENTIAL_ELISION) || defined(PCTL_SEQUENTIAL_BASELINE)
  f1();
  f2();
  return;
#endif
  execmode_type mode = my_execmode();
  if ( (mode == Sequential) || (mode == Force_sequential) ) {
    f1();
    f2();
  } else {
    primitive_fork2([&] {
      execmode.mine().block(mode, f1);
    }, [&] {
      execmode.mine().block(mode, f2);
    });
  }
}

/***********************************************************************/

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_GRANULARITY_H_ */
