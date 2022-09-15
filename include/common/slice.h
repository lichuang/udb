#pragma once

// Copyright (c) 2011 The LevelDB Authors. All rights reserved.

// Slice is a simple structure containing a pointer into some external
// storage and a size.  The user of a Slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
//
// Multiple threads can invoke const methods on a Slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Slice must use
// external synchronization.

#include "common/debug.h"
#include "common/export.h"
#include <cstring>
#include <string>

namespace udb {
class UDB_EXPORT Slice {
public:
  // Create an empty slice.
  Slice() : data_(nullptr), size_(0) {}

  // Create a slice that refers to d[0,n-1].
  Slice(const char *d, size_t n) : data_(d), size_(n) {}

  // Create a slice that refers to the contents of "s"
  Slice(const std::string &s) : data_(s.data()), size_(s.size()) {}

  // Create a slice that refers to s[0,strlen(s)-1]
  Slice(const char *s) : data_(s), size_(strlen(s)) {}

  // Intentionally copyable.
  Slice(const Slice &) = default;
  Slice &operator=(const Slice &) = default;

  // Return a pointer to the beginning of the referenced data
  const char *Data() const { return data_; }

  // Return the length (in bytes) of the referenced data
  size_t Size() const { return size_; }

  // Return true iff the length of the referenced data is zero
  bool Empty() const { return size_ == 0; }

  // Return the ith byte in the referenced data.
  // REQUIRES: n < size()
  char operator[](size_t n) const {
    Assert(n < size());
    return data_[n];
  }

  // Change this slice to refer to an empty array
  void Clear() {
    data_ = "";
    size_ = 0;
  }

  // Return a string that contains the copy of the referenced data.
  std::string String() const { return std::string(data_, size_); }

  // Three-way comparison.  Returns value:
  //   <  0 iff "*this" <  "b",
  //   == 0 iff "*this" == "b",
  //   >  0 iff "*this" >  "b"
  int Compare(const char *data, size_t len) const;

private:
  const char *data_;
  size_t size_;
};

inline bool operator==(const Slice &x, const Slice &y) {
  return ((x.Size() == y.Size()) &&
          (memcmp(x.Data(), y.Data(), x.Size()) == 0));
}

inline bool operator!=(const Slice &x, const Slice &y) { return !(x == y); }

inline bool operator>(const Slice &x, const Slice &y) {
  x.Compare(y.Data(), y.Size()) > 0;
}

inline bool operator>(const Slice &x, const Slice &y) { x.Compare(y) > 0; }

inline int Slice::Compare(const char *data, size_t len) const {
  const size_t minLen = std::min(len, size_);
  int r = memcmp(data_, data, minLen);
  if (r != 0) {
    return r;
  }

  return (size_ < len) ? -1 : 1;
}

} // namespace udb