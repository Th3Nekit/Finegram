/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 *
 * Based on Cherrygram (Copyright github.com/arsLan4k1390, 2022-2026),
 * itself based on Telegram for Android (github.com/DrKLO/Telegram).
 */

package com.th3nekit.finegram.core;

import com.th3nekit.finegram.core.ui.FGTitles;
import static org.telegram.messenger.LocaleController.getString;

import android.app.Activity;
import android.hardware.biometrics.BiometricManager;
import android.hardware.fingerprint.FingerprintManager;
import android.os.Build;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.biometric.BiometricPrompt;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.FragmentActivity;

import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.FingerprintController;
import org.telegram.messenger.R;
import org.telegram.messenger.support.fingerprint.FingerprintManagerCompat;

import java.util.ArrayList;

import com.th3nekit.finegram.core.configs.FinegramDebugConfig;
import com.th3nekit.finegram.core.configs.FinegramPrivacyConfig;

public class FGBiometricPrompt {

    private static BiometricPrompt.PromptInfo createPromptInfo() {
        BiometricPrompt.PromptInfo.Builder builder = new BiometricPrompt.PromptInfo.Builder();
        builder.setTitle(FGTitles.appName());
        if (!FinegramPrivacyConfig.INSTANCE.getAllowSystemPasscode()) {
            builder.setNegativeButtonText(getString(R.string.Cancel));
        }

        builder.setDeviceCredentialAllowed(FinegramPrivacyConfig.INSTANCE.getAllowSystemPasscode());
        builder.setConfirmationRequired(false);

        return builder.build();
    }

    private static BiometricPrompt.AuthenticationCallback createCallback(
            java.util.function.Consumer<BiometricPrompt.AuthenticationResult> onSuccess,
            Runnable onFailed,
            java.util.function.BiConsumer<Integer, CharSequence> onError
    ) {
        return new BiometricPrompt.AuthenticationCallback() {
            @Override
            public void onAuthenticationSucceeded(@NonNull BiometricPrompt.AuthenticationResult result) {
                if (onSuccess != null) onSuccess.accept(result);
            }

            @Override
            public void onAuthenticationFailed() {
                if (onFailed != null) onFailed.run();
            }

            @Override
            public void onAuthenticationError(int errorCode, @NonNull CharSequence errString) {
                if (onError != null) onError.accept(errorCode, errString);
            }
        };
    }

    public static void callBiometricPrompt(Activity activity, FGBiometricListener listener) {
        BiometricPrompt prompt = new BiometricPrompt(
                (FragmentActivity) activity,
                ContextCompat.getMainExecutor(activity),
                createCallback(
                        listener::onSuccess,
                        listener::onFailed,
                        listener::onError
                )
        );
        prompt.authenticate(createPromptInfo());
    }

    public static void prompt(Activity activity, Runnable successCallback) {
        prompt(activity, successCallback, null);
    }

    public static void prompt(Activity activity, Runnable successCallback, Runnable failCallback) {
        FGBiometricPrompt.callBiometricPrompt(activity, new FGBiometricPrompt.FGBiometricListener() {
            @Override
            public void onSuccess(BiometricPrompt.AuthenticationResult result) {
                if (successCallback != null) successCallback.run();
                if (FinegramDebugConfig.INSTANCE.getShowRPCErrors())
                    Toast.makeText(activity, "Success", Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onFailed() {
                if (failCallback != null) failCallback.run();
                if (FinegramDebugConfig.INSTANCE.getShowRPCErrors())
                    Toast.makeText(activity, "Fail", Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onError(int error, CharSequence msg) {
                if (failCallback != null) failCallback.run();
                if (FinegramDebugConfig.INSTANCE.getShowRPCErrors())
                    Toast.makeText(activity, msg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    public interface FGBiometricListener {
        void onSuccess(BiometricPrompt.AuthenticationResult result);
        void onFailed();
        void onError(int error, CharSequence msg);
    }

    public static boolean hasBiometricEnrolled() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            BiometricManager biometricManager = ApplicationLoader.applicationContext.getSystemService(BiometricManager.class);
            if (biometricManager == null) {
                return false;
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                return biometricManager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK) == BiometricManager.BIOMETRIC_SUCCESS;
            } else {
                return biometricManager.canAuthenticate() == BiometricManager.BIOMETRIC_SUCCESS;
            }
        } else {
            return hasEnrolledFingerprints();
        }
    }

    public static boolean hasEnrolledFingerprints() {
        FingerprintManager fingerprintManager = ApplicationLoader.applicationContext.getSystemService(FingerprintManager.class);
        if (fingerprintManager != null) {
            try {
                return fingerprintManager.isHardwareDetected() && fingerprintManager.hasEnrolledFingerprints();
            } catch (SecurityException e) {
                FinegramLogger.e(e);
                return false;
            }
        } else {
            try {
                FingerprintManagerCompat compat = FingerprintManagerCompat.from(ApplicationLoader.applicationContext);
                return compat.isHardwareDetected() && compat.hasEnrolledFingerprints();
            } catch (Throwable e) {
                FinegramLogger.e(e);
                return false;
            }
        }
    }

    public static int getBiometricIconResId() {
        boolean hasFingerprint = hasEnrolledFingerprints();
        boolean hasFace = false;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            BiometricManager bm = ApplicationLoader.applicationContext.getSystemService(BiometricManager.class);
            if (bm != null && bm.canAuthenticate() == BiometricManager.BIOMETRIC_SUCCESS && !hasFingerprint) {
                hasFace = true;
            } else if (bm != null && bm.canAuthenticate() == BiometricManager.BIOMETRIC_SUCCESS && hasFingerprint) {
                hasFace = true;
            }
        }

        if (hasFingerprint && hasFace) return R.drawable.fingerprint;
        if (hasFingerprint) return R.drawable.fingerprint;
        if (hasFace) return R.drawable.face_scan_square_filled_solar;
        return R.drawable.fingerprint;
    }

    private static final ArrayList<BiometricPrompt> pendingAuths = new ArrayList<>();
    private static final String TAG = "FGBiometricPrompt";

    public static void fixFingerprint(Activity activity, FGBiometricListener callback) {

        FingerprintController.checkKeyReady();
        FingerprintController.deleteInvalidKey();
        FingerprintController.checkKeyReady();

        cancelPendingAuthentications();

        BiometricPrompt prompt = new BiometricPrompt(
                (FragmentActivity) activity,
                ContextCompat.getMainExecutor(activity),
                createCallback(
                        result -> {
                            FinegramLogger.d(TAG, () -> "PasscodeView onAuthenticationSucceeded");
                            if (FingerprintController.isKeyReady() && FingerprintController.checkDeviceFingerprintsChanged()) {
                                FingerprintController.deleteInvalidKey();
                            }
                            callback.onSuccess(result);
                        },
                        callback::onFailed,
                        (code, errStr) -> {
                            FinegramLogger.d(TAG, () -> "PasscodeView onAuthenticationError: " + code + " \"" + errStr + "\"");
                            callback.onError(code, errStr);
                        }
                )
        );

        pendingAuths.add(prompt);
        prompt.authenticate(createPromptInfo());
    }

    public static void cancelPendingAuthentications() {
        for (BiometricPrompt prompt : pendingAuths) {
            prompt.cancelAuthentication();
        }
        pendingAuths.clear();
    }

    private static boolean isFirstCheck = true;
    private static boolean fingerprintCachedState = false;

    public static boolean hasFingerprintCached() {
        if (isFirstCheck) {
            reloadFingerprintState();
        }

        return fingerprintCachedState;
    }

    public static void reloadFingerprintState() {
        isFirstCheck = false;
        fingerprintCachedState = hasFingerprintInternal();
    }

    private static boolean hasFingerprintInternal() {
        try {
            FinegramLogger.d(TAG, () -> "Starting fingerprint check...");

            FingerprintManagerCompat fingerprintManager = FingerprintManagerCompat.from(ApplicationLoader.applicationContext);

            boolean conditions = fingerprintManager.isHardwareDetected();
            boolean finalConditions = conditions;
            FinegramLogger.d(TAG, () -> "Fingerprint hardware detected: " + finalConditions);

            conditions &= fingerprintManager.hasEnrolledFingerprints();
            FinegramLogger.d(TAG, () -> "Enrolled fingerprints: " + fingerprintManager.hasEnrolledFingerprints());

            conditions &= FingerprintController.isKeyReady();
            FinegramLogger.d(TAG, () -> "Fingerprint key ready: " + FingerprintController.isKeyReady());

            conditions &= !FingerprintController.checkDeviceFingerprintsChanged();
            FinegramLogger.d(TAG, () -> "Device fingerprints changed: " + !FingerprintController.checkDeviceFingerprintsChanged());

            FinegramLogger.d(TAG, () -> "Final fingerprint check result: " + finalConditions);
            return conditions;
        } catch (Throwable e) {
            FinegramLogger.e(TAG, () -> "Error checking fingerprint availability", e);
        }
        return false;
    }

}
