#include "cpdn_zip.h"
#include "ZipLib/ZipFile.h"
#include <iostream>

bool cpdn_zip(
    const std::filesystem::path& zip_filepath, 
    const std::vector<std::filesystem::path>& files_to_zip)
{
    try
    {
        // If the zip file exists, ZipFile::AddFile will append to it.
        // To create a fresh archive, we should remove it first.
        if (std::filesystem::exists(zip_filepath))
{
            std::filesystem::remove(zip_filepath);
        }

        for (const auto& file_path : files_to_zip)
        {
            if (std::filesystem::exists(file_path))
            {
                // Use the static AddFile method.
                // It takes the archive path, the file to add, and the name inside the archive.
                ZipFile::AddFile(zip_filepath.string(), file_path.string(), file_path.filename().string());
            }
            else
            {
                std::cerr << "cpdn_zip error: File not found : " << file_path << std::endl;
                return false;
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "cpdn_zip exception: " << e.what() << std::endl;
        return false;
    }
}


bool cpdn_unzip(
    const std::filesystem::path& zip_filepath, 
    const std::filesystem::path& output_directory)
{
    try
    {
        // Open the archive to inspect its contents
        auto archive = ZipFile::Open(zip_filepath.string());

        for (size_t i = 0; i < archive->GetEntriesCount(); ++i)
        {
            auto entry = archive->GetEntry(i);
            if (entry)
            {
                // Construct the full destination path for the file
                auto destination_path = output_directory / entry->GetName();
                
                // Ensure the parent directory exists
                if (destination_path.has_parent_path())
                {
                    std::filesystem::create_directories(destination_path.parent_path());
                }

                // Use the static ExtractFile method for each entry
                ZipFile::ExtractFile(zip_filepath.string(), entry->GetName(), destination_path.string());
            }
        }
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "cpdn_unzip exception: " << e.what() << std::endl;
        return false;
    }
}
