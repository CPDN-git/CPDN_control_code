#pragma once

#include <vector>
#include <filesystem>

/**
 * @brief Zips a list of files into a single zip archive using ZipLib.
 *
 * @param zip_filepath The path to the output zip archive to be created.
 * @param files_to_zip A vector of paths to the files that should be included in the zip.
 * @return bool Returns true on success, false on failure.
 */
bool cpdn_zip(
    const std::filesystem::path& zip_filepath, 
    const std::vector<std::filesystem::path>& files_to_zip
);

/**
 * @brief Unzips a zip archive to a specified directory using ZipLib.
 *
 * @param zip_filepath The path to the zip archive to be extracted.
 * @param output_directory The directory where the contents should be extracted.
 * @return bool Returns true on success, false on failure.
 */
bool cpdn_unzip(
    const std::filesystem::path& zip_filepath, 
    const std::filesystem::path& output_directory
);

