// -*- mode: c++ -*-

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

// dwarf2reader::CompilationUnit is a simple and direct parser for
// DWARF data, but its handler interface is not convenient to use.  In
// particular:
//
// - CompilationUnit calls Dwarf2Handler's member functions to report
//   every attribute's value, regardless of what sort of DIE it is.
//   As a result, the ProcessAttributeX functions end up looking like
//   this:
//
//     switch (parent_die_tag) {
//       case DW_TAG_x:
//         switch (attribute_name) {
//           case DW_AT_y:
//             handle attribute y of DIE type x
//           ...
//         } break;
//       ...
//     }
//
//   In C++ it's much nicer to use virtual function dispatch to find
//   the right code for a given case than to switch on the DIE tag
//   like this.
//
// - Processing different kinds of DIEs requires different sets of
//   data: lexical block DIEs have start and end addresses, but struct
//   type DIEs don't.  It would be nice to be able to have separate
//   handler classes for separate kinds of DIEs, each with the members
//   appropriate to its role, instead of having one handler class that
//   needs to hold data for every DIE type.
//
// - There should be a separate instance of the appropriate handler
//   class for each DIE, instead of a single object with tables
//   tracking all the dies in the compilation unit.
//
// - It's not convenient to take some action after all a DIE's
//   attributes have been seen, but before visiting any of its
//   children.  The only indication you have that a DIE's attribute
//   list is complete is that you get either a StartDIE or an EndDIE
//   call.
//
// - It's not convenient to make use of the tree structure of the
//   DIEs.  Skipping all the children of a given die requires
//   maintaining state and returning false from StartDIE until we get
//   an EndDIE call with the appropriate offset.
//
// This interface tries to take care of all that.  (You're shocked, I'm sure.)
//
// Using the classes here, you provide an initial handler for the root
// DIE of the compilation unit.  Each handler receives its DIE's
// attributes, and provides fresh handler objects for children of
// interest, if any.  The three classes are:
//
// - DIEHandler: the base class for your DIE-type-specific handler
//   classes.
//
// - RootDIEHandler: derived from DIEHandler, the base class for your
//   root DIE handler class.
//
// - DIEDispatcher: derived from Dwarf2Handler, an instance of this
//   invokes your DIE-type-specific handler objects.
//
// In detail:
//
// - Define handler classes specialized for the DIE types you're
//   interested in.  These handler classes must inherit from
//   DIEHandler.  Thus:
//
//     class My_DW_TAG_X_Handler: public DIEHandler { ... };
//     class My_DW_TAG_Y_Handler: public DIEHandler { ... };
//
//   DIEHandler subclasses needn't correspond exactly to single DIE
//   types, as shown here; the point is that you can have several
//   different classes appropriate to different kinds of DIEs.
//
// - In particular, define a handler class for the compilation
//   unit's root DIE, that inherits from RootDIEHandler:
//
//     class My_DW_TAG_compile_unit_Handler: public RootDIEHandler { ... };
//
//   RootDIEHandler inherits from DIEHandler, adding a few additional
//   member functions for examining the compilation unit as a whole,
//   and other quirks of rootness.
//
// - Then, create a DIEDispatcher instance, passing it an instance of
//   your root DIE handler class, and use that DIEDispatcher as the
//   dwarf2reader::CompilationUnit's handler:
//
//     My_DW_TAG_compile_unit_Handler root_die_handler(...);
//     DIEDispatcher die_dispatcher(&root_die_handler);
//     CompilationUnit reader(sections, offset, bytereader, &die_dispatcher);
//
//   Here, 'die_dispatcher' acts as a shim between 'reader' and the
//   various DIE-specific handlers you have defined.
//
// - When you call reader.Start(), die_dispatcher behaves as follows,
//   starting with your root die handler and the compilation unit's
//   root DIE:
//
//   - It calls the handler's ProcessAttributeX member functions for
//     each of the DIE's attributes.
//
//   - It calls the handler's EndAttributes member function.  This
//     should return true if any of the DIE's children should be
//     visited, in which case:
//
//     - For each of the DIE's children, die_dispatcher calls the
//       DIE's handler's FindChildHandler member function.  If that
//       returns a pointer to a DIEHandler instance, then
//       die_dispatcher uses that handler to process the child, using
//       this procedure recursively.  Alternatively, if
//       FindChildHandler returns NULL, die_dispatcher ignores that
//       child and its descendants.
//
//   - When die_dispatcher has finished processing all the DIE's
//     children, it invokes the handler's Finish() member function,
//     and destroys the handler.  (As a special case, it doesn't
//     destroy the root DIE handler.)
//
// This allows the code for handling a particular kind of DIE to be
// gathered together in a single class, makes it easy to skip all the
// children or individual children of a particular DIE, and provides
// appropriate parental context for each die.

#ifndef COMMON_DWARF_DWARF2DIEHANDLER_H__
#define COMMON_DWARF_DWARF2DIEHANDLER_H__

#include <stack>
#include <string>

#include "common/dwarf/types.h"
#include "common/dwarf/dwarf2enums.h"
#include "common/dwarf/dwarf2reader.h"
#include "common/using_std_string.h"

namespace dwarf2reader {

class DIEHandler {
 public:
  DIEHandler() { }
  virtual ~DIEHandler() { }

  virtual void ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                        enum DwarfForm form,
                                        uint64 data) { }
  virtual void ProcessAttributeSigned(enum DwarfAttribute attr,
                                      enum DwarfForm form,
                                      int64 data) { }
  virtual void ProcessAttributeReference(enum DwarfAttribute attr,
                                         enum DwarfForm form,
                                         uint64 data) { }
  virtual void ProcessAttributeBuffer(enum DwarfAttribute attr,
                                      enum DwarfForm form,
                                      const char* data,
                                      uint64 len) { }
  virtual void ProcessAttributeString(enum DwarfAttribute attr,
                                      enum DwarfForm form,
                                      const string& data) { }
  virtual void ProcessAttributeSignature(enum DwarfAttribute attr,
                                         enum DwarfForm form,
                                         uint64 signture) { }

  virtual bool EndAttributes() { return false; }

  virtual DIEHandler *FindChildHandler(uint64 offset, enum DwarfTag tag) {
    return NULL;
  }

  virtual void Finish() { };
};

class RootDIEHandler: public DIEHandler {
 public:
  RootDIEHandler() { }
  virtual ~RootDIEHandler() { }

  virtual bool StartCompilationUnit(uint64 offset, uint8 address_size,
                                    uint8 offset_size, uint64 cu_length,
                                    uint8 dwarf_version) { return true; }

  virtual bool StartRootDIE(uint64 offset, enum DwarfTag tag) { return true; }
};

class DIEDispatcher: public Dwarf2Handler {
 public:

  DIEDispatcher(RootDIEHandler *root_handler) : root_handler_(root_handler) { }

  ~DIEDispatcher();
  bool StartCompilationUnit(uint64 offset, uint8 address_size,
                            uint8 offset_size, uint64 cu_length,
                            uint8 dwarf_version);
  bool StartDIE(uint64 offset, enum DwarfTag tag);
  void ProcessAttributeUnsigned(uint64 offset,
                                enum DwarfAttribute attr,
                                enum DwarfForm form,
                                uint64 data);
  void ProcessAttributeSigned(uint64 offset,
                              enum DwarfAttribute attr,
                              enum DwarfForm form,
                              int64 data);
  void ProcessAttributeReference(uint64 offset,
                                 enum DwarfAttribute attr,
                                 enum DwarfForm form,
                                 uint64 data);
  void ProcessAttributeBuffer(uint64 offset,
                              enum DwarfAttribute attr,
                              enum DwarfForm form,
                              const char* data,
                              uint64 len);
  void ProcessAttributeString(uint64 offset,
                              enum DwarfAttribute attr,
                              enum DwarfForm form,
                              const string &data);
  void ProcessAttributeSignature(uint64 offset,
                                 enum DwarfAttribute attr,
                                 enum DwarfForm form,
                                 uint64 signature);
  void EndDIE(uint64 offset);

 private:

  struct HandlerStack {

    uint64 offset_;

    DIEHandler *handler_;

    bool reported_attributes_end_;
  };

  std::stack<HandlerStack> die_handlers_;

  RootDIEHandler *root_handler_;
};

}
#endif
