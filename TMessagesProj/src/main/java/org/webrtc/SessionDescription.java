/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.Locale;

public class SessionDescription {

  public static enum Type {
    OFFER,
    PRANSWER,
    ANSWER,
    ROLLBACK;

    public String canonicalForm() {
      return name().toLowerCase(Locale.US);
    }

    @CalledByNative("Type")
    public static Type fromCanonicalForm(String canonical) {
      return Type.valueOf(Type.class, canonical.toUpperCase(Locale.US));
    }
  }

  public final Type type;
  public final String description;

  @CalledByNative
  public SessionDescription(Type type, String description) {
    this.type = type;
    this.description = description;
  }

  @CalledByNative
  String getDescription() {
    return description;
  }

  @CalledByNative
  String getTypeInCanonicalForm() {
    return type.canonicalForm();
  }
}
