#ifndef __VST_VIDEO_UTILS_H__
#define __VST_VIDEO_UTILS_H__

#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

void InitVideoUtils();

WASM_EXPORT void dumpMetaData     (int reqId, std::string db, std::string filename);
WASM_EXPORT void readMetaData     (int reqId, std::string db, std::string filename);
WASM_EXPORT void transcodeRotation(int reqId, std::string db, std::string src, std::string dst);
WASM_EXPORT void transmuxStripMeta(int reqId, std::string db, std::string src, std::string dst);

// objtracking.cpp
WASM_EXPORT void createTrackingContext(int reqId, double x, double y, double radius);
WASM_EXPORT void destroyTrackingContext(int reqId, int trackingCtxId);
WASM_EXPORT void trackObjectNextFrame(int reqId, int trackingCtxId, double timeStamp, int width, int height, uint32_t pbuf);

#endif
