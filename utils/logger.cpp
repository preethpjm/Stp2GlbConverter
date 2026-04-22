#include "logger.h"
#include <iostream>

void Logger::Info(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

void Logger::Error(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

void Logger::Warning(const std::string& msg) {
    std::cout << "[WARNING] " << msg << std::endl;
}