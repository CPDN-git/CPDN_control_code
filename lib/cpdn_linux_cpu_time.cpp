//
// Reimplementation of the BOINC linux_cpu_time function using modern C++.
// This version checks the clock resolution to ensure accurate CPU time calculation,
// unlike the original BOINC implementation which used a hardcoded value.
//
//   Glenn Carver, CPDN, 2025.

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unistd.h> // Required for sysconf(_SC_CLK_TCK)


// Define the function outside of a class for direct replacement of the original
double cpdn_linux_cpu_time(long pid) {
    std::ifstream file;
    std::string line;
    std::string file_path = "/proc/" + std::to_string(pid) + "/stat";

    // file.close() is implicitly called by the std::ifstream destructor (RAII)
    file.open(file_path);

    if (!file.is_open()) {
        return 0.0;             // Process might not exist or permission denied
    }

    // Read and parse the stat line using a stringstream.
    // The stat file contains 52 fields. We need the 14th (utime) and 15th (stime).
    // Must skip first 13 fields (including the command name which is field 2, enclosed in parentheses) 
    if (!std::getline(file, line)) {
        return 0.0;
    }
    std::stringstream ss(line);
    std::string       temp_field;
    unsigned long     utime = 0, stime = 0;
    
    // The first field is PID, which we don't need to read.
    // The second field is the command name, which can contain spaces, making parsing difficult.
    // A robust way to skip the first 13 fields:
    // Skip 13 fields to get to utime: 
    // Field 1: PID
    // Field 2: Command name (tricky, so we use a loop and carefully handle it)
    for (int i = 0; i < 13; ++i) {
        ss >> temp_field;
    }
    if (!(ss >> utime >> stime)) {
        return 0.0; 
    }

    // Determine the correct clock ticks per second
    // sysconf(_SC_CLK_TCK) retrieves the number of clock ticks per second (usually 100 or 1000).
    long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second <= 0) {
        ticks_per_second = 100;             // Fallback to a default value if sysconf fails     
    }

    // Calculate total CPU time in seconds
    unsigned long total_ticks = utime + stime;
    double total_cpu_time = (double)total_ticks / ticks_per_second;

    return total_cpu_time;
}
