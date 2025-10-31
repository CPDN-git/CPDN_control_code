#
#  Makefile to generate the openifs control code.
#
#  Creates both production & debug versions.
#  Modified:   Uses newer & memory safe(r) cpdn_zip functions built on ZipLib.
#
#       Glenn 

VERSION = 43r3_1.00
TARGET  = oifs_$(VERSION)_x86_64-pc-linux-gnu
DEBUG   = oifs_$(VERSION)_x86_64-pc-linux-gnu-debug
TEST    = oifs_43r3_test.exe
SRC     = lib/utils.cpp   openifs.cpp cpdn_control.cpp

CC       = g++
CVERSION := -DCODE_VERSION='"$(shell git rev-parse HEAD | cut -c 1-8)"'	# use single quotes to preserve the double quotes in the code
CFLAGS   = -g -static -pthread -std=c++17 -Wall
# Address Sanitizer (ASan) can't use -static
CDEBUG   = -fsanitize=address -ggdb3 -pthread -std=c++17 -Wall

# For github actions, allow BOINC_DIR to be overridden
BOINC_DIR ?= ../boinc-8.0.2-x86_64
ZIP_DIR   = zip/install
INCLUDES  = -Ilib -I$(BOINC_DIR)/include -I$(ZIP_DIR)/include -I$(ZIP_DIR)/include/ZipLib
BOINC_LIB = -L$(BOINC_DIR)/lib -lboinc_api -lboinc

CPDNZIP_LIB  = -L$(ZIP_DIR)/lib -lcpdn_zip		# note this is a combined static library with cpdn_zip and ZipLib

LIBS = $(BOINC_LIB) $(CPDNZIP_LIB)



all: $(TARGET) $(DEBUG) $(TEST)

$(TARGET): $(SRC)
	$(CC) $(CVERSION) $(SRC) $(CFLAGS) $(INCLUDES) $(LIBS) -o $(TARGET)

$(DEBUG): $(SRC)
	$(CC) $(CVERSION) $(SRC) $(CDEBUG) $(INCLUDES)  $(LIBS) -o $(DEBUG)

$(TEST): oifs_43r3_test.cpp
	$(CC) -g -std=c++17 -Wall -o $(TEST) oifs_43r3_test.cpp

clean:
	$(RM) *.o $(TARGET) $(DEBUG) $(TEST)
