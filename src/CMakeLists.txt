cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_STANDARD 11)

add_library(videoutils STATIC
            videoutils.cpp
            ffmpegutils.cpp
            indexeddb.cpp)

set(FFMPEG_LIBS
  avfilter
  avformat
  avcodec
  avutil
)

add_executable(vstvideoutils main.cpp)
target_link_libraries(vstvideoutils videoutils ${FFMPEG_LIBS})

# OpenH264
include(FindPkgConfig)
pkg_search_module(OPENH264 REQUIRED openh264)


target_include_directories(vstvideoutils SYSTEM PUBLIC ${OPENH264_INCLUDE_DIRS})
#link_directories(vstvideoutils ${OPENH264_LIBRARY_DIRS})
target_link_libraries(vstvideoutils ${OPENH264_LIBRARIES})


if (APPLE)
  set_target_properties(vstvideoutils PROPERTIES
   LINK_FLAGS "-L${OPENH264_LIBRARY_DIRS} -lz -liconv -llzma -lbz2 -framework AudioToolbox -framework CoreFoundation -framework CoreVideo -framework CoreMedia -framework VideoToolbox -framework CoreGraphics -framework CoreImage -framework Foundation -framework OpenGL -framework Cocoa"
 )
endif()



set(DEBUG False)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DEBUG True)
endif()


if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten")
  set(WASM 1)

  set(CMAKE_EXECUTABLE_SUFFIX ".js")

  set(POST_JS ${CMAKE_CURRENT_SOURCE_DIR}/messaging.js)

  set(WASM_LINK_FLAGS
    --post-js ${POST_JS}
    --bind
    -s ASSERTIONS=1
    -s EXTRA_EXPORTED_RUNTIME_METHODS=['ccall,cwrap']
    -s SAFE_HEAP=1
    -s WASM=1
    -s NO_EXIT_RUNTIME=1
    -s ALLOW_MEMORY_GROWTH=1
    -s BUILD_AS_WORKER=1
    -s FORCE_FILESYSTEM=1
    -s DEMANGLE_SUPPORT=1
    -lm
    -L${OPENH264_LIBRARY_DIRS}
  )

  # -s NO_FILESYSTEM=1

  if (DEBUG)
    list(APPEND WASM_LINK_FLAGS
      --emrun
      -g
    )
  endif()

  string(REPLACE ";" " " WASM_LINK_FLAGS "${WASM_LINK_FLAGS}")
  set_target_properties(vstvideoutils PROPERTIES LINK_FLAGS "${CMAKE_LDFLAGS} ${WASM_LINK_FLAGS}")

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/VideoUtils.js DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

endif()

