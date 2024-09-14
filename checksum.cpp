#include "checksum.h"
#include <fstream>
#include <iostream>

unsigned int compute_checksum(const char* filename, int buffer_size) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "File not found: " << filename << std::endl;
        return 0;
    }

    unsigned int checksum = 0;
    char* buffer = new char[buffer_size];
    while (file.read(buffer, buffer_size)) {
        for (int i = 0; i < file.gcount(); ++i) {
            checksum += buffer[i];
        }
    }
    file.close();
    delete[] buffer;
    return checksum;
}
