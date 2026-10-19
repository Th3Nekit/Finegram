/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.th3nekit.finegram.hook;

/**
 * Обработчик перехваченного метода.
 *
 * before зовётся до оригинала и может его отменить, задав результат; after — после, и
 * может результат подменить. Обработчики одного метода идут по убыванию приоритета:
 * before — в этом порядке, after — в обратном.
 */
public abstract class HookCallback {

    public static final int PRIORITY_DEFAULT = 50;

    final int priority;

    protected HookCallback() {
        this(PRIORITY_DEFAULT);
    }

    protected HookCallback(int priority) {
        this.priority = priority;
    }

    public void before(HookFrame frame) throws Throwable {
    }

    public void after(HookFrame frame) throws Throwable {
    }
}
