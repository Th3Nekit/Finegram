/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

public final class FGSubscriptionMetadata {
    public final String profileTitle;
    public final boolean hasUserInfo;
    public final long upload;
    public final long download;
    public final long total;
    public final long expire;

    FGSubscriptionMetadata(String title, boolean hasUserInfo, long upload, long download, long total, long expire) {
        profileTitle = title;
        this.hasUserInfo = hasUserInfo;
        this.upload = upload;
        this.download = download;
        this.total = total;
        this.expire = expire;
    }

    static FGSubscriptionMetadata empty() {
        return new FGSubscriptionMetadata("", false, -1, -1, -1, -1);
    }

    static FGSubscriptionMetadata parse(Map<String, String> headers, String bodyTitle) throws FGSubscriptionError {
        String title = headers.containsKey("profile-title") ? FGSubscriptionParser.displayTitle(headers.get("profile-title")) : bodyTitle;
        Map<String, Long> values = new HashMap<>();
        String info = headers.get("subscription-userinfo");
        if (info != null) {
            try {
                for (String part : info.split(";")) {
                    String[] pair = part.trim().split("=", 2);
                    if (pair.length != 2) throw new IllegalArgumentException();
                    String key = pair[0].trim().toLowerCase(Locale.ROOT);
                    String value = pair[1].trim();
                    if (!value.matches("[0-9]{1,19}") || values.containsKey(key)) throw new IllegalArgumentException();
                    values.put(key, Long.parseLong(value));
                }
            } catch (RuntimeException ignored) {
                values.clear();
            }
        }
        boolean hasInfo = values.containsKey("upload") || values.containsKey("download") || values.containsKey("total") || values.containsKey("expire");
        return new FGSubscriptionMetadata(title, hasInfo, values.getOrDefault("upload", -1L),
                values.getOrDefault("download", -1L), values.getOrDefault("total", -1L), values.getOrDefault("expire", -1L));
    }

    FGSubscriptionMetadata merge(FGSubscriptionMetadata old) {
        return new FGSubscriptionMetadata(profileTitle.isEmpty() ? old.profileTitle : profileTitle,
                hasUserInfo || old.hasUserInfo, hasUserInfo ? upload : old.upload,
                hasUserInfo ? download : old.download, hasUserInfo ? total : old.total, hasUserInfo ? expire : old.expire);
    }
}
