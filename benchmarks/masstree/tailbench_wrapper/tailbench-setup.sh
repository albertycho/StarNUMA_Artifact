#!/bin/bash


echo -e "\033[92mInstalling Tailbench Dependencies\033[0m"
sudo apt-get -y --assume-yes install gcc automake autoconf libtool bison swig build-essential vim python3.7 pkg-config python3-pip zlib1g-dev uuid-dev libboost-all-dev cmake libgtk2.0-dev pkg-config libqt4-dev unzip wget libjasper-dev libpng-dev libjpeg-dev libtiff5-dev libgdk-pixbuf2.0-dev libopenexr-dev libbz2-dev tk-dev tcl-dev g++ git subversion automake libtool zlib1g-dev libicu-dev libboost-all-dev liblzma-dev python-dev graphviz imagemagick make cmake libgoogle-perftools-dev autoconf doxygen libgtop2-dev libncurses-dev ant libnuma-dev libmysqld-dev libaio-dev libjemalloc-dev libdb5.3++-dev libreadline-dev


cd ./masstree
echo 'in masstree'
echo 'calling configure'
sudo ./configure
echo 'calling build'
./build.sh
cd ..
./build.sh
cd ..


