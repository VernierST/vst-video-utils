#include "videoutils.h"
#include "ffmpegutils.h"
#include "indexeddb.h"

#include <functional>

#include <cstdio>
#include <vector>

extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/parseutils.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
}

#ifdef __EMSCRIPTEN__
#define sendError(reqId,errmsg) do { \
    EM_ASM({ \
      sendError($0,{ \
        error: errmsg \
      }); \
    }, reqId); \
} while(0)
#else
static void sendError(int reqId, const char *errmsg)
{
  fprintf(stderr, "[***] Request Failed (id=%d): %s\n", reqId, errmsg ? errmsg : "(null)");
}
#endif


struct CopyFileCxt
{
  int reqId = 0;
  std::string db;
  std::string src;
  std::string dst;
  std::vector<uint8_t> data;
};


struct ReadFileMetaCtx
{
  int reqId = 0;
  std::string db;
  std::string filename;

  std::function<void(AVFormatContext *cxt)> onSuccess;
  std::function<void()> onError;
};


namespace
{
  void sendResponse(int id)
  {
#ifdef __EMSCRIPTEN__
    EM_ASM({ sendResult($0) }, id);
#else
    printf("[***] Success (id=%d)\n", id);
#endif
  }


  void sendResponse(int id, const VideoMetaData &meta)
  {
#ifdef __EMSCRIPTEN__
      EM_ASM({
        sendResult($0, {
          avgFrameRate: $1,
          realFrameRate: $2,
          numFrames: $3,
          duration: $4,
          rotation: $5,
          vidWidth: $6,
          vidHeight: $7,
        });
      },
        id,
        meta.avgFrameRate,
        meta.realFrameRate,
        meta.numFrames,
        meta.duration,
        meta.rotation,
        meta.vidWidth,
        meta.vidHeight
      );
#else
    printf("[***] Success (id=%d)\n", id);
    printf("\t === Video MetaData ===\n");
    printf("\t   avgFrameRate: %f\n", meta.avgFrameRate);
    printf("\t   realFrameRate: %f\n", meta.realFrameRate);
    printf("\t   numFrames: %d\n", meta.numFrames);
    printf("\t   duration: %f\n", meta.duration);
    printf("\t   rotation: %d\n", meta.rotation);
    printf("\t   vidWidth: %d\n", meta.vidWidth);
    printf("\t   vidHeight: %d\n", meta.vidHeight);
#endif
  }
} // end namespace



void InitVideoUtils()
{
  InitFFmpegUtils();
}



void dumpMetaData(int reqId, std::string db, std::string filename)
{
  ///
  auto onSuccess = [=](const uint8_t *buf, size_t size)
  {
    int result = 0;
    AVFormatContext *ic = CreateInputFormatContext((const uint8_t*)buf, size, result);

    if (0 == result && ic)
    {
      auto name = db + ':' + filename;
      av_dump_format(ic, 0, name.c_str(), 0);
      sendResponse(reqId);
    }
    else
      sendError(reqId, "Failed to read video file");

    if (ic)
      FreeInputFormatContext(ic);
  };

  ///
  auto onError = [=]()
  {
    sendError(reqId, "Failed to load file");
  };

  IDBLoadAsync(db, filename, onSuccess, onError);
}



void readMetaData(int reqId, std::string db, std::string filename)
{
  ///
  auto onSuccess = [=](const uint8_t *buf, size_t size)
  {
    int result = 0;
    AVFormatContext *ic = CreateInputFormatContext((const uint8_t*)buf, size, result);

    if (0 == result && ic)
    {
      VideoMetaData meta;
      bool success = GetVideoMetaData(ic, meta);
      if (success)
        sendResponse(reqId, meta);
      else
        sendError(reqId, "Failed to read file metadata");
    }
    else
      sendError(reqId, "Failed to read video file");

    if (ic)
      FreeInputFormatContext(ic);

  };

  ///
  auto onError = [=]()
  {
    sendError(reqId, "Failed to load file");
  };

  IDBLoadAsync(db, filename, onSuccess, onError);
}



void transcodeRotation(int reqId, std::string db, std::string src, std::string dst)
{
  ///
  auto onSuccess = [=](const uint8_t *buf, size_t size)
  {
    int result = 0;
    AVFormatContext *ic = CreateInputFormatContext((const uint8_t*)buf, size, result);

    if (0 == result && ic)
    {
      int errCode = 0;
      auto *bytes = new std::vector<uint8_t>;
      bytes->reserve(size); // the resulting file should be of similar size
      bool success = TranscodeRotation(ic, dst, *bytes, errCode);
      if (!success) {
        fprintf(stderr, "Failed to transcode video: errCode=%d\n", errCode);
        sendError(reqId, "Failed to transcode video");
        return;
      }

      // write the file
      IDBStoreAsync(db, dst, bytes->data(), bytes->size(),
                    // onSuccess
                    [=]() {
                      sendResponse(reqId);
                      delete bytes;
                    },
                    // onError
                    [=]() {
                      sendError(reqId, "Failed to write file");
                      delete bytes;
                    });
    }
    else
      sendError(reqId, "Failed to read video file");

    if (ic)
      FreeInputFormatContext(ic);
  };

  ///
  auto onError = [=]()
  {
    sendError(reqId, "Failed to load file");
  };

  IDBLoadAsync(db, src, onSuccess, onError);
}


void transmuxStripMeta(int reqId, std::string db, std::string src, std::string dst)
{
  ///
  auto onSuccess = [=](const uint8_t *buf, size_t size)
  {
    int result = 0;
    AVFormatContext *ic = CreateInputFormatContext((const uint8_t*)buf, size, result);

    if (0 == result && ic)
    {
      int errCode = 0;
      auto *bytes = new std::vector<uint8_t>;
      bytes->reserve(size); // the resulting file should be of similar size
      bool success = TransmuxStripMeta(ic, dst, *bytes, errCode);
      if (!success) {
        fprintf(stderr, "Failed to transmux video: errCode=%d\n", errCode);
        sendError(reqId, "Failed to transmux video");
        return;
      }

      // write the file
      IDBStoreAsync(db, dst, bytes->data(), bytes->size(),
                    // onSuccess
                    [=]() {
                      sendResponse(reqId);
                      delete bytes;
                    },
                    // onError
                    [=]() {
                      sendError(reqId, "Failed to write file");
                      delete bytes;
                    });
    }
    else
      sendError(reqId, "Failed to read video file");

    if (ic)
      FreeInputFormatContext(ic);
  };

  ///
  auto onError = [=]()
  {
    sendError(reqId, "Failed to load file");
  };

  IDBLoadAsync(db, src, onSuccess, onError);
}