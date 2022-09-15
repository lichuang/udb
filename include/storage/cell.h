#pragma once

#include "common/code.h"
#include "common/types.h"

namespace udb {

enum CellType {
  InvalidCell = 0,
  InternalCell = 1,
  LeafCell = 2,
};

class Cell {
public:
  Cell();

  Cell(const Cell &) = delete;
  Cell &operator=(const Cell &) = delete;

  ~Cell();

  Code ParseFrom(const unsigned char *data);

  bool IsInvalid() const { return type_ == InvalidCell; }
  bool IsLeafPageCell() const { return type_ == LeafCell; };
  PageNo LeftChild() const { return leftChild_; }

  void Reset();
  bool IsEmpty() const { return cellSize_ == 0; }
  uint16_t KeySize() const { return keySize_; }
  const char *Key() const { return key_; }
  uint16_t PayloadSize() const { return payLoadSize_; }

private:
  uint16_t keySize_;
  uint16_t payLoadSize_; // Bytes of payload.
  char *key_;            // Pointer to the start of the key.
  char *payload_;        // Pointer to the start of the payload.
  uint16_t localSize_;   // Amount of payload held locally, not on overflow.
  uint16_t cellSize_;    // Size of the cell content on the main b-tree page.
  PageNo leftChild_;     // The left child page number(if any).
  CellType type_;
};
} // namespace udb