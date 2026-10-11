/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.hook;

import java.lang.reflect.Member;

public final class HookHandle {

    private final HookTarget target;
    private final HookCallback callback;

    HookHandle(HookTarget target, HookCallback callback) {
        this.target = target;
        this.callback = callback;
    }

    public Member getMember() {
        return target.member;
    }

    public HookCallback getCallback() {
        return callback;
    }

    public void unhook() {
        target.remove(callback);
    }
}
