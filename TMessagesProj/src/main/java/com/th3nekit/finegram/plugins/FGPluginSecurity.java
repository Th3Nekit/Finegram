package com.th3nekit.finegram.plugins;

import static org.telegram.messenger.LocaleController.formatString;
import static org.telegram.messenger.LocaleController.getString;

import org.json.JSONArray;
import org.json.JSONObject;
import org.telegram.messenger.AndroidUtilities;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.AlertDialog;
import org.telegram.ui.ActionBar.BaseFragment;
import org.telegram.ui.Components.BulletinFactory;
import java.io.File;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.LinkedHashSet;

public final class FGPluginSecurity {
    private FGPluginSecurity() {}

    public static JSONObject inspect(File file) {
        return inspect(file, true);
    }

    public static JSONObject inspect(File file, boolean check) {
        try {
            return new JSONObject(FGPluginsController.securityReport(file, check));
        } catch (Exception e) {
            return new JSONObject();
        }
    }

    public static boolean hasFindings(JSONObject report) {
        JSONArray findings = report.optJSONArray("findings");
        return report.optBoolean("requires_review", true) || (findings != null && findings.length() > 0);
    }

    public static boolean canInstall(JSONObject reviewed, JSONObject current, boolean acceptsRisk) {
        String fingerprint = reviewed.optString("fingerprint");
        return reviewed.optBoolean("reviewable") && current.optBoolean("reviewable")
                && !fingerprint.isEmpty() && fingerprint.equals(current.optString("fingerprint"))
                && (!current.optBoolean("requires_review", true) || acceptsRisk);
    }

    public static byte[] reviewedBytes(File file, String fingerprint, boolean acceptsRisk) {
        return reviewedBytes(file, fingerprint, acceptsRisk, false);
    }

    public static byte[] reviewedBytes(File file, String fingerprint, boolean acceptsRisk, boolean installDisabled) {
        File snapshot = null;
        try {
            if (fingerprint == null || fingerprint.isEmpty() || !file.isFile()) return null;
            byte[] bytes;
            try (FileInputStream input = new FileInputStream(file);
                 ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[8192];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    if (output.size() + count > 32 * 1024 * 1024) return null;
                    output.write(buffer, 0, count);
                }
                bytes = output.toByteArray();
            }
            String name = file.getName();
            int dot = name.lastIndexOf('.');
            String suffix = dot < 0 ? ".plugin" : name.substring(dot);
            snapshot = File.createTempFile("finegram-confirmed-", suffix,
                    ApplicationLoader.applicationContext.getCacheDir());
            try (FileOutputStream output = new FileOutputStream(snapshot)) {
                output.write(bytes);
            }
            JSONObject current = inspect(snapshot);
            JSONObject reviewed = new JSONObject().put("fingerprint", fingerprint).put("reviewable", true);
            if (!canInstall(reviewed, current, acceptsRisk || installDisabled)) return null;
            if (!installDisabled && current.optBoolean("requires_review", true)
                    && !FGPluginsController.approveSecurity(snapshot, fingerprint)) return null;
            return bytes;
        } catch (Exception e) {
            return null;
        } finally {
            if (snapshot != null) snapshot.delete();
        }
    }

    public static String findingsSummary(JSONObject report) {
        JSONArray findings = report.optJSONArray("findings");
        if (findings == null) return "";
        LinkedHashSet<String> reasons = new LinkedHashSet<>();
        for (int i = 0; i < findings.length(); i++) {
            JSONObject finding = findings.optJSONObject(i);
            if (finding != null) reasons.add(getString(reason(finding.optString("code"))));
        }
        StringBuilder text = new StringBuilder();
        for (String value : reasons) {
            if (text.length() > 0) text.append('\n');
            text.append("• ").append(value);
        }
        return text.toString();
    }

    public static void reviewOrFail(BaseFragment fragment, FGPluginsController.Plugin plugin,
                                    Runnable retry, Runnable failure) {
        reviewOrFail(fragment, plugin.getFile(), plugin.getName(), retry, failure);
    }

    public static void reviewOrFail(BaseFragment fragment, File file, String name,
                                    Runnable retry, Runnable failure) {
        FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
            final JSONObject result = inspect(file);
            AndroidUtilities.runOnUIThread(() -> {
                if (fragment.isFinished || fragment.getParentActivity() == null) return;
                if (!result.optBoolean("requires_review", true)) {
                    if (failure != null) failure.run();
                    return;
                }
                AlertDialog.Builder builder = new AlertDialog.Builder(fragment.getParentActivity(), fragment.getResourceProvider());
                builder.setTitle(getString(R.string.FG_Plugins_Security_Title));
                builder.setMessage(message(name, result));
                builder.setNegativeButton(getString(R.string.FG_Plugins_Security_KeepDisabled), null);
                if (result.optBoolean("reviewable")) {
                    final String fingerprint = result.optString("fingerprint");
                    builder.setPositiveButton(getString(R.string.FG_Plugins_Security_Allow), (dialog, which) ->
                            FGPluginsController.INSTANCE.getQueue().postRunnable(() -> {
                            boolean approved = FGPluginsController.approveSecurity(file, fingerprint);
                                AndroidUtilities.runOnUIThread(() -> {
                                    if (fragment.isFinished || fragment.getParentActivity() == null) return;
                                    if (approved) {
                                        retry.run();
                                    } else {
                                        BulletinFactory.of(fragment).createErrorBulletin(
                                                getString(R.string.FG_Plugins_Security_Changed)).show();
                                    }
                                });
                            }));
                }
                fragment.showDialog(builder.create());
            });
        });
    }

    private static String message(String name, JSONObject report) {
        if (!report.optBoolean("reviewable")) return getString(R.string.FG_Plugins_Security_Unreadable);
        String subject = report.optString("subject", name);
        StringBuilder text = new StringBuilder(formatString(R.string.FG_Plugins_Security_Info, shorten(subject, 100)));
        JSONArray findings = report.optJSONArray("findings");
        if (findings != null) {
            for (int i = 0; i < Math.min(findings.length(), 10); i++) {
                JSONObject finding = findings.optJSONObject(i);
                if (finding == null) continue;
                text.append("\n\n").append(getString(reason(finding.optString("code"))));
                String file = shorten(finding.optString("file"), 160);
                int line = finding.optInt("line");
                text.append("\n").append(file);
                if (line > 0) text.append(':').append(line);
                String detail = finding.optString("detail");
                if (!detail.isEmpty()) text.append(" · ").append(shorten(detail, 80));
            }
        }
        text.append("\n\n").append(getString(R.string.FG_Plugins_Security_Warning));
        return text.toString();
    }

    private static String shorten(String value, int length) {
        return value.length() <= length ? value : value.substring(0, length) + "…";
    }

    private static int reason(String code) {
        switch (code) {
            case "remote_code": return R.string.FG_Plugins_Security_RemoteCode;
            case "hidden_code": return R.string.FG_Plugins_Security_HiddenCode;
            case "system_command": return R.string.FG_Plugins_Security_SystemCommand;
            case "sensitive_import": return R.string.FG_Plugins_Security_SensitiveImport;
            case "session_access": return R.string.FG_Plugins_Security_SessionAccess;
            case "native_code": return R.string.FG_Plugins_Security_NativeCode;
            case "scan_failed": return R.string.FG_Plugins_Security_Unreadable;
            default: return R.string.FG_Plugins_Security_DynamicCode;
        }
    }
}
