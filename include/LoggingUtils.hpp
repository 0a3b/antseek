#pragma once

#include <iostream>
#include <mutex>
#include <string>

namespace LoggingUtils {

    inline std::mutex mtx;

    inline void writeToStderr(const std::string& message) {
        std::lock_guard<std::mutex> lock(mtx);
        std::cerr << message << std::endl;
    }

    inline void showProgress(int percent) {
        std::lock_guard<std::mutex> lock(mtx);
        std::cerr << "\rProgress: " << percent << "%   ";
        std::cerr.flush();
    }

}

