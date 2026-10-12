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

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <algorithm>

#include <google/protobuf/compiler/importer.h>

#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {

#ifdef _WIN32
#ifndef F_OK
#define F_OK 00  // not defined by MSVC for whatever reason
#endif
#include <ctype.h>
#endif

static bool IsWindowsAbsolutePath(const string& text) {
#if defined(_WIN32) || defined(__CYGWIN__)
  return text.size() >= 3 && text[1] == ':' &&
         isalpha(text[0]) &&
         (text[2] == '/' || text[2] == '\\') &&
         text.find_last_of(':') == 1;
#else
  return false;
#endif
}

MultiFileErrorCollector::~MultiFileErrorCollector() {}

class SourceTreeDescriptorDatabase::SingleFileErrorCollector
    : public io::ErrorCollector {
 public:
  SingleFileErrorCollector(const string& filename,
                           MultiFileErrorCollector* multi_file_error_collector)
    : filename_(filename),
      multi_file_error_collector_(multi_file_error_collector),
      had_errors_(false) {}
  ~SingleFileErrorCollector() {}

  bool had_errors() { return had_errors_; }

  void AddError(int line, int column, const string& message) {
    if (multi_file_error_collector_ != NULL) {
      multi_file_error_collector_->AddError(filename_, line, column, message);
    }
    had_errors_ = true;
  }

 private:
  string filename_;
  MultiFileErrorCollector* multi_file_error_collector_;
  bool had_errors_;
};

SourceTreeDescriptorDatabase::SourceTreeDescriptorDatabase(
    SourceTree* source_tree)
  : source_tree_(source_tree),
    error_collector_(NULL),
    using_validation_error_collector_(false),
    validation_error_collector_(this) {}

SourceTreeDescriptorDatabase::~SourceTreeDescriptorDatabase() {}

bool SourceTreeDescriptorDatabase::FindFileByName(
    const string& filename, FileDescriptorProto* output) {
  scoped_ptr<io::ZeroCopyInputStream> input(source_tree_->Open(filename));
  if (input == NULL) {
    if (error_collector_ != NULL) {
      error_collector_->AddError(filename, -1, 0, "File not found.");
    }
    return false;
  }

  SingleFileErrorCollector file_error_collector(filename, error_collector_);
  io::Tokenizer tokenizer(input.get(), &file_error_collector);

  Parser parser;
  if (error_collector_ != NULL) {
    parser.RecordErrorsTo(&file_error_collector);
  }
  if (using_validation_error_collector_) {
    parser.RecordSourceLocationsTo(&source_locations_);
  }

  output->set_name(filename);
  return parser.Parse(&tokenizer, output) &&
         !file_error_collector.had_errors();
}

bool SourceTreeDescriptorDatabase::FindFileContainingSymbol(
    const string& symbol_name, FileDescriptorProto* output) {
  return false;
}

bool SourceTreeDescriptorDatabase::FindFileContainingExtension(
    const string& containing_type, int field_number,
    FileDescriptorProto* output) {
  return false;
}

SourceTreeDescriptorDatabase::ValidationErrorCollector::
ValidationErrorCollector(SourceTreeDescriptorDatabase* owner)
  : owner_(owner) {}

SourceTreeDescriptorDatabase::ValidationErrorCollector::
~ValidationErrorCollector() {}

void SourceTreeDescriptorDatabase::ValidationErrorCollector::AddError(
    const string& filename,
    const string& element_name,
    const Message* descriptor,
    ErrorLocation location,
    const string& message) {
  if (owner_->error_collector_ == NULL) return;

  int line, column;
  owner_->source_locations_.Find(descriptor, location, &line, &column);
  owner_->error_collector_->AddError(filename, line, column, message);
}

Importer::Importer(SourceTree* source_tree,
                   MultiFileErrorCollector* error_collector)
  : database_(source_tree),
    pool_(&database_, database_.GetValidationErrorCollector()) {
  database_.RecordErrorsTo(error_collector);
}

Importer::~Importer() {}

const FileDescriptor* Importer::Import(const string& filename) {
  return pool_.FindFileByName(filename);
}

SourceTree::~SourceTree() {}

DiskSourceTree::DiskSourceTree() {}

DiskSourceTree::~DiskSourceTree() {}

static inline char LastChar(const string& str) {
  return str[str.size() - 1];
}

static string CanonicalizePath(string path) {
#ifdef _WIN32

  if (HasPrefixString(path, "\\\\")) {

    path = "\\\\" + StringReplace(path.substr(2), "\\", "/", true);
  } else {
    path = StringReplace(path, "\\", "/", true);
  }
#endif

  vector<string> parts;
  vector<string> canonical_parts;
  SplitStringUsing(path, "/", &parts);
  for (int i = 0; i < parts.size(); i++) {
    if (parts[i] == ".") {

    } else {
      canonical_parts.push_back(parts[i]);
    }
  }
  string result = JoinStrings(canonical_parts, "/");
  if (!path.empty() && path[0] == '/') {

    result = '/' + result;
  }
  if (!path.empty() && LastChar(path) == '/' &&
      !result.empty() && LastChar(result) != '/') {

    result += '/';
  }
  return result;
}

static inline bool ContainsParentReference(const string& path) {
  return path == ".." ||
         HasPrefixString(path, "../") ||
         HasSuffixString(path, "/..") ||
         path.find("/../") != string::npos;
}

static bool ApplyMapping(const string& filename,
                         const string& old_prefix,
                         const string& new_prefix,
                         string* result) {
  if (old_prefix.empty()) {

    if (ContainsParentReference(filename)) {

      return false;
    }
    if (HasPrefixString(filename, "/") ||
        IsWindowsAbsolutePath(filename)) {

      return false;
    }
    result->assign(new_prefix);
    if (!result->empty()) result->push_back('/');
    result->append(filename);
    return true;
  } else if (HasPrefixString(filename, old_prefix)) {

    if (filename.size() == old_prefix.size()) {

      *result = new_prefix;
      return true;
    } else {

      int after_prefix_start = -1;
      if (filename[old_prefix.size()] == '/') {
        after_prefix_start = old_prefix.size() + 1;
      } else if (filename[old_prefix.size() - 1] == '/') {

        after_prefix_start = old_prefix.size();
      }
      if (after_prefix_start != -1) {

        string after_prefix = filename.substr(after_prefix_start);
        if (ContainsParentReference(after_prefix)) {

          return false;
        }
        result->assign(new_prefix);
        if (!result->empty()) result->push_back('/');
        result->append(after_prefix);
        return true;
      }
    }
  }

  return false;
}

void DiskSourceTree::MapPath(const string& virtual_path,
                             const string& disk_path) {
  mappings_.push_back(Mapping(virtual_path, CanonicalizePath(disk_path)));
}

DiskSourceTree::DiskFileToVirtualFileResult
DiskSourceTree::DiskFileToVirtualFile(
    const string& disk_file,
    string* virtual_file,
    string* shadowing_disk_file) {
  int mapping_index = -1;
  string canonical_disk_file = CanonicalizePath(disk_file);

  for (int i = 0; i < mappings_.size(); i++) {

    if (ApplyMapping(canonical_disk_file, mappings_[i].disk_path,
                     mappings_[i].virtual_path, virtual_file)) {

      mapping_index = i;
      break;
    }
  }

  if (mapping_index == -1) {
    return NO_MAPPING;
  }

  for (int i = 0; i < mapping_index; i++) {
    if (ApplyMapping(*virtual_file, mappings_[i].virtual_path,
                     mappings_[i].disk_path, shadowing_disk_file)) {
      if (access(shadowing_disk_file->c_str(), F_OK) >= 0) {

        return SHADOWED;
      }
    }
  }
  shadowing_disk_file->clear();

  scoped_ptr<io::ZeroCopyInputStream> stream(OpenDiskFile(disk_file));
  if (stream == NULL) {
    return CANNOT_OPEN;
  }

  return SUCCESS;
}

bool DiskSourceTree::VirtualFileToDiskFile(const string& virtual_file,
                                           string* disk_file) {
  scoped_ptr<io::ZeroCopyInputStream> stream(OpenVirtualFile(virtual_file,
                                                             disk_file));
  return stream != NULL;
}

io::ZeroCopyInputStream* DiskSourceTree::Open(const string& filename) {
  return OpenVirtualFile(filename, NULL);
}

io::ZeroCopyInputStream* DiskSourceTree::OpenVirtualFile(
    const string& virtual_file,
    string* disk_file) {
  if (virtual_file != CanonicalizePath(virtual_file) ||
      ContainsParentReference(virtual_file)) {

    return NULL;
  }

  for (int i = 0; i < mappings_.size(); i++) {
    string temp_disk_file;
    if (ApplyMapping(virtual_file, mappings_[i].virtual_path,
                     mappings_[i].disk_path, &temp_disk_file)) {
      io::ZeroCopyInputStream* stream = OpenDiskFile(temp_disk_file);
      if (stream != NULL) {
        if (disk_file != NULL) {
          *disk_file = temp_disk_file;
        }
        return stream;
      }

      if (errno == EACCES) {

        GOOGLE_LOG(WARNING) << "Read access is denied for file: " << temp_disk_file;
        return NULL;
      }
    }
  }

  return NULL;
}

io::ZeroCopyInputStream* DiskSourceTree::OpenDiskFile(
    const string& filename) {
  int file_descriptor;
  do {
    file_descriptor = open(filename.c_str(), O_RDONLY);
  } while (file_descriptor < 0 && errno == EINTR);
  if (file_descriptor >= 0) {
    io::FileInputStream* result = new io::FileInputStream(file_descriptor);
    result->SetCloseOnDelete(true);
    return result;
  } else {
    return NULL;
  }
}

}
}
}
