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
#include <sstream>

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
  
#ifdef TARGET_MAC_OS
#include <sys/time.h>
//clock_gettime is not implemented on OSX
int clock_gettime(int /*clk_id*/, struct timespec* t) {
  struct timeval now;
  int rv = gettimeofday(&now, NULL);
  if (rv) return rv;
  t->tv_sec  = now.tv_sec;
  t->tv_nsec = now.tv_usec * 1000;
  return 0;
}
long double get_wall_time() {
  struct timespec t;
  clock_gettime(0, &t);
  return t.tv_sec * 1000000000LL + t.tv_nsec;
}
#else
static inline
long double get_wall_time() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return t.tv_sec * 1000000000LL + t.tv_nsec;
}
#endif
  
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
//  std::string executable = deepsea::cmdline::name_of_my_executable();
//  return executable + ".cst";
  return "constants.txt";
}

static std::string get_path_to_constants_file_from_cmdline(std::string flag) {
  std::string outfile;
  if (deepsea::cmdline::parse_or_default_bool(flag, false, false))
    return get_dflt_constant_path();
  else
    return deepsea::cmdline::parse_or_default_string(flag + "_in", "", false);
}

static bool loaded = false;

static void try_read_constants_from_file() {
  if (loaded) {
    return;
  }
  loaded = true;
  std::string infile_path = get_dflt_constant_path();//get_path_to_constants_file_from_cmdline("read_csts");
  if (infile_path == "")
    return;
  std::string cst_str;
  std::ifstream infile;
  infile.open (infile_path.c_str());
  if (!infile.good()) {
    return;
  }
  std::cerr << "Load constants from constants.txt\n";
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
int nb_proc = 40;
#if defined(PLOGGING) || defined(THREADS_CREATED)
pasl::pctl::perworker::array<int, pasl::pctl::perworker::get_my_id> threads_number(0);
#endif
  
template <class Body_fct1, class Body_fct2>
void primitive_fork2(const Body_fct1& f1, const Body_fct2& f2) {
#if defined(PLOGGING) || defined(THREADS_CREATED)
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
  
#if defined(PLOGGING) || defined(THREADS_CREATED)
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
  Force_parallel = 0,
  Force_sequential = 1,
  Sequential = 2,
  Parallel = 3,
  Unknown_sequential = 4,
  Unknown_parallel = 5
};

// `p` configuration of caller; `c` callee
static inline
execmode_type execmode_combine(execmode_type p, execmode_type c) {
  // callee gives priority to Force*
  if (c & 6 == 0) {
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
#if defined(EASYOPTIMISTIC)
static constexpr cost_type pessimistic = std::numeric_limits<double>::infinity();
#else
static constexpr cost_type pessimistic = 1.0;
#endif
} // end namespace

namespace {
  
/*double cpu_frequency_ghz = 2.1;
double local_ticks_per_microsecond = cpu_frequency_ghz * 1000.0;*/

#if defined(KAPPA25)
cost_type kappa = 25.0;
#elif defined(KAPPA30)
cost_type kappa = 30.0;
#elif defined(KAPPA50)
cost_type kappa = 50.0;
#elif defined(KAPPA100)
cost_type kappa = 100.0;
#elif defined(KAPPA200)
cost_type kappa = 200.0;
#elif defined(KAPPA300)
cost_type kappa = 300.0;
#elif defined(KAPPA400)
cost_type kappa = 400.0;
#elif defined(KAPPA500)
cost_type kappa = 500.0;
#else
cost_type kappa = 300.0;
#endif

class estimator : pasl::pctl::callback::client {
//private:
public:

  constexpr static double cpu_frequency_ghz = 2.1;
  constexpr static double local_ticks_per_microsecond = cpu_frequency_ghz * 1000.0;

  
  constexpr static const double min_report_shared_factor = 2.0;
  constexpr static const double weighted_average_factor = 8.0;
  
  cost_type shared;

  perworker_type<cost_type> privates;

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
#ifdef EASYOPTIMISTIC
  // 5 cold runs
  constexpr static const int number_of_cold_runs = 5;
  bool estimated;
  perworker_type<double> first_estimation;
  perworker_type<int> estimations_left;
#endif

#ifdef SMART_ESTIMATOR
  cost_type size_for_shared;
  perworker_type<cost_type> last_reported_size;
#endif

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

#ifdef SMART_ESTIMATOR
  double update_size_ratio = 1.1;

  void load() {
    cost_type shared_size = size_for_shared;
    cost_type size = last_reported_size.mine();
    if (shared_size > update_size_ratio * size) { // || (size < shared_size && shared < privates.mine() / min_report_shared_factor)) {
      last_reported_size.mine() = shared_size;
      privates.mine() = shared;
    }
  }

  void update(cost_type new_cst, double new_size) {
    cost_type shared_size = size_for_shared;
    if (update_size_ratio * shared_size < new_size || (shared_size < new_size && new_cst < shared / min_report_shared_factor)) {
#ifdef PLOGGING
      pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_UPDATE_SHARED_SIZE, name.c_str(), new_size, new_cst, new_size * new_cst);
#endif
      size_for_shared = new_size;
      shared = new_cst;
    }

    cost_type size = last_reported_size.mine();
    if (update_size_ratio * size < new_size || (size < new_size && new_cst < privates.mine() / min_report_shared_factor)) {
#ifdef PLOGGING
      pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_UPDATE_SIZE, name.c_str(), new_size, new_cst, new_size * new_cst);
#endif
      last_reported_size.mine() = size;
      privates.mine() = new_cst;
    }/* else if (size / update_size_ratio <= new_size) {
      privates.mine() = (7 * privates.mine() + new_cst) / 8;
#ifdef PLOGGING
      pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_UPDATE_SIZE, name.c_str(), new_size, privates.mine(), new_size * new_cst);
#endif
    }*/
  }
#else
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
#endif // SMART_ESTIMATOR
  
//public:
  
  void init();
  void output();
  void destroy();

  estimator() {
    init();
  }

  estimator(std::string name) {
//  : name(name.substr(0, std::min(40, (int)name.length()))) {
    std::stringstream stream;
    stream << name.substr(0, std::min(40, (int)name.length())) << this;
    this->name = stream.str();
    init();
//    this->name = name.substr(0, std::min(40, (int)name.length()));
#ifdef PLOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_NAME, this->name.c_str());
#endif
    pasl::pctl::callback::register_client(this);
  }

  std::string get_name() {
    return name;
  }

#ifdef EASYOPTIMISTIC
  bool is_undefined() {
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
#ifdef REPORT_THRESHOLD
    if (elapsed < 100) {
      return;
    }
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
    
    double elapsed_time = elapsed / local_ticks_per_microsecond;
    cost_type measured_cst = elapsed_time / complexity;    

#ifdef REPORTS
    reports_number.mine()++;
#endif

#ifdef PLOGGING
//    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_REPORT, name.c_str(), complexity, elapsed_time, measured_cst);
#endif

#ifdef SMART_ESTIMATOR
    if (elapsed_time >= 10 * kappa) {
      return;
    }
    if (shared == cost::undefined) {
      estimated = true;
    }
    load();
    update(measured_cst, complexity);

    return;
#else
#ifdef EASYOPTIMISTIC
    if (is_undefined()) {  
      int& x = estimations_left.mine();
      x--;
      if (shared == cost::undefined || shared > measured_cst * min_report_shared_factor) {
        shared = measured_cst;
      }
      if (x > 0) {
        return;
      }
      estimated = true;
      return;
    }

    cost_type cst = get_constant();
#else
    cost_type cst = get_constant();
    if (cst == cost::undefined) {
      // handle the first measure without average
      update(measured_cst);
    } else
#endif
    {
      // compute weighted average
      update(((weighted_average_factor * cst) + measured_cst)
             / (weighted_average_factor + 1.0));
    }
#endif // SMART_ESTIMATOR
  }

  void report(complexity_type complexity, cost_type elapsed) {
    report(complexity, elapsed, false);
  }
  
  cost_type predict(complexity_type complexity) {
    // tiny complexity leads to tiny cost
    if (complexity == complexity::tiny) {
      return cost::tiny;
    }
#ifdef SMART_ESTIMATOR
    load();
    if (complexity > 2 * last_reported_size.mine()) {
      return kappa + 1;
    }
    if (complexity <= last_reported_size.mine()) {
      return kappa - 1;
    }
    return privates.mine() * ((double) complexity);
#else
    // complexity shouldn't be undefined
    assert (complexity >= 0);
    // compute the constant multiplied by the complexity
    cost_type cst = get_constant_or_pessimistic();

#ifdef PLOGGING
//    pasl::pctl::logging::log(pasl::pctl::logging::ESTIM_PREDICT, name.c_str(), complexity, cst * complexity, cst);
#endif

    return cst * ((double) complexity);
#endif
  }
};

} // end namespace

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
#ifdef EASYOPTIMISTIC
    estimated = false;
    estimations_left.init(5);//estimator::number_of_cold_runs);
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
#ifdef SMART_ESTIMATOR
  size_for_shared = 0;
  last_reported_size.init(0);
#endif

  try_read_constants_from_file();

  constant_map_t::iterator preloaded = preloaded_constants.find(get_name());
  if (preloaded != preloaded_constants.end()) {
#ifdef EASYOPTIMISTIC
    estimator::estimated = true;
#endif
    shared = preloaded->second;
  }
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

static inline
double since_in_cycles(long double start) {
  return (get_wall_time() - start) * estimator::cpu_frequency_ghz;
}

#ifdef EASYOPTIMISTIC
perworker_type<long double> timer(0);
#endif
perworker_type<cost_type> work(0);

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

template <class Body_fct>
void cstmt_unknown(execmode_type c, complexity_type m, Body_fct& body_fct, estimator& estimator) {
  cost_type upper_work = work.mine() + since_in_cycles(timer.mine());
  work.mine() = 0;

  timer.mine() = get_wall_time();

  execmode.mine().block(c, body_fct);

  work.mine() += since_in_cycles(timer.mine());

  estimator.report(std::max((complexity_type) 1, m), work.mine(), estimator.is_undefined());

  work.mine() = upper_work + work.mine();
  timer.mine() = get_wall_time();
}

template <class Seq_body_fct>
void cstmt_sequential_with_reporting(complexity_type m,
                                     const Seq_body_fct& seq_body_fct,
                                     estimator& estimator) {
  cost_type start = now();
  execmode.mine().block(Sequential, seq_body_fct);
  cost_type elapsed = since(start);
  estimator.report(std::max((complexity_type)1, m), elapsed);
#ifdef PLOGGING
    pasl::pctl::logging::log(pasl::pctl::logging::SEQUENTIAL_RUN, estimator.name.c_str(), m, elapsed / estimator::local_ticks_per_microsecond);
#endif
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
class Seq_complexity_measure_fct,
class Par_complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(control_by_prediction& contr,
           const Par_complexity_measure_fct& par_complexity_measure_fct,
           const Seq_complexity_measure_fct& seq_complexity_measure_fct,
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
  complexity_type m = seq_complexity_measure_fct();
  cost_type predicted;
  execmode_type c;
#ifdef EASYOPTIMISTIC
  if (estimator.is_undefined()) {
    c = Parallel;
  } else {
    if (my_execmode() == Sequential) {
      execmode.mine().block(Sequential, seq_body_fct);
      return;
    }
#endif
    if (m == complexity::tiny) {
      c = Sequential;
    } else if (m == complexity::undefined) {
      c = Parallel;
    } else {
#if !defined(EASYOPTIMISTIC)
      if (my_execmode() == Sequential) {
          c = Sequential;
      } else {
#endif
        complexity_type comp = std::max((complexity_type)1, m);
        predicted = estimator.predict(comp);
        if (predicted <= kappa) {
          c = Sequential;
        } else {
          c = Parallel;
        }
#if !defined(EASYOPTIMISTIC)
      }
#endif
    }
#if defined(EASYOPTIMISTIC)
  }
#endif
  c = execmode_combine(my_execmode(), c);
  if (c == Sequential) {
    cstmt_sequential_with_reporting(m, seq_body_fct, estimator);
  } else {
#ifdef EASYOPTIMISTIC
    cstmt_unknown(c, par_complexity_measure_fct(), par_body_fct, estimator);
#else
    cstmt_parallel(c, par_body_fct);
#endif // EASYOPTIMISTIC
  }
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
  cost_type predicted;
  execmode_type c;
#ifdef EASYOPTIMISTIC
  if (estimator.is_undefined()) {
    c = Parallel;
  } else {
    if (my_execmode() == Sequential) {
      execmode.mine().block(Sequential, seq_body_fct);
      return;
    }
#endif
    if (m == complexity::tiny) {
      c = Sequential;
    } else if (m == complexity::undefined) {
      c = Parallel;
    } else {
#if !defined(EASYOPTIMISTIC)
      if (my_execmode() == Sequential) {
        c = Sequential;
      } else {
#endif
        complexity_type comp = std::max((complexity_type)1, m);
        predicted = estimator.predict(comp);
        if (predicted <= kappa) {
          c = Sequential;
        } else {
          c = Parallel;
        }
#if !defined(EASYOPTIMISTIC)
      }
#endif
    }
#if defined(EASYOPTIMISTIC)
  }
#endif
  c = execmode_combine(my_execmode(), c);
  if (c == Sequential) {
    cstmt_sequential_with_reporting(m, seq_body_fct, estimator);
  } else {
#ifdef EASYOPTIMISTIC
    cstmt_unknown(c, m, par_body_fct, estimator);
#else
    cstmt_parallel(c, par_body_fct);
#endif // EASYOPTIMISTIC
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
/*template <
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
}*/

/***********************************************************************/
template <class Last>
std::string type_name() {
  return std::string(typeid(Last).name());
}

template <class First, class Second, class ... Types>
std::string type_name() {
  return type_name<First>() + "_" + type_name<Second, Types...>();
}

template <const char* method_name, int id, class ... Types>
class controller_holder {
public:
  static control_by_prediction controller;
};

template <const char* method_name, int id, class ... Types>
control_by_prediction controller_holder<method_name, id, Types ...>::controller(std::string("controller_holder_") + std::string(method_name) + "_" + std::to_string(id) + "_" + type_name<Types ...>());

// controlled statement with built in estimators
constexpr char default_name[] = "auto";

template <
class Par_complexity_measure_fct,
class Seq_complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(const Par_complexity_measure_fct& par_complexity_measure_fct,
           const Seq_complexity_measure_fct& seq_complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<default_name, 1, Par_complexity_measure_fct, Seq_complexity_measure_fct, Par_body_fct, Seq_body_fct>;
    cstmt(controller_type::controller, par_complexity_measure_fct, seq_complexity_measure_fct, par_body_fct, seq_body_fct);
}


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
class Par_complexity_measure_fct,
class Seq_complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(const Par_complexity_measure_fct& par_complexity_measure_fct,
           const Seq_complexity_measure_fct& seq_complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<estimator_name, 1, int>;
    cstmt(controller_type::controller, par_complexity_measure_fct, seq_complexity_measure_fct, par_body_fct, seq_body_fct);
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
class Par_complexity_measure_fct,
class Seq_complexity_measure_fct,
class Par_body_fct,
class Seq_body_fct
>
void cstmt(const Par_complexity_measure_fct& par_complexity_measure_fct,
           const Seq_complexity_measure_fct& seq_complexity_measure_fct,
           const Par_body_fct& par_body_fct,
           const Seq_body_fct& seq_body_fct) {
    using controller_type = pasl::pctl::granularity::controller_holder<method_name, id, Types...>;
    cstmt(controller_type::controller, par_complexity_measure_fct, seq_complexity_measure_fct, par_body_fct, seq_body_fct);
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
  if ((mode == Sequential) || (mode == Force_sequential)) {
    f1();
    f2();
  } else {
#ifdef EASYOPTIMISTIC
      cost_type upper_work = work.mine() + since_in_cycles(timer.mine());
      work.mine() = 0;
      cost_type left_work, right_work;
      primitive_fork2([&] {
        work.mine() = 0;
        timer.mine() = get_wall_time();
        execmode.mine().block(mode, f1);
        left_work = work.mine() + since_in_cycles(timer.mine());
      }, [&] {
        work.mine() = 0;
        timer.mine() = get_wall_time();
        execmode.mine().block(mode, f2);
        right_work = work.mine() + since_in_cycles(timer.mine());
      });
      work.mine() = upper_work + left_work + right_work;
        timer.mine() = get_wall_time();
      return;
#else
    primitive_fork2([&] {
      execmode.mine().block(mode, f1);
    }, [&] {
      execmode.mine().block(mode, f2);
    });
#endif
  }
}



} // end namespace
} // end namespace
} // end namespace


#endif /*! _PCTL_GRANULARITY_H_ */
