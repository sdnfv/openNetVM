#!/bin/bash
# A script to install static analysis tools pylint, shellcheck, and cppcheck.

# Install cppcheck=1.90-4build1
sudo apt-get install cppcheck=1.90-4build1 -y
cppcheck --version

# Install shellcheck-v0.7.0
sudo apt install xz-utils
wget -qO- "https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.linux.x86_64.tar.xz" | tar -xJv
sudo cp "shellcheck-v0.7.0/shellcheck" /usr/bin/
rm -r -f shellcheck-v0.7.0
shellcheck --version

# Install pylint 2.4.4
sudo apt install python3-pip
pip3 --version
pip3 install pylint==2.4.4
echo export PATH="$HOME"/.local/bin:"$PATH" >> ~/.bashrc
echo "Please run source ~/.bashrc"
