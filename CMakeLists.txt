cmake_minimum_required(VERSION 2.8.9)
project (enumg)

set(
	SOURCES
	ini.c
	main.cpp
	State.cpp
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "./../lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "./../lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "./../bin")

set(CMAKE_CXX_FLAGS "-Wall -std=c++17 -g -rdynamic -O0")

add_executable(
	enumg
	${SOURCES}
) 

install(TARGETS enumg DESTINATION /usr/bin)
