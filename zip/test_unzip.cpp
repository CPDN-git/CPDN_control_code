#include "cpdn_zip.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cassert>

int main(int argc, char** argv) {
    // --- Setup Test Environment ---
    const std::filesystem::path test_dir = "zip_tdir";
    const std::filesystem::path slot_dir = test_dir / "slots/0";
    const std::filesystem::path extraction_dir = slot_dir;

    // Clean up previous test runs
    std::filesystem::remove_all(test_dir);

    // Create directories
    std::filesystem::create_directory(test_dir);
    std::filesystem::create_directories(slot_dir);

    // Read file to be unzipped from command argument
    std::string infile = argv[1];
    std::cerr << "Input file to be unzipped: " << infile << '\n';

    // --- Test cpdn_unzip ---
    const std::filesystem::path zip_archive_path = infile;

    std::cout << "\n--- Testing cpdn_unzip ---" << std::endl;
    bool unzip_result = cpdn_unzip(zip_archive_path, extraction_dir);

    std::cout << "cpdn_unzip returned: " << (unzip_result ? "success" : "failure") << std::endl;
    assert(unzip_result && "cpdn_unzip should return true on success.");

    // --- Clean up ---
    //std::cout << "\nCleaning up test directory..." << std::endl;
    //std::filesystem::remove_all(test_dir);

    std::cout << "\nAll tests passed successfully!" << std::endl;

    return 0;
}
