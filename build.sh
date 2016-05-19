#!/bin/bash

#g++ *.c *.cpp -oenumg -std=c++11 -g -rdynamic

cd build
make clean
cmake ..
make
sudo make install
cd ..
