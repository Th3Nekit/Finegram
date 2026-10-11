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
// This file is the public interface to the .proto file parser.

#ifndef GOOGLE_PROTOBUF_COMPILER_IMPORTER_H__
#define GOOGLE_PROTOBUF_COMPILER_IMPORTER_H__

#include <string>
#include <vector>
#include <set>
#include <utility>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/compiler/parser.h>

namespace google {
namespace protobuf {

namespace io { class ZeroCopyInputStream; }

namespace compiler {

class Importer;
class MultiFileErrorCollector;
class SourceTree;
class DiskSourceTree;

class LIBPROTOBUF_EXPORT SourceTreeDescriptorDatabase : public DescriptorDatabase {
 public:
  SourceTreeDescriptorDatabase(SourceTree* source_tree);
  ~SourceTreeDescriptorDatabase();

  void RecordErrorsTo(MultiFileErrorCollector* error_collector) {
    error_collector_ = error_collector;
  }

  DescriptorPool::ErrorCollector* GetValidationErrorCollector() {
    using_validation_error_collector_ = true;
    return &validation_error_collector_;
  }

  bool FindFileByName(const string& filename, FileDescriptorProto* output);
  bool FindFileContainingSymbol(const string& symbol_name,
                                FileDescriptorProto* output);
  bool FindFileContainingExtension(const string& containing_type,
                                   int field_number,
                                   FileDescriptorProto* output);

 private:
  class SingleFileErrorCollector;

  SourceTree* source_tree_;
  MultiFileErrorCollector* error_collector_;

  class LIBPROTOBUF_EXPORT ValidationErrorCollector : public DescriptorPool::ErrorCollector {
   public:
    ValidationErrorCollector(SourceTreeDescriptorDatabase* owner);
    ~ValidationErrorCollector();

    void AddError(const string& filename,
                  const string& element_name,
                  const Message* descriptor,
                  ErrorLocation location,
                  const string& message);

   private:
    SourceTreeDescriptorDatabase* owner_;
  };
  friend class ValidationErrorCollector;

  bool using_validation_error_collector_;
  SourceLocationTable source_locations_;
  ValidationErrorCollector validation_error_collector_;
};

class LIBPROTOBUF_EXPORT Importer {
 public:
  Importer(SourceTree* source_tree,
           MultiFileErrorCollector* error_collector);
  ~Importer();

  const FileDescriptor* Import(const string& filename);

  inline const DescriptorPool* pool() const {
    return &pool_;
  }

 private:
  SourceTreeDescriptorDatabase database_;
  DescriptorPool pool_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Importer);
};

class LIBPROTOBUF_EXPORT MultiFileErrorCollector {
 public:
  inline MultiFileErrorCollector() {}
  virtual ~MultiFileErrorCollector();

  virtual void AddError(const string& filename, int line, int column,
                        const string& message) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MultiFileErrorCollector);
};

class LIBPROTOBUF_EXPORT SourceTree {
 public:
  inline SourceTree() {}
  virtual ~SourceTree();

  virtual io::ZeroCopyInputStream* Open(const string& filename) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(SourceTree);
};

class LIBPROTOBUF_EXPORT DiskSourceTree : public SourceTree {
 public:
  DiskSourceTree();
  ~DiskSourceTree();

  void MapPath(const string& virtual_path, const string& disk_path);

  enum DiskFileToVirtualFileResult {
    SUCCESS,
    SHADOWED,
    CANNOT_OPEN,
    NO_MAPPING
  };

  DiskFileToVirtualFileResult
    DiskFileToVirtualFile(const string& disk_file,
                          string* virtual_file,
                          string* shadowing_disk_file);

  bool VirtualFileToDiskFile(const string& virtual_file, string* disk_file);

  io::ZeroCopyInputStream* Open(const string& filename);

 private:
  struct Mapping {
    string virtual_path;
    string disk_path;

    inline Mapping(const string& virtual_path, const string& disk_path)
      : virtual_path(virtual_path), disk_path(disk_path) {}
  };
  vector<Mapping> mappings_;

  io::ZeroCopyInputStream* OpenVirtualFile(const string& virtual_file,
                                           string* disk_file);

  io::ZeroCopyInputStream* OpenDiskFile(const string& filename);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DiskSourceTree);
};

}
}

}
#endif
