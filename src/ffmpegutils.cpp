#include "ffmpegutils.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cctype>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavfilter/version.h>
#include <libavutil/version.h>
}

#define VERBOSE_LOGGING if(0)


namespace
{
  bool CheckFileExtension(std::string name, std::string extension)
  {
    std::transform(name.begin(), name.end(), name.begin(), &std::tolower);
    std::transform(extension.begin(), extension.end(), extension.begin(), &std::tolower);

    if (extension.size() > extension.size()) return false;
    return std::equal(extension.rbegin(), extension.rend(), name.rbegin());
  }

  //////////////////////////////
  struct ReadContext
  {
    ReadContext(const uint8_t *base, int64_t size)
      : base(base), ptr(base), size(size), totalSize(size) {}

    ~ReadContext() {
      VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "ReadContext deleted\n");
    }

    const uint8_t * const base;
    const uint8_t *ptr;
    int64_t size; ///< size left in the buffer
    const int64_t totalSize;

    int Read(uint8_t *buf, int bufSize)
    {
      bufSize = FFMIN(bufSize, size);

      // copy internal buffer data to buf
      std::memcpy(buf, ptr, bufSize);
      ptr  += bufSize;
      size -= bufSize;

      VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "\t read: size=%lld (%lld)\n", size, totalSize - size);

      return bufSize;
    }

    int64_t Seek(int64_t offset, int whence)
    {
      if (whence & AVSEEK_SIZE) {
        return totalSize;
      }

      switch (whence)
      {
        case SEEK_SET:
        ptr = base + offset;
        size = totalSize - offset;
        break;

        case SEEK_CUR:
        ptr += offset;
        size -= offset;
        break;

        case SEEK_END:
        // verify
        ptr = base + size + offset;
        size = -offset;
        break;

        default:
          av_log(NULL, AV_LOG_DEBUG, "ReadContext::Seek: Unhandled whence=%d\n", whence);
      }

      VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "\t seek: size=%lld (%lld)\n", size, totalSize - size);

      // TODO: bounds check


      // do we return the current pos
      return totalSize - size; // return current offset (?)
    }
  };

  int ReadPacket(void *opaque, uint8_t *buf, int bufSize)
  {
    VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "   <> READ: (buf_size=%d)\n", bufSize);
    auto *bd = (ReadContext*)opaque;
    return bd->Read(buf, bufSize);
  }


  int64_t Seek(void *opaque, int64_t offset, int whence)
  {
    VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "   <> SEEK: offset=%lld, whence=%d\n", offset, whence);
    auto *bd = (ReadContext*)opaque;
    return bd->Seek(offset, whence);
  }


  //////////////////////////////
  struct WriteContext
  {
    WriteContext(std::vector<uint8_t> &bytes) : bytes(bytes) {}
    ~WriteContext() {
      VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "WriteContext destroyed.\n");
    }

    int Write(uint8_t *buf, int buf_size)
    {
      if (pos == bytes.size()) // append bytes
      {
        bytes.insert(bytes.end(), buf, buf + buf_size);
        pos = bytes.size();
      }
      else if (pos < bytes.size()) // overwrite bytes
      {
#if 0
        // remove current bytes
        {
          auto start = bytes.begin() + pos;
          auto end = start + buf_size;
          bytes.erase(start, end);
        }

        // insert new bytes
        bytes.insert(bytes.begin()+pos, buf, buf + buf_size);
#else
        // TODO: make sure we don't overrun the buffer
        auto *ptr = bytes.data()+pos;
        std::memcpy(ptr, buf, buf_size);
#endif

        pos += buf_size;
      }
      else {
        av_log(NULL, AV_LOG_ERROR, "INVALID Write Position: %d, size=%d\n", (int)pos, (int)bytes.size());
      }

      return buf_size;
    }

    int64_t Seek(int64_t offset, int whence)
    {
      if (whence & AVSEEK_SIZE) {
        return bytes.size();
      }

      switch (whence)
      {
        case SEEK_SET:
          pos = offset;
          break;

        case SEEK_CUR:
          // verify
          pos += offset;
          break;

        case SEEK_END:
          // verify
          pos = bytes.size() - offset;
          break;

        default:
          av_log(NULL, AV_LOG_DEBUG, "WriteContext::Seek: Unhandled whence=%d\n", whence);
      }

      return pos;
    }

    std::vector<uint8_t> &bytes;
    int64_t pos = 0;
  };

  static int WritePacket(void *opaque, uint8_t *buf, int buf_size)
  {
    VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "   <> WRITE: buf=%p, size=%d\n", buf, buf_size);
    auto *cxt = (WriteContext*)opaque;
    return cxt->Write(buf, buf_size);
  }

  int64_t SeekWritePacket(void *opaque, int64_t offset, int whence)
  {
    VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "   <> WRITE_SEEK: offset=%lld, whence=%d\n", offset, whence);
    auto *cxt = (WriteContext*)opaque;
    return cxt->Seek(offset, whence);

  }

}


void InitFFmpegUtils()
{
  static bool init = false;

  if (init) return;
  // This gives me a deprecation warning when compiling against FFmpeg 4.4.2
  // However, things break if I don't use it.
  // TODO: Is there a newer alternative to call instead?
  av_register_all();
  avfilter_register_all();

#if 0
  const auto versionStr = [](unsigned int version) -> std::string {
    std::string str;
    str += std::to_string(AV_VERSION_MAJOR(version)) + ".";
    str += std::to_string(AV_VERSION_MINOR(version)) + ".";
    str += std::to_string(AV_VERSION_MICRO(version));
    return str;
  };

  av_log(NULL, AV_LOG_DEBUG, "=== FFmpeg ===\n");
  av_log(NULL, AV_LOG_DEBUG, "  Version: %s\n", av_version_info());

  av_log(NULL, AV_LOG_DEBUG, "=== AVFormat: %s ===\n", LIBAVFORMAT_IDENT);
  av_log(NULL, AV_LOG_DEBUG, "  Version: %s\n",  versionStr(avformat_version()).c_str());
  av_log(NULL, AV_LOG_DEBUG, "  Configuration: %s\n", avformat_configuration());
  av_log(NULL, AV_LOG_DEBUG, "  License: %s\n", avformat_license());

  av_log(NULL, AV_LOG_DEBUG, "=== AVCodec: %s ===\n", LIBAVCODEC_IDENT);
  av_log(NULL, AV_LOG_DEBUG, "  Version: %s\n", versionStr(avcodec_version()).c_str());
  av_log(NULL, AV_LOG_DEBUG, "  Configuration: %s\n", avcodec_configuration());
  av_log(NULL, AV_LOG_DEBUG, "  License: %s\n", avcodec_license());

  av_log(NULL, AV_LOG_DEBUG, "=== AVFilter: %s ===\n", LIBAVFILTER_IDENT);
  av_log(NULL, AV_LOG_DEBUG, "  Version: %s\n",  versionStr(avfilter_version()).c_str());
  av_log(NULL, AV_LOG_DEBUG, "  Configuration: %s\n", avfilter_configuration());
  av_log(NULL, AV_LOG_DEBUG, "  License: %s\n", avfilter_license());

  av_log(NULL, AV_LOG_DEBUG, "=== AVUtil: %s ===\n", LIBAVUTIL_IDENT);
  av_log(NULL, AV_LOG_DEBUG, "  Version: %s\n",  versionStr(avutil_version()).c_str());
  av_log(NULL, AV_LOG_DEBUG, "  Configuration: %s\n", avutil_configuration());
  av_log(NULL, AV_LOG_DEBUG, "  License: %s\n", avutil_license());
#endif

  init = true;
}


AVFormatContext* CreateInputFormatContext(const uint8_t *buf, int size, int &outErrCode)
{
  bool success = false;
  AVFormatContext *fmt_ctx = NULL;
  AVIOContext *avio_ctx = NULL;
  uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
  size_t buffer_size, avio_ctx_buffer_size = 4096;
  int ret = 0;
  int errCode = 0;

  // fill opaque structure used by the AVIOContext read callback
  VERBOSE_LOGGING av_log(NULL, AV_LOG_DEBUG, "FILE_INFO: size=%d, base=%p\n", size, buf);
  if (!(fmt_ctx = avformat_alloc_context())) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  avio_ctx = CreateIOReadContext(buf, size, errCode);
  if (!avio_ctx) {
    ret = errCode;
    goto end;
  }

  fmt_ctx->pb = avio_ctx;
  ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not open input\n");
    goto end;
  }

  ret = avformat_find_stream_info(fmt_ctx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Could not find stream information\n");
    goto end;
  }

end:
  if (ret < 0)
  {
    av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
    FreeInputFormatContext(fmt_ctx);
    fmt_ctx = nullptr;
  }

  outErrCode = ret;

  return fmt_ctx;
}


void FreeInputFormatContext(AVFormatContext *ic)
{
  FreeIOReadContext(ic->pb);
  avformat_close_input(&ic);
}


AVStream* GetFirstStreamForType(AVFormatContext *cxt, AVMediaType type)
{
  // TODO: use av_find_best_stream() ?

  AVStream *stream = nullptr;

  for (int i = 0; !stream && i < cxt->nb_streams; ++i)
  {
    AVStream *st = cxt->streams[i];
    if (st->codecpar->codec_type == type)
      stream = st;
  }

  return stream;
}

bool GetVideoMetaData(AVFormatContext *cxt, VideoMetaData &meta)
{
  bool valid = false;
  auto *st = GetFirstStreamForType(cxt, AVMEDIA_TYPE_VIDEO);

  if (st)
  {
    int rotation = 0;

    AVDictionaryEntry *entry = nullptr;
    while ( ( entry = av_dict_get(st->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)) )
    {
      if (!strcmp(entry->key, "rotate")) {
        rotation = std::atoi(entry->value);
      }
    }
    // the rotation matrix can also be obtained from:
    // st->side_data[i].type == AV_PKT_DATA_DISPLAYMATRIX, av_display_rotation_get((int32_t *)sd.data)

    meta.avgFrameRate  = av_q2d(st->avg_frame_rate);
    meta.realFrameRate = av_q2d(st->r_frame_rate);
    meta.numFrames     = st->nb_frames;
    meta.duration      = (double)cxt->duration / AV_TIME_BASE;
    meta.rotation      = rotation;
    meta.vidWidth      = st->codecpar->width;
    meta.vidHeight     = st->codecpar->height;
    valid = true;
  }

  return valid;
}




AVIOContext* CreateIOReadContext(const uint8_t *buf, int size, int &outErrCode)
{
  AVIOContext *iocxt = NULL;
  uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
  size_t buffer_size, avio_ctx_buffer_size = 0x1000;
  int ret = 0;
  auto *bd = new ReadContext(buf, size); // TODO: needs to be deleted

  avio_ctx_buffer = (unsigned char*)av_malloc(avio_ctx_buffer_size);
  if (!avio_ctx_buffer) {
    ret = AVERROR(ENOMEM);
    outErrCode = ret;
    return nullptr;
  }
  iocxt = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                               0, bd, &ReadPacket, nullptr, &Seek);
  if (!iocxt) {
    ret = AVERROR(ENOMEM);
    outErrCode = ret;
    return nullptr;
  }

  return iocxt;
};




void FreeIOReadContext(AVIOContext *io)
{
  if (io) {
    if (io->opaque)
      delete reinterpret_cast<ReadContext*>(io->opaque);
    if (io->buffer)
      av_freep(&io->buffer);
    av_freep(&io);
  }
}



AVIOContext* CreateIOWriteContext(std::vector<uint8_t> &bytes, int &outErrCode)
{
  AVIOContext *iocxt = nullptr;
  uint8_t *buffer = nullptr, *avio_ctx_buffer = nullptr;
  size_t avio_ctx_buffer_size = 0x1000;
  int ret;

  auto *cxt = new WriteContext(bytes);

  avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
  if (!avio_ctx_buffer) {
    ret = AVERROR(ENOMEM);
    return nullptr;
  }
  iocxt = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                1, cxt, NULL, &WritePacket, &SeekWritePacket);
  if (!iocxt) {
    ret = AVERROR(ENOMEM);
    return nullptr;
  }

  return iocxt;
}


void FreeIOWriteContext(AVIOContext *io)
{
  if (io) {
    if (io->opaque)
      delete reinterpret_cast<WriteContext*>(io->opaque);
  }
}









































































//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


#include "ffmpegutils.h"
#include <vector>
#include <cstdio>
#include <string>

// set to 0 to use stdio for file I/O
#define USE_AVIO_WRITE 1
#define USE_AVIO_READ  1

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


struct TranscodeContext
{
  bool transmuxOnly = false;

  AVFormatContext *ifmt_ctx = nullptr;
  AVFormatContext *ofmt_ctx = nullptr;
  AVCodecContext *dec_ctx = nullptr;
  AVCodecContext *enc_ctx = nullptr;
  AVFilterContext *buffersink_ctx = nullptr;
  AVFilterContext *buffersrc_ctx = nullptr;
  AVFilterGraph *filter_graph = nullptr;

  int video_stream_index = -1; // video stream index in input file

  // mapping from input file to output file streams
  // streams that don't exist in the output file are
  // represented by -1
  std::vector<int> stream_map;

  int rotation = 0;

  std::string filter_descr = "null";
  std::string videoEncoderName = "libopenh264"; // "mpeg4";

  void setRotation(int rotation)
  {
    this->rotation = rotation;
    switch(rotation)
    {
      case 0:
        filter_descr = "null";
        break;

      case 90:
        filter_descr = "transpose=clock";
        break;

      case 180:
        filter_descr = "transpose=clock,transpose=clock";
        break;

      case 270: // untested
        filter_descr = "transpose=clock,transpose=clock,transpose=clock";
        break;

      default:
        av_log(NULL, AV_LOG_ERROR, "Unhandled rotation angle: %d\n", rotation);
        filter_descr = "null";
        this->rotation = 0;
    }
  }
};


static int setup_input_file(TranscodeContext &ctx, AVFormatContext *ic)
{
  int ret = -1;
  AVCodec *dec = nullptr;

  int errcode = 0;

  // Cleanup
  ctx.ifmt_ctx = ic;

  // select the video stream
  ret = av_find_best_stream(ctx.ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
    return ret;
  }

  ctx.video_stream_index = ret;

  // create decoding context
  ctx.dec_ctx = avcodec_alloc_context3(dec);
  if (!ctx.dec_ctx)
    return AVERROR(ENOMEM);

  avcodec_parameters_to_context(ctx.dec_ctx, ctx.ifmt_ctx->streams[ctx.video_stream_index]->codecpar);
  ctx.dec_ctx->framerate = av_guess_frame_rate(ctx.ifmt_ctx, ctx.ifmt_ctx->streams[ctx.video_stream_index], NULL);

  // init the video decoder
  if ((ret = avcodec_open2(ctx.dec_ctx, dec, NULL)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
    return ret;
  }

  return 0;
}





static int open_output_file(TranscodeContext &ctx, std::vector<uint8_t> &outBytes, const char *filename)
{
  int ret = -1;

  ctx.ofmt_ctx = NULL;


  avformat_alloc_output_context2(&ctx.ofmt_ctx, NULL, NULL, filename);
  if (!ctx.ofmt_ctx)
  {
    av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
    return AVERROR_UNKNOWN;
  }

  auto *avio_ctx = CreateIOWriteContext(outBytes, ret);
  if (!avio_ctx) {
    av_log(NULL, AV_LOG_ERROR, "EXITING EARLY: errCode=%d\n", ret);;
     return ret;
  }
  ctx.ofmt_ctx->pb = avio_ctx;

  ctx.stream_map.resize(ctx.ifmt_ctx->nb_streams, -1);

  for (int i = 0; i < ctx.ifmt_ctx->nb_streams; ++i)
  {
    AVStream *in_stream = ctx.ifmt_ctx->streams[i];

    auto const codec_type = in_stream->codecpar->codec_type;

    // we only handle video and audio streams
    if (codec_type != AVMEDIA_TYPE_VIDEO &&
        codec_type != AVMEDIA_TYPE_AUDIO) {
        continue;
    }

    ctx.stream_map[i] = ctx.ofmt_ctx->nb_streams;

    AVStream *out_stream = avformat_new_stream(ctx.ofmt_ctx, NULL);
    if (!out_stream)
    {
      av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
      return AVERROR_UNKNOWN;
    }

    if (!ctx.transmuxOnly && in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      AVCodec *encoder = avcodec_find_encoder_by_name(ctx.videoEncoderName.c_str());
      if (!encoder)
      {
        av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
        return AVERROR_INVALIDDATA;
      }

      ctx.enc_ctx = avcodec_alloc_context3(encoder);
      if (!ctx.enc_ctx)
      {
        av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
        return AVERROR(ENOMEM);
      }

      // In this example, we transcode to same properties (picture size,
      // sample rate etc.). These properties can be changed for output
      // streams easily using filters
      if (ctx.dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
      {
        if (ctx.rotation == 90 || ctx.rotation == 270)
        {
          ctx.enc_ctx->height = ctx.dec_ctx->width;
          ctx.enc_ctx->width = ctx.dec_ctx->height;
        }
        else
        {
          ctx.enc_ctx->height = ctx.dec_ctx->height;
          ctx.enc_ctx->width = ctx.dec_ctx->width;
        }

        ctx.enc_ctx->sample_aspect_ratio = ctx.dec_ctx->sample_aspect_ratio;
        // take first format from list of supported formats
        if (encoder->pix_fmts)
          ctx.enc_ctx->pix_fmt = encoder->pix_fmts[0];
        else
          ctx.enc_ctx->pix_fmt = ctx.dec_ctx->pix_fmt;

//        ctx.enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

        // video time_base can be set to whatever is handy and supported by encoder
        ctx.enc_ctx->time_base = av_inv_q(ctx.dec_ctx->framerate); // invert rational
      }

      if (ctx.ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx.enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

      // Third parameter can be used to pass settings to encoder
      AVDictionary *opts = nullptr; // TODO: does this need to be cleaned up?
      av_dict_set(&opts, "b", "2.5M", 0); // bit rate
      ret = avcodec_open2(ctx.enc_ctx, encoder, &opts);
      if (ret < 0)
      {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
        return ret;
      }

      ret = avcodec_parameters_from_context(out_stream->codecpar, ctx.enc_ctx);
      if (ret < 0)
      {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", i);
        return ret;
      }

      out_stream->time_base = ctx.enc_ctx->time_base;
    }
    else // ELSE if AVMEDIA_TYPE_AUDIO
    {
      // if this stream must be remuxed
      ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
      if (ret < 0)
      {
        av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
        return ret;
      }
      out_stream->time_base = in_stream->time_base;
    }
    // ELSE
      // DON'T CREATE THE STREAM
  }

  av_dump_format(ctx.ofmt_ctx, 0, filename, 1);

#if 0 // OLD CODE - REMOVE ME
  if (!(ctx.ofmt_ctx->oformat->flags & AVFMT_NOFILE))
  {
    ret = avio_open(&ctx.ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
      return ret;
    }
  }
#endif

  // init muxer, write output file header
  ret = avformat_write_header(ctx.ofmt_ctx, NULL);
  if (ret < 0)
  {
    av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
    return ret;
  }

  return 0;
}





static int init_filters(TranscodeContext &ctx, const char *filters_descr)
{
  char args[512];
  int ret = 0;
  const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
  const AVFilter *buffersink = avfilter_get_by_name("buffersink");

  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs  = avfilter_inout_alloc();

  AVRational time_base = ctx.ifmt_ctx->streams[ctx.video_stream_index]->time_base;

  ctx.filter_graph = avfilter_graph_alloc();
  if (!outputs || !inputs || !ctx.filter_graph) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  // buffer video source: the decoded frames from the decoder will be inserted here.
  snprintf(args, sizeof(args),
          "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
          ctx.dec_ctx->width, ctx.dec_ctx->height,
          ctx.dec_ctx->pix_fmt,
          time_base.num, time_base.den,
          ctx.dec_ctx->sample_aspect_ratio.num, ctx.dec_ctx->sample_aspect_ratio.den);
  ret = avfilter_graph_create_filter(&ctx.buffersrc_ctx, buffersrc, "in", args, NULL, ctx.filter_graph);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
    goto end;
  }

  // buffer video sink: to terminate the filter chain.
  ret = avfilter_graph_create_filter(&ctx.buffersink_ctx, buffersink, "out", NULL, NULL, ctx.filter_graph);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
    goto end;
  }

  // Set the endpoints for the filter graph. The filter_graph will
  // be linked to the graph described by filters_descr.

  // The buffer source output must be connected to the input pad of
  // the first filter described by filters_descr; since the first
  // filter input label is not specified, it is set to "in" by default.
  outputs->name       = av_strdup("in");
  outputs->filter_ctx = ctx.buffersrc_ctx;
  outputs->pad_idx    = 0;
  outputs->next       = NULL;

  // The buffer sink input must be connected to the output pad of
  // the last filter described by filters_descr; since the last
  // filter output label is not specified, it is set to "out" by default.
  inputs->name       = av_strdup("out");
  inputs->filter_ctx = ctx.buffersink_ctx;
  inputs->pad_idx    = 0;
  inputs->next       = NULL;

  if ((ret = avfilter_graph_parse_ptr(ctx.filter_graph, filters_descr, &inputs, &outputs, NULL)) < 0)
    goto end;
  if ((ret = avfilter_graph_config(ctx.filter_graph, NULL)) < 0)
    goto end;

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);
  return ret;
}


///////////////////////////////////////////////////////////////////////////
// Encode and write frame to the output file
static int encode_write_frame(TranscodeContext &ctx, AVFrame *frame, unsigned int stream_index, int *got_frame)
{
  // av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
  AVPacket enc_pkt;
  enc_pkt.data = NULL;
  enc_pkt.size = 0;
  av_init_packet(&enc_pkt);

  if (got_frame)
    *got_frame = false;

  // send the frame to the encoder
  int ret = avcodec_send_frame(ctx.enc_ctx, frame);
  av_frame_free(&frame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "avcodec_send_frame returned %d\n", ret);
    return ret;
  }

  // pull packets from the encoder and write them to the output file
  while (ret >= 0)
  {
    ret = avcodec_receive_packet(ctx.enc_ctx, &enc_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      return 0;
    }
    else if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "Error during encoding\n");
      return -1;
    }

    if (got_frame)
      *got_frame = true;

    // prepare packet for muxing
    auto in_stream = ctx.ifmt_ctx->streams[stream_index];
    auto out_stream = ctx.ofmt_ctx->streams[ctx.stream_map[stream_index]];
    enc_pkt.pts = av_rescale_q_rnd(enc_pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    enc_pkt.dts = av_rescale_q_rnd(enc_pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    enc_pkt.duration = av_rescale_q(enc_pkt.duration, in_stream->time_base, out_stream->time_base);
    enc_pkt.stream_index = ctx.stream_map[stream_index];
    enc_pkt.pos = -1;

    ret = av_interleaved_write_frame(ctx.ofmt_ctx, &enc_pkt);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "av_interleaved_write_frame returned %d\n", ret);
    }

    // ???
    // av_packet_unref(enc_pkt);
  }

  return ret;
}


// apply filter to frame, encode and write to output file
static int filter_encode_write_frame(TranscodeContext &ctx, AVFrame *frame, unsigned int stream_index)
{
  // push the decoded frame into the filtergraph
  int ret = av_buffersrc_add_frame_flags(ctx.buffersrc_ctx, frame, 0);
  if (ret < 0)
  {
    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
    return ret;
  }

  // pull filtered frames from the filtergraph
  while (1)
  {
    auto *filt_frame = av_frame_alloc(); // filtered frame
    if (!filt_frame)
    {
      ret = AVERROR(ENOMEM);
      break;
    }

    ret = av_buffersink_get_frame(ctx.buffersink_ctx, filt_frame);
    if (ret < 0)
    {
      // if no more frames for output - returns AVERROR(EAGAIN)
      // if flushed and no more frames for output - returns AVERROR_EOF
      // rewrite retcode to 0 to show it as normal procedure completion
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        ret = 0;
      av_frame_free(&filt_frame);
      break;
    }

    filt_frame->pts = filt_frame->best_effort_timestamp;
    filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
    ret = encode_write_frame(ctx, filt_frame, stream_index, NULL);
    if (ret < 0)
      break;
  }

  return ret;
}


// flush any pending frames to the output file
static int flush_encoder(TranscodeContext &ctx, unsigned int stream_index)
{
  int ret = -1;
  int got_frame = 0;

  if (!(ctx.enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
    return 0;

  while (1)
  {
    av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
    ret = encode_write_frame(ctx, NULL, stream_index, &got_frame);
    if (ret < 0)
      break;
    if (!got_frame)
      return 0;
  }

  return ret;
}

static bool Transcode(TranscodeContext &ctx,
                      AVFormatContext *ic,
                      const std::string &filename, // filename extension used to determine output container type
                      std::vector<uint8_t> &outBytes,
                      int &outErrCode)
{
  int ret = -1;

  VideoMetaData meta;

  AVPacket packet;
  AVFrame *frame = av_frame_alloc();
  AVFrame *filt_frame = av_frame_alloc();
  if (!frame || !filt_frame)
  {
    if (frame) av_frame_free(&frame);
    if (filt_frame) av_frame_free(&filt_frame);
    outErrCode = 1; //
    return false;
  }

  if ((ret = setup_input_file(ctx, ic)) < 0)
    goto end;

  (void)GetVideoMetaData(ctx.ifmt_ctx, meta);
  ctx.setRotation(meta.rotation);

  if ((ret = open_output_file(ctx, outBytes, filename.c_str())) < 0)
    goto end;

  av_log(NULL, AV_LOG_INFO, "===== INPUT FILE =====\n");
  av_dump_format(ctx.ifmt_ctx, 0, "infile", 0);

  av_log(NULL, AV_LOG_INFO, "===== OUTPUT FILE =====\n");
  av_dump_format(ctx.ofmt_ctx, 1, filename.c_str(), 1);

  if ((ret = init_filters(ctx, ctx.filter_descr.c_str())) < 0)
    goto end;

  // read all packets
  while (1)
  {
    if ((ret = av_read_frame(ctx.ifmt_ctx, &packet)) < 0) {
      if (ret != AVERROR_EOF)
        av_log(NULL, AV_LOG_ERROR, "av_read_frame returned: %d (%d)\n", ret, AVERROR_EOF);
      break;
    }

    if (!ctx.transmuxOnly && packet.stream_index == ctx.video_stream_index)
    {
      ret = avcodec_send_packet(ctx.dec_ctx, &packet);
      if (ret < 0)
      {
        av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
        break;
      }

      while (ret >= 0)
      {
        ret = avcodec_receive_frame(ctx.dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        } else if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
          goto end;
        }
        frame->pts = frame->best_effort_timestamp;

        // push the decoded frame into the filtergraph
        if (av_buffersrc_add_frame_flags(ctx.buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
        {
          av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
          break;
        }

        // pull filtered frames from the filtergraph
        while (1)
        {
          ret = av_buffersink_get_frame(ctx.buffersink_ctx, filt_frame);
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
          if (ret < 0)
            goto end;

          frame->pts = frame->best_effort_timestamp;
          ret = filter_encode_write_frame(ctx, frame, packet.stream_index);
          if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "filter_encode_write_frame returned %d\n", ret);
            break;
          }

          av_frame_unref(filt_frame);
        }

        av_frame_unref(frame);
      }

      av_packet_unref(&packet);
    }
    else if (ctx.stream_map[packet.stream_index] >= 0)
    {
      // remux this frame without reencoding
      av_packet_rescale_ts(&packet,
                          ctx.ifmt_ctx->streams[packet.stream_index]->time_base,
                          ctx.ofmt_ctx->streams[ctx.stream_map[packet.stream_index]]->time_base);

      ret = av_interleaved_write_frame(ctx.ofmt_ctx, &packet);
      if (ret < 0) {
        goto end;
      }
    }

    av_packet_unref(&packet);
  }

  // flush filters and encoders
  if (!ctx.transmuxOnly)
  {
    for (int i = 0; i < ctx.ifmt_ctx->nb_streams; ++i)
    {
      if (i == ctx.video_stream_index)
      {
        // flush filter
        if (!ctx.filter_graph)
          continue;
        ret = filter_encode_write_frame(ctx, NULL, i);
        if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
          goto end;
        }

        // flush encoder
        ret = flush_encoder(ctx, i);
        if (ret < 0) {
          av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
          goto end;
        }
      }
    }
  }

  ret = av_write_trailer(ctx.ofmt_ctx);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "av_write_trailer failed\n");
  }

end:
  outErrCode = ret;
  avfilter_graph_free(&ctx.filter_graph);
  FreeIOWriteContext(ctx.ofmt_ctx->pb);
  avformat_free_context(ctx.ofmt_ctx);
  avcodec_free_context(&ctx.dec_ctx);

  av_frame_free(&frame);
  av_frame_free(&filt_frame);
  if (ret < 0 && ret != AVERROR_EOF)
  {
    av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
    outErrCode = ret;
    return false;
  }

  return true;
}





bool TranscodeRotation(AVFormatContext *ic,
                       const std::string &filename, // filename extension used to determine output container type
                       std::vector<uint8_t> &outBytes,
                       int &outErrCode)
{
  TranscodeContext ctx;
  return Transcode(ctx, ic, filename, outBytes, outErrCode);
}



// transmux the given file and strip out metadata
bool TransmuxStripMeta(AVFormatContext *ic,
                       const std::string &filename, // filename extension used to determine output container type
                       std::vector<uint8_t> &outBytes,
                       int &outErrCode)
{
  TranscodeContext ctx;
  ctx.transmuxOnly = true;
  return Transcode(ctx, ic, filename, outBytes, outErrCode);
}
