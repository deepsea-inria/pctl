/*!
  \file
  \brief File contains two simple implementations of array.

  This file contains two simple implementations of array, which
  supports fill and subarray functions.
*/

#include <algorithm>
#include <stdexcept>
#include <atomic>
#include <stdlib.h>
#include "ploop.hpp"
#include "defines.hpp"

#ifndef _PARUTILS_ARRAY_H_
#define _PARUTILS_ARRAY_H_

namespace parutils {
namespace array {

/*!
  \brief Simple array class, which is the wrapper of standart c++ array with additional parallel
  fill function and O(1) subarray function.

  \warning Not totally thread-safe. If there is copy-constructor and destruction happens together, then undefined behaviour.
*/

template <class T>
class array {
protected:
  std::atomic<int>* links; // smart pointer
  T* memory;
  int_t length;
  int_t left;
  int_t right;

  void copy(const array<T>& src) {
    links = src.links;
    (*links)++;
    memory = src.memory;
    length = src.length;
    left = src.left;
    right = src.right;
  }

  void clear() {
    if (length + 1 > 0) {
      if (!(--(*links))) {
        if (!std::is_integral<T>()) {
          for (int i = 0; i < length; i++) {
            memory[i].~T();
          }
        }
        free(memory);
        delete links;
      }
    }
  }

public:
  /*!
    Empty constructor.
  */
  array(): length(-1) { }

  /*!
    Constructor, which creates array of given length.
    \param _length the length of array
  */
  array(int_t _length): length(_length), left(0), right(_length) {
    memory = (T*) std::malloc(sizeof(T) * _length);
    links = new std::atomic<int>(1);
  }

  /*!
    Constructor, which wraps c++ array.
    \param _memory c++ array
    \param _length the length of passed array
  */
  array(T* _memory, int_t _length): memory(_memory), length(_length), left(0), right(_length) {
    links = new std::atomic<int>(1);
  }

  /*!
    Subarray constructor.
    \param src array
    \param l start position of subarray
    \param r end position of subarray (exclusive)
    \throw std::out_of_range If the range is incorrect.
  */
  array(array<T>& src, int_t l, int_t r) {
    if (l >= r || r > src.right - src.left) {
      throw std::out_of_range("Out of bounds");
    }
    links = src.links;
    (*links)++;
    memory = src.memory;
    length = src.length;
    left = src.left + l;
    right = src.left + r;
  }

  /*!
    Copy constructor, which doesn't copy the underneath array.
    \param src array
  */
  array(const array<T>& src) {
    copy(src);
  }

  array& operator=(const array<T>& src) {
    if (this != &src) {
      clear();
      copy(src);
    }
    return *this;
  }

  /*!
    \return length of the array.
  */
  int size() const {
    return right - left;
  }

  /*!
    Fills array with given value in parallel.
    \param value the value to fill with
  */
  void fill(T value) {
    if (length <= BLOCK_SIZE) {
      std::fill_n(memory, length, value);
    }
    pasl::pctl::parallel_for((int_t)0, (length + BLOCK_SIZE - 1) / BLOCK_SIZE, [&] (int_t i) {
      std::fill(memory + i * BLOCK_SIZE, memory + std::min((int_t) (i + 1) * BLOCK_SIZE, length), value);
    });
  }

  /*!
    Returns element at given position.
    \param index the position of element in array
  */
  T& operator[](int_t index) {
    return memory[index + left];
  }

  /*!
    Checks bounds of array and returns element at given position.
    \param index the position of element in array
    \throw std::out_of_range If index is out of bounds.
  */                           
  T& at(int_t index) {
    if (index < 0 || index >= right - left) {
      throw std::out_of_range("Out of bounds");
    }
    return memory[index + left];
  }

  T* begin() {
    return memory + left;
  }

  T* end() {
    return memory + right;
  }

  /*!
    Destructor of array, which deletes underlying array only if there is no more links to it.
  */
  ~array() {
    clear();
  }
};

/*!
  \brief Array class, which is extension of simple array class, with constant time fill function.
  To support constant time fill, array_fast_fill use additional c++ array of long longs of the same size
  as data array.
*/
template <class T>
class array_fast_fill : public array<T> {
private:
  long long* time;
  T value;
  long long current_time;
public: 
  /*!
    Constructor, which creates array of given length and fills with initial value.
    \param _length the length of the array
    \param _value the value to initialize with
  */
  array_fast_fill(int_t _length, T _value): array<T>(_length), value(_value) {
    time = new long long[_length];
    if (_length <= BLOCK_SIZE) {
      std::fill_n(time, _length, 0LL);
    }
    pasl::pctl::parallel_for((int_t)0, (_length + BLOCK_SIZE - 1) / BLOCK_SIZE, [&] (int i) {
      std::fill(time + i * BLOCK_SIZE, time + std::min((int_t) (i + 1) * BLOCK_SIZE, _length), 0LL);
    });
    current_time = 1;
  }

  /*!
    Subarray constructor.
    \param src array
    \param l start position of subarray
    \param r end position of subarray (exclusive)
  */
  array_fast_fill(array_fast_fill<T>& src, int_t l, int_t r): array<T>(src, l, r) {
    time = src.time;
    value = src.value;
    current_time = src.current_time;
  }

  /*!
    Copy constructor, which doesn't copy the underneath array.
    \param src array.
  */
  array_fast_fill(const array_fast_fill<T>& src): array<T>(src) {
    time = src.time;
    value = src.value;
    current_time = src.current_time;
  }

  /*!
    Fills array with given value in constant time.
    \param value the value to fill with
  */
  void fill(T value) {
    this->value = value;
    current_time++;
  }

  /*!
    Returns element at given position.
    \param index the position of element in array
  */
  T& operator[](int_t index) {
    index += array<T>::left;
    if (time[index] < current_time) {
      array<T>::memory[index] = value;
    }
    return array<T>::memory[index];
  }

  /*!
    Checks bounds and returns element at given position.
    \param index the position of element in array
    \throw std::out_of_range If index is out of bounds
  */
  T& at(int_t index) {
    if (index < 0 || index >= array<T>::right - array<T>::left) {
      throw std::out_of_range("Out of bounds");
    }
    return this->operator[](index);
  }

  /*!
    Destructor of array, which deletes underlying array only if there is no more links to it.
  */
  ~array_fast_fill() {
    if (array<T>::length + 1 > 0 && !((*array<T>::links)--)) {
      delete [] time;
      free(array<T>::memory);
      delete array<T>::links;
    }
  }
};

} //end namespace
} //end namespace
#endif // _PARUTILS_ARRAY_H_