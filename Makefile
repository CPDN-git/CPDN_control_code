#
#  Makefile to generate the openifs wrapper.
#
#  Creates both production & debug versions.
#
#       Glenn 

VERSION = 43r3_1.00
TARGET  = oifs_$(VERSION)_x86_64-pc-linux-gnu
DEBUG   = oifs_$(VERSION)_x86_64-pc-linux-gnu-debug
TEST    = oifs_43r3_test.exe
SRC     = openifs.cpp CPDN_control_code.cpp

CC       = g++
CVERSION := -DCODE_VERSION='"$(shell git rev-parse HEAD | cut -c 1-8)"'	# use single quotes to preserve the double quotes in the code
CFLAGS   = -g -static -pthread -std=c++17 -Wall
# Address Sanitizer (ASan) can't use -static
CDEBUG   = -fsanitize=address -ggdb3 -pthread -std=c++17 -Wall
INCLUDES  = -I../boinc-install/include
LIBDIR    = ../boinc-install/lib
LIBS      = -lboinc_api -lboinc_zip -lboinc


all: $(TARGET) $(DEBUG)

$(TARGET): $(SRC)
	$(CC) $(CVERSION) $(SRC) $(CFLAGS) $(INCLUDES) -L$(LIBDIR) $(LIBS) -o $(TARGET)

$(DEBUG): $(SRC)
	$(CC) $(CVERSION) $(SRC) $(CDEBUG) $(INCLUDES) $(LIBDIR)/libboinc_api.a $(LIBDIR)/libboinc_zip.a $(LIBDIR)/libboinc.a  -o $(DEBUG)

$(TEST): oifs_43r3_test.cpp
	$(CC) -g -std=c++17 -Wall -o $(TEST) oifs_43r3_test.cpp

clean:
	$(RM) *.o $(TARGET) $(DEBUG) $(TEST)
