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

import java.util.Map;

public class RTCStats {
  private final long timestampUs;
  private final String type;
  private final String id;
  private final Map<String, Object> members;

  public RTCStats(long timestampUs, String type, String id, Map<String, Object> members) {
    this.timestampUs = timestampUs;
    this.type = type;
    this.id = id;
    this.members = members;
  }

  public double getTimestampUs() {
    return timestampUs;
  }

  public String getType() {
    return type;
  }

  public String getId() {
    return id;
  }

  public Map<String, Object> getMembers() {
    return members;
  }

  @Override
  public String toString() {
    StringBuilder builder = new StringBuilder();
    builder.append("{ timestampUs: ")
        .append(timestampUs)
        .append(", type: ")
        .append(type)
        .append(", id: ")
        .append(id);
    boolean first = true;
    for (Map.Entry<String, Object> entry : members.entrySet()) {
      builder.append(", ").append(entry.getKey()).append(": ");
      appendValue(builder, entry.getValue());
    }
    builder.append(" }");
    return builder.toString();
  }

  private static void appendValue(StringBuilder builder, Object value) {
    if (value instanceof Object[]) {
      Object[] arrayValue = (Object[]) value;
      builder.append('[');
      for (int i = 0; i < arrayValue.length; ++i) {
        if (i != 0) {
          builder.append(", ");
        }
        appendValue(builder, arrayValue[i]);
      }
      builder.append(']');
    } else if (value instanceof String) {

      builder.append('"').append(value).append('"');
    } else {
      builder.append(value);
    }
  }

  @SuppressWarnings("unchecked")
  @CalledByNative
  static RTCStats create(long timestampUs, String type, String id, Map members) {
    return new RTCStats(timestampUs, type, id, members);
  }
}
