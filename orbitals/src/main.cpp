// src/main.cpp

#include <boost/program_options.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <png.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using Complex = std::complex<double>;
namespace po = boost::program_options;

struct OrbitalResult {
   std::vector<Complex> points;
   int iterations{0};
   bool escaped{false};
};

struct CoordinateGrid {
   std::vector<double> coordinates;
   double scale;
};

class HistogramGrid {
 public:
   HistogramGrid() = default;

   HistogramGrid(int width, int height) { set_size(width, height); }

   void set_size(int width, int height) {
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

   bool increment(int row, int column) noexcept {
      if (row < 0 || row >= height_ || column < 0 || column >= width_) {
         return false;
      }

      const auto index =
         static_cast<std::size_t>(row) * static_cast<std::size_t>(width_) +
         static_cast<std::size_t>(column);

      ++counts_[index];
      return true;
   }

   bool increment(const std::complex<double>& z) noexcept {
      const double real = z.real();
      const double imag = z.imag();

      if (!std::isfinite(real) || !std::isfinite(imag) || real < -2.0 ||
          real > 2.0 || imag < -2.0 || imag > 2.0) {
         return false;
      }

      const int column =
         real == 2.0 ? width_ - 1 : static_cast<int>((real + 2.0) * x_scale_);

      const int row =
         imag == -2.0 ? height_ - 1 : static_cast<int>((2.0 - imag) * y_scale_);

      return increment(row, column);
   }

   int width() const noexcept { return width_; }

   int height() const noexcept { return height_; }

   const std::vector<std::uint64_t>& data() const noexcept { return counts_; }

 private:
   std::vector<std::uint64_t> counts_;

   int width_{0};
   int height_{0};

   double x_scale_{0.0};
   double y_scale_{0.0};
};

inline void compute_orbits(Complex c, int max_iterations,
                           OrbitalResult& result) {
   result.points.clear();
   result.points.reserve(static_cast<std::size_t>(max_iterations));

   result.iterations = 0;
   result.escaped = false;

   Complex z{0.0, 0.0};

   for (int i = 0; i < max_iterations; ++i) {
      z = z * z + c;
      ++result.iterations;

      if (std::norm(z) > 4.0) {
         result.escaped = true;
         break;
      }

      result.points.push_back(z);
   }
}

std::vector<double> make_coordinate_lut(int width) {
   const double step = 4.0 / width;

   std::vector<double> coordinates;
   coordinates.reserve(static_cast<std::size_t>(width));

   for (int i = 0; i < width; ++i) {
      coordinates.push_back(-2.0 + (i + 0.5) * step);
   }

   return coordinates;
}

std::optional<int> pixel_index(double x, int width) {
   if (!std::isfinite(x) || x < -2.0 || x > 2.0) {
      return std::nullopt;
   }

   if (x == 2.0) {
      return width - 1;
   }

   return static_cast<int>((x + 2.0) * width / 4.0);
}

struct Configuration {
   static constexpr int default_max_iterations{1000};
   static constexpr int default_width{640};
   static constexpr Complex default_top_left{-2.0, 2.0};
   static constexpr Complex default_bottom_right{2.0, -2.0};

   Complex top_left{default_top_left};
   Complex bottom_right{default_bottom_right};
   int max_iterations{default_max_iterations};
   int width{default_width};
   int height{0};
};

Complex parse_complex(const std::string& value) {
   std::istringstream stream{value};
   stream.imbue(std::locale::classic());

   double real{};
   double imag{};
   char separator{};

   if (!(stream >> real >> separator >> imag) || separator != ',') {
      throw std::invalid_argument("Expected complex coordinate as real,imag");
   }

   stream >> std::ws;

   if (!stream.eof() || !std::isfinite(real) || !std::isfinite(imag)) {
      throw std::invalid_argument("Invalid complex coordinate: " + value);
   }

   return {real, imag};
}

int compute_height(const Configuration& config) {
   const double rectangle_width =
      config.bottom_right.real() - config.top_left.real();

   const double rectangle_height =
      config.top_left.imag() - config.bottom_right.imag();

   const double height = config.width * (rectangle_height / rectangle_width);

   if (!std::isfinite(height) || height < 1.0 ||
       height > std::numeric_limits<int>::max() - 1.0) {
      throw std::invalid_argument("Computed image height is out of range");
   }

   // Round to the nearest even integer.
   const auto rounded = std::lround(height / 2.0) * 2;

   return static_cast<int>(std::max(2L, rounded));
}

std::optional<Configuration> parse_arguments(int argc, char* argv[]) {
   po::options_description options{"Options"};

   auto add = options.add_options();
   add("help,h", "Display this help message");
   add("version,v", "Display version information");
   add("max-iterations,n",
       po::value<int>()->default_value(Configuration::default_max_iterations),
       "Maximum iterations per orbit");
   add("width,w", po::value<int>()->default_value(Configuration::default_width),
       "Image width in pixels (sample points)");
   add("top-left", po::value<std::string>(),
       "Top-left complex coordinate (real,imag)");
   add("bottom-right", po::value<std::string>(),
       "Bottom-right complex coordinate (real,imag)");

   po::variables_map vm;
   po::store(po::parse_command_line(argc, argv, options), vm);

   if (vm.count("help") != 0) {
      std::cout << "Usage: orbitals [options]\n\n";
      std::cout << options << '\n';
      return std::nullopt;
   }

   if (vm.count("version") != 0) {
      std::cout << "orbitals " << ORBITALS_VERSION << '\n';
      return std::nullopt;
   }

   po::notify(vm);

   Configuration config;
   config.max_iterations = vm["max-iterations"].as<int>();
   config.width = vm["width"].as<int>();

   // Validate options and fail hard on invalid option
   if (config.max_iterations <= 0) {
      throw std::invalid_argument("max-iterations must be positive");
   }
   if (config.width <= 0 || config.width % 2 != 0) {
      throw std::invalid_argument(
         "width must be an even number greater than 0");
   }
   if (vm.count("top-left") != 0) {
      config.top_left = parse_complex(vm["top-left"].as<std::string>());
   }
   if (vm.count("bottom-right") != 0) {
      config.bottom_right = parse_complex(vm["bottom-right"].as<std::string>());
   }
   if (config.top_left.real() >= config.bottom_right.real()) {
      throw std::invalid_argument("top-left must be left of bottom-right");
   }
   if (config.top_left.imag() <= config.bottom_right.imag()) {
      throw std::invalid_argument("top-left must be above bottom-right");
   }

   config.height = compute_height(config);

   return config;
}

std::vector<std::uint8_t> make_grayscale(const HistogramGrid& histogram,
                                         std::uint64_t max_count) {
   const auto& counts = histogram.data();
   std::vector<std::uint8_t> pixels(counts.size(), 0);

   if (max_count == 0) {
      return pixels;
   }

   const double scale = 255.0 / std::log1p(static_cast<double>(max_count));

   for (std::size_t i = 0; i < counts.size(); ++i) {
      if (counts[i] > 0) {
         const double intensity =
            std::log1p(static_cast<double>(counts[i])) * scale;
         pixels[i] = static_cast<std::uint8_t>(
            std::clamp(std::lround(intensity), 0L, 255L));
      }
   }

   return pixels;
}

void write_grayscale_png(const std::string& filename, int width, int height,
                         const std::vector<std::uint8_t>& pixels) {
   if (width <= 0 || height <= 0) {
      throw std::invalid_argument("PNG dimensions must be positive");
   }

   const auto expected_size =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

   if (pixels.size() != expected_size) {
      throw std::invalid_argument(
         "Pixel buffer size does not match image dimensions");
   }

   FILE* fp = std::fopen(filename.c_str(), "wb");

   if (fp == nullptr) {
      throw std::runtime_error("Failed to open output PNG file");
   }

   png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

   if (png_ptr == nullptr) {
      std::fclose(fp);
      throw std::runtime_error("Failed to create PNG write structure");
   }

   png_infop info_ptr = png_create_info_struct(png_ptr);

   if (info_ptr == nullptr) {
      png_destroy_write_struct(&png_ptr, nullptr);
      std::fclose(fp);
      throw std::runtime_error("Failed to create PNG info structure");
   }

   if (setjmp(png_jmpbuf(png_ptr))) {
      png_destroy_write_struct(&png_ptr, &info_ptr);
      std::fclose(fp);
      throw std::runtime_error("libpng failed while writing PNG");
   }

   png_init_io(png_ptr, fp);

   png_set_IHDR(png_ptr, info_ptr, static_cast<png_uint_32>(width),
                static_cast<png_uint_32>(height), 8, PNG_COLOR_TYPE_GRAY,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);

   png_write_info(png_ptr, info_ptr);

   for (int row = 0; row < height; ++row) {
      const auto offset =
         static_cast<std::size_t>(row) * static_cast<std::size_t>(width);

      auto* row_data = const_cast<png_bytep>(pixels.data() + offset);
      png_write_row(png_ptr, row_data);
   }

   png_write_end(png_ptr, info_ptr);

   png_destroy_write_struct(&png_ptr, &info_ptr);
   std::fclose(fp);
}

int main(int argc, char* argv[]) try {
   const auto config = parse_arguments(argc, argv);

   if (!config) {
      return EXIT_SUCCESS;
   }

   std::cout << "Beginning computation...\n";
   std::cout << "Maximum iterations: " << config->max_iterations << '\n';
   std::cout << "Image width: " << config->width << '\n';
   std::cout << "Image height: " << config->height << '\n';
   std::cout << "Top left point: " << config->top_left << '\n';
   std::cout << "Bottom right point: " << config->bottom_right << '\n';

   OrbitalResult result;
   const auto coordinates = make_coordinate_lut(config->width);
   HistogramGrid histogram{config->width, config->width};

   for (double imag : coordinates) {
      for (double real : coordinates) {
         const Complex c{real, imag};
         compute_orbits(c, config->max_iterations, result);

         // Process the resulting orbit.
         if (!result.escaped) {
            for (const auto& z : result.points) {
               histogram.increment(z);
            }
         }
      }
   }

   const auto& counts = histogram.data();
   const auto max_count = std::max_element(counts.begin(), counts.end());

   if (max_count != counts.end()) {
      std::cout << "Maximum histogram count: " << *max_count << '\n';
   }

   auto pixel_data = make_grayscale(histogram, *max_count);

   // Make the PNG file
   write_grayscale_png("out.png", histogram.width(), histogram.height(),
                       pixel_data);

   std::cout << "Wrote out.png\n";
   std::cout << '\n';

   return EXIT_SUCCESS;

} catch (const std::exception& ex) {
   std::cerr << "Error: " << ex.what() << '\n';
   return EXIT_FAILURE;
}

