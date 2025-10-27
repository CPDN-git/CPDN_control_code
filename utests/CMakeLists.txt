# Unit tests CMakeLists.txt
#
#  To add a new test:
#  1.  Write new test code: t_example.cpp.
#  2.  Add new to unit_tests.cpp in map() in main().
#  4.  Add declaration of new test to unit_tests.h.
#  5.  In this file:
#      (a) add source code to 'add_executable'
#      (b) add new 'add_test' for new test name.


# Create an executable target for our unit tests
add_executable(unit_tests unit_tests.cpp
                        t_read_rcf_file.cpp
                        t_read_progress_file.cpp
)

# Link the test executable to the control code
target_link_libraries(unit_tests PRIVATE control_code)

# Add the targets to the CTest test suite
# The command is the path to the executable just built.
# CTest automatically runs this executable and checks its return code (0 = PASS, non-zero = FAIL).
add_test( NAME Control_code_RCFTest       COMMAND unit_tests "Read RCF File" )
add_test( NAME Control_code_ProgressTest  COMMAND unit_tests "Read Progress File" )
