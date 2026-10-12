// Copyright (c) 2010 Google Inc.
// All rights reserved.
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
//
// basic_source_line_resolver.cc: BasicSourceLineResolver implementation.
//
// See basic_source_line_resolver.h and basic_source_line_resolver_types.h
// for documentation.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "processor/basic_source_line_resolver_types.h"
#include "processor/module_factory.h"

#include "processor/tokenize.h"

using std::map;
using std::vector;
using std::make_pair;

namespace google_breakpad {

#ifdef _WIN32
#define strtok_r strtok_s
#define strtoull _strtoui64
#endif

static const char *kWhitespace = " \r\n";
static const int kMaxErrorsPrinted = 5;
static const int kMaxErrorsBeforeBailing = 100;

BasicSourceLineResolver::BasicSourceLineResolver() :
    SourceLineResolverBase(new BasicModuleFactory) { }

void BasicSourceLineResolver::Module::LogParseError(
   const string &message,
   int line_number,
   int *num_errors) {
  if (++(*num_errors) <= kMaxErrorsPrinted) {
    if (line_number > 0) {
      BPLOG(ERROR) << "Line " << line_number << ": " << message;
    } else {
      BPLOG(ERROR) << message;
    }
  }
}

bool BasicSourceLineResolver::Module::LoadMapFromMemory(
    char *memory_buffer,
    size_t memory_buffer_size) {
  linked_ptr<Function> cur_func;
  int line_number = 0;
  int num_errors = 0;
  char *save_ptr;

  if (memory_buffer_size == 0) {
    return true;
  }

  size_t last_null_terminator = memory_buffer_size - 1;
  if (memory_buffer[last_null_terminator] != '\0') {
    memory_buffer[last_null_terminator] = '\0';
  }

  bool has_null_terminator_in_the_middle = false;
  while (last_null_terminator > 0 &&
         memory_buffer[last_null_terminator - 1] == '\0') {
    last_null_terminator--;
  }
  for (size_t i = 0; i < last_null_terminator; i++) {
    if (memory_buffer[i] == '\0') {
      memory_buffer[i] = '_';
      has_null_terminator_in_the_middle = true;
    }
  }
  if (has_null_terminator_in_the_middle) {
    LogParseError(
       "Null terminator is not expected in the middle of the symbol data",
       line_number,
       &num_errors);
  }

  char *buffer;
  buffer = strtok_r(memory_buffer, "\r\n", &save_ptr);

  while (buffer != NULL) {
    ++line_number;

    if (strncmp(buffer, "FILE ", 5) == 0) {
      if (!ParseFile(buffer)) {
        LogParseError("ParseFile on buffer failed", line_number, &num_errors);
      }
    } else if (strncmp(buffer, "STACK ", 6) == 0) {
      if (!ParseStackInfo(buffer)) {
        LogParseError("ParseStackInfo failed", line_number, &num_errors);
      }
    } else if (strncmp(buffer, "FUNC ", 5) == 0) {
      cur_func.reset(ParseFunction(buffer));
      if (!cur_func.get()) {
        LogParseError("ParseFunction failed", line_number, &num_errors);
      } else {

        functions_.StoreRange(cur_func->address, cur_func->size, cur_func);
      }
    } else if (strncmp(buffer, "PUBLIC ", 7) == 0) {

      cur_func.reset();

      if (!ParsePublicSymbol(buffer)) {
        LogParseError("ParsePublicSymbol failed", line_number, &num_errors);
      }
    } else if (strncmp(buffer, "MODULE ", 7) == 0) {

    } else if (strncmp(buffer, "INFO ", 5) == 0) {

    } else {
      if (!cur_func.get()) {
        LogParseError("Found source line data without a function",
                       line_number, &num_errors);
      } else {
        Line *line = ParseLine(buffer);
        if (!line) {
          LogParseError("ParseLine failed", line_number, &num_errors);
        } else {
          cur_func->lines.StoreRange(line->address, line->size,
                                     linked_ptr<Line>(line));
        }
      }
    }
    if (num_errors > kMaxErrorsBeforeBailing) {
      break;
    }
    buffer = strtok_r(NULL, "\r\n", &save_ptr);
  }
  is_corrupt_ = num_errors > 0;
  return true;
}

void BasicSourceLineResolver::Module::LookupAddress(StackFrame *frame) const {
  MemAddr address = frame->instruction - frame->module->base_address();

  linked_ptr<Function> func;
  linked_ptr<PublicSymbol> public_symbol;
  MemAddr function_base;
  MemAddr function_size;
  MemAddr public_address;
  if (functions_.RetrieveNearestRange(address, &func,
                                      &function_base, &function_size) &&
      address >= function_base && address - function_base < function_size) {
    frame->function_name = func->name;
    frame->function_base = frame->module->base_address() + function_base;

    linked_ptr<Line> line;
    MemAddr line_base;
    if (func->lines.RetrieveRange(address, &line, &line_base, NULL)) {
      FileMap::const_iterator it = files_.find(line->source_file_id);
      if (it != files_.end()) {
        frame->source_file_name = files_.find(line->source_file_id)->second;
      }
      frame->source_line = line->line;
      frame->source_line_base = frame->module->base_address() + line_base;
    }
  } else if (public_symbols_.Retrieve(address,
                                      &public_symbol, &public_address) &&
             (!func.get() || public_address > function_base)) {
    frame->function_name = public_symbol->name;
    frame->function_base = frame->module->base_address() + public_address;
  }
}

WindowsFrameInfo *BasicSourceLineResolver::Module::FindWindowsFrameInfo(
    const StackFrame *frame) const {
  MemAddr address = frame->instruction - frame->module->base_address();
  scoped_ptr<WindowsFrameInfo> result(new WindowsFrameInfo());

  linked_ptr<WindowsFrameInfo> frame_info;
  if ((windows_frame_info_[WindowsFrameInfo::STACK_INFO_FRAME_DATA]
       .RetrieveRange(address, &frame_info))
      || (windows_frame_info_[WindowsFrameInfo::STACK_INFO_FPO]
          .RetrieveRange(address, &frame_info))) {
    result->CopyFrom(*frame_info.get());
    return result.release();
  }

  linked_ptr<Function> function;
  MemAddr function_base, function_size;
  if (functions_.RetrieveNearestRange(address, &function,
                                      &function_base, &function_size) &&
      address >= function_base && address - function_base < function_size) {
    result->parameter_size = function->parameter_size;
    result->valid |= WindowsFrameInfo::VALID_PARAMETER_SIZE;
    return result.release();
  }

  linked_ptr<PublicSymbol> public_symbol;
  MemAddr public_address;
  if (public_symbols_.Retrieve(address, &public_symbol, &public_address) &&
      (!function.get() || public_address > function_base)) {
    result->parameter_size = public_symbol->parameter_size;
  }

  return NULL;
}

CFIFrameInfo *BasicSourceLineResolver::Module::FindCFIFrameInfo(
    const StackFrame *frame) const {
  MemAddr address = frame->instruction - frame->module->base_address();
  MemAddr initial_base, initial_size;
  string initial_rules;

  if (!cfi_initial_rules_.RetrieveRange(address, &initial_rules,
                                        &initial_base, &initial_size)) {
    return NULL;
  }

  scoped_ptr<CFIFrameInfo> rules(new CFIFrameInfo());
  if (!ParseCFIRuleSet(initial_rules, rules.get()))
    return NULL;

  map<MemAddr, string>::const_iterator delta =
    cfi_delta_rules_.lower_bound(initial_base);

  while (delta != cfi_delta_rules_.end() && delta->first <= address) {
    ParseCFIRuleSet(delta->second, rules.get());
    delta++;
  }

  return rules.release();
}

bool BasicSourceLineResolver::Module::ParseFile(char *file_line) {
  long index;
  char *filename;
  if (SymbolParseHelper::ParseFile(file_line, &index, &filename)) {
    files_.insert(make_pair(index, string(filename)));
    return true;
  }
  return false;
}

BasicSourceLineResolver::Function*
BasicSourceLineResolver::Module::ParseFunction(char *function_line) {
  uint64_t address;
  uint64_t size;
  long stack_param_size;
  char *name;
  if (SymbolParseHelper::ParseFunction(function_line, &address, &size,
                                       &stack_param_size, &name)) {
    return new Function(name, address, size, stack_param_size);
  }
  return NULL;
}

BasicSourceLineResolver::Line* BasicSourceLineResolver::Module::ParseLine(
    char *line_line) {
  uint64_t address;
  uint64_t size;
  long line_number;
  long source_file;

  if (SymbolParseHelper::ParseLine(line_line, &address, &size, &line_number,
                                   &source_file)) {
    return new Line(address, size, source_file, line_number);
  }
  return NULL;
}

bool BasicSourceLineResolver::Module::ParsePublicSymbol(char *public_line) {
  uint64_t address;
  long stack_param_size;
  char *name;

  if (SymbolParseHelper::ParsePublicSymbol(public_line, &address,
                                           &stack_param_size, &name)) {

    if (address == 0) {
      return true;
    }

    linked_ptr<PublicSymbol> symbol(new PublicSymbol(name, address,
                                                     stack_param_size));
    return public_symbols_.Store(address, symbol);
  }
  return false;
}

bool BasicSourceLineResolver::Module::ParseStackInfo(char *stack_info_line) {

  stack_info_line += 6;

  while (*stack_info_line == ' ')
    stack_info_line++;
  const char *platform = stack_info_line;
  while (!strchr(kWhitespace, *stack_info_line))
    stack_info_line++;
  *stack_info_line++ = '\0';

  if (strcmp(platform, "WIN") == 0) {
    int type = 0;
    uint64_t rva, code_size;
    linked_ptr<WindowsFrameInfo>
      stack_frame_info(WindowsFrameInfo::ParseFromString(stack_info_line,
                                                         type,
                                                         rva,
                                                         code_size));
    if (stack_frame_info == NULL)
      return false;

    windows_frame_info_[type].StoreRange(rva, code_size, stack_frame_info);
    return true;
  } else if (strcmp(platform, "CFI") == 0) {

    return ParseCFIFrameInfo(stack_info_line);
  } else {

    return false;
  }
}

bool BasicSourceLineResolver::Module::ParseCFIFrameInfo(
    char *stack_info_line) {
  char *cursor;

  char *init_or_address = strtok_r(stack_info_line, " \r\n", &cursor);
  if (!init_or_address)
    return false;

  if (strcmp(init_or_address, "INIT") == 0) {

    char *address_field = strtok_r(NULL, " \r\n", &cursor);
    if (!address_field) return false;

    char *size_field = strtok_r(NULL, " \r\n", &cursor);
    if (!size_field) return false;

    char *initial_rules = strtok_r(NULL, "\r\n", &cursor);
    if (!initial_rules) return false;

    MemAddr address = strtoul(address_field, NULL, 16);
    MemAddr size    = strtoul(size_field,    NULL, 16);
    cfi_initial_rules_.StoreRange(address, size, initial_rules);
    return true;
  }

  char *address_field = init_or_address;
  char *delta_rules = strtok_r(NULL, "\r\n", &cursor);
  if (!delta_rules) return false;
  MemAddr address = strtoul(address_field, NULL, 16);
  cfi_delta_rules_[address] = delta_rules;
  return true;
}

bool SymbolParseHelper::ParseFile(char *file_line, long *index,
                                  char **filename) {

  assert(strncmp(file_line, "FILE ", 5) == 0);
  file_line += 5;

  vector<char*> tokens;
  if (!Tokenize(file_line, kWhitespace, 2, &tokens)) {
    return false;
  }

  char *after_number;
  *index = strtol(tokens[0], &after_number, 10);
  if (!IsValidAfterNumber(after_number) || *index < 0 ||
      *index == std::numeric_limits<long>::max()) {
    return false;
  }

  *filename = tokens[1];
  if (!filename) {
    return false;
  }

  return true;
}

bool SymbolParseHelper::ParseFunction(char *function_line, uint64_t *address,
                                      uint64_t *size, long *stack_param_size,
                                      char **name) {

  assert(strncmp(function_line, "FUNC ", 5) == 0);
  function_line += 5;

  vector<char*> tokens;
  if (!Tokenize(function_line, kWhitespace, 4, &tokens)) {
    return false;
  }

  char *after_number;
  *address = strtoull(tokens[0], &after_number, 16);
  if (!IsValidAfterNumber(after_number) ||
      *address == std::numeric_limits<unsigned long long>::max()) {
    return false;
  }
  *size = strtoull(tokens[1], &after_number, 16);
  if (!IsValidAfterNumber(after_number) ||
      *size == std::numeric_limits<unsigned long long>::max()) {
    return false;
  }
  *stack_param_size = strtol(tokens[2], &after_number, 16);
  if (!IsValidAfterNumber(after_number) ||
      *stack_param_size == std::numeric_limits<long>::max() ||
      *stack_param_size < 0) {
    return false;
  }
  *name = tokens[3];

  return true;
}

bool SymbolParseHelper::ParseLine(char *line_line, uint64_t *address,
                                  uint64_t *size, long *line_number,
                                  long *source_file) {

  vector<char*> tokens;
  if (!Tokenize(line_line, kWhitespace, 4, &tokens)) {
    return false;
  }

  char *after_number;
  *address  = strtoull(tokens[0], &after_number, 16);
  if (!IsValidAfterNumber(after_number) ||
      *address == std::numeric_limits<unsigned long long>::max()) {
    return false;
  }
  *size = strtoull(tokens[1], &after_number, 16);
  if (!IsValidAfterNumber(after_number) ||
      *size == std::numeric_limits<unsigned long long>::max()) {
    return false;
  }
  *line_number = strtol(tokens[2], &after_number, 10);
  if (!IsValidAfterNumber(after_number) ||
      *line_number == std::numeric_limits<long>::max()) {
    return false;
  }
  *source_file = strtol(tokens[3], &after_number, 10);
  if (!IsValidAfterNumber(after_number) || *source_file < 0 ||
      *source_file == std::numeric_limits<long>::max()) {
    return false;
  }

  if (*line_number < 0) {
    return false;
  }

  return true;
}

bool SymbolParseHelper::ParsePublicSymbol(char *public_line,
                                          uint64_t *address,
                                          long *stack_param_size,
                                          char **name) {

  assert(strncmp(public_line, "PUBLIC ", 7) == 0);
  public_line += 7;

  vector<char*> tokens;
  if (!Tokenize(public_line, kWhitespace, 3, &tokens)) {
    return false;
  }

  char *after_number;
  *address = strtoull(tokens[0], &after_number, 16);
  if (!IsValidAfterNumber(after_number) ||
      *address == std::numeric_limits<unsigned long long>::max()) {
    return false;
  }
  *stack_param_size = strtol(tokens[1], &after_number, 16);
  if (!IsValidAfterNumber(after_number) ||
      *stack_param_size == std::numeric_limits<long>::max() ||
      *stack_param_size < 0) {
    return false;
  }
  *name = tokens[2];

  return true;
}

bool SymbolParseHelper::IsValidAfterNumber(char *after_number) {
  if (after_number != NULL && strchr(kWhitespace, *after_number) != NULL) {
    return true;
  }
  return false;
}

}
