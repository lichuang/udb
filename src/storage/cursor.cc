#include "storage/cursor.h"
#include "buffer/buffer_manager.h"
#include "buffer/mem_page.h"
#include "common/debug.h"
#include "storage/btree.h"

namespace udb {
Cursor::Cursor(TxnImpl *txn) { Reset(); }

Cursor::~Cursor() {}

void Cursor::Reset() {
  tree_ = nullptr;
  root_ = kInvalidPageNo;
  location_ = Invalid;
  cellIndex_ = -1;
  curIndex_ = -1;
  page_ = nullptr;
  key_.Clear();
}

bool Cursor::IsReseted() const {
  return tree_ == nullptr && root_ == kInvalidPageNo && location_ == Invalid &&
         key_.Empty();
}

void Cursor::GetCell() {
  if (cell_.IsEmpty()) {
  }
}

Status Cursor::MoveTo(BTree *tree, const Slice &key) {
  Assert(IsReseted());

  MemPage *page;
  Status status;
  PageNo childNo;

  // First initialize the cursor.
  tree_ = tree;
  key_ = key;
  root_ = tree->Root();

  // Second move to the root page of btree.
  status = MoveToRoot();
  if (!status.Ok()) {
    return status;
  }

  // Third search the key in the tree.
  while (true) {
    page = page_;

    status = page->Search(key, this, &childNo);
    if (!status.Ok()) {
      return status;
    }

    if (page->IsLeaf()) {
      break;
    }

    status = MoveToChild(childNo);
    if (!status.Ok()) {
      break;
    }
  }

  return status;
}

Status Cursor::MoveToRoot() {
  Assert(root_ != kInvalidPageNo);

  Status status;
  MemPage *rootPage;

  // Load the root page of b-tree

  if (curIndex_ >= 0) {
    // curIndex_ >= 0 means that the root has been loaded.
    page_ = pageStack_[0];
  } else {
    // else load the page from pager
    status = Pager->GetPage(root_, &page_);
    if (!status.Ok()) {
      return status;
    }
    curIndex_ = 0;
  }
  rootPage = page_;
  Assert(rootPage->MemPageNo() == root_);

  return status;
}

void Cursor::ParseCell() {}

} // namespace udb