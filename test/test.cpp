/*!
 * \file nested_loop.cpp
 * \brief Nested loops example
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
#include "array.hpp"
#include "scan.hpp"
#include <math.h>
#include <chrono>

/***********************************************************************/

/*
  To get expected behaviour, one should run with n=10000000, m=400 and n=400,m=1000000
*/

namespace pasl {
  namespace pctl {
    void ex() {
      int n = pasl::util::cmdline::parse_or_default_int("n", 1000);
      parutils::array::array<int> x(n);
      x.fill(1);
      int result = parutils::array::utils::reduce(x, 0, [&] (int a, int b) { return a + b; });
      printf("%d\n", result);
      for (int i = 0; i < 10; i++) {
        printf("x[%d] = %d\n", i * n / 10, x[i * n / 10]);
      }
      parutils::array::array<int> scan_exclusive_result = parutils::array::utils::scan_exclusive(x, 0, [&] (int a, int b) { return a + b; });
      for (int i = 0; i < 10; i++) {
        printf("scan_exclusive[%d] = %d\n", i * n / 10, scan_exclusive_result[i * n / 10]);
      }
      parutils::array::array<int> scan_inclusive_result = parutils::array::utils::scan_inclusive(x, 0, [&] (int a, int b) { return a + b; });
      for (int i = 0; i < 10; i++) {
        printf("scan_inclusive[%d] = %d\n", i * n / 10, scan_inclusive_result[i * n / 10]);
      }
    }                                                                                                                  
  }
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pbbs::launch(argc, argv, [&] {
    auto start = std::chrono::system_clock::now();
    pasl::pctl::ex();
    auto end = std::chrono::system_clock::now();

    //del;

    std::chrono::duration<float> diff = end - start;
    printf ("exectime %.3lf\n", diff.count());
  });
  return 0;

}



/***********************************************************************/