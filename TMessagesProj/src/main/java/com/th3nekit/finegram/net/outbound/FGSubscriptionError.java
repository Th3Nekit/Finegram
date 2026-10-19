/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

public final class FGSubscriptionError extends Exception {
    public enum Code {
        INVALID_URL, INVALID_IDENTITY, INVALID_USER_AGENT, HWID_REQUIRED, APP_NOT_SUPPORTED, HTTP_STATUS, NETWORK, TIMEOUT,
        TOO_MANY_REDIRECTS, HTTPS_DOWNGRADE, BODY_TOO_LARGE, INVALID_ENCODING,
        INVALID_FORMAT, UNSUPPORTED_FORMAT, INVALID_LINK, UNSUPPORTED_LINK, UNSUPPORTED_PROTOCOL,
        NO_SERVERS, TOO_MANY_SERVERS, SUSPICIOUS_SHRINK, STORAGE, NOT_FOUND, LIMIT, BUSY
    }

    public final Code code;
    public final int httpStatus;

    FGSubscriptionError(Code code) {
        this(code, 0);
    }

    FGSubscriptionError(Code code, int httpStatus) {
        super(code.name(), null, false, false);
        this.code = code;
        this.httpStatus = httpStatus;
    }
}
