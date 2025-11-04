/**
 * Utility/library functions for the CPDN task controller
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include "cpdn_linux_cpu_time.h"

#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <sys/stat.h>  // for chmod

namespace  fs = std::filesystem;



/** 
 * @brief Sets incoming arg/value as environment variable
 *        Do not use putenv, it stores the pointer of memory passed in (see multiple stackexchange posts on this issue)
 */
bool set_env_var(const std::string& name, const std::string& val) {
    return (setenv(name.c_str(), val.c_str(), 1) == 0);     // 1 = overwrite existing value, true on success.
}


/**
 * @brief  Check whether a file exists
 */
bool file_exists(const std::string& filename) {
    std::ifstream infile(filename.c_str());
    return infile.good();
}


/** 
 * @brief Check whether file is zero bytes long
 * from: https://stackoverflow.com/questions/2390912/checking-for-an-empty-file-in-c
 * returns True if file is zero bytes, otherwise False.
 */
bool file_is_empty(const std::string& fpath) {
   return ( fs::file_size(fpath) == 0);
}


/**
 * @brief Set executable permissions on a file
 *        This is a workaround currently as cpdn_unzip does not set unix permissions correctly.
 */
bool set_exec_perms(const std::string& filepath) {
    // 0755 is a standard permission set:
    // Owner: Read, Write, Execute
    // Group: Read, Execute
    // Others: Read, Execute

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    if (chmod(filepath.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 ) {
        return false;
    }
#endif

    return true;
}



/**
 * @brief Attempts to parse a single line as a key/value pair.
 * * Handles common shell formats like "VAR=VALUE" or "export VAR='VALUE'".
 *
 * @param line  The line of text to parse.
 * @param key   Returned parameter for the key.
 * @param value Returned parameter for the value.
 * @return true if successful, false if the line is empty, a comment, or invalid.
 */
bool parse_key_value(const std::string& line, std::string& key, std::string& value)
{
    std::string working_line = line;

    // Trim leading whitespace
    working_line.erase(0, working_line.find_first_not_of(" \t\n\r"));
    
    // Ignore comments and empty lines
    if (working_line.empty() || working_line[0] == '#') {
        return false;
    }

    // Strip 'export' keyword if present
    const std::string export_prefix = "export ";
    if (working_line.rfind(export_prefix, 0) == 0) {
        working_line.erase(0, export_prefix.length());
    }

    // Find the '=' delimiter
    auto eq_pos = working_line.find('=');
    if (eq_pos == std::string::npos || eq_pos == 0) {
        return false;
    }

    key   = working_line.substr(0, eq_pos);
    value = working_line.substr(eq_pos + 1);

    // Tidy up the value (remove surrounding quotes if present)
    value.erase(0, value.find_first_not_of(" \t\n\r"));     // Trim leading whitespace
    value.erase(value.find_last_not_of(" \t\n\r") + 1);     // Trim trailing whitespace

    if ( value.length() >= 2 && 
       ( (value.front() == '"' && value.back() == '"') || 
         (value.front() == '\'' && value.back() == '\'') ) ) 
    {
        // Remove surrounding quotes
        value = value.substr(1, value.length() - 2);
    }
    
    // Tidy up the key (trimming is sufficient)
    key.erase(key.find_last_not_of(" \t\n\r") + 1);
    
    return true;
}


/**
 * @brief Searches a line for a specific key and extracts the value substring.
 * 
 * This function is designed to extract the value of an input 'key' in a typical
 * key=value pair contained in the input line.
 * NOTE! It is similar to read_delimited_line but that functions works by a 
 * positional search of a delimiter, rather than expecting a key/value pair.
 * 
 * @param line The input string to search (e.g., the line read from a file).
 * @param key The substring to look for (e.g., "IFSDATA_FILE").
 * @param delimiter The character separating the key from the value (e.g., '=' or ':').
 * @param out_value Reference to a string where the extracted and stripped value will be stored.
 * @return true if the key was found and a value successfully extracted; false otherwise.
 */
bool extract_key_value(const std::string& line, const std::string& key, char delimiter, std::string& out_value ) 
{
    if (line.find(key) == std::string::npos) {
        return false;
    }

    auto pos = line.find(delimiter);
    if (pos == std::string::npos) {
        return false;
    }

    // Extract the substring (the value part)
    // substr(pos + 1) takes the rest of the string after the delimiter.
    out_value = line.substr(pos + 1);

    // Remove space and trailing commas (as in a namelist entry).
    out_value.erase( std::remove(out_value.begin(), out_value.end(), ','), out_value.end() );
    out_value.erase( std::remove(out_value.begin(), out_value.end(), ' '), out_value.end() );

    return true;
}


/**
 * @brief Extracts a substring following a positional delimiter (if found).
 *        This function does a similar job to extract_key_value but works by
 *        searching for a positional delimiter rather than a key/value pair.
 *        TODO: Could be combined with extract_key_value to reduce code duplication or replaced by extract_key_value.
 * 
 * @return true if value was found false otherwise. Found substring is in 'returned_value' parameter
 */
bool read_delimited_line(std::string file_line, const std::string& delimiter, const std::string& to_find, int position, std::string& returned_value)
{
    size_t pos = 0;
    int count = 0;

    if (file_line.find(to_find) != std::string::npos ) {
       // From the file line take the field specified by the position
       while ((pos = file_line.find(delimiter)) != std::string::npos) {
          count = count + 1;
          if (count == position) {  
             returned_value = file_line.substr(0,pos);

             // Remove whitespace
             returned_value.erase( std::remove_if( returned_value.begin(), \
                                   returned_value.end(), ::isspace ), returned_value.end() );
          }
          file_line.erase(0, pos + delimiter.length());
       }
    }
    return !returned_value.empty();
}


/**
 * @brief Opens a file if exists and uses circular buffer to read & print last lines of file to stderr
 * @return zero : either can't open file or file is empty
 *          > 0  : no. of lines in file (may be less than maxlines)
 */
int print_last_lines(const std::string& filename, int maxlines)
{
   int     count = 0;
   std::string  lines[maxlines];
   std::ifstream filein(filename);

   if ( filein.is_open() ) {
      while ( getline(filein, lines[count%maxlines]) )
         count++;
   }

   if ( count > 0 ) {
      // find the oldest lines first in the buffer, will not be at start if count > maxlines
      int start = count > maxlines ? (count%maxlines) : 0;
      int end   = std::min(maxlines,count);

      std::cerr << ">>> Printing last " << end << " lines from file: " << filename << '\n';
      for ( int i=0; i<end; i++ ) {
         std::cerr << lines[ (start+i)%maxlines ] << '\n';
      }
      std::cerr << "------------------------------------------------" << '\n';
   }

   return count;
}


// Calculate the cpu_time
double cpu_time(long handleProcess) {
    #ifdef __APPLE_CC__
       double x;
       int retval = boinc_calling_thread_cpu_time(x);
       return x;
    // Placeholder for Windows
    //#elif defined(_WIN32) || defined(_WIN64)
    //   double x;
    //   int retval = boinc_process_cpu_time(GetCurrentProcess(), x);
    //   return x;
    #else
       return cpdn_linux_cpu_time(handleProcess);       // recoded version of boinc linux_cpu_time().
    #endif
}


/**
 * @brief Reads and returns the last line of a file.
 * 
 * This function maintains its state between calls to track the last read position
 * in the file, allowing it to return only new lines added since the last call.
 * It behaves similarly to the 'tail -f' command.
 * 
 *     Glenn Carver, CPDN, 2025.
 * 
 * @param fname   The name of the file to read.
 * @param logline Last line read from file; stored between calls.
 * @return        True if a new line was read; returns false and logline unchanged
 *                if no new line was read; returns false and empty logline
 *                if the file does not exist.
 */
bool fread_last_line(const std::string& fname, std::string& logline) {

    static std::streamoff last_offset = 0;
    static std::string    last_line;
    std::string           line;

    // Check file exists and non-empty
    std::ifstream logfile(fname, std::ios::in);
    if (!logfile.is_open()) {
        logline.clear();
        last_offset = 0;
        std::cerr << ".. file_last_line(): warning, " << fname << " does not exist." << std::endl;
        return false;
    }

   // Seek to last offset and read lines to file end
   logfile.seekg(last_offset, std::ios::beg);

   while (std::getline(logfile, line)) {
      last_line = line;
   }
   //std::cerr << "fread_last_line: last line read: " << last_line << '\n';

   // Update last_offset for next call
   last_offset = logfile.tellg();
   if (last_offset == -1) {
      // At EOF, set to file size
      logfile.clear();                   // must clear stream error before attempting to read again
      logfile.seekg(0, std::ios::end);   // seek backwards to start of file to get size.
      last_offset = logfile.tellg();
    }

    logfile.close();

    if (!last_line.empty()) {
        logline = last_line;
        return true;
    }
    return false;    // no new line read and arg logline unchanged
}
