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

#include "histogram-grid.hpp"

using Complex = std::complex<double>;
namespace po = boost::program_options;

struct OrbitalResult {
   std::vector<Complex> points;
   int iterations{0};
   bool escaped{false};
};

struct CoordinateGrid {
   std::vector<double> real;
   std::vector<double> imag;

   CoordinateGrid(Complex top_left, Complex bottom_right, int columns, int rows)
      : real(make_axis(top_left.real(), bottom_right.real(), columns))
      , imag(make_axis(top_left.imag(), bottom_right.imag(), rows)) {}

 private:
   static std::vector<double> make_axis(double start, double end, int samples) {
      const double step = (end - start) / samples;

      std::vector<double> coordinates;
      coordinates.reserve(static_cast<std::size_t>(samples));

      for (int i = 0; i < samples; ++i) {
         coordinates.push_back(start + (i + 0.5) * step);
      }

      return coordinates;
   }
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

struct Configuration {
   static constexpr int default_max_iterations{1000};
   static constexpr int default_width{640};
   static constexpr int default_samples{640};
   static constexpr Complex default_top_left{-2.0, 2.0};
   static constexpr Complex default_bottom_right{2.0, -2.0};
   static constexpr const char* default_output{"out"};

   Complex top_left{default_top_left};
   Complex bottom_right{default_bottom_right};

   std::string output{default_output};
   int max_iterations{default_max_iterations};
   int width{default_width};
   int samples{default_samples};
   int sample_rows{0};
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

int compute_sample_rows(const Configuration& config) {
   const double rectangle_width =
      config.bottom_right.real() - config.top_left.real();

   const double rectangle_height =
      config.top_left.imag() - config.bottom_right.imag();

   const double rows = config.samples * (rectangle_height / rectangle_width);

   if (!std::isfinite(rows) ||
       rows > static_cast<double>(std::numeric_limits<int>::max() - 1)) {
      throw std::invalid_argument("Computed sampling height is out of range");
   }

   return static_cast<int>(std::max(1L, std::lround(rows)));
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
       "Square output image width in pixels");
   add("samples,s",
       po::value<int>()->default_value(Configuration::default_samples),
       "Number of sampling columns in the complex plane");
   add("top-left", po::value<std::string>(),
       "Top-left complex coordinate (real,imag)");
   add("bottom-right", po::value<std::string>(),
       "Bottom-right complex coordinate (real,imag)");
   add("output,o",
       po::value<std::string>()->default_value(Configuration::default_output),
       "Output filename without extension");

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
   config.samples = vm["samples"].as<int>();
   config.output = vm["output"].as<std::string>();

   // Validate options and fail hard on invalid option
   if (config.max_iterations <= 0) {
      throw std::invalid_argument("max-iterations must be positive");
   }
   if (config.width <= 0 || config.width % 2 != 0) {
      throw std::invalid_argument(
         "width must be an even number greater than 0");
   }
   if (config.samples < 16) {
      throw std::invalid_argument("Samples must be at least 16");
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
   if (config.output.empty()) {
      throw std::invalid_argument("output filename must not be empty");
   }

   config.sample_rows = compute_sample_rows(config);

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
   std::cout << "Top left point: " << config->top_left << '\n';
   std::cout << "Bottom right point: " << config->bottom_right << '\n';
   std::cout << "Image dimensions: " << config->width << " x " << config->width
             << '\n';
   std::cout << "Sampling dimensions: " << config->samples << " x "
             << config->sample_rows << '\n';

   OrbitalResult result;

   const CoordinateGrid sampling{config->top_left, config->bottom_right,
                                 config->samples, config->sample_rows};

   HistogramGrid histogram{config->width, config->width};

   for (double imag : sampling.imag) {
      for (double real : sampling.real) {
         const Complex c{real, imag};

         compute_orbits(c, config->max_iterations, result);

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
   const std::string filename = config->output + ".png";

   write_grayscale_png(filename, histogram.width(), histogram.height(),
                       pixel_data);

   std::cout << "Wrote " << filename << '\n';

   return EXIT_SUCCESS;

} catch (const std::exception& ex) {
   std::cerr << "Error: " << ex.what() << '\n';
   return EXIT_FAILURE;
}

