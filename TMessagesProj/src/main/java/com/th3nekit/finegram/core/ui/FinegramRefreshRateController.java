package com.th3nekit.finegram.core.ui;

import android.app.Activity;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.view.Display;
import android.view.Window;
import android.view.WindowManager;


import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;

import java.util.ArrayList;
import java.util.WeakHashMap;

public final class FinegramRefreshRateController implements Application.ActivityLifecycleCallbacks,
        DisplayManager.DisplayListener, SharedPreferences.OnSharedPreferenceChangeListener {

    private static final String PREFERENCE = "FG_UseDisplayRefreshRate";
    private static FinegramRefreshRateController instance;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final WeakHashMap<Activity, Float> resumed = new WeakHashMap<>();
    private final PowerManager powerManager;
    private boolean powerSave;

    private final BroadcastReceiver powerReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (PowerManager.ACTION_POWER_SAVE_MODE_CHANGED.equals(intent.getAction())) {
                powerSave = readPowerSave(powerManager);
                refreshAll();
            }
        }
    };

    private FinegramRefreshRateController(Application application) {
        PowerManager manager;
        try {
            manager = (PowerManager) application.getSystemService(Context.POWER_SERVICE);
        } catch (RuntimeException ignored) {
            manager = null;
        }
        powerManager = manager;
        powerSave = readPowerSave(powerManager);
    }

    public static void install(Application application) {
        if (application == null || instance != null) {
            return;
        }
        FinegramRefreshRateController controller = new FinegramRefreshRateController(application);
        instance = controller;
        application.registerActivityLifecycleCallbacks(controller);
        application.getSharedPreferences("mainconfig", Context.MODE_PRIVATE)
                .registerOnSharedPreferenceChangeListener(controller);
        try {
            DisplayManager manager = (DisplayManager) application.getSystemService(Context.DISPLAY_SERVICE);
            if (manager != null) {
                manager.registerDisplayListener(controller, controller.handler);
            }
        } catch (RuntimeException ignored) {
        }
        try {
            IntentFilter filter = new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED);
            if (Build.VERSION.SDK_INT >= 33) {
                application.registerReceiver(controller.powerReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
            } else {
                application.registerReceiver(controller.powerReceiver, filter);
            }
        } catch (RuntimeException ignored) {
        }
    }


    public static void refresh(Activity activity) {
        FinegramRefreshRateController controller = instance;
        if (controller == null || activity == null) {
            return;
        }
        if (Looper.myLooper() == Looper.getMainLooper()) {
            controller.update(activity);
        } else {
            controller.handler.post(() -> controller.update(activity));
        }
    }

    private static boolean readPowerSave(PowerManager manager) {
        try {
            return manager == null || manager.isPowerSaveMode();
        } catch (RuntimeException ignored) {
            return true;
        }
    }

    private static boolean validRate(float rate) {
        return rate > 0f && !Float.isNaN(rate) && !Float.isInfinite(rate);
    }

    private static float supportedRate(Display display, float requestedRate) {
        if (display == null || !validRate(requestedRate)) {
            return 0f;
        }
        try {
            Display.Mode current = display.getMode();
            if (current == null) {
                return 0f;
            }
            float best = 0f;
            float currentRate = current.getRefreshRate();
            if (validRate(currentRate) && currentRate <= requestedRate + 0.2f) {
                best = currentRate;
            }
            Display.Mode[] modes = display.getSupportedModes();
            if (modes != null) {
                for (Display.Mode mode : modes) {
                    if (mode == null || mode.getPhysicalWidth() != current.getPhysicalWidth()
                            || mode.getPhysicalHeight() != current.getPhysicalHeight()) {
                        continue;
                    }
                    float rate = mode.getRefreshRate();
                    if (validRate(rate) && rate > best && rate <= requestedRate + 0.2f) {
                        best = rate;
                    }
                }
            }
            return best;
        } catch (RuntimeException ignored) {
            return 0f;
        }
    }

    private void refreshAll() {
        for (Activity activity : new ArrayList<>(resumed.keySet())) {
            update(activity);
        }
    }

    private void update(Activity activity) {
        if (activity == null || activity.isFinishing() || activity.isDestroyed() || !resumed.containsKey(activity)) {
            return;
        }
        try {
            Window window = activity.getWindow();
            if (window == null || window.getDecorView() == null) {
                return;
            }
            Display display = window.getDecorView().getDisplay();
            if (display != null && (activity.hasWindowFocus() || resumed.size() == 1)) {
                float actualRate = display.getRefreshRate();
                if (validRate(actualRate)) {
                    AndroidUtilities.screenRefreshRate = actualRate;
                    AndroidUtilities.screenRefreshTime = 1000f / actualRate;
                    float maximumRate = supportedRate(display, Float.MAX_VALUE);
                    AndroidUtilities.screenMaxRefreshRate = maximumRate > 0f ? maximumRate : actualRate;
                }
            }
            resumed.put(activity, Float.NaN);
        } catch (RuntimeException ignored) {
        }
    }

    private void release(Activity activity) {
        Float lastRate = resumed.remove(activity);
        if (lastRate == null || Float.isNaN(lastRate)) {
            return;
        }
        try {
            Window window = activity.getWindow();
            if (window == null) {
                return;
            }
            WindowManager.LayoutParams params = window.getAttributes();
            if (params.preferredDisplayModeId == 0 && params.preferredRefreshRate > 0f
                    && Math.abs(params.preferredRefreshRate - lastRate) <= 0.2f) {
                params.preferredRefreshRate = 0f;
                window.setAttributes(params);
            }
        } catch (RuntimeException ignored) {
        }
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences preferences, String key) {
        if (key == null || PREFERENCE.equals(key)) {
            handler.post(this::refreshAll);
        }
    }

    @Override
    public void onDisplayChanged(int displayId) {
        refreshAll();
    }

    @Override
    public void onDisplayAdded(int displayId) {
        refreshAll();
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        refreshAll();
    }

    @Override
    public void onActivityResumed(Activity activity) {
        powerSave = readPowerSave(powerManager);
        resumed.put(activity, Float.NaN);
        update(activity);
        handler.post(() -> update(activity));
    }

    @Override
    public void onActivityPaused(Activity activity) {
        release(activity);
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        release(activity);
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
    }

    @Override
    public void onActivityStarted(Activity activity) {
    }

    @Override
    public void onActivityStopped(Activity activity) {
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
    }
}
