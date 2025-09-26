//
// Control code header file for the OpenIFS application in the climateprediction.net project
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) November 2022
// Contributions from Glenn Carver (ex-ECMWF), 2022->
//

#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>  // required by file_is_empty
#include <exception>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <regex.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include "boinc/boinc_api.h"
#include "boinc/boinc_zip.h"
#include "boinc/diagnostics.h"
#include "boinc/util.h"
#include "rapidxml.hpp"
#include <algorithm>

int initialise_boinc(std::string&, std::string&, std::string&, int&);
int move_and_unzip_app_file(std::string, std::string, std::string, std::string);
const char* strip_path(const char* path);
int check_child_status(long, int);
int check_boinc_status(long, int);
long launch_process_oifs(const std::string, const char*, const char*, const std::string);
long launch_process_wrf(const std::string, const char*);
std::string get_tag(const std::string &str);
void process_trickle(double, const std::string, const std::string, const std::string, int, int);
bool file_exists(const std::string &str);
bool file_is_empty(std::string &str);
double cpu_time(long);
double model_frac_done(double, double, int);
std::string get_second_part(const std::string&, const std::string&);
int move_result_file(std::string, std::string, std::string, std::string);
bool check_stoi(std::string& cin);
bool oifs_parse_stat(const std::string&, std::string&, const int);
bool fread_last_line(const std::string&, std::string&);
bool oifs_valid_step(std::string&,int);
int  print_last_lines(std::string filename, int nlines);
void read_progress_file(std::string, int&, int&, std::string&, int&, int&);
void update_progress_file(std::string, int, int, std::string, int, int);
bool read_rcf_file(std::ifstream&, std::string&, std::string&);
bool read_delimited_line(std::string&, std::string, std::string, int, std::string&);
void call_boinc_begin_critical_section();
void call_boinc_end_critical_section();
int call_boinc_unzip(const std::string&, const std::string&);
int call_boinc_zip(const std::string&, const ZipFileList*);
int call_boinc_copy(std::string, std::string);
int call_boinc_resolve_filename_s(std::string, std::string&);
void call_boinc_upload_file(std::string);
int call_boinc_upload_status(std::string);
void call_boinc_report_app_status(double, int, double);
void call_boinc_fraction_done(double);
void call_boinc_finish(int);
int copy_and_unzip(std::string, std::string, std::string, std::string);

// GC. recoded to not use putenv, it takes control of the memory passed in (see multiple stackexchange posts)
bool set_env_var(const std::string& name, std::string& val) {
    return (setenv(name.c_str(), val.c_str(), 1) == 0);     // 1 = overwrite existing value, true on success.
}

using namespace std;
using namespace std::chrono;
using namespace std::this_thread;
using namespace rapidxml;
