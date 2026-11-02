package com.th3nekit.finegram.camera;

import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.os.Handler;
import android.util.Size;
import android.view.Surface;

import java.util.ArrayList;
import java.util.List;

public final class Camera2LensSessions {
    private Camera2LensSessions() {}

    public static boolean supports(Camera2LensCatalog.Route route, Size... sizes) {
        StreamConfigurationMap map = route.characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if (map == null) return false;
        Size[] outputs = map.getOutputSizes(SurfaceTexture.class);
        if (outputs == null) return false;
        for (Size size : sizes) {
            boolean found = false;
            for (Size output : outputs) if (output.equals(size)) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }

    public static boolean supportsJpeg(Camera2LensCatalog.Route route, Size size) {
        StreamConfigurationMap map = route.characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if (map == null) return false;
        Size[] outputs = map.getOutputSizes(android.graphics.ImageFormat.JPEG);
        if (outputs != null) for (Size output : outputs) if (output.equals(size)) return true;
        return false;
    }

    public static void create(CameraDevice device, List<Surface> surfaces,
                              Camera2LensCatalog.Route route,
                              CameraCaptureSession.StateCallback callback, Handler handler)
            throws CameraAccessException {
        if (route != null && route.physicalId != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            ArrayList<OutputConfiguration> outputs = new ArrayList<>();
            for (Surface surface : surfaces) {
                OutputConfiguration output = new OutputConfiguration(surface);
                output.setPhysicalCameraId(route.physicalId);
                outputs.add(output);
            }
            device.createCaptureSession(new SessionConfiguration(SessionConfiguration.SESSION_REGULAR,
                    outputs, handler::post, callback));
        } else {
            device.createCaptureSession(surfaces, callback, handler);
        }
    }
}
