//
// Created by sayan on 4/13/25.
//

#ifndef MATRIX_H
#define MATRIX_H

#include <array>
#include <experimental/mdspan>
#include <ranges>

template <typename T, size_t N, size_t M> class matrix;

template <typename T, size_t N, size_t M> class matrix {
private:
  using row_extent = std::extents<size_t, N>;
  using column_extent = std::extents<size_t, M>;
  using Row = std::mdspan<T, row_extent>;
  using Column = std::mdspan<T, column_extent, std::layout_left>;
  std::array<T, N * M> data;
  const std::mdspan<T, std::extents<size_t, N, M>> mat;

public:
  constexpr T &operator[](size_t i, size_t j) { return mat[i, j]; }
  constexpr T &operator[](size_t i, size_t j) const { return mat[i, j]; }

  constexpr matrix(const std::array<T, N * M> &d) : data{d}, mat{data.data()} {}
  matrix(const matrix &) = default;

  friend matrix operator+(matrix a, const matrix &b) {
    for (size_t i = 0; i < a.mat.extent(0); i++)
      for (size_t j = 0; j < a.mat.extent(1); j++)
        a[i, j] += b[i, j];
    return a;
  }

  friend matrix operator-(matrix a, const matrix &b) {
    for (size_t i = 0; i < a.mat.extent(0); i++)
      for (size_t j = 0; j < a.mat.extent(1); j++)
        a[i, j] -= b[i, j];
    return a;
  }

  friend matrix operator*(matrix a, const matrix &b) {
    for (size_t i = 0; i < a.mat.extent(0); ++i)
      for (size_t j = 0; j < b.mat.extent(1); ++j)
        for (size_t k = 0; k < b.mat.extent(0); ++k)
          a[i, j] += a[i, k] * b[k, j];
    return a;
  }

  bool operator==(const matrix &b) const {
    if (mat.extent(0) != b.mat.extent(0) || mat.extent(1) != b.mat.extent(1))
      return false;
    for (size_t i = 0; i < mat.extent(0); ++i)
      for (size_t j = 0; j < mat.extent(1); ++j)
        if (mat[i, j] != b.mat[i, j])
          return false;
    return true;
  }
  friend std::ostream &operator<<(std::ostream &out, const matrix &e) {
    for (auto i = 0; i < e.mat.extent(0); ++i) {
      for (auto j = 0; j < e.mat.extent(1); ++j) {
        out << e.mat[i, j] << ", ";
      }
      out << "\n";
    }
    return out;
  }
};

#endif // MATRIX_H
