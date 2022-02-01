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

export PATH=/opt/circleci/.pyenv/versions/2.7.12/bin/:$PATH
pyenv global 2.7.12

python --version

bash ./build.sh