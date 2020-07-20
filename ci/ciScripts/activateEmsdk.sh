echo "Cloning EMSDK"

git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
pwd

EMSDK_VERSION=sdk-fastcomp-1.38.30-64bit

# Download and install the right SDK tool
./emsdk install $EMSDK_VERSION

# Activate the right version of the SDK
./emsdk activate $EMSDK_VERSION

# activate PATH and other env variables in the current terminal

cd ..
