/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins;

public enum AppEvent {
    START("app_start"),
    STOP("app_stop"),
    PAUSE("app_pause"),
    RESUME("app_resume"),
    ACCOUNT_SWITCHED("account_switched");

    private final String value;

    AppEvent(String value) {
        this.value = value;
    }

    public String getValue() {
        return value;
    }

    @Override
    public String toString() {
        return value;
    }
}
