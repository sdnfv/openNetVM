#!/usr/bin/bash
# usage ./install_zmq.sh <version> 
# example ./install_zmq.sh 4.2.2
install_from_git() {
#version of zmq ex. 4.2.2 as first argument
ver=$1
if [ -z $ver ] 
then
#change the version as necessary	
ver=4.2.2
fi

# Download zeromq
# Ref http://zeromq.org/intro:get-the-software
wget https://github.com/zeromq/libzmq/releases/download/v${ver}/zeromq-${ver}.tar.gz

# Unpack tarball package
tar xvzf zeromq-${ver}.tar.gz

# Install dependencies
sudo apt-get update && \
sudo apt-get install -y libtool pkg-config build-essential autoconf automake uuid-dev

# Create make file
cd zeromq-${ver}
./configure

# Build and install(root permission only)
sudo make install

# Install zeromq driver on linux
sudo ldconfig

# Check installed
ldconfig -p | grep zmq

# Expected
############################################################
# libzmq.so.5 (libc6,x86-64) => /usr/local/lib/libzmq.so.5
# libzmq.so (libc6,x86-64) => /usr/local/lib/libzmq.so
############################################################
}

apt_install_zmq() {
	#update and install the version in the apt package manager
	sudo apt-get update && sudo apt-get install zeromq libzmq3-dev
}

#apt_install_zmg

#prefer to install from git
install_zmq_from_git $1
