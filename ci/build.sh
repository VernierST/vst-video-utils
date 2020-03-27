#!/usr/bin/env bash

sh ./ciScripts/initSubmodules.sh

sh ./ciScripts/installDependencies.sh

sh ./ciScripts/activateEmsdk.sh

cd emsdk
. ./emsdk_env.sh

./emsdk list

emcc -v

cd ..

echo $EMSDK