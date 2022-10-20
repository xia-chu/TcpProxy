#!/bin/bash
git submodule update --init
cd ZLMediaKit
git submodule update --init
cd ..
brew install hiredis
brew install libev
mkdir -p mac_build
cd mac_build
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl/1.0.2j/
make -j4
sudo make install
