/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

public interface Predicate<T> {

  boolean test(T arg);

  default Predicate<T> or(Predicate<? super T> other) {
    return new Predicate<T>() {
      @Override
      public boolean test(T arg) {
        return Predicate.this.test(arg) || other.test(arg);
      }
    };
  }

  default Predicate<T> and(Predicate<? super T> other) {
    return new Predicate<T>() {
      @Override
      public boolean test(T arg) {
        return Predicate.this.test(arg) && other.test(arg);
      }
    };
  }

  default Predicate<T> negate() {
    return new Predicate<T>() {
      @Override
      public boolean test(T arg) {
        return !Predicate.this.test(arg);
      }
    };
  }
}