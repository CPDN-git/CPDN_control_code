#
#  Makefile to generate the openifs wrapper.
#
#  Creates both production & debug versions.
#
#       Glenn 

VERSION = 43r3_1.00
TARGET  = oifs_$(VERSION)_x86_64-pc-linux-gnu
DEBUG   = oifs_$(VERSION)_x86_64-pc-linux-gnu-debug
SRC     = openifs.cpp CPDN_control_code.cpp

CC       = g++
CVERSION = -DCODE_VERSION="\"$(git rev-parse HEAD)\""
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

test: oifs_43r3_test.cpp
	$(CC) -g -std=c++17 -Wall -o oifs_43r3_test.exe oifs_43r3_test.cpp

clean:
	$(RM) *.o $(TARGET) $(DEBUG)
