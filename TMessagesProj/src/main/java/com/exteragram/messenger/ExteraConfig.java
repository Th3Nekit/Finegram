/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.core.configs.FinegramCameraConfig;
import com.th3nekit.finegram.core.ui.FGAvatars;

/**
 * Общие настройки клиента под теми именами, под которыми их читают плагины.
 *
 * Полного набора здесь нет и не будет — только то, что плагины действительно спрашивают.
 * Где у нас есть своя настройка с тем же смыслом, отдаём её значение.
 */
public final class ExteraConfig {

    /**
     * Безопасный режим плагинов: в нём плагин не должен трогать чужие данные.
     *
     * У нас такого режима нет, плагины работают на общих правах, поэтому всегда false.
     */
    public static boolean pluginsSafeMode = false;

    /** Движок плагинов включён: у нас он есть всегда. */
    private static boolean pluginsEngine = true;

    private static boolean forceSnow;

    /** Настройки читаются по требованию, заранее поднимать нечего. */
    public static void init() {
    }

    public static boolean getPluginsEngine() {
        return pluginsEngine;
    }

    public static void setPluginsEngine(boolean value) {
        pluginsEngine = value;
    }

    public static boolean getForceSnow() {
        return forceSnow;
    }

    public static void setForceSnow(boolean value) {
        forceSnow = value;
    }

    // --- скругление аватарок: наша настройка из «Внешнего вида» ---

    /** Скругление в условных единицах: 0 — квадрат, 28 и больше — круг при размере 56. */
    public static float getAvatarCorners() {
        return FinegramAppearanceConfig.INSTANCE.getAvatarCorners();
    }

    public static void setAvatarCorners(float value) {
        FinegramAppearanceConfig.INSTANCE.setAvatarCorners(value);
    }

    /** Радиус для аватарки размером size (в dp) — в пикселях. */
    public static int getAvatarCorners(float size) {
        return FGAvatars.corners(size, false);
    }

    /** То же; inPixels — размер уже в пикселях. */
    public static int getAvatarCorners(float size, boolean inPixels) {
        return FGAvatars.corners(size, inPixels);
    }

    /** С учётом того, что у форумов аватарки квадратнее. */
    public static int getAvatarCorners(float size, boolean inPixels, boolean forum) {
        return FGAvatars.cornersForChat(size, forum, inPixels);
    }

    public static int getAvatarCorners(float size, boolean inPixels, boolean forum, boolean community) {
        return FGAvatars.cornersForChat(size, forum, inPixels);
    }

    public static int getAvatarCorners(float size, boolean inPixels, AvatarCornerType type) {
        return FGAvatars.cornersForChat(size, type == AvatarCornerType.FORUM, inPixels);
    }

    public static int getAvatarCorners(float size, boolean inPixels, AvatarCornerType type, boolean ignored) {
        return getAvatarCorners(size, inPixels, type);
    }

    // --- камера ---

    public static CameraType getCameraType() {
        switch (FinegramCameraConfig.INSTANCE.getCameraType()) {
            case FinegramCameraConfig.CAMERA_X:
                return CameraType.CAMERA_X;
            case FinegramCameraConfig.CAMERA_2:
                return CameraType.CAMERA_2;
            default:
                return CameraType.CAMERA_1;
        }
    }

    public static void setCameraType(CameraType type) {
        if (type == null) {
            return;
        }
        switch (type) {
            case CAMERA_X:
                FinegramCameraConfig.INSTANCE.setCameraType(FinegramCameraConfig.CAMERA_X);
                break;
            case CAMERA_2:
                FinegramCameraConfig.INSTANCE.setCameraType(FinegramCameraConfig.CAMERA_2);
                break;
            default:
                FinegramCameraConfig.INSTANCE.setCameraType(FinegramCameraConfig.TELEGRAM_CAMERA);
                break;
        }
    }

    // --- боковое меню ---

    /**
     * Порядок и скрытые пункты бокового меню.
     *
     * Своё меню мы собираем сами и этих списков не ведём. Плагин по пустым спискам видит,
     * что вставлять ему некуда, и обходится без своего пункта — это лучше, чем падение на
     * отсутствующем методе.
     */
    public static java.util.ArrayList<Integer> getMainMenuLayout() {
        return new java.util.ArrayList<>();
    }

    public static java.util.ArrayList<Integer> getMainMenuHiddenItems() {
        return new java.util.ArrayList<>();
    }

    private ExteraConfig() {
    }
}
