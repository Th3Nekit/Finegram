/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.hook;

import java.lang.reflect.Member;

public final class HookFrame {

    public final Member method;
    public Object thisObject;
    public Object[] args;

    private final HookTarget target;
    private Object result;
    private Throwable throwable;
    private boolean returnEarly;

    private Object[] states;
    int current;

    HookFrame(HookTarget target, Object thisObject, Object[] args, int callbacks) {
        this.target = target;
        this.method = target.member;
        this.thisObject = thisObject;
        this.args = args;
        this.states = callbacks > 0 ? new Object[callbacks] : null;
    }

    public Object getResult() {
        return result;
    }

    public void setResult(Object result) {
        this.result = result;
        this.throwable = null;
        this.returnEarly = true;
    }

    public Throwable getThrowable() {
        return throwable;
    }

    public boolean hasThrowable() {
        return throwable != null;
    }

    public void setThrowable(Throwable throwable) {
        this.throwable = throwable;
        this.result = null;
        this.returnEarly = true;
    }

    public Object getResultOrThrowable() throws Throwable {
        if (throwable != null) {
            throw throwable;
        }
        return result;
    }

    public boolean isReturnEarly() {
        return returnEarly;
    }

    public Object invokeOriginalMethod() throws Throwable {
        return target.invokeOriginal(thisObject, args);
    }

    public Object invokeOriginalMethod(Object thisObject, Object... args) throws Throwable {
        return target.invokeOriginal(thisObject, args);
    }

    public Object getState() {
        return states == null ? null : states[current];
    }

    public void setState(Object value) {
        if (states != null) {
            states[current] = value;
        }
    }

    void resetForCallbackFailure() {
        result = null;
        throwable = null;
        returnEarly = false;
    }

    void restore(Object lastResult, Throwable lastThrowable) {
        result = lastResult;
        throwable = lastThrowable;
    }

    void completeWith(Object value) {
        result = value;
        throwable = null;
    }

    void completeWithThrowable(Throwable error) {
        result = null;
        throwable = error;
    }
}
