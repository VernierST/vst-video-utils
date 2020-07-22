#!/usr/bin/env bash
# set -x

echo "$PATH"
cmake --version

if [[ "$EMSDK" == "" ]]; then
  echo "EMSDK must be set"
  exit 1
fi

# . $EMSDK/emsdk_env.sh

######
if [[ "$BUILD_OPENH264" == "" ]]; then
  BUILD_OPENH264=1
fi
if [[ "$BUILD_OPENCV" == "" ]]; then
  BUILD_OPENCV=1
fi
if [[ "$BUILD_FFMPEG" == "" ]]; then
  BUILD_FFMPEG=1
fi
if [[ "$BUILD_MODULE" == "" ]]; then
  BUILD_MODULE=1
fi
EXTRA_MAKE_ARGS=-j8

MODULE_BUILD_TESTS=OFF
MODULE_BUILD_TYPE=Release

######
BUILDDIR=build
OPENH264_SRCDIR=${PWD}/openh264
OPENH264_BUILDDIR=${PWD}/${BUILDDIR}/openh264
OPENCV_SRCDIR=${PWD}/opencv
OPENCV_BUILDDIR=${PWD}/${BUILDDIR}/opencv
FFMPEG_SRCDIR=${PWD}/FFmpeg
FFMPEG_BUILDDIR=${PWD}/${BUILDDIR}/ffmpeg

# prepare build directories
mkdir -p "$BUILDDIR" 2>/dev/null
mkdir -p "$OPENH264_BUILDDIR" 2>/dev/null
mkdir -p "$OPENCV_BUILDDIR"  2>/dev/null
mkdir -p "$FFMPEG_BUILDDIR" 2>/dev/null

# setup openh264
export PKG_CONFIG_PATH=${OPENH264_BUILDDIR}/lib/pkgconfig
export EM_PKG_CONFIG_PATH=${PKG_CONFIG_PATH}


function exitError() {
  echo "ERROR: $*"
  exit 1
}

function checkError(){
  if [[ $? -ne 0 ]]; then
    exitError "$*"
  fi
}

######################
### Build OpenH264 ###
######################
if [[ $BUILD_OPENH264 -ne 0 ]]; then
  pushd "$OPENH264_BUILDDIR" >/dev/null

  echo "*************************"
  echo "*** Building OpenH264 ***"
  echo "*************************"

  emmake make $EXTRA_MAKE_ARGS PREFIX="${OPENH264_BUILDDIR}" -f ${OPENH264_SRCDIR}/Makefile install

  checkError "Build OpenH264"

  popd 2> /dev/null
fi


####################
### Build OpenCV ###
####################
if [[ $BUILD_OPENCV -ne 0 ]]; then
  pushd "$OPENCV_BUILDDIR" >/dev/null

  echo "***********************"
  echo "*** Building OpenCV ***"
  echo "***********************"

  python ${OPENCV_SRCDIR}/platforms/js/build_js.py . --build_wasm # --threads --simd
  
  checkError "Build OpenCV"

  cmake --build . --target install
  
  checkError "Build OpenCV failed"
 
  popd 2> /dev/null
fi


####################
### Build FFmpeg ###
####################
if [[ $BUILD_FFMPEG -ne 0 ]]; then
  pushd "$FFMPEG_BUILDDIR" >/dev/null

  echo "**************************"
  echo "*** Configuring FFmpeg ***"
  echo "**************************"
  emconfigure "${FFMPEG_SRCDIR}/configure" \
              --prefix="${FFMPEG_BUILDDIR}" \
              --disable-x86asm \
              --disable-asm \
              --disable-inline-asm \
              --disable-network \
              --disable-doc \
              --ar=emar \
              --cc=emcc \
              --cxx=em++ \
              --objcc=emcc \
              --dep-cc=emcc \
              --enable-small \
              --disable-runtime-cpudetect \
              --disable-debug \
              --disable-programs \
              --enable-cross-compile \
              --disable-avdevice \
              --disable-swresample \
              --disable-swscale \
              --disable-postproc \
              --disable-pthreads \
              --enable-libopenh264 \
              --disable-sdl2
  
  checkError "Configure FFmpeg"


  echo "***********************"
  echo "*** Building FFmpeg ***"
  echo "***********************"
  emmake make $EXTRA_MAKE_ARGS install

  checkError "Building FFmpeg"

  popd >/dev/null
fi


#########################
### Build WASM Module ###
#########################
if [[ $BUILD_MODULE -ne 0 ]]; then
  pushd "$BUILDDIR" >/dev/null

  echo "******************************"
  echo "*** Configuring VideoUtils ***"
  echo "******************************"
  emconfigure cmake .. -DCMAKE_BUILD_TYPE=${MODULE_BUILD_TYPE} -DINCLUDE_TESTS=${MODULE_BUILD_TESTS}
  
  checkError "Configuring VideoUtils"

  echo "***************************"
  echo "*** Building VideoUtils ***"
  echo "***************************"
  emmake make $EXTRA_MAKE_ARGS install

  checkError "Building VideoUtils"

  popd >/dev/null
fi

exit 0
