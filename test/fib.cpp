/*!
 * \file fib.cpp
 * \brief Fibonacci example
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
#include <math.h>
#include <chrono>

/***********************************************************************/

namespace pasl {
  namespace pctl {
    granularity::control_by_prediction cfib("fib");

    double phi = (1 + sqrt(5)) / 2;

    double comp(int n) {
      return pow(phi, n);
    }

    long long fib_seq(int n) {
      if (n == 0 || n == 1) {
        return n;
      } else {
        return fib_seq(n - 1) + fib_seq(n - 2);
      }
    }

    long long fib_par(int n) {
      long long result = 0;
      granularity::cstmt(cfib, [&] { return comp(n); }, [&] {
        if (n == 0 || n == 1) {
          result = n;
          return;
        }
        long long a = 0, b = 0;
        granularity::fork2(
          [&] { a = fib_par(n - 1); },
          [&] { b = fib_par(n - 2); }
        );
        result = a + b;
      }, [&] { result = fib_seq(n); });
      return result;
    }

    void ex() {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1000);
      long long ans = fib_par(n);
      std::cout << ans << std::endl;
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