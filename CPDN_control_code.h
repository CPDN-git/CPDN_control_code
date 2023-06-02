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
#include "boinc/util.h"
#include "rapidxml.hpp"
#include <algorithm>

int initialise_boinc(std::string, std::string, std::string);
int move_and_unzip_app_file(std::string, std::string, std::string, std::string);
const char* strip_path(const char* path);
int check_child_status(long, int);
int check_boinc_status(long, int);
long launch_process_oifs(const std::string, const char*, const char*, const std::string);
long launch_process_wrf(const std::string, const char*);
std::string get_tag(const std::string &str);
void process_trickle(double, const std::string, const std::string, const std::string, int);
bool file_exists(const std::string &str);
bool file_is_empty(std::string &str);
double cpu_time(long);
double model_frac_done(double, double, int);
std::string get_second_part(const std::string, const std::string);
int move_result_file(std::string, std::string, std::string, std::string);
bool check_stoi(std::string& cin);
bool oifs_parse_stat(std::string&, std::string&, int);
bool oifs_get_stat(std::ifstream&, std::string&);
bool oifs_valid_step(std::string&,int);
int  print_last_lines(std::string filename, int nlines);
void update_progress_file(std::string, int, int, std::string, int, int);

using namespace std;
using namespace std::chrono;
using namespace std::this_thread;
using namespace rapidxml;
