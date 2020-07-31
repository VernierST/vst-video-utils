#ifndef __VST_FFMPEG_UTILS_H__
#define __VST_FFMPEG_UTILS_H__

extern "C" {
#include <libavformat/avformat.h>
}

#include <vector>
#include <string>

struct VideoMetaData
{
  double avgFrameRate = 0;
  double realFrameRate = 0; // may be 0
  int numFrames = 0; // may be 0
  double duration = 0;
  int rotation = 0; // rotation angle in degrees
  int vidWidth = 0;
  int vidHeight = 0;
  std::string vidCodec;
};

// safe to call more than once
void InitFFmpegUtils();

// result must be freed with: FreeInputFormatContext()
// may return: NULL
AVFormatContext* CreateInputFormatContext(const uint8_t *buf, int size, int &outErrCode);
void FreeInputFormatContext(AVFormatContext *ic);

bool GetVideoMetaData(AVFormatContext *cxt, VideoMetaData &meta);

// If the given video contains rotation metadata, this function will bake that rotation into the
// resulting video and remove the rotation metadata.  Safari and Firefox have issues with video
// that contain rotation metadata, so this functionality is here to work around that.
bool TranscodeRotation(AVFormatContext *ic,
                       const std::string &filename, // filename extension used to determine output container type
                       std::vector<uint8_t> &outBytes,
                       int &outErrCode);

// transmux the given file and strip out metadata
bool TransmuxStripMeta(AVFormatContext *ic,
                       const std::string &filename, // filename extension used to determine output container type
                       std::vector<uint8_t> &outBytes,
                       int &outErrCode);



//////////////////////
// Internal Helpers //
//////////////////////

// the returned stream is owned by the AVFormatContext
AVStream* GetFirstStreamForType(AVFormatContext *cxt, AVMediaType type);

// buf must exist through this object's lifetime
AVIOContext* CreateIOReadContext(const uint8_t *buf, int size, int &outErrCode);
void FreeIOReadContext(AVIOContext *io);

AVIOContext* CreateIOWriteContext(std::vector<uint8_t> &outBytes, int &outErrCode);
void FreeIOWriteContext(AVIOContext *io);


#endif
