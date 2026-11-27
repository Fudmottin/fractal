// src/histogram-grid.hpp

#ifndef ORBITALS_HISTOGRAM_GRID_HPP
#define ORBITALS_HISTOGRAM_GRID_HPP

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

class HistogramGrid {
 public:
   HistogramGrid() = delete;

   HistogramGrid(int width, int height) { set_size(width, height); }

   inline void set_size(int width, int height) {
      if (width <= 0 || height <= 0) {
         throw std::invalid_argument("Histogram dimensions must be positive");
      }

      std::vector<std::uint64_t> new_counts(
         static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);

      counts_ = std::move(new_counts);

      width_ = width;
      height_ = height;

      x_scale_ = static_cast<double>(width) / 4.0;
      y_scale_ = static_cast<double>(height) / 4.0;
   }

   inline bool increment(int row, int column) noexcept {
      if (row < 0 || row >= height_ || column < 0 || column >= width_) {
         return false;
      }

      const auto index =
         static_cast<std::size_t>(row) * static_cast<std::size_t>(width_) +
         static_cast<std::size_t>(column);

      ++counts_[index];
      return true;
   }

   inline bool increment(const std::complex<double>& z) noexcept {
      const double real = z.real();
      const double imag = z.imag();

      if (!std::isfinite(real) || !std::isfinite(imag) || real < -2.0 ||
          real > 2.0 || imag < -2.0 || imag > 2.0) {
         return false;
      }

      const int column =
         std::clamp(static_cast<int>((real + 2.0) * x_scale_), 0, width_ - 1);

      const int row =
         std::clamp(static_cast<int>((2.0 - imag) * y_scale_), 0, height_ - 1);

      return increment(row, column);
   }

   inline int width() const noexcept { return width_; }

   inline int height() const noexcept { return height_; }

   inline const std::vector<std::uint64_t>& data() const noexcept {
      return counts_;
   }

 private:
   std::vector<std::uint64_t> counts_;

   int width_{0};
   int height_{0};

   double x_scale_{0.0};
   double y_scale_{0.0};
};

#endif // ORBITALS_HISTOGRAM_GRID_HPP

