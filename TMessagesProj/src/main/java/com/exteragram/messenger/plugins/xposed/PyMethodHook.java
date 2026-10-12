/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger.plugins.xposed;

import com.chaquo.python.PyObject;

import java.util.ArrayList;
import java.util.List;

import de.robv.android.xposed.XC_MethodHook;

public class PyMethodHook extends XC_MethodHook {

    protected final String pluginId;
    protected final PyObject callback;

    private List<Object> beforeFilters;
    private List<Object> afterFilters;

    public PyMethodHook(String pluginId, PyObject callback) {
        this.pluginId = pluginId;
        this.callback = callback;
    }

    public PyMethodHook(String pluginId, PyObject callback, int priority) {
        super(priority);
        this.pluginId = pluginId;
        this.callback = callback;
    }

    public void setBeforeHookedFilters(List<Object> filters) {
        this.beforeFilters = filters == null ? null : new ArrayList<>(filters);
    }

    public void setAfterHookedFilters(List<Object> filters) {
        this.afterFilters = filters == null ? null : new ArrayList<>(filters);
    }

    public List<Object> getBeforeHookedFilters() {
        return beforeFilters;
    }

    public List<Object> getAfterHookedFilters() {
        return afterFilters;
    }

    public String getPluginId() {
        return pluginId;
    }

    @Override
    protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
        call("before_hooked_method", param);
    }

    @Override
    protected void afterHookedMethod(MethodHookParam param) throws Throwable {
        call("after_hooked_method", param);
    }

    protected void call(String method, MethodHookParam param) {
        if (callback == null) {
            return;
        }
        try {
            if (callback.containsKey(method)) {
                callback.callAttr(method, param);
            }
        } catch (Throwable e) {
            org.telegram.messenger.FileLog.e("плагин " + pluginId + ": " + method + " упал", e);
        }
    }
}
