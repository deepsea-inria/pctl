/*!
  \file
  \brief File contains two simple implementations of array.

  This file contains two simple implementations of array, which
  supports fill and subarray functions.
*/

#include <algorithm>
#include <stdexcept>
#include "ploop.hpp"

#ifndef _PARUTILS_ARRAY_H_
#define _PARUTILS_ARRAY_H_

namespace parutils {
namespace array {

#define BLOCK_SIZE 1024

/*!
  \brief Simple array class, which is the wrapper of standart c++ array with additional parallel
  fill function and O(1) subarray function.
*/

template <class T>
class array {
protected:
  T* memory;
  size_t length;
  bool array_holder = true;

public:
  /*!
    Constructor, which creates array of given length.
    \param _length the length of array
  */
  array(size_t _length): length(_length) {
    memory = new T[_length];
  }

  /*!
    Constructor, which wraps c++ array.
    \param _memory c++ array
    \param _length the length of passed array
  */
  array(T* _memory, size_t _length): memory(_memory), length(_length) {
  }

  /*!
    Subarray constructor.
    \param src array
    \param l start position of subarray
    \param r end position of subarray (exclusive)
    \throw std::out_of_range If the range is incorrect.
  */
  array(array<T>& src, size_t l, size_t r) {
    if (l >= r || r > length) {
      throw std::out_of_range("Out of bounds");
    }
    memory = src.memory + l;
    length = r - l;
    array_holder = false;
  }

  /*!
    Copy constructor, which doesn't copy the underneath array.
    \param src array
  */
  array(const array<T>& src) {
    memory = src.memory;
    length = src.length;
  }

  /*!
    \return length of the array.
  */
  int size() {
    return length;
  }

  /*!
    Fills array with given value in parallel.
    \param value the value to fill with
  */
  void fill(T value) {
    if (length <= BLOCK_SIZE) {
      std::fill_n(memory, length, value);
    }
    pasl::pctl::parallel_for((size_t)0, (length + BLOCK_SIZE - 1) / BLOCK_SIZE, [&] (size_t i) {
      std::fill(memory + i * BLOCK_SIZE, memory + std::min((size_t) (i + 1) * BLOCK_SIZE, length), value);
    });
  }

  /*!
    Returns element at given position.
    \param index the position of element in array
  */
  T& operator[](size_t index) {
    return memory[index];
  }

  /*!
    Checks bounds of array and returns element at given position.
    \param index the position of element in array
    \throw std::out_of_range If index is out of bounds.
  */                           
  T& at(size_t index) {
    if (index < 0 || index >= length) {
      throw std::out_of_range("Out of bounds");
    }
    return memory[index];
  }

  /*!
    Destructor of array, which deletes underlying array only if was not created as subarray.
  */
  ~array() {
    if (array_holder) {
      delete [] memory;
    }
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
  array_fast_fill(size_t _length, T _value): array<T>(_length), value(_value) {
    time = new long long[_length];
    if (_length <= BLOCK_SIZE) {
      std::fill_n(time, _length, 0LL);
    }
    pasl::pctl::parallel_for((size_t)0, (_length + BLOCK_SIZE - 1) / BLOCK_SIZE, [&] (int i) {
      std::fill(time + i * BLOCK_SIZE, time + std::min((size_t) (i + 1) * BLOCK_SIZE, _length), 0LL);
    });
    current_time = 1;
  }

  /*!
    Subarray constructor.
    \param src array
    \param l start position of subarray
    \param r end position of subarray (exclusive)
  */
  array_fast_fill(array_fast_fill<T>& src, size_t l, size_t r): array<T>(src, l, r) {
    time = src.time + l;
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
  T& operator[](size_t index) {
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
  T& at(size_t index) {
    if (index < 0 || index >= array<T>::length) {
      throw std::out_of_range("Out of bounds");
    }
    return this->operator[](index);
  }

  /*!
    Destructor of array, which deletes underlying array only if it was not created as subarray.
  */
  ~array_fast_fill() {
    if (array<T>::array_holder) {
      delete [] time;
    }
  }
};

} //end namespace
} //end namespace
#endif // _PARUTILS_ARRAY_H_