/* COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file granularity.hpp
 * \brief Granularity-controlled parallel primitives
 *
 */

#include <atomic>
#include <chrono>
#include <string>
#include <iostream>

#if defined(USE_PASL_RUNTIME)
#include "threaddag.hpp"
#include "native.hpp"
#elif defined(USE_CILK_PLUS_RUNTIME)
#include <cilk/cilk.h>
#endif

#include "perworker.hpp"
#include "logging.hpp"

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

#ifdef LOGGING
pasl::pctl::perworker::array<int, pasl::pctl::perworker::get_my_id> threads_number;
#endif
  
template <class Body_fct1, class Body_fct2>
void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
#ifdef LOGGING
  threads_number.mine()++;
#endif
#if defined(USE_PASL_RUNTIME)
  pasl::sched::native::fork2(f1, f2);
#elif defined(USE_CILK_PLUS_RUNTIME)
  cilk_spawn f1();
  f2();
  cilk_sync;
#else
  f1();
  f2();
#endif
}

} // end namespace
  
#ifdef LOGGING
int threads_created() {
  return threads_number.reduce([&] (int a, int b) { return a + b; }, 1);
}
#endif

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
  Parallel,
  Unknown
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
/*  
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
  
};*/
  
template <class Item>
using perworker_type = perworker::array<Item, perworker::get_my_id>;
  
perworker_type<dynidentifier<execmode_type>> execmode;
  
static inline
execmode_type& my_execmode() {
  return execmode.mine().back();
}
  
} // end namespace
  
/*---------------------------------------------------------------------*/
/* Constant-estimator data structure */
  
using complexity_type = double;
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

#ifdef HONEST
  perworker_type<bool> to_be_estimated;
#endif

  std::atomic<bool> estimated;
  
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
#ifdef LOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_UPDATE_SHARED, name.c_str(), new_cst);
#endif
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
#ifdef LOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_UPDATE, name.c_str(), new_cst);
#endif
    privates.mine() = new_cst;
  }
  
  void init() {
    shared = cost::undefined;
    privates.init(cost::undefined);
#ifdef HOMEST
    to_be_estimated.init(false);
#endif
    estimated = false;
  }
  
public:
  
  estimator() {
    init();
  }

  estimator(std::string name)
  : name(name) {
    init();
#ifdef LOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_NAME, name.c_str());
#endif
  }

#ifdef HONEST
  void set_to_be_estimated(bool value) {
    to_be_estimated.mine() = value;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }
#endif

  bool is_undefined() {
<<<<<<< HEAD
    return estimated.load();
  }
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST

  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }

  bool is_undefined() {
    return estimated.load();
  }
<<<<<<< HEAD

  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }

  bool is_undefined() {
    return estimated.load();
  }

  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }

  bool is_undefined() {
    return estimated.load();
  }
<<<<<<< HEAD
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======

  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }

  bool is_undefined() {
    return estimated.load();
  }
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======

  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }

  bool is_undefined() {
    return estimated.load();
  }
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======

  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }

  bool is_undefined() {
    return estimated.load();
=======
    return !estimated.load();
>>>>>>> suddenly unknown mode works
  }
<<<<<<< HEAD
>>>>>>> merge
  
=======

>>>>>>> nested loops works
  void report(complexity_type complexity, cost_type elapsed) {
    double elapsed_time = elapsed / local_ticks_per_microsecond;
    cost_type measured_cst = elapsed_time / complexity;
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD

#ifdef LOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_REPORT, name.c_str(), complexity, elapsed_time, measured_cst);
#endif

<<<<<<< HEAD
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
#if defined(OPTIMISTIC) || defined(HONEST)
    if (!estimated.exchange(true)) {
#else
    cost_type cst = get_constant();
    if (cst == cost::undefined) {
#endif
      // handle the first measure without average
      update(measured_cst);
    } else {
#if defined(OPTIMISTIC) || defined(HONEST)
      cost_type cst = get_constant();
#endif
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

#ifdef LOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_PREDICT, name.c_str(), complexity, cst * complexity, cst);
#endif

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

#ifdef OPTIMISTIC
perworker_type<cost_type> time_adjustment(0);
#elif HONEST
perworker_type<int> nested_unknown(0);
#endif

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

template <class Par_body_fct>
void cstmt_unknown(complexity_type m, Par_body_fct& par_body_fct, estimator& estimator) {
#ifdef OPTIMISTIC
  double upper_adjustment = time_adjustment.mine();
  time_adjustment.mine() = 0;
#elif HONEST
  if (estimator.is_undefined() && !estimator.is_to_be_estimated()) {
    nested_unknown.mine()++;
    estimator.set_to_be_estimated(true);
  }
#endif

  cost_type start = now();
  execmode.mine().block(Unknown, par_body_fct);
  cost_type elapsed = since(start);

  if (estimator.is_undefined()) {
#ifdef OPTIMISTIC
    estimator.report(std::max((complexity_type) 1, m), elapsed + time_adjustment.mine());
#elif HONEST
    estimator.report(std::max((complexity_type) 1, m), elapsed);
#endif
  }

#ifdef HONEST
  if (estimator.is_to_be_estimated()) {
    estimator.set_to_be_estimated(false);
    nested_unknown.mine()--;
  }
#endif

#ifdef OPTIMISTIC
  time_adjustment.mine() = upper_adjustment + estimator.predict(std::max((complexity_type) 1, m)) - elapsed;
#endif
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
#if defined(PCTL_SEQUENTIAL_ELISION) || defined(PCTL_PARALLEL_ELISION)
  par_body_fct();
  return;
#endif
  estimator& estimator = contr.get_estimator();
  complexity_type m = complexity_measure_fct();
  execmode_type c;
#ifdef OPTIMISTIC
  if (estimator.is_undefined()) {
    c = Unknown;
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
  } else {
#elif HONEST
  if (estimator.is_undefined() || nested_unknown.mine() > 0) {
    c = Unknown;
<<<<<<< HEAD
  } else {
=======
  } else {
#elif HONEST
  if (estimator.is_undefined()) {
    c = Unknown;
  } else if (nested_unknown.mine() > 0) {
    c = Sequential;
  } else {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
  } else {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
#endif
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
#if defined(OPTIMISTIC) || defined(HONEST)
  }
#endif

  
  if (c == Unknown) {
    cstmt_unknown(m, par_body_fct, estimator);
  } else if (c == Sequential) {
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
#ifdef PCTL_PARALLEL_ELISION
  primitive_fork2(f1, f2);
  return;
#endif
  execmode_type mode = my_execmode();
  if ( (mode == Sequential) || (mode == Force_sequential)
#ifdef HONEST
    || (mode == Unknown)
#endif
  ) {
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

template <class Last>
std::string type_name() {
  return std::string(typeid(Last).name());
}

template <class First, class Second, class ... Types>
std::string type_name() {
  return type_name<First>() + " " + type_name<Second, Types...>();
}

template <const char* method_name, int id, class ... Types>
class controller_holder {
public:
  static control_by_prediction controller;
};

template <const char* method_name, int id, class ... Types>
control_by_prediction controller_holder<method_name, id, Types ...>::controller(std::string("controller_holder ") + std::string(method_name) + " " + std::to_string(id) + " " + type_name<Types ...>());


} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_GRANULARITY_H_ */
