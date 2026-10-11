/*
 * This is the source code of Telegram for Android v. 7.x.x.
 * It is licensed under GNU GPL v. 2 or later.
 * You should have received a copy of the license in this archive (see LICENSE).
 *
 * Copyright Nikolai Kudashov, 2013-2020.
 */

package org.telegram.messenger;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import com.th3nekit.finegram.core.configs.FinegramCoreConfig;
import com.th3nekit.finegram.Extra;
import com.th3nekit.finegram.misc.Constants;

import com.android.billingclient.api.ProductDetails;

import java.util.Objects;

public class BuildVars {

    public static boolean DEBUG_VERSION = BuildConfig.DEBUG_VERSION;
    public static boolean LOGS_ENABLED = BuildConfig.DEBUG_VERSION;
    public static boolean DEBUG_PRIVATE_VERSION = BuildConfig.DEBUG_PRIVATE_VERSION;
    public static boolean USE_CLOUD_STRINGS = true;
    public static boolean CHECK_UPDATES = !FinegramCoreConfig.isPlayStoreBuild();
    public static boolean NO_SCOPED_STORAGE = Build.VERSION.SDK_INT <= 29;
    public static String BUILD_VERSION_STRING = BuildConfig.BUILD_VERSION_STRING;

    public static int BUILD_VERSION = parseBuildCode();

    public static boolean IS_LITE_VERSION = false;

    public static String GITHUB_APP_URL = "https://t.me/th3nek1t_projects";

    private static int parseBuildCode() {
        try {
            return Integer.parseInt(BuildConfig.BUILD_CODE_VERSION);
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    public static int APP_ID = 4;
    public static String APP_HASH = "014b35b6184100b085b0d0572f9b5103";

    public static String SAFETYNET_KEY = "";
    public static String PLAYSTORE_APP_URL = "https://play.google.com/store/apps/details?id=org.telegram.messenger";
    public static String HUAWEI_STORE_URL = "https://appgallery.huawei.com/app/C101184875";
    public static String GOOGLE_AUTH_CLIENT_ID = "119732785963-ooptebss6v859a1ojsqn05j6oejon5ug.apps.googleusercontent.com";

    public static String HUAWEI_APP_ID = "106911607";

    public static boolean IS_BILLING_UNAVAILABLE = FinegramCoreConfig.isPlayStoreBuild();

    public static boolean SUPPORTS_PASSKEYS = Build.VERSION.SDK_INT >= 34;

    static {
        APP_ID = Extra.INSTANCE.getAppID();
        APP_HASH = Extra.INSTANCE.getAppHash();
        PLAYSTORE_APP_URL = Constants.UPDATE_APP_URL;
        HUAWEI_STORE_URL = Constants.UPDATE_APP_URL;

        if (ApplicationLoader.applicationContext != null) {
            SharedPreferences sharedPreferences = ApplicationLoader.applicationContext.getSharedPreferences("systemConfig", Context.MODE_PRIVATE);
            LOGS_ENABLED = DEBUG_VERSION || sharedPreferences.getBoolean("logsEnabled", DEBUG_VERSION);
            if (LOGS_ENABLED) {
                final Thread.UncaughtExceptionHandler pastHandler = Thread.getDefaultUncaughtExceptionHandler();
                Thread.setDefaultUncaughtExceptionHandler((thread, exception) -> {
                    FileLog.fatal(exception, false);
                    if (pastHandler != null) {
                        pastHandler.uncaughtException(thread, exception);
                    }
                });
            }
        }
    }

    public static boolean useInvoiceBilling() {
        return BillingController.billingClientEmpty || DEBUG_VERSION && false || ApplicationLoader.isStandaloneBuild() || isBetaApp() && false || isHuaweiStoreApp() || hasDirectCurrency();
    }

    private static boolean hasDirectCurrency() {
        if (!BillingController.getInstance().isReady() || BillingController.PREMIUM_PRODUCT_DETAILS == null) {
            return false;
        }
        for (ProductDetails.SubscriptionOfferDetails offerDetails : BillingController.PREMIUM_PRODUCT_DETAILS.getSubscriptionOfferDetails()) {
            for (ProductDetails.PricingPhase phase : offerDetails.getPricingPhases().getPricingPhaseList()) {
                for (String cur : MessagesController.getInstance(UserConfig.selectedAccount).directPaymentsCurrency) {
                    if (Objects.equals(phase.getPriceCurrencyCode(), cur)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    public static boolean isBetaApp() {

        return FinegramCoreConfig.isStandaloneBetaBuild();
    }

    public static boolean isHuaweiStoreApp() {
        return ApplicationLoader.isHuaweiStoreBuild();
    }

    public static String getSmsHash() {

        return Extra.SMS_HASH;
    }
}
