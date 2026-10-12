/*
 * This is the source code of Telegram for Android v. 5.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2018.
 */

package org.telegram.messenger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

public class LruCache<T> {
    private final LinkedHashMap<String, T> map;
    private final LinkedHashMap<String, ArrayList<String>> mapFilters;

    private int size;
    private int maxSize;

    public LruCache(int maxSize) {
        if (maxSize <= 0) {
            throw new IllegalArgumentException("maxSize <= 0");
        }
        this.maxSize = maxSize;
        this.map = new LinkedHashMap<>(0, 0.75f, true);
        this.mapFilters = new LinkedHashMap<>();
    }

    public final T get(String key) {
        if (key == null) {
            throw new NullPointerException("key == null");
        }

        T mapValue;
        synchronized (this) {
            mapValue = map.get(key);
            if (mapValue != null) {
                return mapValue;
            }
        }
        return null;
    }

    public ArrayList<String> getFilterKeys(String key) {
        ArrayList<String> arr = mapFilters.get(key);
        if (arr != null) {
            return new ArrayList<>(arr);
        }
        return null;
    }

    public void moveToFront(String key) {
        T value = map.remove(key);
        if (value != null) {
            map.put(key, value);
        }
    }

    public T put(String key, T value) {
        if (key == null || value == null) {
            throw new NullPointerException("key == null || value == null");
        }

        T previous;
        synchronized (this) {
            size += safeSizeOf(key, value);
            previous = map.put(key, value);
            if (previous != null) {
                size -= safeSizeOf(key, previous);
            }
        }

        String[] args = key.split("@");
        if (args.length > 1) {
            ArrayList<String> arr = mapFilters.get(args[0]);
            if (arr == null) {
                arr = new ArrayList<>();
                mapFilters.put(args[0], arr);
            }
            if (!arr.contains(args[1])) {
                arr.add(args[1]);
            }
        }

        if (previous != null) {
            entryRemoved(false, key, previous, value);
        }

        trimToSize(maxSize, key);
        return previous;
    }

    private void trimToSize(int maxSize, String justAdded) {
        synchronized (this) {
            Iterator<HashMap.Entry<String, T>> iterator = map.entrySet().iterator();
            while (iterator.hasNext()) {
                if (size <= maxSize || map.isEmpty()) {
                    break;
                }
                HashMap.Entry<String, T> entry = iterator.next();

                String key = entry.getKey();
                if (justAdded != null && justAdded.equals(key)) {
                    continue;
                }
                T value = entry.getValue();
                size -= safeSizeOf(key, value);
                iterator.remove();

                String[] args = key.split("@");
                if (args.length > 1) {
                    ArrayList<String> arr = mapFilters.get(args[0]);
                    if (arr != null) {
                        arr.remove(args[1]);
                        if (arr.isEmpty()) {
                            mapFilters.remove(args[0]);
                        }
                    }
                }

                entryRemoved(true, key, value, null);
            }
        }
    }

    public final T remove(String key) {
        if (key == null) {
            throw new NullPointerException("key == null");
        }

        T previous;
        synchronized (this) {
            previous = map.remove(key);
            if (previous != null) {
                size -= safeSizeOf(key, previous);
            }
        }

        if (previous != null) {
            String[] args = key.split("@");
            if (args.length > 1) {
                ArrayList<String> arr = mapFilters.get(args[0]);
                if (arr != null) {
                    arr.remove(args[1]);
                    if (arr.isEmpty()) {
                        mapFilters.remove(args[0]);
                    }
                }
            }

            entryRemoved(false, key, previous, null);
        }

        return previous;
    }

    public boolean contains(String key){
    	return map.containsKey(key);
    }

    protected void entryRemoved(boolean evicted, String key, T oldValue, T newValue) {}

    private int safeSizeOf(String key, T value) {
        int result = sizeOf(key, value);
        if (result < 0) {
            throw new IllegalStateException("Negative size: " + key + "=" + value);
        }
        return result;
    }

    protected int sizeOf(String key, T value) {
        return 1;
    }

    public final void evictAll() {
        trimToSize(-1, null);
    }

    public synchronized final int size() {
        return size;
    }

    public synchronized final int maxSize() {
        return maxSize;
    }

    public synchronized final Set<Map.Entry<String, T>> entrySet() {
        return map.entrySet();
    }
}
