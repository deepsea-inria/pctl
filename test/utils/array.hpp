#include <algorithm>
#include <stdexcept>
#include "ploop.hpp"

#ifndef _PARUTILS_ARRAY_H_
#define _PARUTILS_ARRAY_H_

namespace parutils {
namespace array {

#define BLOCK_SIZE 1024

template <class T>
class array {
protected:
  T* memory;
  size_t length;
  bool subarray;

public:
  array(size_t _length): length(_length) {
    memory = new T[_length];
  }

  array(T* _memory, size_t _length): memory(_memory), length(_length) {}

  array(array<T>& src, size_t l, size_t r) {
    if (l >= r || r > length) {
      throw std::out_of_range("Out of bounds");
    }
    memory = src.memory + l;
    length = r - l;
    subarray = true;
  }

  array(const array<T>& src) {
    memory = src.memory;
    length = src.length;
  }

  int size() {
    return length;
  }

  void fill(T value) {
    if (length <= BLOCK_SIZE) {
      std::fill_n(memory, length, value);
    }
    pasl::pctl::parallel_for((size_t)0, (length + BLOCK_SIZE - 1) / BLOCK_SIZE, [&] (size_t i) {
      std::fill(memory + i * BLOCK_SIZE, memory + std::min((size_t) (i + 1) * BLOCK_SIZE, length), value);
    });
  }

  T& operator[](size_t index) {
    return memory[index];
  }

  T& at(size_t index) {
    if (index < 0 || index >= length) {
      throw std::out_of_range("Out of bounds");
    }
    return memory[index];
  }

  ~array() {
    if (!subarray) {
      delete [] memory;
    }
  }
};

template <class T>
class array_fast_fill : public array<T> {
private:
  long long* time;
  T zero;
  long long current_time;
public: 
  array_fast_fill(size_t _length, T& _zero): array<T>(_length), zero(_zero) {
    time = new long long[_length];
    if (_length <= BLOCK_SIZE) {
      std::fill_n(time, _length, 0LL);
    }
    pasl::pctl::parallel_for((size_t)0, (_length + BLOCK_SIZE - 1) / BLOCK_SIZE, [&] (int i) {
      std::fill(time + i * BLOCK_SIZE, time + std::min((size_t) (i + 1) * BLOCK_SIZE, _length), 0LL);
    });
    current_time = 1;
  }

  array_fast_fill(const array_fast_fill& src): array<T>(src) {
    time = src.time;
    zero = src.zero;
    current_time = src.current_time;
  }

  void clear() {
    current_time++;
  }

  T& operator[](size_t index) {
    if (time[index] < current_time) {
      array<T>::memory[index] = zero;
    }
    return array<T>::memory[index];
  }

  T& at(size_t index) {
    if (time[index] < current_time) {
      array<T>::memory[index] = zero;
    }
    return array<T>::at(index);
  }

  ~array_fast_fill() {
    delete [] time;
  }
};

} //end namespace
} //end namespace
#endif