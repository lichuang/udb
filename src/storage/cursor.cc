#include "storage/cursor.h"
#include "buffer/buffer_manager.h"
#include "buffer/mem_page.h"
#include "common/debug.h"
#include "common/string.h"
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

Code Cursor::MoveTo(BTree *tree, const Slice &key) {
  Assert(IsReseted());

  MemPage *page;
  Code code;
  PageNo childNo;

  // First initialize the cursor.
  if (tree_ && tree->Root() != tree_->Root()) {
    Reset();
  }
  tree_ = tree;
  key_ = key;
  root_ = tree->Root();

  // Second move to the root page of btree.
  code = MoveToRoot();
  if (code != kOk) {
    return code;
  }

  // Third search the key in the tree.
  while (true) {
    page = page_;

    // Search the key in the page
    code = page->Search(key, this, &childNo, &location_);
    if (code != kOk) {
      return code;
    }

    // Has reached the leaf page, break out of loop.
    if (page->IsLeaf()) {
      break;
    }

    // check if or not has reached the max depth of tree
    if (curIndex_ >= kTreeMaxDepth) {
      return SaveErrorStatus(Status(
          kCursorOverflow,
          FormatString("Cursor has overflowed when searching key {} in tree {}",
                       key.String(), tree_->Name())));
    }

    code = MoveToChild(childNo);
    if (code != kOk) {
      break;
    }
  }

  // When out of the loop:
  // 1. page_ point to the leaf page of the key(if found)
  // 2. location_ save the match information of the page.
  // 3. cellIndex_ save the key cell index of the page cell array.

  return code;
}

Code Cursor::MoveToRoot() {
  Assert(root_ != kInvalidPageNo);

  Code code;

  // Load the root page of b-tree

  if (curIndex_ >= 0) {
    // curIndex_ >= 0 means that the root has been loaded.
    page_ = pageStack_[0];
  } else {
    // else load the page from pager
    code = Pager->GetPage(root_, &page_);
    if (code != kOk) {
      return code;
    }
  }
  Assert(page_->MemPageNo() == root_);
  curIndex_ = 0;
  pageStack_[curIndex_] = page_;

  return code;
}

Code Cursor::MoveToChild(PageNo chidNo) {
  Assert(chidNo != kInvalidPageNo);

  Code code;

  code = Pager->GetPage(root_, &page_);
  if (code != kOk) {
    return code;
  }
  pageStack_[++curIndex_] = page_;
}

void Cursor::ParseCell() {}

void Cursor::Reset() {
  root_ = kInvalidPageNo;
  tree_ = nullptr;
  page_ = nullptr;
  curIndex_ = cellIndex_ = -1;
}

} // namespace udb