/*!
 * \file fib.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "example.hpp"
#include "io.hpp"
#include "dpsdatapar.hpp"
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
      if (n == 0 || n == 1) {
        return n;
      }
      long long a = 0, b = 0;
      granularity::fork2(
        [&] { granularity::cstmt(cfib, [&] { return comp(n - 1); }, [&] { a = fib_par(n - 1); }, [&] { a = fib_seq(n - 1); }); },
        [&] { granularity::cstmt(cfib, [&] { return comp(n - 2); }, [&] { b = fib_par(n - 2); }, [&] { b = fib_seq(n - 2); }); }
      );
      return a + b;
    }

    void ex() {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1000);
      long long ans = 0;
      granularity::cstmt(cfib, [&] { return comp(n); }, [&] { ans = fib_par(n); }, [&] { ans = fib_seq(n); });
      std::cout << ans << std::endl;
    }
  }
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pbbs::launch(argc, argv, [&] {
    pasl::util::cmdline::set(argc, argv);
    auto start = std::chrono::system_clock::now();
    pasl::pctl::ex();
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<float> diff = end - start;
    printf ("exectime %.3lf\n", diff.count());
  });
  return 0;

}



/***********************************************************************/