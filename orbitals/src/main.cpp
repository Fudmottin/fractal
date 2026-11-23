// src/main.cpp

#include <boost/program_options.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
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
   if (config.top_left.real() > config.bottom_right.real()) {
      throw std::invalid_argument("top-left must be left of bottom-right");
   }
   if (config.top_left.imag() < config.bottom_right.imag()) {
      throw std::invalid_argument("top-left must be above bottom-right");
   }

   config.height = compute_height(config);

   return config;
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

   // Temporary tests of orbital computation.
   const std::vector<Complex> c_vals{
      {0.0, 0.0}, {-1.0, 0.0}, {3.0, 1.5}, {2.0, 0.0}};

   OrbitalResult result;

   for (const auto& c : c_vals) {
      compute_orbits(c, config->max_iterations, result);

      std::cout << "C = " << c << '\n';
      std::cout << "Escaped: " << std::boolalpha << result.escaped << '\n';
      std::cout << "Iterations: " << result.iterations << '\n';

      for (const auto& z : result.points) {
         std::cout << z << '\n';
      }

      std::cout << '\n';
   }

   return EXIT_SUCCESS;

} catch (const std::exception& ex) {
   std::cerr << "Error: " << ex.what() << '\n';
   return EXIT_FAILURE;
}

