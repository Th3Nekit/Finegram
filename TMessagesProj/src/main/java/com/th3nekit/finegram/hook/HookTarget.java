/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.hook;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.Arrays;

public final class HookTarget {

    private static final Object[] NO_ARGS = new Object[0];
    private static final HookCallback[] NO_CALLBACKS = new HookCallback[0];

    final Member member;
    private final boolean isStatic;
    private final Class<?> returnType;
    private final Class<?>[] parameterTypes;

    volatile Object backup;
    private volatile HookCallback[] callbacks = NO_CALLBACKS;

    HookTarget(Member member) {
        this.member = member;
        this.isStatic = Modifier.isStatic(member.getModifiers());
        this.returnType = member instanceof Method ? ((Method) member).getReturnType() : void.class;
        this.parameterTypes = member instanceof Method ? ((Method) member).getParameterTypes()
                : ((Constructor<?>) member).getParameterTypes();
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
        final Object[] converted = coerceArguments(args);
        try {
            if (original instanceof Method) {
                return ((Method) original).invoke(thisObject, converted);
            }
            return ArtHook.nativeInvoke(original, thisObject, converted);
        } catch (InvocationTargetException e) {
            throw e.getCause() != null ? e.getCause() : e;
        }
    }

    private Object[] coerceArguments(Object[] args) {
        final Object[] values = args == null ? NO_ARGS : args;
        if (values.length != parameterTypes.length) {
            throw new IllegalArgumentException("wrong argument count for " + member);
        }
        Object[] converted = values;
        for (int i = 0; i < parameterTypes.length; i++) {
            if (!parameterTypes[i].isPrimitive()) continue;
            final Object value = coercePrimitive(parameterTypes[i], values[i], "argument " + i);
            if (value != values[i]) {
                if (converted == values) converted = values.clone();
                converted[i] = value;
            }
        }
        return converted;
    }

    private Object coerce(Object value) {
        if (returnType == void.class || !returnType.isPrimitive()) {
            return value;
        }
        if (returnType == boolean.class) {
            return value instanceof Boolean ? value : Boolean.FALSE;
        }
        return coercePrimitive(returnType,
                value instanceof Number || value instanceof Character ? value : 0, "result");
    }

    private Object coercePrimitive(Class<?> type, Object value, String position) {
        if (type == boolean.class && value instanceof Boolean
                || type == byte.class && value instanceof Byte
                || type == short.class && value instanceof Short
                || type == char.class && value instanceof Character
                || type == int.class && value instanceof Integer
                || type == long.class && value instanceof Long
                || type == float.class && value instanceof Float
                || type == double.class && value instanceof Double) {
            return value;
        }
        if (!(value instanceof Number) && !(value instanceof Character) || type == boolean.class) {
            throw invalidPrimitive(type, position);
        }
        final Number number = value instanceof Character ? (int) (Character) value : (Number) value;
        if (type == float.class || type == double.class) {
            final double result = number.doubleValue();
            final boolean explicitNonfinite = (number instanceof Float || number instanceof Double)
                    && !Double.isFinite(result);
            if (!Double.isFinite(result) && !explicitNonfinite) throw invalidPrimitive(type, position);
            if (type == double.class) return result;
            final float narrowed = number.floatValue();
            if (!Float.isFinite(narrowed) && !explicitNonfinite) throw invalidPrimitive(type, position);
            return narrowed;
        }
        final long result;
        try {
            if (number instanceof Byte || number instanceof Short || number instanceof Integer
                    || number instanceof Long) {
                result = number.longValue();
            } else if (number instanceof Float || number instanceof Double) {
                final double integral = number.doubleValue();
                if (!Double.isFinite(integral) || integral != Math.rint(integral)
                        || integral < -0x1p63 || integral >= 0x1p63) {
                    throw invalidPrimitive(type, position);
                }
                result = (long) integral;
            } else if (number instanceof BigInteger) {
                result = ((BigInteger) number).longValueExact();
            } else if (number instanceof BigDecimal) {
                result = ((BigDecimal) number).longValueExact();
            } else {
                result = new BigDecimal(number.toString()).longValueExact();
            }
        } catch (ArithmeticException | NumberFormatException error) {
            throw new IllegalArgumentException(position + " is outside " + type + " for " + member, error);
        }
        if (type == long.class) return result;
        if (type == int.class && result >= Integer.MIN_VALUE && result <= Integer.MAX_VALUE) return (int) result;
        if (type == short.class && result >= Short.MIN_VALUE && result <= Short.MAX_VALUE) return (short) result;
        if (type == byte.class && result >= Byte.MIN_VALUE && result <= Byte.MAX_VALUE) return (byte) result;
        if (type == char.class && result >= Character.MIN_VALUE && result <= Character.MAX_VALUE) return (char) result;
        throw invalidPrimitive(type, position);
    }

    private IllegalArgumentException invalidPrimitive(Class<?> type, String position) {
        return new IllegalArgumentException(position + " cannot be represented as " + type + " for " + member);
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
