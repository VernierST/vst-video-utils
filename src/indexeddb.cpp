#include "indexeddb.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#else

#include <cstdio>
#include <vector>
namespace
{
  bool LoadFileBytes(const std::string &filename, std::vector<uint8_t> &bytes)
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
      success = total == bytes.size();
      fclose(f);
    }
    return success;
  }
}
#endif


void IDBLoadAsync(const std::string &db,
                  const std::string &filename,
                  IDBLoadFunc onSuccess,
                  IDBErrorFunc onError)
{
  struct IDBLoadContext
  {
    IDBLoadFunc onSuccess;
    IDBErrorFunc onError;
  };

  static auto onSuccessCB = [](void *userdata, void *buf, int size)
  {
    auto *cxt = (IDBLoadContext*)userdata;
    cxt->onSuccess(static_cast<const uint8_t*>(buf), static_cast<size_t>(size));
    delete cxt;
  };

  static auto onErrorCB = [](void *userdata)
  {
    auto *cxt = (IDBLoadContext*)userdata;
    cxt->onError();
    delete cxt;
  };

  auto *cxt = new IDBLoadContext { onSuccess, onError };
#ifdef __EMSCRIPTEN__
  emscripten_idb_async_load (db.c_str(), filename.c_str(), (void*)cxt, onSuccessCB, onErrorCB);
#else
  {
    std::vector<uint8_t> bytes;
    if (LoadFileBytes(filename, bytes))
      onSuccessCB(cxt, (void*)bytes.data(), bytes.size());
    else
      onErrorCB(cxt);
  }
#endif
}


void IDBStoreAsync(const std::string &db,
                   const std::string &filename,
                   const uint8_t *buf,
                   size_t size,
                   IDBStoreFunc onSuccess,
                   IDBErrorFunc onError)
{
  struct IDBStoreContext
  {
    IDBStoreFunc onSuccess;
    IDBErrorFunc onError;
  };

  static auto onSuccessCB = [](void *userdata)
  {
    auto *cxt = (IDBStoreContext*)userdata;
    cxt->onSuccess();
    delete cxt;
  };

  static auto onErrorCB = [](void *userdata)
  {
    auto *cxt = (IDBStoreContext*)userdata;
    cxt->onError();
    delete cxt;
  };

  auto *cxt = new IDBStoreContext { onSuccess, onError };
#ifdef __EMSCRIPTEN__
  emscripten_idb_async_store(db.c_str(), filename.c_str(), (void*)buf, (int)size, (void*)cxt, onSuccessCB, onErrorCB);
#else
  {
    FILE *f = fopen(filename.c_str(), "wb");
    if (f) {
      fwrite(buf, 1, size, f);
      fclose(f);
      onSuccessCB(cxt);
    }
    else
      onErrorCB(cxt);
  }
#endif
}

