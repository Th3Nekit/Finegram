/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.misc.widgets;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.widget.RemoteViews;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.R;

public class KaboomWidget extends AppWidgetProvider {

    @Override
    public void onReceive(Context context, Intent intent) {
        if (context == null || intent == null) return;
        super.onReceive(context, intent);
    }

    @Override
    public void onUpdate(Context context, AppWidgetManager appWidgetManager, int[] appWidgetIds) {
        if (context == null || appWidgetManager == null || appWidgetIds == null) return;
        for (int appWidgetId : appWidgetIds) {
            updateAppWidget(context, appWidgetManager, appWidgetId);
        }
    }

    @Override
    public void onAppWidgetOptionsChanged(Context context, AppWidgetManager appWidgetManager, int appWidgetId, Bundle newOptions) {
        if (context == null || appWidgetManager == null) return;
        updateAppWidget(context, appWidgetManager, appWidgetId);
        super.onAppWidgetOptionsChanged(context, appWidgetManager, appWidgetId, newOptions);
    }

    public static void updateAppWidget(Context context, AppWidgetManager appWidgetManager, int appWidgetId) {
        if (context == null || appWidgetManager == null) return;
        ApplicationLoader.postInitApplication();

        RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.kaboom_widget);

        Intent intent = new Intent(ApplicationLoader.applicationContext, KaboomWidgetActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);

        PendingIntent pendingIntent = PendingIntent.getActivity(
                ApplicationLoader.applicationContext,
                appWidgetId,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE
        );

        views.setOnClickPendingIntent(R.id.tvWidget, pendingIntent);
        appWidgetManager.updateAppWidget(appWidgetId, views);
    }

    public static void forceUpdateKaboomWidgets(Context context) {
        if (context == null) return;
        AppWidgetManager appWidgetManager = AppWidgetManager.getInstance(context);
        ComponentName thisWidget = new ComponentName(context, KaboomWidget.class);
        int[] allWidgetIds = appWidgetManager.getAppWidgetIds(thisWidget);
        for (int appWidgetId : allWidgetIds) {
            updateAppWidget(context, appWidgetManager, appWidgetId);
        }
    }

}