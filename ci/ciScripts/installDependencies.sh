sudo apt-get update
sudo apt-get install git
sudo add-apt-repository --yes ppa:deadsnakes/ppa
sudo apt-get update
sudo apt-get install python3.7
pyenv global 3.7.0

wget https://github.com/Kitware/CMake/releases/download/v3.18.0/cmake-3.18.0-Linux-x86_64.tar.gz
tar -xvf cmake-3.18.0-Linux-x86_64.tar.gz 
export PATH=/home/circleci/repo/cmake-3.18.0-Linux-x86_64/bin:$PATH
