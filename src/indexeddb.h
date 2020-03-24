#ifndef __VST_INDEXED_DB__
#define __VST_INDEXED_DB__

#include <string>
#include <functional>

using IDBLoadFunc  = std::function<void(const uint8_t *buf, size_t size)>; // buf is only valid during callback
using IDBStoreFunc = std::function<void()>;
using IDBErrorFunc = std::function<void()>;

void IDBLoadAsync (const std::string &db,
                   const std::string &filename,
                   IDBLoadFunc onSuccess,
                   IDBErrorFunc onError);

void IDBStoreAsync(const std::string &db,
                   const std::string &filename,
                   const uint8_t *buf,
                   size_t size,
                   IDBStoreFunc onSuccess,
                   IDBErrorFunc onError);

#endif
