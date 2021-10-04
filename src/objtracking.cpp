#include "videoutils.h"
#include <functional>

#include "objtracking/VSTVideoTracker.hpp"

using vst::TrackerResult;
using vst::VSTVideoTracker;
using OpStatus = vst::TrackerResult::OpStatus;


#ifdef __EMSCRIPTEN__
#define sendError(reqId,errmsg) do { \
    EM_ASM({ \
      self.sendError($0,{ \
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


namespace
{
  void sendResponse(int id)
  {
#ifdef __EMSCRIPTEN__
    EM_ASM({ self.sendResult($0) }, id);
#else
    printf("[***] Success (id=%d)\n", id);
#endif
  }
  void sendTrackingContextResponse(int id, int trackingCtxId)
  {
#ifdef __EMSCRIPTEN__
      EM_ASM({
        self.sendResult($0, $1);
      },
        id, trackingCtxId
      );
#else
    printf("[***] sendTrackingContextResponse (id=%d, trackingCtxId=%d)\n", id, trackingCtxId);
#endif
  }

  void sendTrackObjectResponse(int id, double x, double y, double timeStamp)
  {
#ifdef __EMSCRIPTEN__
      EM_ASM({
        self.sendResult($0, {
          x: $1,
          y: $2,
          timeStamp: $3
        });
      },
        id, x, y, timeStamp
      );
#else
    printf("[***] sendTrackObjectResponse (id=%d, %f,%f timeStamp=%f)\n", id, x, y, timeStamp);
#endif
  }
}




// only one context is currently supported.
static int _nextId = 1;
static VSTVideoTracker *__tracker = nullptr;

static int CreateTrackingContext(double x, double y, double radius)
{
  int trackingCtxId = _nextId++;

  int w = radius * 2;
  int h = radius * 2;
  auto templateLoc = cv::Rect(x-radius,y-radius,w,h);
  int subtractionPeriod = 1;
  __tracker = new VSTVideoTracker(templateLoc, subtractionPeriod);

  return trackingCtxId;
}

static VSTVideoTracker* LookupTracker(int trackingCtxId)
{
  return __tracker;
}

static bool DestroyTrackingContext(int trackingCtxId)
{
  bool found = false;
  if (__tracker)
  {
    delete __tracker;
    __tracker = nullptr;
    found = true;
  }
  return found;
}

void createTrackingContext(int reqId, double x, double y, double radius)
{
  int trackingCtxId = CreateTrackingContext(x,y,radius);
  sendTrackingContextResponse(reqId, trackingCtxId);
}

void destroyTrackingContext(int reqId, int trackingCtxId)
{
  if (DestroyTrackingContext(trackingCtxId))
    sendResponse(reqId);
  else
    sendError(reqId, "Failed to destroy tracking context");
}

void trackObjectNextFrame(int reqId, int trackingCtxId, double timeStamp, int width, int height, uint32_t pbuf)
{
  auto *tracker = LookupTracker(trackingCtxId);
  if (!tracker) {
    sendError(reqId, "Invalid Tracking Context");
    return;
  }

  auto buf = reinterpret_cast<uint8_t*>(pbuf); // WASM32

  cv::Mat frame(height, width, CV_8UC4, (void*)buf);
  auto result = tracker->TrackObjectInFrame(frame, timeStamp);

  if (result.Status() == TrackerResult::success)
  {
    double timeStamp = result.TimeStamp();
    cv::Point point = result.ObjectCenter();
    sendTrackObjectResponse(reqId, point.x, point.y, timeStamp);
  }
  else
  {
    switch(result.Status()) {
      case TrackerResult::suspicionFailure: sendError(reqId, "Suspicion Engine Failure"); break;
      case TrackerResult::openCVError:      sendError(reqId, "OpenCV Error"); break;
      case TrackerResult::otherFailure:     sendError(reqId, "Other Failure"); break;
      default:
        sendError(reqId, "unknown");
    }
  }

  free(buf); // allocated in js code
}
