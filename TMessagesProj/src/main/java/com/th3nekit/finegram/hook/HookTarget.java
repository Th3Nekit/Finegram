/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.hook;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;

public final class HookTarget {

    private static final Object[] NO_ARGS = new Object[0];
    private static final HookCallback[] NO_CALLBACKS = new HookCallback[0];

    final Member member;
    private final boolean isStatic;
    private final Class<?> returnType;

    volatile Object backup;
    private volatile HookCallback[] callbacks = NO_CALLBACKS;

    HookTarget(Member member) {
        this.member = member;
        this.isStatic = Modifier.isStatic(member.getModifiers());
        this.returnType = member instanceof Method ? ((Method) member).getReturnType() : void.class;
    }

    public Object callback(Object[] raw) throws Throwable {
        final Object thisObject;
        final Object[] args;
        if (raw == null) {
            thisObject = null;
            args = NO_ARGS;
        } else if (isStatic) {
            thisObject = null;
            args = raw;
        } else {
            thisObject = raw[0];
            args = raw.length > 1 ? Arrays.copyOfRange(raw, 1, raw.length) : NO_ARGS;
        }

        final HookCallback[] snapshot = callbacks;
        if (snapshot.length == 0 || ArtHook.suspended) {
            return coerce(invokeOriginal(thisObject, args));
        }

        final HookFrame frame = new HookFrame(this, thisObject, args, snapshot.length);
        int before = 0;
        do {
            frame.current = before;
            try {
                snapshot[before].before(frame);
            } catch (Throwable t) {
                ArtHook.report("before", member, t);

                frame.resetForCallbackFailure();
                continue;
            }
            if (frame.isReturnEarly()) {
                before++;
                break;
            }
        } while (++before < snapshot.length);

        if (!frame.isReturnEarly()) {
            try {
                frame.completeWith(invokeOriginal(frame.thisObject, frame.args));
            } catch (Throwable t) {
                frame.completeWithThrowable(t);
            }
        }

        for (int after = before - 1; after >= 0; after--) {
            final Object lastResult = frame.getResult();
            final Throwable lastThrowable = frame.getThrowable();
            frame.current = after;
            try {
                snapshot[after].after(frame);
            } catch (Throwable t) {
                ArtHook.report("after", member, t);
                frame.restore(lastResult, lastThrowable);
            }
        }

        if (frame.hasThrowable()) {
            throw frame.getThrowable();
        }
        return coerce(frame.getResult());
    }

    Object invokeOriginal(Object thisObject, Object[] args) throws Throwable {
        Object original = backup;
        if (original == null) {

            synchronized (ArtHook.LOCK) {
                original = backup;
            }
            if (original == null) {
                throw new IllegalStateException("original is not ready: " + member);
            }
        }
        try {
            if (original instanceof Method) {
                return ((Method) original).invoke(thisObject, args);
            }

            return ArtHook.nativeInvoke(original, thisObject, args);
        } catch (InvocationTargetException e) {
            throw e.getCause() != null ? e.getCause() : e;
        }
    }

    private Object coerce(Object value) {
        if (returnType == void.class || !returnType.isPrimitive()) {
            return value;
        }
        if (returnType == boolean.class) {
            return value instanceof Boolean ? value : Boolean.FALSE;
        }
        if (returnType == char.class) {
            if (value instanceof Character) return value;
            return value instanceof Number ? (char) ((Number) value).intValue() : '\0';
        }
        final Number number = value instanceof Number ? (Number) value
                : value instanceof Character ? (int) (Character) value : 0;
        if (returnType == int.class) return value instanceof Integer ? value : number.intValue();
        if (returnType == long.class) return value instanceof Long ? value : number.longValue();
        if (returnType == float.class) return value instanceof Float ? value : number.floatValue();
        if (returnType == double.class) return value instanceof Double ? value : number.doubleValue();
        if (returnType == short.class) return value instanceof Short ? value : number.shortValue();
        if (returnType == byte.class) return value instanceof Byte ? value : number.byteValue();
        return value;
    }

    synchronized void add(HookCallback callback) {
        final HookCallback[] current = callbacks;
        for (HookCallback existing : current) {
            if (existing == callback) return;
        }

        int index = current.length;
        for (int i = 0; i < current.length; i++) {
            if (current[i].priority < callback.priority) {
                index = i;
                break;
            }
        }
        final HookCallback[] next = new HookCallback[current.length + 1];
        System.arraycopy(current, 0, next, 0, index);
        next[index] = callback;
        System.arraycopy(current, index, next, index + 1, current.length - index);
        callbacks = next;
    }

    synchronized boolean remove(HookCallback callback) {
        final HookCallback[] current = callbacks;
        for (int i = 0; i < current.length; i++) {
            if (current[i] != callback) continue;
            final HookCallback[] next = new HookCallback[current.length - 1];
            System.arraycopy(current, 0, next, 0, i);
            System.arraycopy(current, i + 1, next, i, current.length - i - 1);
            callbacks = next;
            return true;
        }
        return false;
    }

    @Override
    public String toString() {
        return "HookTarget{" + member + ", callbacks=" + callbacks.length + '}';
    }
}
