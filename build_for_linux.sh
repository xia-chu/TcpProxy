#!/bin/bash
sudo apt-get install libev-dev libhiredis-dev
git submodule update --init
cd ZLMediaKit
git submodule update --init
cd ..
mkdir -p linux_build
cd linux_build
cmake ..
make -j4
sudo make install
