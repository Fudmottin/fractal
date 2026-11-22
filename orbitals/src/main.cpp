// src/main.cpp

#include <boost/program_options.hpp>

#include <complex>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
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

   int max_iterations{default_max_iterations};
};

std::optional<Configuration> parse_arguments(int argc, char* argv[]) {
   po::options_description options{"Options"};

   options.add_options()("help,h", "Display this help message")(
      "version,v",
      "Display version information")("max-iterations,n",
                                     po::value<int>()->default_value(
                                        Configuration::default_max_iterations),
                                     "Maximum iterations per orbit");

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

   if (config.max_iterations <= 0) {
      throw std::invalid_argument("max-iterations must be positive");
   }

   return config;
}

int main(int argc, char* argv[]) try {
   const auto config = parse_arguments(argc, argv);

   if (!config) {
      return EXIT_SUCCESS;
   }

   std::cout << "Beginning computation...\n";
   std::cout << "Maximum iterations: " << config->max_iterations << '\n';

   return EXIT_SUCCESS;

} catch (const std::exception& ex) {
   std::cerr << "Error: " << ex.what() << '\n';
   return EXIT_FAILURE;
}

