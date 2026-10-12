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
// RepeatedField and RepeatedPtrField are used by generated protocol message
// classes to manipulate repeated fields.  These classes are very similar to
// STL's vector, but include a number of optimizations found to be useful
// specifically in the case of Protocol Buffers.  RepeatedPtrField is
// particularly different from STL vector as it manages ownership of the
// pointers that it contains.
//
// Typically, clients should not need to access RepeatedField objects directly,
// but should instead use the accessor functions generated automatically by the
// protocol compiler.

#ifndef GOOGLE_PROTOBUF_REPEATED_FIELD_H__
#define GOOGLE_PROTOBUF_REPEATED_FIELD_H__

#include <string>
#include <iterator>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message_lite.h>

namespace google {

namespace protobuf {

class Message;

namespace internal {

LIBPROTOBUF_EXPORT int StringSpaceUsedExcludingSelf(const string& str);

}

template <typename Element>
class RepeatedField {
 public:
  RepeatedField();
  RepeatedField(const RepeatedField& other);
  ~RepeatedField();

  RepeatedField& operator=(const RepeatedField& other);

  int size() const;

  const Element& Get(int index) const;
  Element* Mutable(int index);
  void Set(int index, const Element& value);
  void Add(const Element& value);
  Element* Add();

  void RemoveLast();
  void Clear();
  void MergeFrom(const RepeatedField& other);
  void CopyFrom(const RepeatedField& other);

  void Reserve(int new_size);

  void Truncate(int new_size);

  void AddAlreadyReserved(const Element& value);
  Element* AddAlreadyReserved();
  int Capacity() const;

  Element* mutable_data();
  const Element* data() const;

  void Swap(RepeatedField* other);

  void SwapElements(int index1, int index2);

  typedef Element* iterator;
  typedef const Element* const_iterator;
  typedef Element value_type;

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

  int SpaceUsedExcludingSelf() const;

 private:
  static const int kInitialSize = 4;

  Element* elements_;
  int      current_size_;
  int      total_size_;

  Element  initial_space_[kInitialSize];

  void MoveArray(Element to[], Element from[], int size);

  void CopyArray(Element to[], const Element from[], int size);
};

namespace internal {
template <typename It> class RepeatedPtrIterator;
template <typename It> class RepeatedPtrOverPtrsIterator;
}

namespace internal {

class LIBPROTOBUF_EXPORT RepeatedPtrFieldBase {
 protected:

  friend class GeneratedMessageReflection;

  friend class ExtensionSet;

  RepeatedPtrFieldBase();

  template <typename TypeHandler>
  void Destroy();

  int size() const;

  template <typename TypeHandler>
  const typename TypeHandler::Type& Get(int index) const;
  template <typename TypeHandler>
  typename TypeHandler::Type* Mutable(int index);
  template <typename TypeHandler>
  typename TypeHandler::Type* Add();
  template <typename TypeHandler>
  void RemoveLast();
  template <typename TypeHandler>
  void Clear();
  template <typename TypeHandler>
  void MergeFrom(const RepeatedPtrFieldBase& other);
  template <typename TypeHandler>
  void CopyFrom(const RepeatedPtrFieldBase& other);

  void Reserve(int new_size);

  int Capacity() const;

  void* const* raw_data() const;
  void** raw_mutable_data() const;

  template <typename TypeHandler>
  typename TypeHandler::Type** mutable_data();
  template <typename TypeHandler>
  const typename TypeHandler::Type* const* data() const;

  void Swap(RepeatedPtrFieldBase* other);

  void SwapElements(int index1, int index2);

  template <typename TypeHandler>
  int SpaceUsedExcludingSelf() const;

  template <typename TypeHandler>
  typename TypeHandler::Type* AddFromCleared();

  template <typename TypeHandler>
  void AddAllocated(typename TypeHandler::Type* value);
  template <typename TypeHandler>
  typename TypeHandler::Type* ReleaseLast();

  int ClearedCount() const;
  template <typename TypeHandler>
  void AddCleared(typename TypeHandler::Type* value);
  template <typename TypeHandler>
  typename TypeHandler::Type* ReleaseCleared();

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(RepeatedPtrFieldBase);

  static const int kInitialSize = 4;

  void** elements_;
  int    current_size_;
  int    allocated_size_;
  int    total_size_;

  void*  initial_space_[kInitialSize];

  template <typename TypeHandler>
  static inline typename TypeHandler::Type* cast(void* element) {
    return reinterpret_cast<typename TypeHandler::Type*>(element);
  }
  template <typename TypeHandler>
  static inline const typename TypeHandler::Type* cast(const void* element) {
    return reinterpret_cast<const typename TypeHandler::Type*>(element);
  }
};

template <typename GenericType>
class GenericTypeHandler {
 public:
  typedef GenericType Type;
  static GenericType* New() { return new GenericType; }
  static void Delete(GenericType* value) { delete value; }
  static void Clear(GenericType* value) { value->Clear(); }
  static void Merge(const GenericType& from, GenericType* to) {
    to->MergeFrom(from);
  }
  static int SpaceUsed(const GenericType& value) { return value.SpaceUsed(); }
};

template <>
inline void GenericTypeHandler<MessageLite>::Merge(
    const MessageLite& from, MessageLite* to) {
  to->CheckTypeAndMergeFrom(from);
}

class LIBPROTOBUF_EXPORT StringTypeHandlerBase {
 public:
  typedef string Type;
  static string* New();
  static void Delete(string* value);
  static void Clear(string* value) { value->clear(); }
  static void Merge(const string& from, string* to) { *to = from; }
};

class StringTypeHandler : public StringTypeHandlerBase {
 public:
  static int SpaceUsed(const string& value)  {
    return sizeof(value) + StringSpaceUsedExcludingSelf(value);
  }
};

}

template <typename Element>
class RepeatedPtrField : public internal::RepeatedPtrFieldBase {
 public:
  RepeatedPtrField();
  RepeatedPtrField(const RepeatedPtrField& other);
  ~RepeatedPtrField();

  RepeatedPtrField& operator=(const RepeatedPtrField& other);

  int size() const;

  const Element& Get(int index) const;
  Element* Mutable(int index);
  Element* Add();
  void RemoveLast();
  void Clear();
  void MergeFrom(const RepeatedPtrField& other);
  void CopyFrom(const RepeatedPtrField& other);

  void Reserve(int new_size);

  int Capacity() const;

  Element** mutable_data();
  const Element* const* data() const;

  void Swap(RepeatedPtrField* other);

  void SwapElements(int index1, int index2);

  typedef internal::RepeatedPtrIterator<Element> iterator;
  typedef internal::RepeatedPtrIterator<const Element> const_iterator;
  typedef Element value_type;

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

  typedef internal::RepeatedPtrOverPtrsIterator<Element> pointer_iterator;
  pointer_iterator pointer_begin();
  pointer_iterator pointer_end();

  int SpaceUsedExcludingSelf() const;

  void AddAllocated(Element* value);

  Element* ReleaseLast();

  int ClearedCount() const;

  void AddCleared(Element* value);

  Element* ReleaseCleared();

 protected:

  class TypeHandler;

};

template <typename Element>
inline RepeatedField<Element>::RepeatedField()
  : elements_(initial_space_),
    current_size_(0),
    total_size_(kInitialSize) {
}

template <typename Element>
inline RepeatedField<Element>::RepeatedField(const RepeatedField& other)
  : elements_(initial_space_),
    current_size_(0),
    total_size_(kInitialSize) {
  CopyFrom(other);
}

template <typename Element>
RepeatedField<Element>::~RepeatedField() {
  if (elements_ != initial_space_) {
    delete [] elements_;
  }
}

template <typename Element>
inline RepeatedField<Element>&
RepeatedField<Element>::operator=(const RepeatedField& other) {
  CopyFrom(other);
  return *this;
}

template <typename Element>
inline int RepeatedField<Element>::size() const {
  return current_size_;
}

template <typename Element>
inline int RepeatedField<Element>::Capacity() const {
  return total_size_;
}

template<typename Element>
inline void RepeatedField<Element>::AddAlreadyReserved(const Element& value) {
  GOOGLE_DCHECK_LT(size(), Capacity());
  elements_[current_size_++] = value;
}

template<typename Element>
inline Element* RepeatedField<Element>::AddAlreadyReserved() {
  GOOGLE_DCHECK_LT(size(), Capacity());
  return &elements_[current_size_++];
}

template <typename Element>
inline const Element& RepeatedField<Element>::Get(int index) const {
  GOOGLE_DCHECK_LT(index, size());
  return elements_[index];
}

template <typename Element>
inline Element* RepeatedField<Element>::Mutable(int index) {
  GOOGLE_DCHECK_LT(index, size());
  return elements_ + index;
}

template <typename Element>
inline void RepeatedField<Element>::Set(int index, const Element& value) {
  GOOGLE_DCHECK_LT(index, size());
  elements_[index] = value;
}

template <typename Element>
inline void RepeatedField<Element>::Add(const Element& value) {
  if (current_size_ == total_size_) Reserve(total_size_ + 1);
  elements_[current_size_++] = value;
}

template <typename Element>
inline Element* RepeatedField<Element>::Add() {
  if (current_size_ == total_size_) Reserve(total_size_ + 1);
  return &elements_[current_size_++];
}

template <typename Element>
inline void RepeatedField<Element>::RemoveLast() {
  GOOGLE_DCHECK_GT(current_size_, 0);
  --current_size_;
}

template <typename Element>
inline void RepeatedField<Element>::Clear() {
  current_size_ = 0;
}

template <typename Element>
inline void RepeatedField<Element>::MergeFrom(const RepeatedField& other) {
  Reserve(current_size_ + other.current_size_);
  CopyArray(elements_ + current_size_, other.elements_, other.current_size_);
  current_size_ += other.current_size_;
}

template <typename Element>
inline void RepeatedField<Element>::CopyFrom(const RepeatedField& other) {
  Clear();
  MergeFrom(other);
}

template <typename Element>
inline Element* RepeatedField<Element>::mutable_data() {
  return elements_;
}

template <typename Element>
inline const Element* RepeatedField<Element>::data() const {
  return elements_;
}

template <typename Element>
void RepeatedField<Element>::Swap(RepeatedField* other) {
  Element* swap_elements     = elements_;
  int      swap_current_size = current_size_;
  int      swap_total_size   = total_size_;

  Element swap_initial_space[kInitialSize];
  MoveArray(swap_initial_space, initial_space_, kInitialSize);

  elements_     = other->elements_;
  current_size_ = other->current_size_;
  total_size_   = other->total_size_;
  MoveArray(initial_space_, other->initial_space_, kInitialSize);

  other->elements_     = swap_elements;
  other->current_size_ = swap_current_size;
  other->total_size_   = swap_total_size;
  MoveArray(other->initial_space_, swap_initial_space, kInitialSize);

  if (elements_ == other->initial_space_) {
    elements_ = initial_space_;
  }
  if (other->elements_ == initial_space_) {
    other->elements_ = other->initial_space_;
  }
}

template <typename Element>
void RepeatedField<Element>::SwapElements(int index1, int index2) {
  std::swap(elements_[index1], elements_[index2]);
}

template <typename Element>
inline typename RepeatedField<Element>::iterator
RepeatedField<Element>::begin() {
  return elements_;
}
template <typename Element>
inline typename RepeatedField<Element>::const_iterator
RepeatedField<Element>::begin() const {
  return elements_;
}
template <typename Element>
inline typename RepeatedField<Element>::iterator
RepeatedField<Element>::end() {
  return elements_ + current_size_;
}
template <typename Element>
inline typename RepeatedField<Element>::const_iterator
RepeatedField<Element>::end() const {
  return elements_ + current_size_;
}

template <typename Element>
inline int RepeatedField<Element>::SpaceUsedExcludingSelf() const {
  return (elements_ != initial_space_) ? total_size_ * sizeof(elements_[0]) : 0;
}

template <typename Element>
void RepeatedField<Element>::Reserve(int new_size) {
  if (total_size_ >= new_size) return;

  Element* old_elements = elements_;
  total_size_ = max(total_size_ * 2, new_size);
  elements_ = new Element[total_size_];
  MoveArray(elements_, old_elements, current_size_);
  if (old_elements != initial_space_) {
    delete [] old_elements;
  }
}

template <typename Element>
inline void RepeatedField<Element>::Truncate(int new_size) {
  GOOGLE_DCHECK_LE(new_size, current_size_);
  current_size_ = new_size;
}

template <typename Element>
inline void RepeatedField<Element>::MoveArray(
    Element to[], Element from[], int array_size) {
  memcpy(to, from, array_size * sizeof(Element));
}

template <typename Element>
inline void RepeatedField<Element>::CopyArray(
    Element to[], const Element from[], int array_size) {
  memcpy(to, from, array_size * sizeof(Element));
}

namespace internal {

inline RepeatedPtrFieldBase::RepeatedPtrFieldBase()
  : elements_(initial_space_),
    current_size_(0),
    allocated_size_(0),
    total_size_(kInitialSize) {
}

template <typename TypeHandler>
void RepeatedPtrFieldBase::Destroy() {
  for (int i = 0; i < allocated_size_; i++) {
    TypeHandler::Delete(cast<TypeHandler>(elements_[i]));
  }
  if (elements_ != initial_space_) {
    delete [] elements_;
  }
}

inline int RepeatedPtrFieldBase::size() const {
  return current_size_;
}

template <typename TypeHandler>
inline const typename TypeHandler::Type&
RepeatedPtrFieldBase::Get(int index) const {
  GOOGLE_DCHECK_LT(index, size());
  return *cast<TypeHandler>(elements_[index]);
}

template <typename TypeHandler>
inline typename TypeHandler::Type*
RepeatedPtrFieldBase::Mutable(int index) {
  GOOGLE_DCHECK_LT(index, size());
  return cast<TypeHandler>(elements_[index]);
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::Add() {
  if (current_size_ < allocated_size_) {
    return cast<TypeHandler>(elements_[current_size_++]);
  }
  if (allocated_size_ == total_size_) Reserve(total_size_ + 1);
  ++allocated_size_;
  typename TypeHandler::Type* result = TypeHandler::New();
  elements_[current_size_++] = result;
  return result;
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::RemoveLast() {
  GOOGLE_DCHECK_GT(current_size_, 0);
  TypeHandler::Clear(cast<TypeHandler>(elements_[--current_size_]));
}

template <typename TypeHandler>
void RepeatedPtrFieldBase::Clear() {
  for (int i = 0; i < current_size_; i++) {
    TypeHandler::Clear(cast<TypeHandler>(elements_[i]));
  }
  current_size_ = 0;
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::MergeFrom(const RepeatedPtrFieldBase& other) {
  Reserve(current_size_ + other.current_size_);
  for (int i = 0; i < other.current_size_; i++) {
    TypeHandler::Merge(other.template Get<TypeHandler>(i), Add<TypeHandler>());
  }
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::CopyFrom(const RepeatedPtrFieldBase& other) {
  RepeatedPtrFieldBase::Clear<TypeHandler>();
  RepeatedPtrFieldBase::MergeFrom<TypeHandler>(other);
}

inline int RepeatedPtrFieldBase::Capacity() const {
  return total_size_;
}

inline void* const* RepeatedPtrFieldBase::raw_data() const {
  return elements_;
}

inline void** RepeatedPtrFieldBase::raw_mutable_data() const {
  return elements_;
}

template <typename TypeHandler>
inline typename TypeHandler::Type** RepeatedPtrFieldBase::mutable_data() {

  return reinterpret_cast<typename TypeHandler::Type**>(elements_);
}

template <typename TypeHandler>
inline const typename TypeHandler::Type* const*
RepeatedPtrFieldBase::data() const {

  return reinterpret_cast<const typename TypeHandler::Type* const*>(elements_);
}

inline void RepeatedPtrFieldBase::SwapElements(int index1, int index2) {
  std::swap(elements_[index1], elements_[index2]);
}

template <typename TypeHandler>
inline int RepeatedPtrFieldBase::SpaceUsedExcludingSelf() const {
  int allocated_bytes =
      (elements_ != initial_space_) ? total_size_ * sizeof(elements_[0]) : 0;
  for (int i = 0; i < allocated_size_; ++i) {
    allocated_bytes += TypeHandler::SpaceUsed(*cast<TypeHandler>(elements_[i]));
  }
  return allocated_bytes;
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::AddFromCleared() {
  if (current_size_ < allocated_size_) {
    return cast<TypeHandler>(elements_[current_size_++]);
  } else {
    return NULL;
  }
}

template <typename TypeHandler>
void RepeatedPtrFieldBase::AddAllocated(
    typename TypeHandler::Type* value) {

  if (current_size_ == total_size_) {

    Reserve(total_size_ + 1);
    ++allocated_size_;
  } else if (allocated_size_ == total_size_) {

    TypeHandler::Delete(cast<TypeHandler>(elements_[current_size_]));
  } else if (current_size_ < allocated_size_) {

    elements_[allocated_size_] = elements_[current_size_];
    ++allocated_size_;
  } else {

    ++allocated_size_;
  }

  elements_[current_size_++] = value;
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::ReleaseLast() {
  GOOGLE_DCHECK_GT(current_size_, 0);
  typename TypeHandler::Type* result =
      cast<TypeHandler>(elements_[--current_size_]);
  --allocated_size_;
  if (current_size_ < allocated_size_) {

    elements_[current_size_] = elements_[allocated_size_];
  }
  return result;
}

inline int RepeatedPtrFieldBase::ClearedCount() const {
  return allocated_size_ - current_size_;
}

template <typename TypeHandler>
inline void RepeatedPtrFieldBase::AddCleared(
    typename TypeHandler::Type* value) {
  if (allocated_size_ == total_size_) Reserve(total_size_ + 1);
  elements_[allocated_size_++] = value;
}

template <typename TypeHandler>
inline typename TypeHandler::Type* RepeatedPtrFieldBase::ReleaseCleared() {
  GOOGLE_DCHECK_GT(allocated_size_, current_size_);
  return cast<TypeHandler>(elements_[--allocated_size_]);
}

}

template <typename Element>
class RepeatedPtrField<Element>::TypeHandler
    : public internal::GenericTypeHandler<Element> {};

template <>
class RepeatedPtrField<string>::TypeHandler
    : public internal::StringTypeHandler {};

template <typename Element>
inline RepeatedPtrField<Element>::RepeatedPtrField() {}

template <typename Element>
inline RepeatedPtrField<Element>::RepeatedPtrField(
    const RepeatedPtrField& other) {
  CopyFrom(other);
}

template <typename Element>
RepeatedPtrField<Element>::~RepeatedPtrField() {
  Destroy<TypeHandler>();
}

template <typename Element>
inline RepeatedPtrField<Element>& RepeatedPtrField<Element>::operator=(
    const RepeatedPtrField& other) {
  CopyFrom(other);
  return *this;
}

template <typename Element>
inline int RepeatedPtrField<Element>::size() const {
  return RepeatedPtrFieldBase::size();
}

template <typename Element>
inline const Element& RepeatedPtrField<Element>::Get(int index) const {
  return RepeatedPtrFieldBase::Get<TypeHandler>(index);
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::Mutable(int index) {
  return RepeatedPtrFieldBase::Mutable<TypeHandler>(index);
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::Add() {
  return RepeatedPtrFieldBase::Add<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::RemoveLast() {
  RepeatedPtrFieldBase::RemoveLast<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::Clear() {
  RepeatedPtrFieldBase::Clear<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::MergeFrom(
    const RepeatedPtrField& other) {
  RepeatedPtrFieldBase::MergeFrom<TypeHandler>(other);
}

template <typename Element>
inline void RepeatedPtrField<Element>::CopyFrom(
    const RepeatedPtrField& other) {
  RepeatedPtrFieldBase::CopyFrom<TypeHandler>(other);
}

template <typename Element>
inline Element** RepeatedPtrField<Element>::mutable_data() {
  return RepeatedPtrFieldBase::mutable_data<TypeHandler>();
}

template <typename Element>
inline const Element* const* RepeatedPtrField<Element>::data() const {
  return RepeatedPtrFieldBase::data<TypeHandler>();
}

template <typename Element>
void RepeatedPtrField<Element>::Swap(RepeatedPtrField* other) {
  RepeatedPtrFieldBase::Swap(other);
}

template <typename Element>
void RepeatedPtrField<Element>::SwapElements(int index1, int index2) {
  RepeatedPtrFieldBase::SwapElements(index1, index2);
}

template <typename Element>
inline int RepeatedPtrField<Element>::SpaceUsedExcludingSelf() const {
  return RepeatedPtrFieldBase::SpaceUsedExcludingSelf<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::AddAllocated(Element* value) {
  RepeatedPtrFieldBase::AddAllocated<TypeHandler>(value);
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::ReleaseLast() {
  return RepeatedPtrFieldBase::ReleaseLast<TypeHandler>();
}

template <typename Element>
inline int RepeatedPtrField<Element>::ClearedCount() const {
  return RepeatedPtrFieldBase::ClearedCount();
}

template <typename Element>
inline void RepeatedPtrField<Element>::AddCleared(Element* value) {
  return RepeatedPtrFieldBase::AddCleared<TypeHandler>(value);
}

template <typename Element>
inline Element* RepeatedPtrField<Element>::ReleaseCleared() {
  return RepeatedPtrFieldBase::ReleaseCleared<TypeHandler>();
}

template <typename Element>
inline void RepeatedPtrField<Element>::Reserve(int new_size) {
  return RepeatedPtrFieldBase::Reserve(new_size);
}

template <typename Element>
inline int RepeatedPtrField<Element>::Capacity() const {
  return RepeatedPtrFieldBase::Capacity();
}

namespace internal {

template<typename Element>
class RepeatedPtrIterator
    : public std::iterator<
          std::random_access_iterator_tag, Element> {
 public:
  typedef RepeatedPtrIterator<Element> iterator;
  typedef std::iterator<
          std::random_access_iterator_tag, Element> superclass;

  typedef typename superclass::reference reference;
  typedef typename superclass::pointer pointer;
  typedef typename superclass::difference_type difference_type;

  RepeatedPtrIterator() : it_(NULL) {}
  explicit RepeatedPtrIterator(void* const* it) : it_(it) {}

  template<typename OtherElement>
  RepeatedPtrIterator(const RepeatedPtrIterator<OtherElement>& other)
      : it_(other.it_) {

    if (false) {
      implicit_cast<Element*, OtherElement*>(0);
    }
  }

  reference operator*() const { return *reinterpret_cast<Element*>(*it_); }
  pointer   operator->() const { return &(operator*()); }

  iterator& operator++() { ++it_; return *this; }
  iterator  operator++(int) { return iterator(it_++); }
  iterator& operator--() { --it_; return *this; }
  iterator  operator--(int) { return iterator(it_--); }

  bool operator==(const iterator& x) const { return it_ == x.it_; }
  bool operator!=(const iterator& x) const { return it_ != x.it_; }

  bool operator<(const iterator& x) const { return it_ < x.it_; }
  bool operator<=(const iterator& x) const { return it_ <= x.it_; }
  bool operator>(const iterator& x) const { return it_ > x.it_; }
  bool operator>=(const iterator& x) const { return it_ >= x.it_; }

  iterator& operator+=(difference_type d) {
    it_ += d;
    return *this;
  }
  friend iterator operator+(iterator it, difference_type d) {
    it += d;
    return it;
  }
  friend iterator operator+(difference_type d, iterator it) {
    it += d;
    return it;
  }
  iterator& operator-=(difference_type d) {
    it_ -= d;
    return *this;
  }
  friend iterator operator-(iterator it, difference_type d) {
    it -= d;
    return it;
  }

  reference operator[](difference_type d) const { return *(*this + d); }

  difference_type operator-(const iterator& x) const { return it_ - x.it_; }

 private:
  template<typename OtherElement>
  friend class RepeatedPtrIterator;

  void* const* it_;
};

template<typename Element>
class RepeatedPtrOverPtrsIterator
    : public std::iterator<std::random_access_iterator_tag, Element*> {
 public:
  typedef RepeatedPtrOverPtrsIterator<Element> iterator;
  typedef std::iterator<
          std::random_access_iterator_tag, Element*> superclass;

  typedef typename superclass::reference reference;
  typedef typename superclass::pointer pointer;
  typedef typename superclass::difference_type difference_type;

  RepeatedPtrOverPtrsIterator() : it_(NULL) {}
  explicit RepeatedPtrOverPtrsIterator(void** it) : it_(it) {}

  reference operator*() const { return *reinterpret_cast<Element**>(it_); }
  pointer   operator->() const { return &(operator*()); }

  iterator& operator++() { ++it_; return *this; }
  iterator  operator++(int) { return iterator(it_++); }
  iterator& operator--() { --it_; return *this; }
  iterator  operator--(int) { return iterator(it_--); }

  bool operator==(const iterator& x) const { return it_ == x.it_; }
  bool operator!=(const iterator& x) const { return it_ != x.it_; }

  bool operator<(const iterator& x) const { return it_ < x.it_; }
  bool operator<=(const iterator& x) const { return it_ <= x.it_; }
  bool operator>(const iterator& x) const { return it_ > x.it_; }
  bool operator>=(const iterator& x) const { return it_ >= x.it_; }

  iterator& operator+=(difference_type d) {
    it_ += d;
    return *this;
  }
  friend iterator operator+(iterator it, difference_type d) {
    it += d;
    return it;
  }
  friend iterator operator+(difference_type d, iterator it) {
    it += d;
    return it;
  }
  iterator& operator-=(difference_type d) {
    it_ -= d;
    return *this;
  }
  friend iterator operator-(iterator it, difference_type d) {
    it -= d;
    return it;
  }

  reference operator[](difference_type d) const { return *(*this + d); }

  difference_type operator-(const iterator& x) const { return it_ - x.it_; }

 private:
  template<typename OtherElement>
  friend class RepeatedPtrIterator;

  void** it_;
};

}

template <typename Element>
inline typename RepeatedPtrField<Element>::iterator
RepeatedPtrField<Element>::begin() {
  return iterator(raw_data());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_iterator
RepeatedPtrField<Element>::begin() const {
  return iterator(raw_data());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::iterator
RepeatedPtrField<Element>::end() {
  return iterator(raw_data() + size());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::const_iterator
RepeatedPtrField<Element>::end() const {
  return iterator(raw_data() + size());
}

template <typename Element>
inline typename RepeatedPtrField<Element>::pointer_iterator
RepeatedPtrField<Element>::pointer_begin() {
  return pointer_iterator(raw_mutable_data());
}
template <typename Element>
inline typename RepeatedPtrField<Element>::pointer_iterator
RepeatedPtrField<Element>::pointer_end() {
  return pointer_iterator(raw_mutable_data() + size());
}

namespace internal {

template<typename T> class RepeatedFieldBackInsertIterator
    : public std::iterator<std::output_iterator_tag, T> {
 public:
  explicit RepeatedFieldBackInsertIterator(
      RepeatedField<T>* const mutable_field)
      : field_(mutable_field) {
  }
  RepeatedFieldBackInsertIterator<T>& operator=(const T& value) {
    field_->Add(value);
    return *this;
  }
  RepeatedFieldBackInsertIterator<T>& operator*() {
    return *this;
  }
  RepeatedFieldBackInsertIterator<T>& operator++() {
    return *this;
  }
  RepeatedFieldBackInsertIterator<T>& operator++(int ignores_parameter) {
    return *this;
  }

 private:
  RepeatedField<T>* field_;
};

template<typename T> class RepeatedPtrFieldBackInsertIterator
    : public std::iterator<std::output_iterator_tag, T> {
 public:
  RepeatedPtrFieldBackInsertIterator(
      RepeatedPtrField<T>* const mutable_field)
      : field_(mutable_field) {
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator=(const T& value) {
    *field_->Add() = value;
    return *this;
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator=(
      const T* const ptr_to_value) {
    *field_->Add() = *ptr_to_value;
    return *this;
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator*() {
    return *this;
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator++() {
    return *this;
  }
  RepeatedPtrFieldBackInsertIterator<T>& operator++(int ignores_parameter) {
    return *this;
  }

 private:
  RepeatedPtrField<T>* field_;
};

template<typename T> class AllocatedRepeatedPtrFieldBackInsertIterator
    : public std::iterator<std::output_iterator_tag, T> {
 public:
  explicit AllocatedRepeatedPtrFieldBackInsertIterator(
      RepeatedPtrField<T>* const mutable_field)
      : field_(mutable_field) {
  }
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator=(
      T* const ptr_to_value) {
    field_->AddAllocated(ptr_to_value);
    return *this;
  }
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator*() {
    return *this;
  }
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator++() {
    return *this;
  }
  AllocatedRepeatedPtrFieldBackInsertIterator<T>& operator++(
      int ignores_parameter) {
    return *this;
  }

 private:
  RepeatedPtrField<T>* field_;
};
}

template<typename T> internal::RepeatedFieldBackInsertIterator<T>
RepeatedFieldBackInserter(RepeatedField<T>* const mutable_field) {
  return internal::RepeatedFieldBackInsertIterator<T>(mutable_field);
}

template<typename T> internal::RepeatedPtrFieldBackInsertIterator<T>
RepeatedFieldBackInserter(RepeatedPtrField<T>* const mutable_field) {
  return internal::RepeatedPtrFieldBackInsertIterator<T>(mutable_field);
}

template<typename T> internal::AllocatedRepeatedPtrFieldBackInsertIterator<T>
AllocatedRepeatedPtrFieldBackInserter(
    RepeatedPtrField<T>* const mutable_field) {
  return internal::AllocatedRepeatedPtrFieldBackInsertIterator<T>(
      mutable_field);
}

}

}
#endif
