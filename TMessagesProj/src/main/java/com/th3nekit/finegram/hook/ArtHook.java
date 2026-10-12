/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.hook;

import android.os.Build;
import android.util.Log;

import org.lsposed.hiddenapibypass.HiddenApiBypass;

import java.lang.reflect.AccessibleObject;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.HashMap;
import java.util.Map;

public final class ArtHook {

    static final String TAG = "FGHook";
    static final Object LOCK = new Object();

    public static volatile boolean suspended;

    private static final Object[] NO_ARGS = new Object[0];
    private static final Map<Member, HookTarget> targets = new HashMap<>();
    private static final Method CALLBACK;

    static {
        try {
            CALLBACK = HookTarget.class.getDeclaredMethod("callback", Object[].class);
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        }
    }

    private static int state;

    private static String error;

    private ArtHook() {
    }

    public static boolean init() {
        synchronized (LOCK) {
            if (state != 0) {
                return state > 0;
            }
            state = -1;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                try {
                    HiddenApiBypass.addHiddenApiExemptions("");
                } catch (Throwable t) {
                    Log.w(TAG, "hidden api exemptions not applied", t);
                }
            }
            try {

                System.loadLibrary("fghook");
                error = nativeInitError();
                if (error == null) {
                    state = 1;
                } else {
                    Log.e(TAG, "hook engine did not start: " + error);
                }
            } catch (Throwable t) {
                error = String.valueOf(t);
                Log.e(TAG, "hook engine failed to start", t);
            }
            return state > 0;
        }
    }

    public static boolean isReady() {
        synchronized (LOCK) {
            return state > 0;
        }
    }

    public static HookHandle hook(Member member, HookCallback callback) {
        if (member == null || callback == null) {
            throw new IllegalArgumentException("member and callback are required");
        }
        if (!(member instanceof Method) && !(member instanceof Constructor)) {
            throw new IllegalArgumentException("Only methods and constructors can be hooked: " + member);
        }
        if (Modifier.isAbstract(member.getModifiers())) {
            throw new IllegalArgumentException("Cannot hook abstract methods: " + member);
        }
        if (!init()) {
            throw new IllegalStateException("Hook engine is not available: " + error);
        }
        HookTarget target;
        synchronized (LOCK) {
            target = targets.get(member);
            if (target == null) {
                target = new HookTarget(member);
                final Object backup = nativeHook(member, target, CALLBACK);
                if (backup == null) {
                    throw new IllegalStateException("Failed to hook " + member);
                }
                ((AccessibleObject) backup).setAccessible(true);
                target.backup = backup;
                targets.put(member, target);
            }
        }
        target.add(callback);
        return new HookHandle(target, callback);
    }

    public static boolean isHooked(Member member) {
        synchronized (LOCK) {
            return targets.containsKey(member);
        }
    }

    public static Object invokeOriginal(Member member, Object thisObject, Object[] args)
            throws IllegalAccessException, InvocationTargetException {
        if (args == null) {
            args = NO_ARGS;
        }
        final HookTarget target;
        synchronized (LOCK) {
            target = targets.get(member);
        }
        if (target != null) {
            try {
                return target.invokeOriginal(thisObject, args);
            } catch (Throwable t) {
                throw new InvocationTargetException(t);
            }
        }
        if (member instanceof Method) {
            final Method method = (Method) member;
            method.setAccessible(true);
            return method.invoke(thisObject, args);
        }
        if (member instanceof Constructor) {
            if (!init()) {
                throw new IllegalStateException("Hook engine is not available: " + error);
            }
            ((Constructor<?>) member).setAccessible(true);
            return nativeInvoke(member, thisObject, args);
        }
        throw new IllegalArgumentException("Not a method or constructor: " + member);
    }

    public static boolean deoptimize(Member member) {
        if (member == null || !init()) {
            return false;
        }
        try {
            return nativeDeoptimize(member);
        } catch (Throwable t) {
            Log.w(TAG, "deoptimize failed: " + member, t);
            return false;
        }
    }

    static void report(String stage, Member member, Throwable t) {
        Log.e(TAG, stage + " callback failed for " + member, t);
    }

    private static native String nativeInitError();

    private static native Object nativeHook(Member target, Object hooker, Method callback);

    private static native boolean nativeUnhook(Member target);

    private static native boolean nativeDeoptimize(Member method);

    static native Object nativeInvoke(Object backup, Object receiver, Object[] args)
            throws IllegalAccessException, InvocationTargetException;
}
