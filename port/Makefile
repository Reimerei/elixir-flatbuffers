# the compiler
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall -std=c++11

# folders to include
INCLUDES = -I

all: idl_parser idl_gen_text main fb_port

main_only: main fb_port

idl_parser: lib/flatbuffers/idl_parser.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o idl_parser.o lib/flatbuffers/idl_parser.cpp

idl_gen_text: lib/flatbuffers/idl_gen_text.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c -o idl_gen_text.o lib/flatbuffers/idl_gen_text.cpp

fb_port: idl_gen_text.o idl_parser.o fb_port.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o fb_port idl_gen_text.o idl_parser.o fb_port.cpp

fb_nif: fb_nif.cpp idl_gen_text.o idl_parser.o nifpp.h
	$(CXX) $(CFLAGS) $(SFLAGS) $(INCLUDES) -std=c++11 -o fb_nif.so idl_gen_text.o idl_parser.o $<

echo: echo.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o echo echo.cpp

clean:
	$(RM) fb_port *.o
