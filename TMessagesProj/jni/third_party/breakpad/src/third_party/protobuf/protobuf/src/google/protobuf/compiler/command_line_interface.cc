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

#include <google/protobuf/compiler/command_line_interface.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _MSC_VER
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <iostream>
#include <ctype.h>

#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.pb.h>
#include <google/protobuf/compiler/subprocess.h>
#include <google/protobuf/compiler/zip_writer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/stubs/map-util.h>
#include <google/protobuf/stubs/stl_util-inl.h>

namespace google {
namespace protobuf {
namespace compiler {

#if defined(_WIN32)
#define mkdir(name, mode) mkdir(name)
#ifndef W_OK
#define W_OK 02  // not defined by MSVC for whatever reason
#endif
#ifndef F_OK
#define F_OK 00  // not defined by MSVC for whatever reason
#endif
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#endif

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0     // If this isn't defined, the platform doesn't need it.
#endif
#endif

namespace {
#if defined(_WIN32) && !defined(__CYGWIN__)
static const char* kPathSeparator = ";";
#else
static const char* kPathSeparator = ":";
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

void SetFdToTextMode(int fd) {
#ifdef _WIN32
  if (_setmode(fd, _O_TEXT) == -1) {

    GOOGLE_LOG(WARNING) << "_setmode(" << fd << ", _O_TEXT): " << strerror(errno);
  }
#endif

}

void SetFdToBinaryMode(int fd) {
#ifdef _WIN32
  if (_setmode(fd, _O_BINARY) == -1) {

    GOOGLE_LOG(WARNING) << "_setmode(" << fd << ", _O_BINARY): " << strerror(errno);
  }
#endif

}

void AddTrailingSlash(string* path) {
  if (!path->empty() && path->at(path->size() - 1) != '/') {
    path->push_back('/');
  }
}

bool VerifyDirectoryExists(const string& path) {
  if (path.empty()) return true;

  if (access(path.c_str(), W_OK) == -1) {
    cerr << path << ": " << strerror(errno) << endl;
    return false;
  } else {
    return true;
  }
}

bool TryCreateParentDirectory(const string& prefix, const string& filename) {

  vector<string> parts;
  SplitStringUsing(filename, "/", &parts);
  string path_so_far = prefix;
  for (int i = 0; i < parts.size() - 1; i++) {
    path_so_far += parts[i];
    if (mkdir(path_so_far.c_str(), 0777) != 0) {
      if (errno != EEXIST) {
        cerr << filename << ": while trying to create directory "
             << path_so_far << ": " << strerror(errno) << endl;
        return false;
      }
    }
    path_so_far += '/';
  }

  return true;
}

}

class CommandLineInterface::ErrorPrinter : public MultiFileErrorCollector,
                                           public io::ErrorCollector {
 public:
  ErrorPrinter(ErrorFormat format, DiskSourceTree *tree = NULL)
    : format_(format), tree_(tree) {}
  ~ErrorPrinter() {}

  void AddError(const string& filename, int line, int column,
                const string& message) {

    string dfile;
    if (format_ == CommandLineInterface::ERROR_FORMAT_MSVS &&
        tree_ != NULL &&
        tree_->VirtualFileToDiskFile(filename, &dfile)) {
      cerr << dfile;
    } else {
      cerr << filename;
    }

    if (line != -1) {

      switch (format_) {
        case CommandLineInterface::ERROR_FORMAT_GCC:
          cerr << ":" << (line + 1) << ":" << (column + 1);
          break;
        case CommandLineInterface::ERROR_FORMAT_MSVS:
          cerr << "(" << (line + 1) << ") : error in column=" << (column + 1);
          break;
      }
    }

    cerr << ": " << message << endl;
  }

  void AddError(int line, int column, const string& message) {
    AddError("input", line, column, message);
  }

 private:
  const ErrorFormat format_;
  DiskSourceTree *tree_;
};

class CommandLineInterface::GeneratorContextImpl : public GeneratorContext {
 public:
  GeneratorContextImpl(const vector<const FileDescriptor*>& parsed_files);
  ~GeneratorContextImpl();

  bool WriteAllToDisk(const string& prefix);

  bool WriteAllToZip(const string& filename);

  void AddJarManifest();

  io::ZeroCopyOutputStream* Open(const string& filename);
  io::ZeroCopyOutputStream* OpenForInsert(
      const string& filename, const string& insertion_point);
  void ListParsedFiles(vector<const FileDescriptor*>* output) {
    *output = parsed_files_;
  }

 private:
  friend class MemoryOutputStream;

  map<string, string*> files_;
  const vector<const FileDescriptor*>& parsed_files_;
  bool had_error_;
};

class CommandLineInterface::MemoryOutputStream
    : public io::ZeroCopyOutputStream {
 public:
  MemoryOutputStream(GeneratorContextImpl* directory, const string& filename);
  MemoryOutputStream(GeneratorContextImpl* directory, const string& filename,
                     const string& insertion_point);
  virtual ~MemoryOutputStream();

  virtual bool Next(void** data, int* size) { return inner_->Next(data, size); }
  virtual void BackUp(int count)            {        inner_->BackUp(count);    }
  virtual int64 ByteCount() const           { return inner_->ByteCount();      }

 private:

  GeneratorContextImpl* directory_;
  string filename_;
  string insertion_point_;

  string data_;

  scoped_ptr<io::StringOutputStream> inner_;
};

CommandLineInterface::GeneratorContextImpl::GeneratorContextImpl(
    const vector<const FileDescriptor*>& parsed_files)
    : parsed_files_(parsed_files),
      had_error_(false) {
}

CommandLineInterface::GeneratorContextImpl::~GeneratorContextImpl() {
  STLDeleteValues(&files_);
}

bool CommandLineInterface::GeneratorContextImpl::WriteAllToDisk(
    const string& prefix) {
  if (had_error_) {
    return false;
  }

  if (!VerifyDirectoryExists(prefix)) {
    return false;
  }

  for (map<string, string*>::const_iterator iter = files_.begin();
       iter != files_.end(); ++iter) {
    const string& relative_filename = iter->first;
    const char* data = iter->second->data();
    int size = iter->second->size();

    if (!TryCreateParentDirectory(prefix, relative_filename)) {
      return false;
    }
    string filename = prefix + relative_filename;

    int file_descriptor;
    do {
      file_descriptor =
        open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    } while (file_descriptor < 0 && errno == EINTR);

    if (file_descriptor < 0) {
      int error = errno;
      cerr << filename << ": " << strerror(error);
      return false;
    }

    while (size > 0) {
      int write_result;
      do {
        write_result = write(file_descriptor, data, size);
      } while (write_result < 0 && errno == EINTR);

      if (write_result <= 0) {

        if (write_result < 0) {
          int error = errno;
          cerr << filename << ": write: " << strerror(error);
        } else {
          cerr << filename << ": write() returned zero?" << endl;
        }
        return false;
      }

      data += write_result;
      size -= write_result;
    }

    if (close(file_descriptor) != 0) {
      int error = errno;
      cerr << filename << ": close: " << strerror(error);
      return false;
    }
  }

  return true;
}

bool CommandLineInterface::GeneratorContextImpl::WriteAllToZip(
    const string& filename) {
  if (had_error_) {
    return false;
  }

  int file_descriptor;
  do {
    file_descriptor =
      open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  } while (file_descriptor < 0 && errno == EINTR);

  if (file_descriptor < 0) {
    int error = errno;
    cerr << filename << ": " << strerror(error);
    return false;
  }

  io::FileOutputStream stream(file_descriptor);
  ZipWriter zip_writer(&stream);

  for (map<string, string*>::const_iterator iter = files_.begin();
       iter != files_.end(); ++iter) {
    zip_writer.Write(iter->first, *iter->second);
  }

  zip_writer.WriteDirectory();

  if (stream.GetErrno() != 0) {
    cerr << filename << ": " << strerror(stream.GetErrno()) << endl;
  }

  if (!stream.Close()) {
    cerr << filename << ": " << strerror(stream.GetErrno()) << endl;
  }

  return true;
}

void CommandLineInterface::GeneratorContextImpl::AddJarManifest() {
  string** map_slot = &files_["META-INF/MANIFEST.MF"];
  if (*map_slot == NULL) {
    *map_slot = new string(
        "Manifest-Version: 1.0\n"
        "Created-By: 1.6.0 (protoc)\n"
        "\n");
  }
}

io::ZeroCopyOutputStream* CommandLineInterface::GeneratorContextImpl::Open(
    const string& filename) {
  return new MemoryOutputStream(this, filename);
}

io::ZeroCopyOutputStream*
CommandLineInterface::GeneratorContextImpl::OpenForInsert(
    const string& filename, const string& insertion_point) {
  return new MemoryOutputStream(this, filename, insertion_point);
}

CommandLineInterface::MemoryOutputStream::MemoryOutputStream(
    GeneratorContextImpl* directory, const string& filename)
    : directory_(directory),
      filename_(filename),
      inner_(new io::StringOutputStream(&data_)) {
}

CommandLineInterface::MemoryOutputStream::MemoryOutputStream(
    GeneratorContextImpl* directory, const string& filename,
    const string& insertion_point)
    : directory_(directory),
      filename_(filename),
      insertion_point_(insertion_point),
      inner_(new io::StringOutputStream(&data_)) {
}

CommandLineInterface::MemoryOutputStream::~MemoryOutputStream() {

  inner_.reset();

  string** map_slot = &directory_->files_[filename_];

  if (insertion_point_.empty()) {

    if (*map_slot != NULL) {
      cerr << filename_ << ": Tried to write the same file twice." << endl;
      directory_->had_error_ = true;
      return;
    }

    *map_slot = new string;
    (*map_slot)->swap(data_);
  } else {

    if (!data_.empty() && data_[data_.size() - 1] != '\n') {
      data_.push_back('\n');
    }

    if (*map_slot == NULL) {
      cerr << filename_ << ": Tried to insert into file that doesn't exist."
           << endl;
      directory_->had_error_ = true;
      return;
    }
    string* target = *map_slot;

    string magic_string = strings::Substitute(
        "@@protoc_insertion_point($0)", insertion_point_);
    string::size_type pos = target->find(magic_string);

    if (pos == string::npos) {
      cerr << filename_ << ": insertion point \"" << insertion_point_
           << "\" not found." << endl;
      directory_->had_error_ = true;
      return;
    }

    pos = target->find_last_of('\n', pos);
    if (pos == string::npos) {

      pos = 0;
    } else {

      ++pos;
    }

    string indent_(*target, pos, target->find_first_not_of(" \t", pos) - pos);

    if (indent_.empty()) {

      target->insert(pos, data_);
    } else {

      int indent_size = 0;
      for (int i = 0; i < data_.size(); i++) {
        if (data_[i] == '\n') indent_size += indent_.size();
      }

      target->insert(pos, data_.size() + indent_size, '\0');

      string::size_type data_pos = 0;
      char* target_ptr = string_as_array(target) + pos;
      while (data_pos < data_.size()) {

        memcpy(target_ptr, indent_.data(), indent_.size());
        target_ptr += indent_.size();

        string::size_type line_length =
            data_.find_first_of('\n', data_pos) + 1 - data_pos;
        memcpy(target_ptr, data_.data() + data_pos, line_length);
        target_ptr += line_length;
        data_pos += line_length;
      }

      GOOGLE_CHECK_EQ(target_ptr,
          string_as_array(target) + pos + data_.size() + indent_size);
    }
  }
}

CommandLineInterface::CommandLineInterface()
  : mode_(MODE_COMPILE),
    error_format_(ERROR_FORMAT_GCC),
    imports_in_descriptor_set_(false),
    disallow_services_(false),
    inputs_are_proto_path_relative_(false) {}
CommandLineInterface::~CommandLineInterface() {}

void CommandLineInterface::RegisterGenerator(const string& flag_name,
                                             CodeGenerator* generator,
                                             const string& help_text) {
  GeneratorInfo info;
  info.generator = generator;
  info.help_text = help_text;
  generators_[flag_name] = info;
}

void CommandLineInterface::AllowPlugins(const string& exe_name_prefix) {
  plugin_prefix_ = exe_name_prefix;
}

int CommandLineInterface::Run(int argc, const char* const argv[]) {
  Clear();
  if (!ParseArguments(argc, argv)) return 1;

  DiskSourceTree source_tree;
  for (int i = 0; i < proto_path_.size(); i++) {
    source_tree.MapPath(proto_path_[i].first, proto_path_[i].second);
  }

  if (!inputs_are_proto_path_relative_) {
    if (!MakeInputsBeProtoPathRelative(&source_tree)) {
      return 1;
    }
  }

  ErrorPrinter error_collector(error_format_, &source_tree);
  Importer importer(&source_tree, &error_collector);

  vector<const FileDescriptor*> parsed_files;

  for (int i = 0; i < input_files_.size(); i++) {

    const FileDescriptor* parsed_file = importer.Import(input_files_[i]);
    if (parsed_file == NULL) return 1;
    parsed_files.push_back(parsed_file);

    if (disallow_services_ && parsed_file->service_count() > 0) {
      cerr << parsed_file->name() << ": This file contains services, but "
              "--disallow_services was used." << endl;
      return 1;
    }
  }

  typedef hash_map<string, GeneratorContextImpl*> GeneratorContextMap;
  GeneratorContextMap output_directories;

  if (mode_ == MODE_COMPILE) {
    for (int i = 0; i < output_directives_.size(); i++) {
      string output_location = output_directives_[i].output_location;
      if (!HasSuffixString(output_location, ".zip") &&
          !HasSuffixString(output_location, ".jar")) {
        AddTrailingSlash(&output_location);
      }
      GeneratorContextImpl** map_slot = &output_directories[output_location];

      if (*map_slot == NULL) {

        *map_slot = new GeneratorContextImpl(parsed_files);
      }

      if (!GenerateOutput(parsed_files, output_directives_[i], *map_slot)) {
        STLDeleteValues(&output_directories);
        return 1;
      }
    }
  }

  for (GeneratorContextMap::iterator iter = output_directories.begin();
       iter != output_directories.end(); ++iter) {
    const string& location = iter->first;
    GeneratorContextImpl* directory = iter->second;
    if (HasSuffixString(location, "/")) {
      if (!directory->WriteAllToDisk(location)) {
        STLDeleteValues(&output_directories);
        return 1;
      }
    } else {
      if (HasSuffixString(location, ".jar")) {
        directory->AddJarManifest();
      }

      if (!directory->WriteAllToZip(location)) {
        STLDeleteValues(&output_directories);
        return 1;
      }
    }
  }

  STLDeleteValues(&output_directories);

  if (!descriptor_set_name_.empty()) {
    if (!WriteDescriptorSet(parsed_files)) {
      return 1;
    }
  }

  if (mode_ == MODE_ENCODE || mode_ == MODE_DECODE) {
    if (codec_type_.empty()) {

      DescriptorPool pool;
      FileDescriptorProto file;
      file.set_name("empty_message.proto");
      file.add_message_type()->set_name("EmptyMessage");
      GOOGLE_CHECK(pool.BuildFile(file) != NULL);
      codec_type_ = "EmptyMessage";
      if (!EncodeOrDecode(&pool)) {
        return 1;
      }
    } else {
      if (!EncodeOrDecode(importer.pool())) {
        return 1;
      }
    }
  }

  return 0;
}

void CommandLineInterface::Clear() {

  executable_name_.clear();
  proto_path_.clear();
  input_files_.clear();
  output_directives_.clear();
  codec_type_.clear();
  descriptor_set_name_.clear();

  mode_ = MODE_COMPILE;
  imports_in_descriptor_set_ = false;
  disallow_services_ = false;
}

bool CommandLineInterface::MakeInputsBeProtoPathRelative(
    DiskSourceTree* source_tree) {
  for (int i = 0; i < input_files_.size(); i++) {
    string virtual_file, shadowing_disk_file;
    switch (source_tree->DiskFileToVirtualFile(
        input_files_[i], &virtual_file, &shadowing_disk_file)) {
      case DiskSourceTree::SUCCESS:
        input_files_[i] = virtual_file;
        break;
      case DiskSourceTree::SHADOWED:
        cerr << input_files_[i] << ": Input is shadowed in the --proto_path "
                "by \"" << shadowing_disk_file << "\".  Either use the latter "
                "file as your input or reorder the --proto_path so that the "
                "former file's location comes first." << endl;
        return false;
      case DiskSourceTree::CANNOT_OPEN:
        cerr << input_files_[i] << ": " << strerror(errno) << endl;
        return false;
      case DiskSourceTree::NO_MAPPING:

        if (access(input_files_[i].c_str(), F_OK) < 0) {

          cerr << input_files_[i] << ": " << strerror(ENOENT) << endl;
        } else {
          cerr << input_files_[i] << ": File does not reside within any path "
                  "specified using --proto_path (or -I).  You must specify a "
                  "--proto_path which encompasses this file.  Note that the "
                  "proto_path must be an exact prefix of the .proto file "
                  "names -- protoc is too dumb to figure out when two paths "
                  "(e.g. absolute and relative) are equivalent (it's harder "
                  "than you think)." << endl;
        }
        return false;
    }
  }

  return true;
}

bool CommandLineInterface::ParseArguments(int argc, const char* const argv[]) {
  executable_name_ = argv[0];

  for (int i = 1; i < argc; i++) {
    string name, value;

    if (ParseArgument(argv[i], &name, &value)) {

      if (i + 1 == argc || argv[i+1][0] == '-') {
        cerr << "Missing value for flag: " << name << endl;
        if (name == "--decode") {
          cerr << "To decode an unknown message, use --decode_raw." << endl;
        }
        return false;
      } else {
        ++i;
        value = argv[i];
      }
    }

    if (!InterpretArgument(name, value)) return false;
  }

  if (proto_path_.empty()) {
    proto_path_.push_back(make_pair<string, string>("", "."));
  }

  bool decoding_raw = (mode_ == MODE_DECODE) && codec_type_.empty();
  if (decoding_raw && !input_files_.empty()) {
    cerr << "When using --decode_raw, no input files should be given." << endl;
    return false;
  } else if (!decoding_raw && input_files_.empty()) {
    cerr << "Missing input file." << endl;
    return false;
  }
  if (mode_ == MODE_COMPILE && output_directives_.empty() &&
      descriptor_set_name_.empty()) {
    cerr << "Missing output directives." << endl;
    return false;
  }
  if (imports_in_descriptor_set_ && descriptor_set_name_.empty()) {
    cerr << "--include_imports only makes sense when combined with "
            "--descriptor_set_out." << endl;
  }

  return true;
}

bool CommandLineInterface::ParseArgument(const char* arg,
                                         string* name, string* value) {
  bool parsed_value = false;

  if (arg[0] != '-') {

    name->clear();
    parsed_value = true;
    *value = arg;
  } else if (arg[1] == '-') {

    const char* equals_pos = strchr(arg, '=');
    if (equals_pos != NULL) {
      *name = string(arg, equals_pos - arg);
      *value = equals_pos + 1;
      parsed_value = true;
    } else {
      *name = arg;
    }
  } else {

    if (arg[1] == '\0') {

      name->clear();
      *value = arg;
      parsed_value = true;
    } else {
      *name = string(arg, 2);
      *value = arg + 2;
      parsed_value = !value->empty();
    }
  }

  if (parsed_value) {

    return false;
  }

  if (*name == "-h" || *name == "--help" ||
      *name == "--disallow_services" ||
      *name == "--include_imports" ||
      *name == "--version" ||
      *name == "--decode_raw") {

    return false;
  }

  return true;
}

bool CommandLineInterface::InterpretArgument(const string& name,
                                             const string& value) {
  if (name.empty()) {

    if (value.empty()) {
      cerr << "You seem to have passed an empty string as one of the "
              "arguments to " << executable_name_ << ".  This is actually "
              "sort of hard to do.  Congrats.  Unfortunately it is not valid "
              "input so the program is going to die now." << endl;
      return false;
    }

    input_files_.push_back(value);

  } else if (name == "-I" || name == "--proto_path") {

    vector<string> parts;
    SplitStringUsing(value, kPathSeparator, &parts);

    for (int i = 0; i < parts.size(); i++) {
      string virtual_path;
      string disk_path;

      int equals_pos = parts[i].find_first_of('=');
      if (equals_pos == string::npos) {
        virtual_path = "";
        disk_path = parts[i];
      } else {
        virtual_path = parts[i].substr(0, equals_pos);
        disk_path = parts[i].substr(equals_pos + 1);
      }

      if (disk_path.empty()) {
        cerr << "--proto_path passed empty directory name.  (Use \".\" for "
                "current directory.)" << endl;
        return false;
      }

      if (access(disk_path.c_str(), F_OK) < 0) {
        cerr << disk_path << ": warning: directory does not exist." << endl;
      }

      proto_path_.push_back(make_pair<string, string>(virtual_path, disk_path));
    }

  } else if (name == "-o" || name == "--descriptor_set_out") {
    if (!descriptor_set_name_.empty()) {
      cerr << name << " may only be passed once." << endl;
      return false;
    }
    if (value.empty()) {
      cerr << name << " requires a non-empty value." << endl;
      return false;
    }
    if (mode_ != MODE_COMPILE) {
      cerr << "Cannot use --encode or --decode and generate descriptors at the "
              "same time." << endl;
      return false;
    }
    descriptor_set_name_ = value;

  } else if (name == "--include_imports") {
    if (imports_in_descriptor_set_) {
      cerr << name << " may only be passed once." << endl;
      return false;
    }
    imports_in_descriptor_set_ = true;

  } else if (name == "-h" || name == "--help") {
    PrintHelpText();
    return false;

  } else if (name == "--version") {
    if (!version_info_.empty()) {
      cout << version_info_ << endl;
    }
    cout << "libprotoc "
         << protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION)
         << endl;
    return false;

  } else if (name == "--disallow_services") {
    disallow_services_ = true;

  } else if (name == "--encode" || name == "--decode" ||
             name == "--decode_raw") {
    if (mode_ != MODE_COMPILE) {
      cerr << "Only one of --encode and --decode can be specified." << endl;
      return false;
    }
    if (!output_directives_.empty() || !descriptor_set_name_.empty()) {
      cerr << "Cannot use " << name
           << " and generate code or descriptors at the same time." << endl;
      return false;
    }

    mode_ = (name == "--encode") ? MODE_ENCODE : MODE_DECODE;

    if (value.empty() && name != "--decode_raw") {
      cerr << "Type name for " << name << " cannot be blank." << endl;
      if (name == "--decode") {
        cerr << "To decode an unknown message, use --decode_raw." << endl;
      }
      return false;
    } else if (!value.empty() && name == "--decode_raw") {
      cerr << "--decode_raw does not take a parameter." << endl;
      return false;
    }

    codec_type_ = value;

  } else if (name == "--error_format") {
    if (value == "gcc") {
      error_format_ = ERROR_FORMAT_GCC;
    } else if (value == "msvs") {
      error_format_ = ERROR_FORMAT_MSVS;
    } else {
      cerr << "Unknown error format: " << value << endl;
      return false;
    }

  } else if (name == "--plugin") {
    if (plugin_prefix_.empty()) {
      cerr << "This compiler does not support plugins." << endl;
      return false;
    }

    string name;
    string path;

    string::size_type equals_pos = value.find_first_of('=');
    if (equals_pos == string::npos) {

      string::size_type slash_pos = value.find_last_of('/');
      if (slash_pos == string::npos) {
        name = value;
      } else {
        name = value.substr(slash_pos + 1);
      }
      path = value;
    } else {
      name = value.substr(0, equals_pos);
      path = value.substr(equals_pos + 1);
    }

    plugins_[name] = path;

  } else {

    const GeneratorInfo* generator_info = FindOrNull(generators_, name);
    if (generator_info == NULL &&
        (plugin_prefix_.empty() || !HasSuffixString(name, "_out"))) {
      cerr << "Unknown flag: " << name << endl;
      return false;
    }

    if (mode_ != MODE_COMPILE) {
      cerr << "Cannot use --encode or --decode and generate code at the "
              "same time." << endl;
      return false;
    }

    OutputDirective directive;
    directive.name = name;
    if (generator_info == NULL) {
      directive.generator = NULL;
    } else {
      directive.generator = generator_info->generator;
    }

    string::size_type colon_pos = value.find_first_of(':');
    if (colon_pos == string::npos || IsWindowsAbsolutePath(value)) {
      directive.output_location = value;
    } else {
      directive.parameter = value.substr(0, colon_pos);
      directive.output_location = value.substr(colon_pos + 1);
    }

    output_directives_.push_back(directive);
  }

  return true;
}

void CommandLineInterface::PrintHelpText() {

  cerr <<
"Usage: " << executable_name_ << " [OPTION] PROTO_FILES\n"
"Parse PROTO_FILES and generate output based on the options given:\n"
"  -IPATH, --proto_path=PATH   Specify the directory in which to search for\n"
"                              imports.  May be specified multiple times;\n"
"                              directories will be searched in order.  If not\n"
"                              given, the current working directory is used.\n"
"  --version                   Show version info and exit.\n"
"  -h, --help                  Show this text and exit.\n"
"  --encode=MESSAGE_TYPE       Read a text-format message of the given type\n"
"                              from standard input and write it in binary\n"
"                              to standard output.  The message type must\n"
"                              be defined in PROTO_FILES or their imports.\n"
"  --decode=MESSAGE_TYPE       Read a binary message of the given type from\n"
"                              standard input and write it in text format\n"
"                              to standard output.  The message type must\n"
"                              be defined in PROTO_FILES or their imports.\n"
"  --decode_raw                Read an arbitrary protocol message from\n"
"                              standard input and write the raw tag/value\n"
"                              pairs in text format to standard output.  No\n"
"                              PROTO_FILES should be given when using this\n"
"                              flag.\n"
"  -oFILE,                     Writes a FileDescriptorSet (a protocol buffer,\n"
"    --descriptor_set_out=FILE defined in descriptor.proto) containing all of\n"
"                              the input files to FILE.\n"
"  --include_imports           When using --descriptor_set_out, also include\n"
"                              all dependencies of the input files in the\n"
"                              set, so that the set is self-contained.\n"
"  --error_format=FORMAT       Set the format in which to print errors.\n"
"                              FORMAT may be 'gcc' (the default) or 'msvs'\n"
"                              (Microsoft Visual Studio format)." << endl;
  if (!plugin_prefix_.empty()) {
    cerr <<
"  --plugin=EXECUTABLE         Specifies a plugin executable to use.\n"
"                              Normally, protoc searches the PATH for\n"
"                              plugins, but you may specify additional\n"
"                              executables not in the path using this flag.\n"
"                              Additionally, EXECUTABLE may be of the form\n"
"                              NAME=PATH, in which case the given plugin name\n"
"                              is mapped to the given executable even if\n"
"                              the executable's own name differs." << endl;
  }

  for (GeneratorMap::iterator iter = generators_.begin();
       iter != generators_.end(); ++iter) {

    cerr << "  " << iter->first << "=OUT_DIR "
         << string(19 - iter->first.size(), ' ')
         << iter->second.help_text << endl;
  }
}

bool CommandLineInterface::GenerateOutput(
    const vector<const FileDescriptor*>& parsed_files,
    const OutputDirective& output_directive,
    GeneratorContext* generator_context) {

  string error;
  if (output_directive.generator == NULL) {

    GOOGLE_CHECK(HasPrefixString(output_directive.name, "--") &&
          HasSuffixString(output_directive.name, "_out"))
        << "Bad name for plugin generator: " << output_directive.name;

    string plugin_name = plugin_prefix_ + "gen-" +
        output_directive.name.substr(2, output_directive.name.size() - 6);

    if (!GeneratePluginOutput(parsed_files, plugin_name,
                              output_directive.parameter,
                              generator_context, &error)) {
      cerr << output_directive.name << ": " << error << endl;
      return false;
    }
  } else {

    for (int i = 0; i < parsed_files.size(); i++) {
      if (!output_directive.generator->Generate(
          parsed_files[i], output_directive.parameter,
          generator_context, &error)) {

        cerr << output_directive.name << ": " << parsed_files[i]->name() << ": "
             << error << endl;
        return false;
      }
    }
  }

  return true;
}

bool CommandLineInterface::GeneratePluginOutput(
    const vector<const FileDescriptor*>& parsed_files,
    const string& plugin_name,
    const string& parameter,
    GeneratorContext* generator_context,
    string* error) {
  CodeGeneratorRequest request;
  CodeGeneratorResponse response;

  if (!parameter.empty()) {
    request.set_parameter(parameter);
  }

  set<const FileDescriptor*> already_seen;
  for (int i = 0; i < parsed_files.size(); i++) {
    request.add_file_to_generate(parsed_files[i]->name());
    GetTransitiveDependencies(parsed_files[i], &already_seen,
                              request.mutable_proto_file());
  }

  Subprocess subprocess;

  if (plugins_.count(plugin_name) > 0) {
    subprocess.Start(plugins_[plugin_name], Subprocess::EXACT_NAME);
  } else {
    subprocess.Start(plugin_name, Subprocess::SEARCH_PATH);
  }

  string communicate_error;
  if (!subprocess.Communicate(request, &response, &communicate_error)) {
    *error = strings::Substitute("$0: $1", plugin_name, communicate_error);
    return false;
  }

  scoped_ptr<io::ZeroCopyOutputStream> current_output;
  for (int i = 0; i < response.file_size(); i++) {
    const CodeGeneratorResponse::File& output_file = response.file(i);

    if (!output_file.insertion_point().empty()) {

      current_output.reset();
      current_output.reset(generator_context->OpenForInsert(
          output_file.name(), output_file.insertion_point()));
    } else if (!output_file.name().empty()) {

      current_output.reset();
      current_output.reset(generator_context->Open(output_file.name()));
    } else if (current_output == NULL) {
      *error = strings::Substitute(
        "$0: First file chunk returned by plugin did not specify a file name.",
        plugin_name);
      return false;
    }

    io::CodedOutputStream writer(current_output.get());
    writer.WriteString(output_file.content());
  }

  if (!response.error().empty()) {

    *error = response.error();
    return false;
  }

  return true;
}

bool CommandLineInterface::EncodeOrDecode(const DescriptorPool* pool) {

  const Descriptor* type = pool->FindMessageTypeByName(codec_type_);
  if (type == NULL) {
    cerr << "Type not defined: " << codec_type_ << endl;
    return false;
  }

  DynamicMessageFactory dynamic_factory(pool);
  scoped_ptr<Message> message(dynamic_factory.GetPrototype(type)->New());

  if (mode_ == MODE_ENCODE) {
    SetFdToTextMode(STDIN_FILENO);
    SetFdToBinaryMode(STDOUT_FILENO);
  } else {
    SetFdToBinaryMode(STDIN_FILENO);
    SetFdToTextMode(STDOUT_FILENO);
  }

  io::FileInputStream in(STDIN_FILENO);
  io::FileOutputStream out(STDOUT_FILENO);

  if (mode_ == MODE_ENCODE) {

    ErrorPrinter error_collector(error_format_);
    TextFormat::Parser parser;
    parser.RecordErrorsTo(&error_collector);
    parser.AllowPartialMessage(true);

    if (!parser.Parse(&in, message.get())) {
      cerr << "Failed to parse input." << endl;
      return false;
    }
  } else {

    if (!message->ParsePartialFromZeroCopyStream(&in)) {
      cerr << "Failed to parse input." << endl;
      return false;
    }
  }

  if (!message->IsInitialized()) {
    cerr << "warning:  Input message is missing required fields:  "
         << message->InitializationErrorString() << endl;
  }

  if (mode_ == MODE_ENCODE) {

    if (!message->SerializePartialToZeroCopyStream(&out)) {
      cerr << "output: I/O error." << endl;
      return false;
    }
  } else {

    if (!TextFormat::Print(*message, &out)) {
      cerr << "output: I/O error." << endl;
      return false;
    }
  }

  return true;
}

bool CommandLineInterface::WriteDescriptorSet(
    const vector<const FileDescriptor*> parsed_files) {
  FileDescriptorSet file_set;

  if (imports_in_descriptor_set_) {
    set<const FileDescriptor*> already_seen;
    for (int i = 0; i < parsed_files.size(); i++) {
      GetTransitiveDependencies(
          parsed_files[i], &already_seen, file_set.mutable_file());
    }
  } else {
    for (int i = 0; i < parsed_files.size(); i++) {
      parsed_files[i]->CopyTo(file_set.add_file());
    }
  }

  int fd;
  do {
    fd = open(descriptor_set_name_.c_str(),
              O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    perror(descriptor_set_name_.c_str());
    return false;
  }

  io::FileOutputStream out(fd);
  if (!file_set.SerializeToZeroCopyStream(&out)) {
    cerr << descriptor_set_name_ << ": " << strerror(out.GetErrno()) << endl;
    out.Close();
    return false;
  }
  if (!out.Close()) {
    cerr << descriptor_set_name_ << ": " << strerror(out.GetErrno()) << endl;
    return false;
  }

  return true;
}

void CommandLineInterface::GetTransitiveDependencies(
    const FileDescriptor* file,
    set<const FileDescriptor*>* already_seen,
    RepeatedPtrField<FileDescriptorProto>* output) {
  if (!already_seen->insert(file).second) {

    return;
  }

  for (int i = 0; i < file->dependency_count(); i++) {
    GetTransitiveDependencies(file->dependency(i), already_seen, output);
  }

  file->CopyTo(output->Add());
}

}
}
}
