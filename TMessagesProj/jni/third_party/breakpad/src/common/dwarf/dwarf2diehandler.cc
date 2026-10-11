// Copyright (c) 2010 Google Inc. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// dwarf2diehandler.cc: Implement the dwarf2reader::DieDispatcher class.
// See dwarf2diehandler.h for details.

#include <assert.h>

#include <string>

#include "common/dwarf/dwarf2diehandler.h"
#include "common/using_std_string.h"

namespace dwarf2reader {

DIEDispatcher::~DIEDispatcher() {
  while (!die_handlers_.empty()) {
    HandlerStack &entry = die_handlers_.top();
    if (entry.handler_ != root_handler_)
      delete entry.handler_;
    die_handlers_.pop();
  }
}

bool DIEDispatcher::StartCompilationUnit(uint64 offset, uint8 address_size,
                                         uint8 offset_size, uint64 cu_length,
                                         uint8 dwarf_version) {
  return root_handler_->StartCompilationUnit(offset, address_size,
                                             offset_size, cu_length,
                                             dwarf_version);
}

bool DIEDispatcher::StartDIE(uint64 offset, enum DwarfTag tag) {

  HandlerStack *parent = die_handlers_.empty() ? NULL : &die_handlers_.top();

  if (parent && parent->handler_ && !parent->reported_attributes_end_) {
    parent->reported_attributes_end_ = true;
    if (!parent->handler_->EndAttributes()) {

      parent->handler_->Finish();
      if (parent->handler_ != root_handler_)
        delete parent->handler_;
      parent->handler_ = NULL;
      return false;
    }
  }

  DIEHandler *handler;
  if (parent) {
    if (parent->handler_)

      handler = parent->handler_->FindChildHandler(offset, tag);
    else

      handler = NULL;
  } else {

    if (root_handler_->StartRootDIE(offset, tag))
      handler = root_handler_;
    else
      handler = NULL;
  }

  if (handler || !parent || parent->handler_) {
    HandlerStack entry;
    entry.offset_ = offset;
    entry.handler_ = handler;
    entry.reported_attributes_end_ = false;
    die_handlers_.push(entry);
  }

  return handler != NULL;
}

void DIEDispatcher::EndDIE(uint64 offset) {
  assert(!die_handlers_.empty());
  HandlerStack *entry = &die_handlers_.top();
  if (entry->handler_) {

    assert(entry->offset_ == offset);

    if (!entry->reported_attributes_end_)
      entry->handler_->EndAttributes();
    entry->handler_->Finish();
    if (entry->handler_ != root_handler_)
      delete entry->handler_;
  } else {

    if (entry->offset_ != offset)
      return;
  }
  die_handlers_.pop();
}

void DIEDispatcher::ProcessAttributeUnsigned(uint64 offset,
                                             enum DwarfAttribute attr,
                                             enum DwarfForm form,
                                             uint64 data) {
  HandlerStack &current = die_handlers_.top();

  assert(offset == current.offset_);
  current.handler_->ProcessAttributeUnsigned(attr, form, data);
}

void DIEDispatcher::ProcessAttributeSigned(uint64 offset,
                                           enum DwarfAttribute attr,
                                           enum DwarfForm form,
                                           int64 data) {
  HandlerStack &current = die_handlers_.top();

  assert(offset == current.offset_);
  current.handler_->ProcessAttributeSigned(attr, form, data);
}

void DIEDispatcher::ProcessAttributeReference(uint64 offset,
                                              enum DwarfAttribute attr,
                                              enum DwarfForm form,
                                              uint64 data) {
  HandlerStack &current = die_handlers_.top();

  assert(offset == current.offset_);
  current.handler_->ProcessAttributeReference(attr, form, data);
}

void DIEDispatcher::ProcessAttributeBuffer(uint64 offset,
                                           enum DwarfAttribute attr,
                                           enum DwarfForm form,
                                           const char* data,
                                           uint64 len) {
  HandlerStack &current = die_handlers_.top();

  assert(offset == current.offset_);
  current.handler_->ProcessAttributeBuffer(attr, form, data, len);
}

void DIEDispatcher::ProcessAttributeString(uint64 offset,
                                           enum DwarfAttribute attr,
                                           enum DwarfForm form,
                                           const string& data) {
  HandlerStack &current = die_handlers_.top();

  assert(offset == current.offset_);
  current.handler_->ProcessAttributeString(attr, form, data);
}

void DIEDispatcher::ProcessAttributeSignature(uint64 offset,
                                              enum DwarfAttribute attr,
                                              enum DwarfForm form,
                                              uint64 signature) {
  HandlerStack &current = die_handlers_.top();

  assert(offset == current.offset_);
  current.handler_->ProcessAttributeSignature(attr, form, signature);
}

}
