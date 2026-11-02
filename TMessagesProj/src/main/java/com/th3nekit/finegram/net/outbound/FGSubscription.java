/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class FGSubscription {
    public final String id;
    public final String name;
    public final String url;
    public final FGSubscriptionOptions options;
    public final FGSubscriptionMetadata metadata;
    public final int serverCount;
    public final int skipped;
    public final long lastUpdated;
    public final long lastAttempt;
    public final FGSubscriptionError.Code lastError;
    final List<FGSubscriptionParser.Server> members;

    FGSubscription(String id, String name, String url, FGSubscriptionOptions options, FGSubscriptionMetadata metadata,
                   List<FGSubscriptionParser.Server> members, int skipped, long lastUpdated, long lastAttempt,
                   FGSubscriptionError.Code lastError) {
        this.id = id;
        this.name = name;
        this.url = url;
        this.options = options;
        this.metadata = metadata;
        this.members = Collections.unmodifiableList(new ArrayList<>(members));
        serverCount = members.size();
        this.skipped = skipped;
        this.lastUpdated = lastUpdated;
        this.lastAttempt = lastAttempt;
        this.lastError = lastError;
    }

    FGSubscription failed(FGSubscriptionError.Code error, long attempt) {
        return new FGSubscription(id, name, url, options, metadata, members, skipped, lastUpdated, attempt, error);
    }
}
