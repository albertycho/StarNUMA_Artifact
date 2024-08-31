#!/bin/bash


sudo apt-get -y --assume-yes install gcc
sudo apt-get -y --assume-yes install automake
sudo apt-get -y --assume-yes install autoconf
sudo apt-get -y --assume-yes install libtool
sudo apt-get -y --assume-yes install bison
sudo apt-get -y --assume-yes install swig
sudo apt-get -y --assume-yes install build-essential
sudo apt-get -y --assume-yes install vim
sudo apt-get -y --assume-yes install python3.7
sudo apt-get -y --assume-yes install pkg-config
sudo apt-get -y --assume-yes install python3-pip
sudo apt-get -y --assume-yes install zlib1g-dev
sudo apt-get -y --assume-yes install uuid-dev
sudo apt-get -y --assume-yes install libboost-all-dev
sudo apt-get -y --assume-yes install cmake
sudo apt-get -y --assume-yes install libgtk2.0-dev
sudo apt-get -y --assume-yes install pkg-config
sudo apt-get -y --assume-yes install libqt4-dev
sudo apt-get -y --assume-yes install unzip
sudo apt-get -y --assume-yes install wget
sudo apt-get -y --assume-yes install libjasper-dev
sudo apt-get -y --assume-yes install libpng-dev
sudo apt-get -y --assume-yes install libjpeg-dev
sudo apt-get -y --assume-yes install libtiff5-dev
#sudo apt-get -y --assume-yes install libgdk-pixbuf2.0-dev
sudo apt-get -y --assume-yes install libopenexr-dev
#sudo apt-get -y --assume-yes install libbz2-dev
sudo apt-get -y --assume-yes install tk-dev
sudo apt-get -y --assume-yes install tcl-dev
sudo apt-get -y --assume-yes install g++
sudo apt-get -y --assume-yes install git
sudo apt-get -y --assume-yes install subversion
sudo apt-get -y --assume-yes install automake
sudo apt-get -y --assume-yes install libtool
sudo apt-get -y --assume-yes install zlib1g-dev
sudo apt-get -y --assume-yes install libicu-dev
sudo apt-get -y --assume-yes install libboost-all-dev
sudo apt-get -y --assume-yes install liblzma-dev
sudo apt-get -y --assume-yes install python-dev
sudo apt-get -y --assume-yes install graphviz
sudo apt-get -y --assume-yes install imagemagick
sudo apt-get -y --assume-yes install make
sudo apt-get -y --assume-yes install cmake
sudo apt-get -y --assume-yes install libgoogle-perftools-dev
sudo apt-get -y --assume-yes install autoconf
sudo apt-get -y --assume-yes install doxygen
sudo apt-get -y --assume-yes install libgtop2-dev
sudo apt-get -y --assume-yes install libncurses-dev
sudo apt-get -y --assume-yes install ant
sudo apt-get -y --assume-yes install libnuma-dev
sudo apt-get -y --assume-yes install libmysqld-dev
sudo apt-get -y --assume-yes install libaio-dev
sudo apt-get -y --assume-yes install libjemalloc-dev
sudo apt-get -y --assume-yes install libdb5.3++-dev
sudo apt-get -y --assume-yes install libreadline-dev

cd ./harness
./build.sh
cd ..
cd ./masstree
echo 'in masstree'
echo 'calling configure'
sudo ./configure
echo 'calling build'
./build.sh
cd ..
./build.sh


