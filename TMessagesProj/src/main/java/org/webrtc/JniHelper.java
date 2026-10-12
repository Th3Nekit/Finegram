/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.io.UnsupportedEncodingException;
import java.util.Map;

class JniHelper {

  @CalledByNative
  static byte[] getStringBytes(String s) {
    try {
      return s.getBytes("ISO-8859-1");
    } catch (UnsupportedEncodingException e) {
      throw new RuntimeException("ISO-8859-1 is unsupported");
    }
  }

  @CalledByNative
  static Object getStringClass() {
    return String.class;
  }

  @CalledByNative
  static Object getKey(Map.Entry entry) {
    return entry.getKey();
  }

  @CalledByNative
  static Object getValue(Map.Entry entry) {
    return entry.getValue();
  }
}
