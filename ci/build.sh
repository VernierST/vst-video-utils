#!/usr/bin/env bash

sh ./ci/ciScripts/initSubmodules.sh

sh ./ci/ciScripts/installDependencies.sh

sh ./ci/ciScripts/activateEmsdk.sh

cd emsdk
. ./emsdk_env.sh

./emsdk list

emcc -v

cd ..

echo $EMSDK

bash ../build.sh
