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
#include <execinfo.h>
#include <stdio.h>
#include <map>

#if defined(USE_PASL_RUNTIME)
#include "threaddag.hpp"
#include "native.hpp"
#include "logging.hpp"
#elif defined(USE_CILK_PLUS_RUNTIME)
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#endif

#include "perworker.hpp"
#include "plogging.hpp"
#include "pcallback.hpp"
#include "cmdline.hpp"

#ifndef _PCTL_GRANULARITY_H_
#define _PCTL_GRANULARITY_H_

namespace pasl {
namespace pctl {
namespace granularity {

/***********************************************************************/

namespace {

void stacktrace() {
  void *trace[16];
  char **messages = (char **)NULL;
  int i, trace_size = 0;

  trace_size = backtrace(trace, 16);
  /* overwrite sigaction with caller's address */
  messages = backtrace_symbols(trace, trace_size);
  /* skip first stack frame (points here) */
  printf("[bt] Execution path:\n");
  for (i = 1; i < trace_size; ++i)
  {
    printf("[bt] #%d %s\n", i, messages[i]);

    /* find first occurence of '(' or ' ' in message[i] and assume
     * everything before that is the file name. (Don't go beyond 0 though
     * (string terminator)*/
    size_t p = 0;
    while(messages[i][p] != '(' && messages[i][p] != ' '
            && messages[i][p] != 0)
        ++p;

    char syscom[256];
    sprintf(syscom,"addr2line %p -e %.*s", trace[i], p, messages[i]);
        //last parameter is the file name of the symbol
    system(syscom);
  }
}

  
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

/* Read-write estimators constants. */

typedef std::map<std::string, double> constant_map_t;

// values of constants which are read from a file
static constant_map_t preloaded_constants;
// values of constants which are to be written to a file
static constant_map_t recorded_constants;

static void print_constant(FILE* out, std::string name, double cst) {
  fprintf(out,         "%s %lf\n", name.c_str(), cst);
}

static void parse_constant(char* buf, double& cst, std::string line) {
  sscanf(line.c_str(), "%s %lf", buf, &cst);
}

static std::string get_dflt_constant_path() {
  std::string executable = deepsea::cmdline::name_of_my_executable();
  return executable + ".cst";
}

static std::string get_path_to_constants_file_from_cmdline(std::string flag) {
  std::string outfile;
  if (deepsea::cmdline::parse_or_default_bool(flag, false, false))
    return get_dflt_constant_path();
  else
    return deepsea::cmdline::parse_or_default_string(flag + "_in", "", false);
}

static void try_read_constants_from_file() {
  std::string infile_path = get_path_to_constants_file_from_cmdline("read_csts");
  if (infile_path == "")
    return;
  std::string cst_str;
  std::ifstream infile;
  infile.open (infile_path.c_str());
  while(! infile.eof()) {
    getline(infile, cst_str);
    if (cst_str == "")
      continue; // ignore trailing whitespace
    char buf[4096];
    double cst;
    parse_constant(buf, cst, cst_str);
    std::string name(buf);
    preloaded_constants[name] = cst;
  }
}

static void try_write_constants_to_file() {
  std::string outfile_path = get_path_to_constants_file_from_cmdline("write_csts");
  if (outfile_path == "")
    return;
  static FILE* outfile;
  outfile = fopen(outfile_path.c_str(), "w");
  constant_map_t::iterator it;
  for (it = recorded_constants.begin(); it != recorded_constants.end(); it++)
    print_constant(outfile, it->first, it->second);
  fclose(outfile);
}


/*---------------------------------------------------------------------*/
/* */

#if defined(PLOGGING) || defined(ESTIMATOR_LOGGING)
pasl::pctl::perworker::array<int, pasl::pctl::perworker::get_my_id> threads_number(0);
#endif
  
template <class Body_fct1, class Body_fct2>
void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(PLOGGING) || defined(ESTIMATOR_LOGGING)
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
  
#if defined(PLOGGING) || defined(ESTIMATOR_LOGGING)
int threads_created() {
  int value = threads_number.reduce([&] (int a, int b) { return a + b; }, 1);
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

class estimator : pasl::pctl::callback::client {
private:
  
  constexpr static const double min_report_shared_factor = 2.0;
  constexpr static const double weighted_average_factor = 8.0;
  
  cost_type shared;

  perworker_type<cost_type> privates;

#ifdef HONEST
  perworker_type<bool> to_be_estimated;
#endif

#ifdef ESTIMATOR_LOGGING
  perworker_type<long> reports_number;
#endif

#ifdef TIMING
  perworker_type<cycles_type> last_report;
#endif

#ifdef TIMING
  double wait_report = 10 * local_ticks_per_microsecond;
#endif

  std::string name;

  std::atomic<bool> estimated;
  
  cost_type get_constant() {
#ifdef CONSTANTS
    return shared;
#endif
    cost_type cst = privates.mine();
    // if local constant is undefined, use shared cst
    if (cst == cost::undefined) {
      return shared;
    }
    // else return local constant
    return cst;
  }
  
  cost_type get_constant_or_pessimistic() {
#ifdef CONSTANTS
    return shared;
#endif
    cost_type cst = get_constant();
    assert (cst != 0.);
    if (cst == cost::undefined) {
      return cost::pessimistic;
    } else {
      return cst;
    }
  }

  void update_shared(cost_type new_cst) {
#ifdef PLOGGING
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
#ifdef PLOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_UPDATE, name.c_str(), new_cst);
#endif
    privates.mine() = new_cst;
  }
  
public:
  
  void init();
  void output();
  void destroy();

  estimator() {
    init();
  }

  estimator(std::string name)
  : name(name) {
    init();
#ifdef PLOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_NAME, name.c_str());
#endif
    pasl::pctl::callback::register_client(this);
  }

  std::string get_name() {
    return name;
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
    return !estimated.load();
  }

#ifdef ESTIMATOR_LOGGING
  long number_of_reports() {
    return reports_number.reduce([&] (long a, long b) { return a + b; }, 0);
  }
#endif

  void report(complexity_type complexity, cost_type elapsed) {
#ifdef CONSTANTS
    return;
#endif
#ifdef TIMING
    cycles_type now_t = now();
    if (now_t - last_report.mine() < wait_report) {
      return;
    }
    last_report.mine() = now_t;
#endif
    
    double elapsed_time = elapsed / local_ticks_per_microsecond;
    cost_type measured_cst = elapsed_time / complexity;

#ifdef ESTIMATOR_LOGGING
    reports_number.mine()++;
#endif

#ifdef PLOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_REPORT, name.c_str(), complexity, elapsed_time, measured_cst);
#endif

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
#ifdef CONSTANTS
    return shared * ((double) complexity);
#endif
    // tiny complexity leads to tiny cost
    if (complexity == complexity::tiny) {
      return cost::tiny;
    }
    // complexity shouldn't be undefined
    assert (complexity >= 0);
    // compute the constant multiplied by the complexity
    cost_type cst = get_constant_or_pessimistic();

#ifdef PLOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_PREDICT, name.c_str(), complexity, cst * complexity, cst);
#endif

    return cst * ((double) complexity);
  }
};
  
cost_type kappa = 300.0;

} // end namespace

#ifdef ESTIMATOR_LOGGING
std::atomic<int> estimator_id;
estimator* estimators[10];

void print_reports() {
  int total = estimator_id.load();
  for (int i = 0; i < total; i++) {
    std::cout << "Estimator " << estimators[i]->get_name() << " has " << estimators[i]->number_of_reports() << " reports" << std::endl;
  }
}
#endif


void estimator::init() {
    shared = cost::undefined;
    privates.init(cost::undefined);
#ifdef HONEST
    to_be_estimated.init(false);
#endif
    estimated = false;
#ifdef ESTIMATOR_LOGGING
    int id = estimator_id++;
    estimators[id] = this;
    reports_number.init(0);
#endif
#ifdef TIMING
    last_report.init(0);
#endif

  constant_map_t::iterator preloaded = preloaded_constants.find(get_name());
  if (preloaded != preloaded_constants.end())
    shared = preloaded->second;
}

void estimator::destroy() {

}

void estimator::output() {
  recorded_constants[name] = estimator::get_constant();
}
  
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
#ifdef MANUAL_CONTROL
  par_body_fct();
  return;
#endif
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
  } else {
#elif HONEST
  if (estimator.is_undefined()) {
    c = Unknown;
  } else if (nested_unknown.mine() > 0) {
    c = Sequential;
  } else {
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

  c = execmode_combine(my_execmode(), c);
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
#if defined(PCTL_PARALLEL_ELISION) || defined(MANUAL_CONTROL)
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
