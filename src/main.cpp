#include "videoutils.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(videoutils) {
  emscripten::function("dumpMetaData",  &dumpMetaData);
  emscripten::function("readMetaData",  &readMetaData);
  emscripten::function("transcodeRotation", &transcodeRotation);
  emscripten::function("transmuxStripMeta", &transmuxStripMeta);

  emscripten::function("createTrackingContext", &createTrackingContext);
  emscripten::function("destroyTrackingContext", &destroyTrackingContext);
  emscripten::function("trackObjectNextFrame2", &trackObjectNextFrame);
}

int main()
{
  InitVideoUtils();

  EM_ASM(
    Module.trackObjectNextFrame = (reqId,trackingCtxId,timeStamp,width,height,buffer) => {
      const u8 = new Uint8Array(buffer);
      const ptr = Module._malloc(u8.byteLength); // The handler will free this data
      Module.HEAPU8.set(u8, ptr);
      return Module['trackObjectNextFrame2'](reqId,trackingCtxId,timeStamp,width,height,ptr);
    };
  );

  return 0;
}

#else


#include <string>
#include <cstdio>
#include <cstdlib>
extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavfilter/buffersink.h>
  #include <libavfilter/buffersrc.h>
  #include <libavutil/opt.h>
}

#include <vector>
#include "ffmpegutils.h"



static bool loadFileBytes(const std::string &filename, std::vector<uint8_t> &bytes)
{
  bool success = false;

  FILE *f = fopen(filename.c_str(), "rb");
  if (f)
  {
    fseek(f, 0, SEEK_END);
    auto size = ftell(f);
    fseek(f, 0, SEEK_SET);
    bytes.resize(size);
    auto total = fread(bytes.data(), 1, bytes.size(), f);
    success = true;
    fclose(f);
  }

  return success;
}

///////////////////////
// Test/Demo program //
///////////////////////
int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "usage: filename\n");
    return 1;
  }

  std::string filename = argv[1];

  printf("Input File: %s\n", filename.c_str());

  InitFFmpegUtils();

  dumpMetaData(0, "", filename);
  readMetaData(0, "", filename);
  transcodeRotation(0, "", filename, filename + ".mp4");

  return 0;
}


#endif
