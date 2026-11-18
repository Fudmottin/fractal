// src/main.cpp

#include <iostream>
#include <stdexcept>

int main() try {
   std::cout << "Beginning computation...\n";
   return 0;
} catch (const std::exception& ex) {
   std::cerr << "Error: " << ex.what() << "\n";
   return -1;
}
