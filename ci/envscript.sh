echo "Cloning EMSDK"

git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
pwd

# Download and install the right SDK tool
./emsdk install 1.39.7-fastcomp

# Activate the right version of the SDK
#./emsdk activate sdk-fastcomp-tag-1.38.30-64bit
./emsdk activate 1.39.7-fastcomp

# activate PATH and other env variables in the current terminal
source ./emsdk_env.sh

./emsdk list

emcc -v

cd ..

echo $EMSDK