/*!
 * \file nested_3_loop.cpp
 * \brief 3 nested loops example
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "example.hpp"
#include "io.hpp"
#include "datapar.hpp"
#include "cmdline.hpp"
#include "ploop.hpp"
#include <math.h>
#include <chrono>

/***********************************************************************/

namespace pasl {
  namespace pctl {
    int m, k;
    double comp_loop1(int l, int r) {
      return m * k * (r - l);
    }

    double comp_loop2(int l, int r) {
      return k * (r - l);
    }

    double comp_loop3(int l, int r) {
      return r - l;
    }

    void ex() {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1000);
      m = pasl::util::cmdline::parse_or_default_int("m", 1000);
      k = pasl::util::cmdline::parse_or_default_int("k", 1000);

      perworker::array<long, perworker::get_my_id> cnt;
      cnt.init(0);

      range::parallel_for(0, n, &comp_loop1, [&] (int i) { 
        range::parallel_for(0, m, &comp_loop2, [&] (int i) {
          range::parallel_for(0, k, &comp_loop3, [&] (int i) {
            cnt.mine()++;
          });
        });
      });
      std::cout << cnt.reduce([&] (long a, long b) { return a + b; }, 0) << std::endl;
    }
  }
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pbbs::launch(argc, argv, [&] {
    auto start = std::chrono::system_clock::now();
    pasl::pctl::ex();
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<float> diff = end - start;
    printf ("exectime %.3lf\n", diff.count());
#ifdef LOGGING
    pasl::pctl::logging::dump();
    printf("number of created threads: %d\n", pasl::pctl::granularity::threads_created());
#endif
  });
  return 0;

}



/***********************************************************************/