#pragma once

#include "common/types.h"

namespace udb {

class Cell {
public:
  Cell();

  Cell(const Cell &) = delete;
  Cell &operator=(const Cell &) = delete;

  ~Cell();

  void Reset();
  bool IsEmpty() const { return cellSize_ == 0; }
  uint16_t KeySize() const { return keySize_; }
  uint16_t PayloadSize() const { return payLoadSize_; }

private:
  uint16_t keySize_;
  uint16_t payLoadSize_;   // Bytes of payload.
  unsigned char *payload_; // Pointer to the start of the payload.
  uint16_t localSize_;     // Amount of payload held locally, not on overflow.
  uint16_t cellSize_;      // Size of the cell content on the main b-tree page.
};
} // namespace udb