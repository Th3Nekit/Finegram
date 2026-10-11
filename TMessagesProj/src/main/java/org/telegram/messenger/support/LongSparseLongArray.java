/*
 * This is the source code of Telegram for Android v. 5.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2018.
 */

package org.telegram.messenger.support;

public class LongSparseLongArray implements Cloneable {

    private long[] mKeys;
    private long[] mValues;
    private int mSize;

    public LongSparseLongArray() {
        this(10);
    }

    public LongSparseLongArray(int initialCapacity) {
        initialCapacity = ArrayUtils.idealLongArraySize(initialCapacity);

        mKeys = new long[initialCapacity];
        mValues = new long[initialCapacity];
        mSize = 0;
    }

    @Override
    public LongSparseLongArray clone() {
        LongSparseLongArray clone = null;
        try {
            clone = (LongSparseLongArray) super.clone();
            clone.mKeys = mKeys.clone();
            clone.mValues = mValues.clone();
        } catch (CloneNotSupportedException cnse) {

        }
        return clone;
    }

    public long get(long key) {
        return get(key, 0);
    }

    public long get(long key, long valueIfKeyNotFound) {
        int i = binarySearch(mKeys, 0, mSize, key);

        if (i < 0) {
            return valueIfKeyNotFound;
        } else {
            return mValues[i];
        }
    }

    public void delete(long key) {
        int i = binarySearch(mKeys, 0, mSize, key);

        if (i >= 0) {
            removeAt(i);
        }
    }

    public void removeAt(int index) {
        System.arraycopy(mKeys, index + 1, mKeys, index, mSize - (index + 1));
        System.arraycopy(mValues, index + 1, mValues, index, mSize - (index + 1));
        mSize--;
    }

    public void put(long key, long value) {
        int i = binarySearch(mKeys, 0, mSize, key);

        if (i >= 0) {
            mValues[i] = value;
        } else {
            i = ~i;

            if (mSize >= mKeys.length) {
                growKeyAndValueArrays(mSize + 1);
            }

            if (mSize - i != 0) {
                System.arraycopy(mKeys, i, mKeys, i + 1, mSize - i);
                System.arraycopy(mValues, i, mValues, i + 1, mSize - i);
            }

            mKeys[i] = key;
            mValues[i] = value;
            mSize++;
        }
    }

    public int size() {
        return mSize;
    }

    public long keyAt(int index) {
        return mKeys[index];
    }

    public long valueAt(int index) {
        return mValues[index];
    }

    public int indexOfKey(long key) {
        return binarySearch(mKeys, 0, mSize, key);
    }

    public int indexOfValue(long value) {
        for (int i = 0; i < mSize; i++)
            if (mValues[i] == value)
                return i;

        return -1;
    }

    public void clear() {
        mSize = 0;
    }

    public void append(long key, long value) {
        if (mSize != 0 && key <= mKeys[mSize - 1]) {
            put(key, value);
            return;
        }

        int pos = mSize;
        if (pos >= mKeys.length) {
            growKeyAndValueArrays(pos + 1);
        }

        mKeys[pos] = key;
        mValues[pos] = value;
        mSize = pos + 1;
    }

    private void growKeyAndValueArrays(int minNeededSize) {
        int n = ArrayUtils.idealLongArraySize(minNeededSize);

        long[] nkeys = new long[n];
        long[] nvalues = new long[n];

        System.arraycopy(mKeys, 0, nkeys, 0, mKeys.length);
        System.arraycopy(mValues, 0, nvalues, 0, mValues.length);

        mKeys = nkeys;
        mValues = nvalues;
    }

    private static int binarySearch(long[] a, int start, int len, long key) {
        int high = start + len, low = start - 1, guess;

        while (high - low > 1) {
            guess = (high + low) / 2;

            if (a[guess] < key)
                low = guess;
            else
                high = guess;
        }

        if (high == start + len)
            return ~(start + len);
        else if (a[high] == key)
            return high;
        else
            return ~high;
    }
}
