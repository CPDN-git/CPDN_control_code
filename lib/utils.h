#pragma once

#include <string>

bool set_env_var(const std::string&, const std::string&);
bool file_exists(const std::string&);
bool file_is_empty(const std::string&);
bool set_exec_perms(const std::string&);
bool parse_key_value(const std::string&, std::string&, std::string&);
bool extract_key_value(const std::string&, const std::string&, char, std::string& );
bool read_delimited_line(std::string, const std::string&, const std::string&, int, std::string&);
int  print_last_lines(const std::string& filename, int nlines);
double cpu_time(long handleProcess);