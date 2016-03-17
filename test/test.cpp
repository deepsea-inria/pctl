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
#include "weighted_map.hpp"
#include "weighted_reduce.hpp"
#include <math.h>
#include <chrono>

/***********************************************************************/

/*
  To get expected behaviour, one should run with n=10000000, m=400 and n=400,m=1000000
*/

template <class T>
using array = parutils::array::array<T>;
array<array<int>>* x;

namespace pasl {
  namespace pctl {

    void ex() {
      auto array_union = [&] (array<int>& a, array<int>& b) {
        array<int> c(a.size() + b.size());
        int size_a = a.size(), size_b = b.size();
        for (int i = 0; i < size_a; i++) {
          c[i] = a[i];
        }
        for (int i = 0; i < size_b; i++) {
          c[i + size_a] = b[i];
        }
        return c;
      };

//      auto split = parutils::array::utils::splitting::binary_splitting;
//      auto split = parutils::array::utils::splitting::binary_search_splitting;
      auto split = parutils::array::utils::splitting::hybrid_splitting;

      array<int> zero(0);
      array<int> reduce_result = parutils::array::utils::weighted_sequence_reduce(*x, zero, array_union, [&] (const array<int>& x) { return 1; }, split);
      
      for (int i = 0; i < 10; i++) {
        int j = i * x->size() / 10;
        j = (j + 1) * (j + 2) / 2 - 1;
        printf("reduce_result[%d] = %d\n", j, reduce_result.at(j));
      }

/*      parutils::array::array<int> x(n);
      x.fill(1);
      int result = parutils::array::utils::weighted_reduce_constant_multiplication(x, 0, [&] (int a, int b) { return a + b; }, [&] (int x) { return 1; });
      printf("%d\n", result);*
      parutils::array::array<int> map_result = parutils::array::utils::map(x, [&] (int x) { return 2; });//, [&] (int x) { return 1; });
      for (int i = 0; i < 10; i++) {
        printf("map_result[%d] = %d\n", i * n / 10, map_result[i * n / 10]);
      }*/
/*      int result = parutils::array::utils::reduce(x, 0, [&] (int a, int b) { return a + b; });
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
      }*/
    }                                                                                                                  
  }
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pbbs::launch(argc, argv, [&] {
    int n = pasl::util::cmdline::parse_or_default_int("n", 1000);
    x = new array<array<int>>(n);
    for (int i = 0; i < n; i++) {
      new (&x->at(i)) array<int>(i + 1);
      for (int j = 0; j < i + 1; j++) {
        x->at(i).at(j) = j;
      }
    }


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