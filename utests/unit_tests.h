#pragma once

#include <iostream>
#include <cstdlib>      // for EXIT_SUCCESS & EXIT_FAILURE

#include "../CPDN_control_code.h"


// Handy macros to standardize part of the test code.
#define TEST(f)  std::string test_name(f)
#define SUCCESS  std::cout << "TEST " << test_name << " succeeded.\n"
#define FAIL     std::cout << "TEST " << test_name << " FAILED.\n"

// Declare all external test functions for main program (see individual test source files)
int t_read_rcf_file();
int t_read_progress_file();
