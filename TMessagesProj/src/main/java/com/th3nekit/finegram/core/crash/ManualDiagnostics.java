package com.th3nekit.finegram.core.crash;

import static org.telegram.messenger.LocaleController.getString;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Process;
import android.view.Gravity;
import android.widget.FrameLayout;

import com.th3nekit.finegram.preferences.helpers.SettingsHelper;

import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.BuildConfig;
import org.telegram.messenger.BuildVars;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.ActionBar.Theme;
import org.telegram.ui.Components.LayoutHelper;
import org.telegram.ui.Components.UItem;
import org.telegram.utils.settings.SharedSettings;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;

public final class ManualDiagnostics {
    private static final AtomicBoolean EXPORTING = new AtomicBoolean();

    public static void addItems(BaseFragment fragment, ArrayList<UItem> items, int recordId, int shareId) {
        items.add(UItem.asHeader(getString(R.string.FG_Diagnostics)));
        Context context = fragment.getContext();
        if (context != null) items.add(UItem.asCustom(privacyCard(context, fragment.getResourceProvider())));
        items.add(SettingsHelper.asSwitchCG(recordId, getString(R.string.FG_Diagnostics_Record),
                getString(R.string.FG_Diagnostics_Record_Desc)).setChecked(CameraLogUpload.isEnabled()));
        items.add(UItem.asButton(shareId, getString(R.string.FG_Diagnostics_Share)));
        items.add(UItem.asShadow(getString(R.string.FG_Diagnostics_Desc)));
    }

    private static FrameLayout privacyCard(Context context, Theme.ResourcesProvider resourcesProvider) {
        FrameLayout frame = new FrameLayout(context);
        frame.addView(new CameraReportPrivacyView(context,
                        Theme.getColor(Theme.key_windowBackgroundWhite, resourcesProvider),
                        Theme.getColor(Theme.key_windowBackgroundWhiteBlackText, resourcesProvider),
                        Theme.getColor(Theme.key_windowBackgroundWhiteGrayText, resourcesProvider),
                        Theme.getColor(Theme.key_featuredStickers_addButton, resourcesProvider)),
                LayoutHelper.createFrame(LayoutHelper.MATCH_PARENT, LayoutHelper.WRAP_CONTENT, Gravity.TOP, 16, 4, 16, 10));
        return frame;
    }

    public static boolean onClick(BaseFragment fragment, UItem item, int recordId, int shareId, Runnable refresh) {
        if (item.id == recordId) {
            CameraLogUpload.setEnabled(!CameraLogUpload.isEnabled());
            refresh.run();
            return true;
        }
        if (item.id == shareId) {
            if (CameraLogUpload.isEnabled()) capture(fragment);
            else if (fragment.getParentActivity() != null) fragment.showDialog(new AlertDialog.Builder(fragment.getParentActivity(), fragment.getResourceProvider())
                    .setTitle(getString(R.string.FG_Diagnostics_Share))
                    .setMessage(getString(R.string.FG_Diagnostics_ManualConfirm))
                    .setPositiveButton(getString(R.string.Send), (dialog, which) -> capture(fragment))
                    .setNegativeButton(getString(R.string.Cancel), null).create());
            return true;
        }
        return false;
    }

    private static void capture(BaseFragment fragment) {
        Activity activity = fragment.getParentActivity();
        if (activity == null || fragment.isPaused() || !EXPORTING.compareAndSet(false, true)) return;
        AlertDialog progress = new AlertDialog(activity, 3, fragment.getResourceProvider());
        progress.setCanCancel(false);
        progress.setMessage(getString(R.string.FG_Diagnostics_Preparing));
        if (fragment.showDialog(progress) == null) { EXPORTING.set(false); return; }
        CameraLogUpload.sendNow(queued -> AndroidUtilities.runOnUIThread(() -> {
            EXPORTING.set(false);
            progress.dismiss();
            if (fragment.getParentActivity() != activity || activity.isFinishing() || activity.isDestroyed()
                    || fragment.isPaused() || !fragment.isLastFragment()) return;
            fragment.showDialog(new AlertDialog.Builder(activity, fragment.getResourceProvider())
                    .setTitle(getString(R.string.FG_Diagnostics))
                    .setMessage(getString(queued ? R.string.FG_Diagnostics_Queued : R.string.FG_Diagnostics_Error))
                    .setPositiveButton(getString(R.string.OK), null).create());
        }));
    }

    static String snapshot(Context context) {
        StringBuilder out = new StringBuilder("Finegram ").append(BuildConfig.BUILD_VERSION_STRING_FINEGRAM)
                .append("\nTelegram ").append(BuildVars.BUILD_VERSION_STRING)
                .append("\nBuild date: ").append(BuildConfig.GET_BUILD_DATE)
                .append("\nAndroid API: ").append(Build.VERSION.SDK_INT)
                .append("\nDevice: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append("\nABIs: ").append(Arrays.toString(Build.SUPPORTED_ABIS))
                .append("\nPID: ").append(Process.myPid())
                .append("\nGlobal logging: ").append(BuildVars.LOGS_ENABLED)
                .append("\nCamera type values: 0=Telegram, 1=CameraX, 2=Camera2, 3=System; unset=automatic\n")
                .append("Round engine setting: ").append(SharedSettings.roundVideoCamera2Enabled.isSet()
                        ? (SharedSettings.roundVideoCamera2Enabled.get() ? "Camera2" : "legacy") : "automatic (see camera events)")
                .append("\nRuntime engine, active IDs and torch transitions: see camera-events.txt\n");
        SharedPreferences prefs = context.getSharedPreferences("mainconfig", Context.MODE_PRIVATE);
        String[] booleans = {"CP_DisableAttachCam", "CP_UseDualCameraX", "FG_CameraCompatibility", "CP_StartFromUltraWideCam",
                "CP_VideoStabilisation", "CP_OpticalStabilisation", "CP_ContinuousAutofocus", "CP_NoiceReduction", "CP_FaceDetection",
                "CP_BokehEffect", "CP_LensSwitcher", "FG_CameraControlsRight", "FG_RoundZoomScale", "FG_SmoothCameraTransitions",
                "FG_RoundLogicalDisabled", "FG_NewCameraAudio", "FG_ManualCameraId", "FG_RoundLastRear", "FG_RoundTapFocus",
                "FG_RoundDoubleTapSwitch", "FG_VideoMessageWatermark"};
        String[] integers = {"CP_CameraType", "CP_CameraAspectRatio", "CP_CameraResolution", "CP_CameraXFpsRangeValueF",
                "FG_CameraExposureIndex", "CP_ExposureSlider", "FG_RoundStartCamera", "FG_Round_Video_Resolution",
                "FG_RoundVideoBitrate", "FG_RoundAudioBitrate"};
        String[] ids = {"FG_ManualCameraIdFront", "FG_ManualCameraIdRear", "FG_ManualCameraIdUltraWide"};
        for (String key : booleans) setting(out, prefs, key, 0);
        for (String key : integers) setting(out, prefs, key, 1);
        for (String key : ids) setting(out, prefs, key, 2);
        return out.toString();
    }

    private static void setting(StringBuilder out, SharedPreferences prefs, String key, int type) {
        out.append(key).append('=');
        try {
            if (!prefs.contains(key)) out.append("unset (app default)");
            else if (type == 0) out.append(prefs.getBoolean(key, false));
            else if (type == 1) out.append(prefs.getInt(key, 0));
            else {
                String value = prefs.getString(key, "");
                if (value == null || value.length() > 128 || value.indexOf('\n') >= 0 || value.indexOf('\r') >= 0) out.append("invalid camera ID");
                else out.append(DiagnosticText.clean(value));
            }
        } catch (ClassCastException e) {
            out.append("invalid stored type");
        }
        out.append('\n');
    }

    private ManualDiagnostics() {}
}
