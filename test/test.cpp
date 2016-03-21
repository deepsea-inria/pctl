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
#include "filter.hpp"
#include <math.h>
#include <chrono>

/***********************************************************************/

/*
  To get expected behaviour, one should run with n=10000000, m=400 and n=400,m=1000000
*/

template <class T>
using array = parutils::array::array<T>;
array<std::shared_ptr<array<int>>>* x;
int n;

void filter_test() {
  array<int> x(n);
  pasl::pctl::parallel_for(0, n, [&] (int i) {
    x[i] = i;
  });
  array<int> result = parutils::array::utils::filter(x, [&] (int x) { return x % 2 == 0; });
  printf("filter_result size = %d\n", result.size());
  for (int i = 0; i < 10; i++) {
    printf("filter_result[%d] = %d\n", i * result.size() / 10, result.at(i * result.size() / 10));
  }
}

void weighted_reduce_shr_ptr_test() {
  auto split = parutils::array::utils::splitting::binary_splitting;
//  auto split = parutils::array::utils::splitting::binary_search_splitting;
//  auto split = parutils::array::utils::splitting::hybrid_splitting;

  auto array_union = [&] (std::shared_ptr<array<int>> a, std::shared_ptr<array<int>> b) {
    auto c = std::make_shared<array<int>>(a->size() + b->size());
    int size_a = a->size(), size_b = b->size();
//    for (int i = 0; i < size_a; i++) {
    pasl::pctl::parallel_for(0, size_a, [&] (int i) {
      c->at(i) = a->at(i);
    });
//    for (int i = 0; i < size_b; i++) {
    pasl::pctl::parallel_for(0, size_b, [&] (int i) {
      c->at(i + size_a)= b->at(i);
    });
    return c;
  };

  auto zero = std::make_shared<array<int>>(0);
  std::shared_ptr<array<int>> reduce_result = parutils::array::utils::weighted_sequence_reduce(*x, zero, array_union, [&] (std::shared_ptr<array<int>> x) { return x->size(); }, split);

  for (int i = 0; i < 10; i++) {
    int j = i * x->size() / 10;
    j = (j + 1) * (j + 2) / 2 - 1;
    printf("reduce_result[%d] = %d\n", j, reduce_result->at(j));
  }
}

/*void weighted_reduce_staight_test() {
  auto split = parutils::array::utils::splitting::binary_splitting;
//  auto split = parutils::array::utils::splitting::binary_search_splitting;
//  auto split = parutils::array::utils::splitting::hybrid_splitting;

  auto array_union = [&] (array<int>& a, array<int>& b) {
    array<int> c(a.size() + b.size());
    int size_a = a.size(), size_b = b.size();
//    for (int i = 0; i < size_a; i++) {
    pasl::pctl::parallel_for(0, size_a, [&] (int i) {
      c.at(i) = a.at(i);
    });
//    for (int i = 0; i < size_b; i++) {
    pasl::pctl::parallel_for(0, size_b, [&] (int i) {
      c.at(i + size_a)= b.at(i);
    });
    return c;
  };

  array<int> zero(0);
  array<int> reduce_result = parutils::array::utils::weighted_sequence_reduce(*x, zero, array_union, [&] (const array<int>& x) { return x.size(); }, split);

  for (int i = 0; i < 10; i++) {
    int j = i * x->size() / 10;
    j = (j + 1) * (j + 2) / 2 - 1;
    printf("reduce_result[%d] = %d\n", j, reduce_result.at(j));
  }
}*/

void map_test() {
  parutils::array::array<int> x(n);
  x.fill(1);
  parutils::array::array<int> map_result = parutils::array::utils::map(x, [&] (int x) { return 2; });//, [&] (int x) { return 1; });
  for (int i = 0; i < 10; i++) {
    printf("map_result[%d] = %d\n", i * n / 10, map_result[i * n / 10]);
  }
}

void reduce_test() {
  parutils::array::array<int> x(n);
  x.fill(1);
  int result = parutils::array::utils::reduce(x, 0, [&] (int a, int b) { return a + b; });
  printf("%d\b", result);
}

void scan_exclusive_result() {
  parutils::array::array<int> x(n);
  x.fill(1);
  array<int> scan_exclusive_result = parutils::array::utils::scan_exclusive(x, 0, [&] (int a, int b) { return a + b; });
  for (int i = 0; i < 10; i++) {
    printf("scan_exclusive[%d] = %d\n", i * n / 10, scan_exclusive_result[i * n / 10]);
  }
}

void scan_inclusive_result() {
  parutils::array::array<int> x(n);
  x.fill(1);
  array<int> scan_inclusive_result = parutils::array::utils::scan_inclusive(x, 0, [&] (int a, int b) { return a + b; });
  for (int i = 0; i < 10; i++) {
    printf("scan_inclusive[%d] = %d\n", i * n / 10, scan_inclusive_result[i * n / 10]);
  }
}

namespace pasl {
  namespace pctl {

    void ex() {
     filter_test();
//   ret
    }                                                                                                                  
  }
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pbbs::launch(argc, argv, [&] {
    n = pasl::util::cmdline::parse_or_default_int("n", 1000);
/*    x = new array<std::shared_ptr<array<int>>>(n);
    for (int i = 0; i < n; i++) {
      new (&x->at(i)) std::shared_ptr<array<int>>(new array<int>(i + 1));
      for (int j = 0; j < i + 1; j++) {
        x->at(i)->at(j) = j;
      }
    }*/


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