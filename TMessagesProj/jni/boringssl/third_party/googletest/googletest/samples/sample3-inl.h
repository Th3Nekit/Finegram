// Copyright 2005, Google Inc.
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

// A sample program demonstrating using Google C++ testing framework.

#ifndef GOOGLETEST_SAMPLES_SAMPLE3_INL_H_
#define GOOGLETEST_SAMPLES_SAMPLE3_INL_H_

#include <stddef.h>

template <typename E>
class Queue;

template <typename E>
class QueueNode {
  friend class Queue<E>;

 public:

  const E& element() const { return element_; }

  QueueNode* next() { return next_; }
  const QueueNode* next() const { return next_; }

 private:

  explicit QueueNode(const E& an_element)
      : element_(an_element), next_(nullptr) {}

  const QueueNode& operator=(const QueueNode&);
  QueueNode(const QueueNode&);

  E element_;
  QueueNode* next_;
};

template <typename E>
class Queue {
 public:

  Queue() : head_(nullptr), last_(nullptr), size_(0) {}

  ~Queue() { Clear(); }

  void Clear() {
    if (size_ > 0) {

      QueueNode<E>* node = head_;
      QueueNode<E>* next = node->next();
      for (;;) {
        delete node;
        node = next;
        if (node == nullptr) break;
        next = node->next();
      }

      head_ = last_ = nullptr;
      size_ = 0;
    }
  }

  size_t Size() const { return size_; }

  QueueNode<E>* Head() { return head_; }
  const QueueNode<E>* Head() const { return head_; }

  QueueNode<E>* Last() { return last_; }
  const QueueNode<E>* Last() const { return last_; }

  void Enqueue(const E& element) {
    QueueNode<E>* new_node = new QueueNode<E>(element);

    if (size_ == 0) {
      head_ = last_ = new_node;
      size_ = 1;
    } else {
      last_->next_ = new_node;
      last_ = new_node;
      size_++;
    }
  }

  E* Dequeue() {
    if (size_ == 0) {
      return nullptr;
    }

    const QueueNode<E>* const old_head = head_;
    head_ = head_->next_;
    size_--;
    if (size_ == 0) {
      last_ = nullptr;
    }

    E* element = new E(old_head->element());
    delete old_head;

    return element;
  }

  template <typename F>
  Queue* Map(F function) const {
    Queue* new_queue = new Queue();
    for (const QueueNode<E>* node = head_; node != nullptr;
         node = node->next_) {
      new_queue->Enqueue(function(node->element()));
    }

    return new_queue;
  }

 private:
  QueueNode<E>* head_;
  QueueNode<E>* last_;
  size_t size_;

  Queue(const Queue&);
  const Queue& operator=(const Queue&);
};

#endif
