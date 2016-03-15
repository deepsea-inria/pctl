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

#include <sys/time.h>

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

static inline
long long get_wall_time() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return t.tv_sec * 1000000000LL + t.tv_nsec;
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
<<<<<<< HEAD
int nb_proc = 40;
#if defined(PLOGGING) || defined(THREADS)
pasl::pctl::perworker::array<int, pasl::pctl::perworker::get_my_id> threads_number(0);
=======

#if defined(LOGGING) || defined(ESTIMATOR_LOGGING)
pasl::pctl::perworker::array<int, pasl::pctl::perworker::get_my_id> threads_number;
>>>>>>> last changes to commit
#endif
  
template <class Body_fct1, class Body_fct2>
void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
<<<<<<< HEAD
#if defined(PLOGGING) || defined(THREADS)
=======
#if defined(LOGGING) || defined(ESTIMATOR_LOGGING)
>>>>>>> last changes to commit
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
  
<<<<<<< HEAD
#if defined(PLOGGING) || defined(THREADS)
=======
#if defined(LOGGING) || defined(ESTIMATOR_LOGGING)
>>>>>>> last changes to commit
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
<<<<<<< HEAD
  Force_parallel = 0,
  Force_sequential = 1,
  Sequential = 2,
  Parallel = 3,
  Unknown_sequential = 4,
  Unknown_parallel = 5
=======
  Force_parallel,
  Force_sequential,
  Sequential,
  Parallel,
  Unknown
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
};

// `p` configuration of caller; `c` callee
static inline
execmode_type execmode_combine(execmode_type p, execmode_type c) {
  // callee gives priority to Force*
  if (c & 6 == 0) {
    return c;
  }
  // callee gives priority to caller when caller is Sequential
#if defined(HONEST) || defined(OPTIMISTIC)
//  if (p & 1 == 0) { // mode is sequential
//    if (c & 4 != 0) { // mode is unknown
  if (p == Sequential || p == Unknown_sequential) {
    if (c == Unknown_sequential || c == Unknown_parallel) {
      return Unknown_sequential;
    }

    return Sequential;
  }
#else
  if (p == Sequential) {
    return Sequential;
  }
#endif

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
#ifdef OPTIMISTIC
static constexpr cost_type pessimistic = std::numeric_limits<double>::infinity();
#else
static constexpr cost_type pessimistic = 1.0;
#endif
} // end namespace

namespace {
  
<<<<<<< HEAD
/*double cpu_frequency_ghz = 2.1;
double local_ticks_per_microsecond = cpu_frequency_ghz * 1000.0;*/

class estimator : pasl::pctl::callback::client {
//private:
public:

  constexpr static double cpu_frequency_ghz = 2.1;
  constexpr static double local_ticks_per_microsecond = cpu_frequency_ghz * 1000.0;

=======
double cpu_frequency_ghz = 2.1;
double local_ticks_per_microsecond = cpu_frequency_ghz * 1000.0;

class estimator {
private:
>>>>>>> log number of reports to estimators
  
  constexpr static const double min_report_shared_factor = 2.0;
  constexpr static const double weighted_average_factor = 8.0;
  
  cost_type shared;

  perworker_type<cost_type> privates;

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
#ifdef HONEST
  perworker_type<bool> to_be_estimated;
#endif

#ifdef REPORTS
  perworker_type<long> reports_number;
#endif

#ifdef TIMING
  perworker_type<cycles_type> last_report;
#endif

#ifdef TIMING
  double wait_report = 10 * local_ticks_per_microsecond;
#endif

  std::string name;    
#if defined(HONEST) || defined(OPTIMISTIC)
  // 5 cold runs
  constexpr static const int number_of_cold_runs = 5;
  bool estimated;
  perworker_type<double> first_estimation;
  perworker_type<int> estimations_left;
#endif

=======
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
#ifdef HONEST
>>>>>>> nested loops works
  perworker_type<bool> to_be_estimated;
#endif

#ifdef ESTIMATOR_LOGGING
  perworker_type<long> reports_number;
#endif

#ifdef TIMING
  perworker_type<cycles_type> last_report;
  double wait_report = 1000 * local_ticks_per_microsecond;
#endif

  std::atomic<bool> estimated;
  
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
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
/*    cost_type cst = get_constant();
    assert (cst != 0.);
    if (cst == cost::undefined) {
      return cost::pessimistic;
    } else {
      return cst;
    }*/
    cost_type cst = privates.mine();
    if (cst != cost::undefined) {
      return cst;
    } else {
      cst = shared;
      if (cst == cost::undefined) {
        return cost::pessimistic;
      } else {
        return cst;
      }
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
<<<<<<< HEAD
  
<<<<<<< HEAD
//public:
=======
=======

#ifdef ESTIMATOR_LOGGING
  void init();
<<<<<<< HEAD
#else  
>>>>>>> log number of reports to estimators
=======
#else
>>>>>>> last changes to commit
  void init() {
    shared = cost::undefined;
    privates.init(cost::undefined);
#ifdef HOMEST
    to_be_estimated.init(false);
#endif
    estimated = false;
    last_report.init(0);
  }
#endif
  
public:
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
  
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

<<<<<<< HEAD
<<<<<<< HEAD
  std::string get_name() {
    return name;
  }

  std::string get_name() {
    return name;
  }

#ifdef HONEST
  void set_to_be_estimated(bool value) {
    to_be_estimated.mine() = value;
<<<<<<< HEAD
=======
  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> nested loops works
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> nested loops works
#endif

#if defined(HONEST) || defined(OPTIMISTIC)
  bool is_undefined() {
/*    if (is_estimated.mine()) {
      return false;
    }
    if (estimated.load() <= 0) {
      is_estimated.mine() = true;
      return false;
    }
    return true;*/
//    return shared == cost::undefined;
    return !estimated;
  }

  bool locally_undefined() {
    return privates.mine() == cost::undefined;
  }                        
#endif

#ifdef REPORTS
  long number_of_reports() {
    return reports_number.reduce([&] (long a, long b) { return a + b; }, 0);
  }
#endif

  void report(complexity_type complexity, cost_type elapsed, bool forced) {
#ifdef CONSTANTS
    return;
#endif
#ifdef TIMING
    if (!forced) {
      cycles_type now_t = now();
      if (now_t - last_report.mine() < wait_report) {
        return;
      }
      last_report.mine() = now_t;
    }
#endif
    
=======

  bool is_undefined() {
    return estimated.load();
  }

#ifdef ESTIMATOR_LOGGING
  long number_of_reports() {
    return reports_number.reduce([&] (long a, long b) { return a + b; }, 0);
  }
#endif

  void report(complexity_type complexity, cost_type elapsed) {
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
#ifdef TIMING
    cycles_type now_t = now();
    if (now_t - last_report.mine() < wait_report) {
      return;
    }
    last_report.mine() = now_t;
#endif
    
>>>>>>> last changes to commit
    double elapsed_time = elapsed / local_ticks_per_microsecond;
    cost_type measured_cst = elapsed_time / complexity;    

<<<<<<< HEAD
<<<<<<< HEAD
#ifdef REPORTS
    reports_number.mine()++;
=======
=======
#ifdef ESTIMATOR_LOGGING
    reports_number.mine()++;
#endif

>>>>>>> log number of reports to estimators
#ifdef LOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_REPORT, name.c_str(), complexity, elapsed_time, measured_cst);
>>>>>>> nested loops works
#endif

<<<<<<< HEAD
#ifdef PLOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_REPORT, name.c_str(), complexity, elapsed_time, measured_cst);
#endif

#if defined(OPTIMISTIC) || defined(HONEST)
/*    bool cold_run = false;
    if (!is_undefined()) {
      int cnt = estimated--;
      if (cnt == number_of_cold_runs) { // Do not report first cold run
        return;
      }
      cold_run = cnt > 0;
      measured_cst = std::min(measured_cst, shared);
    }*/

/*      if (estimations_left.mine() > 0) { // cold run
        int& x = estimations_left.mine();
        double& estimation = first_estimation.mine();
        estimation = std::min(estimation, measured_cst);
//        std::cerr << estimation << " " << measured_cst << std::endl;
        x--;
        if (x > 0) {
          return;
        }
        measured_cst = estimation;
        estimated = true;
      }*/
      if (is_undefined()) {  
        int& x = estimations_left.mine();
        x--;
        if (shared == cost::undefined || shared > measured_cst * min_report_shared_factor)
          shared = measured_cst;
        if (x > 0) {
          return;
        }
        estimated = true;
        return;
      }
    cost_type cst = get_constant();
/*    if (cst == cost::undefined) {
      // handle the first measure without average
      update(measured_cst);
    } else*/

/* 
      if (!is_estimated.mine()) { // cold run
        is_estimated.mine() = true;
        return;
      }*/
=======

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

  bool set_to_be_estimated() {
    to_be_estimated.mine() = true;
  }

  bool is_to_be_estimated() {
    return to_be_estimated.mine();
  }

  bool is_undefined() {
    return !estimated.load();
  }
  
  void report(complexity_type complexity, cost_type elapsed) {
    double elapsed_time = elapsed / local_ticks_per_microsecond;
    cost_type measured_cst = elapsed_time / complexity;
#if defined(OPTIMISTIC) || defined(HONEST)
    if (!estimated.exchange(true)) {
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
#if defined(OPTIMISTIC) || defined(HONEST)
    if (!estimated.exchange(true)) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
#else
    cost_type cst = get_constant();
    if (cst == cost::undefined) {
#endif
      // handle the first measure without average
      update(measured_cst);
<<<<<<< HEAD
    } else
#endif
    {

=======
    } else {
#if defined(OPTIMISTIC) || defined(HONEST)
      cost_type cst = get_constant();
#endif
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
      // compute weighted average
      update(((weighted_average_factor * cst) + measured_cst)
             / (weighted_average_factor + 1.0));
    }
//    std::cerr << complexity << " " << get_constant() << " " << elapsed << std::endl;
  }

  void report(complexity_type complexity, cost_type elapsed) {
    report(complexity, elapsed, false);
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

<<<<<<< HEAD
#ifdef REPORTS
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
#if defined(HONEST) || defined(OPTIMISTIC)
    estimated = false;
    estimations_left.init(number_of_cold_runs);
    first_estimation.init(std::numeric_limits<double>::max());
#endif
#ifdef REPORTS
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
=======
#ifdef ESTIMATOR_LOGGING
std::atomic<int> estimator_id;
estimator* estimators[10];

void estimator::init() {
    shared = cost::undefined;
    privates.init(cost::undefined);
#ifdef HOMEST
    to_be_estimated.init(false);
#endif
    estimated = false;
    int id = estimator_id++;
    estimators[id] = this;
    reports_number.init(0);
    last_report.init(0);
}

void print_reports() {
  int total = estimator_id.load();
  for (int i = 0; i < total; i++) {
    std::cout << "Estimator " << estimators[i]->get_name() << " has " << estimators[i]->number_of_reports() << " reports" << std::endl;
  }
}
#endif

>>>>>>> log number of reports to estimators
  

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

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
static inline
double since_in_cycles(long long start) {
  return (get_wall_time() - start) * estimator::cpu_frequency_ghz;
}

=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
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

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
template <class Body_fct>
void cstmt_unknown(execmode_type c, complexity_type m, Body_fct& body_fct, estimator& estimator) {
#ifdef OPTIMISTIC
  cost_type upper_adjustment = time_adjustment.mine();
=======
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
template <class Par_body_fct>
void cstmt_unknown(complexity_type m, Par_body_fct& par_body_fct, estimator& estimator) {
#ifdef OPTIMISTIC
  double upper_adjustment = time_adjustment.mine();
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
  time_adjustment.mine() = 0;
#elif HONEST
  if (estimator.is_undefined() && !estimator.is_to_be_estimated()) {
    nested_unknown.mine()++;
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
    estimator.set_to_be_estimated(true);
  }
#endif

#ifdef CYCLES
  cost_type start = now();
#else
  long long start = get_wall_time();
#endif
  execmode.mine().block(c, body_fct);
#ifdef CYCLES
  cost_type elapsed = since(start);
#else
  cost_type elapsed = since_in_cycles(start);
#endif

#ifdef OPTIMISTIC
  if (estimator.is_undefined()) {
//  if (estimator.locally_undefined()) {
    estimator.report(std::max((complexity_type) 1, m), elapsed + time_adjustment.mine(), true);
//    if (pasl::pctl::perworker::get_my_id()() == 0)
//    std::cerr << "Undefined report " << std::max((complexity_type) 1, m) << " " << estimator.estimated << " " << estimator.shared << " " << elapsed << " " << time_adjustment.mine() << " " << estimator.get_name() << std::endl;
  }
#elif HONEST
  if (estimator.is_undefined()) {
    estimator.report(std::max((complexity_type) 1, m), elapsed, true);
  }
#endif

#ifdef HONEST
  if (estimator.is_to_be_estimated()) {
    estimator.set_to_be_estimated(false);
    nested_unknown.mine()--;
  }
#endif

#ifdef OPTIMISTIC
  time_adjustment.mine() = upper_adjustment + time_adjustment.mine();//estimator.predict(std::max((complexity_type) 1, m)) - elapsed, (double)0);
=======
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
    estimator.set_to_be_estimated();
=======
    estimator.set_to_be_estimated(true);
>>>>>>> nested loops works
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
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
#endif
}

template <class Seq_body_fct>
void cstmt_sequential_with_reporting(complexity_type m,
                                     const Seq_body_fct& seq_body_fct,
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
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
//    c = estimator.predict(std::max((complexity_type)1, m)) <= kappa ? Unknown_sequential : Unknown_parallel;
    c = Unknown_parallel;
//    m = complexity_measure_fct();
  } else {
    if (my_execmode() == Sequential || my_execmode() == Unknown_sequential) {
      execmode.mine().block(Sequential, seq_body_fct);
      return;
    }
#elif HONEST
  if (estimator.is_undefined()) {
    c = Unknown_parallel;
  } else if (nested_unknown.mine() > 0) {
    c = Sequential;
  } else {
#endif
//    m = complexity_measure_fct();
=======
    c = Unknown;
  } else {
#elif HONEST
  if (estimator.is_undefined() || nested_unknown.mine() > 0) {
    c = Unknown;
  } else {
#endif
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
    c = Unknown;
  } else {
#elif HONEST
  if (estimator.is_undefined() || nested_unknown.mine() > 0) {
    c = Unknown;
  } else {
#endif
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
    c = Unknown;
  } else {
#elif HONEST
  if (estimator.is_undefined() || nested_unknown.mine() > 0) {
    c = Unknown;
  } else {
#endif
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
    c = Unknown;
  } else {
#elif HONEST
  if (estimator.is_undefined() || nested_unknown.mine() > 0) {
    c = Unknown;
  } else {
#endif
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
    c = Unknown;
  } else {
#elif HONEST
  if (estimator.is_undefined()) {
    c = Unknown;
  } else if (nested_unknown.mine() > 0) {
    c = Sequential;
  } else {
#endif
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
    if (m == complexity::tiny) {
      c = Sequential;
    } else if (m == complexity::undefined) {
      c = Parallel;
    } else {
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
      if (
#ifndef OPTIMISTIC
          my_execmode() == Sequential ||
#endif
          estimator.predict(std::max((complexity_type)1, m)) <= kappa) {
=======
      if (estimator.predict(std::max((complexity_type)1, m)) <= kappa) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
      if (estimator.predict(std::max((complexity_type)1, m)) <= kappa) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
      if (estimator.predict(std::max((complexity_type)1, m)) <= kappa) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
      if (estimator.predict(std::max((complexity_type)1, m)) <= kappa) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
      if (estimator.predict(std::max((complexity_type)1, m)) <= kappa) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
        c = Sequential;
      } else {
        c = Parallel;
      }
    }
#if defined(OPTIMISTIC) || defined(HONEST)
  }
#endif
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
  c = execmode_combine(my_execmode(), c);
  if (c == Unknown_sequential) {
    cstmt_unknown(c, m, seq_body_fct, estimator);
  } else if (c == Unknown_parallel) {
    cstmt_unknown(c, m, par_body_fct, estimator);
  } else if (c == Sequential) {
/*#ifdef OPTIMISTIC
    if (my_execmode() == Sequential || my_execmode() == Unknown_sequential) {
      cstmt_sequential(Sequential, seq_body_fct);
    } else
#endif*/
=======
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST

  c = execmode_combine(my_execmode(), c);
  if (c == Unknown) {
    cstmt_unknown(m, par_body_fct, estimator);
  } else if (c == Sequential) {
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
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

// controlled statement with built in estimators
constexpr char default_name[] = "auto";
template <
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<default_name, 1, Complexity_measure_fct, Par_body_fct, Seq_body_fct>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
}

template <
class Complexity_measure_fct,
class Par_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<default_name, 1, Complexity_measure_fct, Par_body_fct>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct);
}

template <
const char* estimator_name,
class ... Types,
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<estimator_name, 1, int>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
}

template <
const char* estimator_name,
class ... Types,
class Complexity_measure_fct,
class Par_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<estimator_name, 1, int>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct);
}

template <
const char* method_name,
int id,
class ... Types,
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<method_name, id, Types...>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
}

template <
const char* method_name,
int id,
class ... Types,
class Complexity_measure_fct,
class Par_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<method_name, id, Types...>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct);
}

template <
class ... Types,
class Complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<default_name, 1, Types...>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct, seq_body_fct);
}

template <
class ... Types,
class Complexity_measure_fct,
class Par_body_fct
>
void cstmt(const Complexity_measure_fct& complexity_measure_fct,
           const Par_body_fct& par_body_fct
           ) {
    using controller_type = pasl::pctl::granularity::controller_holder<default_name, 1, Types...>;
    cstmt(controller_type::controller, complexity_measure_fct, par_body_fct);
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
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
  if ( (mode == Sequential) || (mode == Force_sequential)
#if defined(HONEST) || defined(OPTIMISTIC)
    || (mode == Unknown_sequential)
#endif
#ifdef HONEST
    || (mode == Unknown_parallel)
#endif
  ) {
=======
  if ( (mode == Sequential) || (mode == Force_sequential) || (mode == Unknown)) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
  if ( (mode == Sequential) || (mode == Force_sequential) || (mode == Unknown)) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
  if ( (mode == Sequential) || (mode == Force_sequential) || (mode == Unknown)) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
  if ( (mode == Sequential) || (mode == Force_sequential) || (mode == Unknown)) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
  if ( (mode == Sequential) || (mode == Force_sequential) || (mode == Unknown)) {
>>>>>>> bootstrapping techniques: OPTIMISTIC and HONEST
=======
  if ( (mode == Sequential) || (mode == Force_sequential)
#ifdef HONEST
    || (mode == Unknown)
#endif
  ) {
>>>>>>> nested loops works
    f1();
    f2();
  } else {
#ifdef OPTIMISTIC
    if (mode == Unknown_parallel) {
      cost_type approximation = time_adjustment.mine();
      cost_type left_approximation, right_approximation;
#ifdef CYCLES
      cost_type start = now();
#else
      long long start = get_wall_time();
#endif
      primitive_fork2([&] {
        time_adjustment.mine() = 0;
#ifdef CYCLES
        cost_type start = now();
#else
        long long start = get_wall_time();
#endif
        execmode.mine().block(mode, f1);
#ifdef CYCLES
        left_approximation = since(start) + time_adjustment.mine();
#else
        left_approximation = since_in_cycles(start) + time_adjustment.mine();
#endif
    
      }, [&] {
        time_adjustment.mine() = 0;
#ifdef CYCLES
        cost_type start = now();
#else
        long long start = get_wall_time();
#endif
        execmode.mine().block(mode, f2);
#ifdef CYCLES
        right_approximation = since(start) + time_adjustment.mine();
#else
        right_approximation = since_in_cycles(start) + time_adjustment.mine();
#endif
      });
#ifdef CYCLES
      cost_type elapsed = since(start);
#else
      cost_type elapsed = since_in_cycles(start);
#endif
      time_adjustment.mine() = approximation + (left_approximation + right_approximation - elapsed);//std::max(left_approximation + right_approximation - elapsed, (double)0);
//      time_adjustment.mine() = approximation + std::max(left_approximation + right_approximation - elapsed, (double)0);
      return;
    }
#endif
    primitive_fork2([&] {
      execmode.mine().block(mode, f1);
    }, [&] {
      execmode.mine().block(mode, f2);
    });
  }
}



template <class Last>
std::string type_name() {
  return std::string(typeid(Last).name());
}

template <class First, class Second, class ... Types>
std::string type_name() {
  return type_name<First>() + " " + type_name<Second, Types...>();
}

template <int id, class ... Types>
class controller_holder {
public:
  static control_by_prediction controller;
};

template <int id, class ... Types>
control_by_prediction controller_holder<id, Types ...>::controller(std::string("controller_holder ") + std::to_string(id) + " " + type_name<Types ...>());

} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_GRANULARITY_H_ */
