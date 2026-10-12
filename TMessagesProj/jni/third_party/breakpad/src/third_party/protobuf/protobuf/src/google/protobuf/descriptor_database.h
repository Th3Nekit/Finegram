// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// Interface for manipulating databases of descriptors.

#ifndef GOOGLE_PROTOBUF_DESCRIPTOR_DATABASE_H__
#define GOOGLE_PROTOBUF_DESCRIPTOR_DATABASE_H__

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {

class DescriptorDatabase;
class SimpleDescriptorDatabase;
class EncodedDescriptorDatabase;
class DescriptorPoolDatabase;
class MergedDescriptorDatabase;

class LIBPROTOBUF_EXPORT DescriptorDatabase {
 public:
  inline DescriptorDatabase() {}
  virtual ~DescriptorDatabase();

  virtual bool FindFileByName(const string& filename,
                              FileDescriptorProto* output) = 0;

  virtual bool FindFileContainingSymbol(const string& symbol_name,
                                        FileDescriptorProto* output) = 0;

  virtual bool FindFileContainingExtension(const string& containing_type,
                                           int field_number,
                                           FileDescriptorProto* output) = 0;

  virtual bool FindAllExtensionNumbers(const string& extendee_type,
                                       vector<int>* output) {
    return false;
  }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DescriptorDatabase);
};

class LIBPROTOBUF_EXPORT SimpleDescriptorDatabase : public DescriptorDatabase {
 public:
  SimpleDescriptorDatabase();
  ~SimpleDescriptorDatabase();

  bool Add(const FileDescriptorProto& file);

  bool AddAndOwn(const FileDescriptorProto* file);

  bool FindFileByName(const string& filename,
                      FileDescriptorProto* output);
  bool FindFileContainingSymbol(const string& symbol_name,
                                FileDescriptorProto* output);
  bool FindFileContainingExtension(const string& containing_type,
                                   int field_number,
                                   FileDescriptorProto* output);
  bool FindAllExtensionNumbers(const string& extendee_type,
                               vector<int>* output);

 private:

  friend class EncodedDescriptorDatabase;

  template <typename Value>
  class DescriptorIndex {
   public:

    bool AddFile(const FileDescriptorProto& file,
                 Value value);
    bool AddSymbol(const string& name, Value value);
    bool AddNestedExtensions(const DescriptorProto& message_type,
                             Value value);
    bool AddExtension(const FieldDescriptorProto& field,
                      Value value);

    Value FindFile(const string& filename);
    Value FindSymbol(const string& name);
    Value FindExtension(const string& containing_type, int field_number);
    bool FindAllExtensionNumbers(const string& containing_type,
                                 vector<int>* output);

   private:
    map<string, Value> by_name_;
    map<string, Value> by_symbol_;
    map<pair<string, int>, Value> by_extension_;

    typename map<string, Value>::iterator FindLastLessOrEqual(
        const string& name);

    bool IsSubSymbol(const string& sub_symbol, const string& super_symbol);

    bool ValidateSymbolName(const string& name);
  };

  DescriptorIndex<const FileDescriptorProto*> index_;
  vector<const FileDescriptorProto*> files_to_delete_;

  bool MaybeCopy(const FileDescriptorProto* file,
                 FileDescriptorProto* output);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(SimpleDescriptorDatabase);
};

class LIBPROTOBUF_EXPORT EncodedDescriptorDatabase : public DescriptorDatabase {
 public:
  EncodedDescriptorDatabase();
  ~EncodedDescriptorDatabase();

  bool Add(const void* encoded_file_descriptor, int size);

  bool AddCopy(const void* encoded_file_descriptor, int size);

  bool FindNameOfFileContainingSymbol(const string& symbol_name,
                                      string* output);

  bool FindFileByName(const string& filename,
                      FileDescriptorProto* output);
  bool FindFileContainingSymbol(const string& symbol_name,
                                FileDescriptorProto* output);
  bool FindFileContainingExtension(const string& containing_type,
                                   int field_number,
                                   FileDescriptorProto* output);
  bool FindAllExtensionNumbers(const string& extendee_type,
                               vector<int>* output);

 private:
  SimpleDescriptorDatabase::DescriptorIndex<pair<const void*, int> > index_;
  vector<void*> files_to_delete_;

  bool MaybeParse(pair<const void*, int> encoded_file,
                  FileDescriptorProto* output);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(EncodedDescriptorDatabase);
};

class LIBPROTOBUF_EXPORT DescriptorPoolDatabase : public DescriptorDatabase {
 public:
  DescriptorPoolDatabase(const DescriptorPool& pool);
  ~DescriptorPoolDatabase();

  bool FindFileByName(const string& filename,
                      FileDescriptorProto* output);
  bool FindFileContainingSymbol(const string& symbol_name,
                                FileDescriptorProto* output);
  bool FindFileContainingExtension(const string& containing_type,
                                   int field_number,
                                   FileDescriptorProto* output);
  bool FindAllExtensionNumbers(const string& extendee_type,
                               vector<int>* output);

 private:
  const DescriptorPool& pool_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DescriptorPoolDatabase);
};

class LIBPROTOBUF_EXPORT MergedDescriptorDatabase : public DescriptorDatabase {
 public:

  MergedDescriptorDatabase(DescriptorDatabase* source1,
                           DescriptorDatabase* source2);

  MergedDescriptorDatabase(const vector<DescriptorDatabase*>& sources);
  ~MergedDescriptorDatabase();

  bool FindFileByName(const string& filename,
                      FileDescriptorProto* output);
  bool FindFileContainingSymbol(const string& symbol_name,
                                FileDescriptorProto* output);
  bool FindFileContainingExtension(const string& containing_type,
                                   int field_number,
                                   FileDescriptorProto* output);

  bool FindAllExtensionNumbers(const string& extendee_type,
                               vector<int>* output);

 private:
  vector<DescriptorDatabase*> sources_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MergedDescriptorDatabase);
};

}

}
#endif
