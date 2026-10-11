/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.xposed;

import com.chaquo.python.PyObject;

import de.robv.android.xposed.XC_MethodHook;

public class PyMethodReplacement extends PyMethodHook {

    public PyMethodReplacement(String pluginId, PyObject callback) {
        super(pluginId, callback);
    }

    public PyMethodReplacement(String pluginId, PyObject callback, int priority) {
        super(pluginId, callback, priority);
    }

    @Override
    protected void beforeHookedMethod(MethodHookParam param) {
        if (callback == null) {
            return;
        }
        try {
            if (callback.containsKey("replace_hooked_method")) {
                PyObject result = callback.callAttr("replace_hooked_method", param);
                param.setResult(result == null ? null : result.toJava(Object.class));
            }
        } catch (Throwable e) {
            org.telegram.messenger.FileLog.e("плагин " + pluginId + ": подмена метода упала", e);
        }
    }

    @Override
    protected void afterHookedMethod(MethodHookParam param) {

    }
}
