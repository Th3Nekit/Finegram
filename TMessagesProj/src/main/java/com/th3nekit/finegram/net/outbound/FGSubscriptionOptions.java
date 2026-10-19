/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.net.outbound;

import java.util.Locale;

public final class FGSubscriptionOptions {
    public enum IdentityMode { NONE, LEAN_EXPLICIT, OWN_DEVICE, EXPLICIT }
    public enum UserAgentMode { FINEGRAM, V2RAYNG, HIDDIFY, CUSTOM }

    public final IdentityMode identityMode;
    public final String leanHwid;
    public final UserAgentMode userAgentMode;
    public final String customUserAgent;

    private FGSubscriptionOptions(IdentityMode mode, String hwid) {
        this(mode, hwid, mode == IdentityMode.LEAN_EXPLICIT ? UserAgentMode.CUSTOM : UserAgentMode.FINEGRAM,
                mode == IdentityMode.LEAN_EXPLICIT ? "Lean/1.2.0" : "");
    }

    private FGSubscriptionOptions(IdentityMode mode, String hwid, UserAgentMode agent, String custom) {
        identityMode = mode;
        leanHwid = hwid;
        userAgentMode = agent;
        customUserAgent = custom;
    }

    public static FGSubscriptionOptions none() {
        return new FGSubscriptionOptions(IdentityMode.NONE, "");
    }

    public static FGSubscriptionOptions lean(String officialHwid) {
        return new FGSubscriptionOptions(IdentityMode.LEAN_EXPLICIT,
                officialHwid == null ? "" : officialHwid.trim().toUpperCase(Locale.ROOT));
    }

    public static FGSubscriptionOptions ownDevice() {
        return new FGSubscriptionOptions(IdentityMode.OWN_DEVICE, "");
    }

    public static FGSubscriptionOptions explicit(String hwid) {
        return new FGSubscriptionOptions(IdentityMode.EXPLICIT, hwid == null ? "" : hwid.trim());
    }

    public FGSubscriptionOptions withUserAgent(UserAgentMode mode, String value) {
        return new FGSubscriptionOptions(identityMode, leanHwid, mode,
                mode == UserAgentMode.CUSTOM && value != null ? value.trim() : "");
    }

    public String userAgent() {
        switch (userAgentMode) {
            case V2RAYNG: return "v2rayNG";
            case HIDDIFY: return "Hiddify";
            case CUSTOM: return customUserAgent;
            default: return "Finegram/Subscription";
        }
    }

    void validate() throws FGSubscriptionError {
        if (identityMode == IdentityMode.LEAN_EXPLICIT && !leanHwid.matches("[0-9A-F]{1,64}")) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_IDENTITY);
        }
        if (identityMode == IdentityMode.EXPLICIT && !leanHwid.matches("[A-Za-z0-9._:-]{1,128}")) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_IDENTITY);
        }
        if (userAgentMode == null || userAgentMode == UserAgentMode.CUSTOM
                && (customUserAgent.isEmpty() || customUserAgent.length() > 256 || !customUserAgent.matches("[\\x20-\\x7e]+"))) {
            throw new FGSubscriptionError(FGSubscriptionError.Code.INVALID_USER_AGENT);
        }
    }
}
