/**
 * This is the source code of Finegram for Android.
 * It is licensed under GNU GPL v. 2 or later.
 */

package com.exteragram.messenger;

import com.th3nekit.finegram.core.configs.FinegramAppearanceConfig;
import com.th3nekit.finegram.core.configs.FinegramCameraConfig;
import com.th3nekit.finegram.core.ui.FGAvatars;

public final class ExteraConfig {

    public static boolean pluginsSafeMode = false;

    private static boolean pluginsEngine = true;

    private static boolean forceSnow;

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

    public static float getAvatarCorners() {
        return FinegramAppearanceConfig.INSTANCE.getAvatarCorners();
    }

    public static void setAvatarCorners(float value) {
        FinegramAppearanceConfig.INSTANCE.setAvatarCorners(value);
    }

    public static int getAvatarCorners(float size) {
        return FGAvatars.corners(size, false);
    }

    public static int getAvatarCorners(float size, boolean inPixels) {
        return FGAvatars.corners(size, inPixels);
    }

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

    public static java.util.ArrayList<Integer> getMainMenuLayout() {
        return new java.util.ArrayList<>();
    }

    public static java.util.ArrayList<Integer> getMainMenuHiddenItems() {
        return new java.util.ArrayList<>();
    }

    private ExteraConfig() {
    }
}
