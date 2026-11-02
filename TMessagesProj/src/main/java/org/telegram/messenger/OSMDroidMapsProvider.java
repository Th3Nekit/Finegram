package org.telegram.messenger;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.location.Location;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.MotionEvent;
import android.view.View;

import androidx.core.content.ContextCompat;
import androidx.core.util.Consumer;

import org.osmdroid.api.IGeoPoint;
import org.osmdroid.config.Configuration;
import org.osmdroid.config.IConfigurationProvider;
import org.osmdroid.events.MapListener;
import org.osmdroid.events.ScrollEvent;
import org.osmdroid.events.ZoomEvent;
import org.osmdroid.tileprovider.tilesource.ITileSource;
import org.osmdroid.util.BoundingBox;
import org.osmdroid.util.GeoPoint;
import org.osmdroid.views.CustomZoomButtonsController;
import org.osmdroid.views.MapView;
import org.osmdroid.views.overlay.CopyrightOverlay;
import org.osmdroid.views.overlay.Marker;
import org.osmdroid.views.overlay.Polygon;
import org.osmdroid.views.overlay.TilesOverlay;
import org.osmdroid.views.overlay.mylocation.GpsMyLocationProvider;
import org.osmdroid.views.overlay.mylocation.IMyLocationConsumer;
import org.osmdroid.views.overlay.mylocation.IMyLocationProvider;
import org.osmdroid.views.overlay.mylocation.MyLocationNewOverlay;
import org.osmdroid.tileprovider.MapTileProviderBasic;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class OSMDroidMapsProvider implements IMapsProvider {

    private static boolean configured;

    public static boolean googleUsable(Context context) {
        try {
            ApplicationInfo info = context.getPackageManager().getApplicationInfo(context.getPackageName(), PackageManager.GET_META_DATA);
            Object key = info.metaData == null ? null : info.metaData.get("com.google.android.maps.v2.API_KEY");
            if (key == null || key.toString().trim().isEmpty()) {
                return false;
            }
            context.getPackageManager().getPackageInfo("com.google.android.gms", 0);
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    private static void configure(Context context) {
        if (configured) {
            return;
        }
        Context app = context.getApplicationContext();
        IConfigurationProvider config = Configuration.getInstance();
        config.load(app, PreferenceManager.getDefaultSharedPreferences(app));
        File base = new File(app.getCacheDir(), "osmdroid");
        base.mkdirs();
        config.setOsmdroidBasePath(base);
        File tiles = new File(base, "tiles");
        tiles.mkdirs();
        config.setOsmdroidTileCache(tiles);
        config.setUserAgentValue("Finegram/" + BuildVars.BUILD_VERSION_STRING + " (Android; osmdroid)");
        config.setTileFileSystemCacheMaxBytes(120L * 1024 * 1024);
        config.setTileFileSystemCacheTrimBytes(90L * 1024 * 1024);
        configured = true;
    }

    @Override
    public void initializeMaps(Context context) {
        configure(context);
    }

    @Override
    public IMapView onCreateMapView(Context context) {
        configure(context);
        return new OSMMapView(context);
    }

    @Override
    public IMarkerOptions onCreateMarkerOptions() {
        return new OSMMarkerOptions();
    }

    @Override
    public ICircleOptions onCreateCircleOptions() {
        return new OSMCircleOptions();
    }

    @Override
    public ILatLngBoundsBuilder onCreateLatLngBoundsBuilder() {
        return new OSMLatLngBoundsBuilder();
    }

    @Override
    public ICameraUpdate newCameraUpdateLatLng(LatLng latLng) {
        return new OSMCameraUpdate(new GeoPoint(latLng.latitude, latLng.longitude), -1, null, 0);
    }

    @Override
    public ICameraUpdate newCameraUpdateLatLngZoom(LatLng latLng, float zoom) {
        return new OSMCameraUpdate(new GeoPoint(latLng.latitude, latLng.longitude), zoom, null, 0);
    }

    @Override
    public ICameraUpdate newCameraUpdateLatLngBounds(ILatLngBounds bounds, int padding) {
        return new OSMCameraUpdate(null, -1, ((OSMLatLngBounds) bounds).box, padding);
    }

    @Override
    public IMapStyleOptions loadRawResourceStyle(Context context, int resId) {
        return new OSMMapStyleOptions();
    }

    @Override
    public String getMapsAppPackageName() {
        return "com.google.android.apps.maps";
    }

    @Override
    public int getInstallMapsString() {
        return R.string.InstallGoogleMaps;
    }

    private static GeoPoint point(LatLng latLng) {
        return new GeoPoint(latLng.latitude, latLng.longitude);
    }

    public static final class OSMCameraUpdate implements ICameraUpdate {
        final GeoPoint center;
        final float zoom;
        final BoundingBox box;
        final int padding;

        OSMCameraUpdate(GeoPoint center, float zoom, BoundingBox box, int padding) {
            this.center = center;
            this.zoom = zoom;
            this.box = box;
            this.padding = padding;
        }
    }

    public static final class OSMMapStyleOptions implements IMapStyleOptions {
    }

    public static final class OSMMapView implements IMapView {
        private final MapView mapView;
        private final OSMMapImpl map;
        private ITouchInterceptor dispatchInterceptor;
        private ITouchInterceptor interceptInterceptor;
        private Runnable onLayoutListener;
        boolean touching;

        OSMMapView(Context context) {
            mapView = new MapView(context, new MapTileProviderBasic(context.getApplicationContext(),
                    com.th3nekit.finegram.maps.FGMapTiles.base(MAP_TYPE_NORMAL, false))) {
                @Override
                public boolean dispatchTouchEvent(MotionEvent ev) {
                    int action = ev.getActionMasked();
                    if (action == MotionEvent.ACTION_DOWN) {
                        touching = true;
                    } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                        touching = false;
                        map.scheduleIdle();
                    }
                    if (dispatchInterceptor != null) {
                        return dispatchInterceptor.onInterceptTouchEvent(ev, super::dispatchTouchEvent);
                    }
                    return super.dispatchTouchEvent(ev);
                }

                @Override
                public boolean onInterceptTouchEvent(MotionEvent ev) {
                    if (interceptInterceptor != null) {
                        return interceptInterceptor.onInterceptTouchEvent(ev, super::onInterceptTouchEvent);
                    }
                    return super.onInterceptTouchEvent(ev);
                }

                @Override
                protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
                    super.onLayout(changed, left, top, right, bottom);
                    if (onLayoutListener != null) {
                        onLayoutListener.run();
                    }
                }
            };
            mapView.setTilesScaledToDpi(true);
            mapView.setMultiTouchControls(true);
            mapView.getZoomController().setVisibility(CustomZoomButtonsController.Visibility.NEVER);
            mapView.setVerticalMapRepetitionEnabled(false);
            mapView.setHorizontalMapRepetitionEnabled(true);
            mapView.setScrollableAreaLimitLatitude(MapView.getTileSystem().getMaxLatitude(), MapView.getTileSystem().getMinLatitude(), 0);
            mapView.setMinZoomLevel(2.0);
            mapView.setMaxZoomLevel(20.0);
            mapView.getController().setZoom(4.0);
            CopyrightOverlay copyright = new CopyrightOverlay(context);
            copyright.setTextSize(9);
            copyright.setAlignRight(true);
            mapView.getOverlays().add(copyright);
            map = new OSMMapImpl(this);
        }

        MapView view() {
            return mapView;
        }

        @Override
        public View getView() {
            return mapView;
        }

        @Override
        public void getMapAsync(Consumer<IMap> callback) {
            mapView.post(() -> callback.accept(map));
        }

        @Override
        public void onResume() {
            mapView.onResume();
            map.resumeLocation();
        }

        @Override
        public void onPause() {
            mapView.onPause();
            map.pauseLocation();
        }

        @Override
        public void onCreate(Bundle savedInstance) {
        }

        @Override
        public void onDestroy() {
            map.pauseLocation();
            mapView.onDetach();
        }

        @Override
        public void onLowMemory() {
        }

        @Override
        public void setOnDispatchTouchEventInterceptor(ITouchInterceptor touchInterceptor) {
            dispatchInterceptor = touchInterceptor;
        }

        @Override
        public void setOnInterceptTouchEventInterceptor(ITouchInterceptor touchInterceptor) {
            interceptInterceptor = touchInterceptor;
        }

        @Override
        public void setOnLayoutListener(Runnable callback) {
            onLayoutListener = callback;
        }
    }

    public static final class OSMMapImpl implements IMap {
        private final OSMMapView host;
        private final MapView mapView;
        private int mapType = MAP_TYPE_NORMAL;
        private boolean dark;
        private final java.util.ArrayList<TilesOverlay> extraTiles = new java.util.ArrayList<>();
        private MyLocationNewOverlay locationOverlay;
        private boolean locationWanted;
        private Consumer<Location> locationListener;
        private Runnable idleListener;
        private Runnable moveListener;
        private OnCameraMoveStartedListener moveStartedListener;
        private OnMarkerClickListener markerClickListener;
        private boolean moving;
        private boolean animating;
        private final Runnable idle = this::fireIdle;

        OSMMapImpl(OSMMapView host) {
            this.host = host;
            this.mapView = host.view();
            mapView.addMapListener(new MapListener() {
                @Override
                public boolean onScroll(ScrollEvent event) {
                    onCameraEvent();
                    return false;
                }

                @Override
                public boolean onZoom(ZoomEvent event) {
                    onCameraEvent();
                    return false;
                }
            });
        }

        private void onCameraEvent() {
            if (!moving) {
                moving = true;
                if (moveStartedListener != null) {
                    moveStartedListener.onCameraMoveStarted(host.touching ? OnCameraMoveStartedListener.REASON_GESTURE
                            : animating ? OnCameraMoveStartedListener.REASON_DEVELOPER_ANIMATION : OnCameraMoveStartedListener.REASON_API_ANIMATION);
                }
            }
            if (moveListener != null) {
                moveListener.run();
            }
            scheduleIdle();
        }

        void scheduleIdle() {
            mapView.removeCallbacks(idle);
            mapView.postDelayed(idle, 280);
        }

        private void fireIdle() {
            if (host.touching || animating) {
                scheduleIdle();
                return;
            }
            moving = false;
            if (idleListener != null) {
                idleListener.run();
            }
        }

        private void applyTiles() {
            Context app = mapView.getContext().getApplicationContext();
            for (TilesOverlay overlay : extraTiles) {
                mapView.getOverlays().remove(overlay);
                overlay.onDetach(mapView);
            }
            extraTiles.clear();
            mapView.setTileSource(com.th3nekit.finegram.maps.FGMapTiles.base(mapType, dark));
            TilesOverlay base = mapView.getOverlayManager().getTilesOverlay();
            base.setColorFilter(com.th3nekit.finegram.maps.FGMapTiles.filter(mapType, dark));
            base.setLoadingBackgroundColor(dark ? 0xff1d2c4d : 0xffeeeeee);
            base.setLoadingLineColor(dark ? 0xff2a3a5e : 0xffdddddd);
            int index = 0;
            for (ITileSource source : com.th3nekit.finegram.maps.FGMapTiles.overlays(mapType)) {
                TilesOverlay overlay = new TilesOverlay(new MapTileProviderBasic(app, source), app);
                overlay.setLoadingBackgroundColor(0);
                overlay.setLoadingLineColor(0);
                mapView.getOverlays().add(index++, overlay);
                extraTiles.add(overlay);
            }
            mapView.invalidate();
        }

        public void reloadTiles() {
            applyTiles();
        }

        @Override
        public void setMapType(int mapType) {
            if (this.mapType != mapType) {
                this.mapType = mapType;
                applyTiles();
            }
        }

        @Override
        public void setMapStyle(IMapStyleOptions style) {
            boolean next = style != null;
            if (dark != next) {
                dark = next;
                applyTiles();
            }
        }

        private void apply(ICameraUpdate update, boolean animated, long duration, ICancelableCallback callback) {
            OSMCameraUpdate u = (OSMCameraUpdate) update;
            if (u.box != null) {
                Runnable run = () -> {
                    animating = animated;
                    mapView.zoomToBoundingBox(u.box, animated, u.padding);
                    finish(animated ? duration : 0, callback);
                };
                if (mapView.getWidth() == 0 || mapView.getHeight() == 0) {
                    mapView.post(run);
                } else {
                    run.run();
                }
                return;
            }
            if (animated) {
                animating = true;
                Double zoom = u.zoom >= 0 ? (double) u.zoom : null;
                mapView.getController().animateTo(u.center, zoom, duration);
                finish(duration, callback);
            } else {
                if (u.zoom >= 0) {
                    mapView.getController().setZoom((double) u.zoom);
                }
                if (u.center != null) {
                    mapView.getController().setCenter(u.center);
                }
                finish(0, callback);
            }
        }

        private void finish(long delay, ICancelableCallback callback) {
            mapView.postDelayed(() -> {
                animating = false;
                scheduleIdle();
                if (callback != null) {
                    callback.onFinish();
                }
            }, delay + 16);
        }

        @Override
        public void animateCamera(ICameraUpdate update) {
            apply(update, true, 450, null);
        }

        @Override
        public void animateCamera(ICameraUpdate update, ICancelableCallback callback) {
            apply(update, true, 450, callback);
        }

        @Override
        public void animateCamera(ICameraUpdate update, int duration, ICancelableCallback callback) {
            apply(update, true, Math.max(1, duration), callback);
        }

        @Override
        public void moveCamera(ICameraUpdate update) {
            apply(update, false, 0, null);
        }

        @Override
        public float getMaxZoomLevel() {
            return (float) mapView.getMaxZoomLevel();
        }

        @Override
        public float getMinZoomLevel() {
            return (float) mapView.getMinZoomLevel();
        }

        @SuppressLint("MissingPermission")
        @Override
        public void setMyLocationEnabled(boolean enabled) {
            locationWanted = enabled;
            if (enabled) {
                if (locationOverlay == null) {
                    Context context = mapView.getContext();
                    GpsMyLocationProvider provider = new GpsMyLocationProvider(context.getApplicationContext());
                    provider.setLocationUpdateMinTime(2000);
                    locationOverlay = new MyLocationNewOverlay(provider, mapView) {
                        @Override
                        public void onLocationChanged(Location location, IMyLocationProvider source) {
                            super.onLocationChanged(location, source);
                            Consumer<Location> listener = locationListener;
                            if (listener != null && location != null) {
                                mapView.post(() -> listener.accept(location));
                            }
                        }
                    };
                    Bitmap dot = locationDot(context);
                    locationOverlay.setPersonIcon(dot);
                    locationOverlay.setDirectionIcon(dot);
                    locationOverlay.setPersonAnchor(0.5f, 0.5f);
                    locationOverlay.setDirectionAnchor(0.5f, 0.5f);
                    locationOverlay.setDrawAccuracyEnabled(true);
                    mapView.getOverlays().add(locationOverlay);
                }
                resumeLocation();
            } else if (locationOverlay != null) {
                locationOverlay.disableMyLocation();
                mapView.getOverlays().remove(locationOverlay);
                locationOverlay = null;
            }
            mapView.invalidate();
        }

        void resumeLocation() {
            if (locationWanted && locationOverlay != null) {
                try {
                    locationOverlay.enableMyLocation();
                } catch (Throwable e) {
                    FileLog.e(e);
                }
            }
        }

        void pauseLocation() {
            if (locationOverlay != null) {
                locationOverlay.disableMyLocation();
            }
        }

        private static Bitmap locationDot(Context context) {
            int size = AndroidUtilities.dp(22);
            Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            paint.setColor(0x33000000);
            canvas.drawCircle(size / 2f, size / 2f + AndroidUtilities.dp(1), size / 2f - AndroidUtilities.dp(1), paint);
            paint.setColor(0xFFFFFFFF);
            canvas.drawCircle(size / 2f, size / 2f, size / 2f - AndroidUtilities.dp(1.5f), paint);
            paint.setColor(0xFF3D8BFF);
            canvas.drawCircle(size / 2f, size / 2f, size / 2f - AndroidUtilities.dp(4.5f), paint);
            return bitmap;
        }

        @Override
        public IUISettings getUiSettings() {
            return new IUISettings() {
                @Override
                public void setZoomControlsEnabled(boolean enabled) {
                    mapView.getZoomController().setVisibility(enabled ? CustomZoomButtonsController.Visibility.SHOW_AND_FADEOUT
                            : CustomZoomButtonsController.Visibility.NEVER);
                }

                @Override
                public void setMyLocationButtonEnabled(boolean enabled) {
                }

                @Override
                public void setCompassEnabled(boolean enabled) {
                }
            };
        }

        @Override
        public void setOnCameraIdleListener(Runnable callback) {
            idleListener = callback;
        }

        @Override
        public void setOnCameraMoveStartedListener(OnCameraMoveStartedListener listener) {
            moveStartedListener = listener;
        }

        @Override
        public CameraPosition getCameraPosition() {
            IGeoPoint center = mapView.getMapCenter();
            return new CameraPosition(new LatLng(center.getLatitude(), center.getLongitude()), (float) mapView.getZoomLevelDouble());
        }

        @Override
        public void setOnMapLoadedCallback(Runnable callback) {
            if (callback == null) {
                return;
            }
            if (mapView.isLayoutOccurred()) {
                mapView.post(callback);
            } else {
                mapView.addOnFirstLayoutListener((v, left, top, right, bottom) -> mapView.post(callback));
            }
        }

        @Override
        public IProjection getProjection() {
            return latLng -> {
                Point out = new Point();
                mapView.getProjection().toPixels(point(latLng), out);
                return out;
            };
        }

        @Override
        public void setPadding(int left, int top, int right, int bottom) {
            mapView.setMapCenterOffset((left - right) / 2, (top - bottom) / 2);
        }

        @Override
        public IMarker addMarker(IMarkerOptions options) {
            OSMMarkerOptions o = (OSMMarkerOptions) options;
            Marker marker = new Marker(mapView);
            marker.setInfoWindow(null);
            if (o.position != null) {
                marker.setPosition(o.position);
            }
            marker.setAnchor(o.anchorU, o.anchorV);
            marker.setFlat(o.flat);
            if (o.title != null) {
                marker.setTitle(o.title);
            }
            if (o.snippet != null) {
                marker.setSnippet(o.snippet);
            }
            Drawable icon = o.icon(mapView.getContext());
            if (icon != null) {
                marker.setIcon(icon);
            }
            OSMMarker wrapper = new OSMMarker(marker);
            marker.setOnMarkerClickListener((m, view) -> {
                OnMarkerClickListener listener = markerClickListener;
                return listener == null || listener.onClick(wrapper);
            });
            mapView.getOverlays().add(marker);
            mapView.invalidate();
            return wrapper;
        }

        @Override
        public void setOnMyLocationChangeListener(Consumer<Location> callback) {
            locationListener = callback;
        }

        @Override
        public void setOnMarkerClickListener(OnMarkerClickListener listener) {
            markerClickListener = listener;
        }

        @Override
        public void setOnCameraMoveListener(Runnable callback) {
            moveListener = callback;
        }

        @Override
        public ICircle addCircle(ICircleOptions options) {
            OSMCircleOptions o = (OSMCircleOptions) options;
            Polygon polygon = new Polygon(mapView);
            polygon.setInfoWindow(null);
            OSMCircle circle = new OSMCircle(polygon, o.center, o.radius);
            Paint outline = polygon.getOutlinePaint();
            outline.setColor(o.strokeColor);
            outline.setStrokeWidth(o.strokeWidth);
            outline.setAntiAlias(true);
            if (o.dash != null) {
                outline.setPathEffect(o.dash);
            }
            polygon.getFillPaint().setColor(o.fillColor);
            circle.rebuild();
            mapView.getOverlays().add(polygon);
            mapView.invalidate();
            return circle;
        }

        final class OSMMarker implements IMarker {
            private final Marker marker;
            private Object tag;

            OSMMarker(Marker marker) {
                this.marker = marker;
            }

            @Override
            public Object getTag() {
                return tag;
            }

            @Override
            public void setTag(Object tag) {
                this.tag = tag;
            }

            @Override
            public LatLng getPosition() {
                GeoPoint p = marker.getPosition();
                return new LatLng(p.getLatitude(), p.getLongitude());
            }

            @Override
            public void setPosition(LatLng latLng) {
                marker.setPosition(point(latLng));
                mapView.invalidate();
            }

            @Override
            public void setRotation(int rotation) {
                marker.setRotation(-rotation);
                mapView.invalidate();
            }

            @Override
            public void setIcon(Bitmap bitmap) {
                marker.setIcon(new BitmapDrawable(mapView.getResources(), bitmap));
                mapView.invalidate();
            }

            @Override
            public void setIcon(int resId) {
                Drawable drawable = ContextCompat.getDrawable(mapView.getContext(), resId);
                if (drawable != null) {
                    marker.setIcon(drawable);
                    mapView.invalidate();
                }
            }

            @Override
            public void remove() {
                mapView.getOverlays().remove(marker);
                mapView.invalidate();
            }
        }

        final class OSMCircle implements ICircle {
            private final Polygon polygon;
            private GeoPoint center;
            private double radius;

            OSMCircle(Polygon polygon, GeoPoint center, double radius) {
                this.polygon = polygon;
                this.center = center == null ? new GeoPoint(0.0, 0.0) : center;
                this.radius = radius;
            }

            void rebuild() {
                polygon.setPoints(Polygon.pointsAsCircle(center, Math.max(1, radius)));
                mapView.invalidate();
            }

            @Override
            public void setStrokeColor(int color) {
                polygon.getOutlinePaint().setColor(color);
                mapView.invalidate();
            }

            @Override
            public void setFillColor(int color) {
                polygon.getFillPaint().setColor(color);
                mapView.invalidate();
            }

            @Override
            public void setRadius(double radius) {
                this.radius = radius;
                rebuild();
            }

            @Override
            public double getRadius() {
                return radius;
            }

            @Override
            public void setCenter(LatLng latLng) {
                center = point(latLng);
                rebuild();
            }

            @Override
            public void remove() {
                mapView.getOverlays().remove(polygon);
                mapView.invalidate();
            }
        }
    }

    public static final class OSMMarkerOptions implements IMarkerOptions {
        GeoPoint position;
        Bitmap bitmap;
        int resId;
        float anchorU = 0.5f;
        float anchorV = 1f;
        String title;
        String snippet;
        boolean flat;

        Drawable icon(Context context) {
            if (bitmap != null) {
                return new BitmapDrawable(context.getResources(), bitmap);
            }
            if (resId != 0) {
                return ContextCompat.getDrawable(context, resId);
            }
            return null;
        }

        @Override
        public IMarkerOptions position(LatLng latLng) {
            position = point(latLng);
            return this;
        }

        @Override
        public IMarkerOptions icon(Bitmap bitmap) {
            this.bitmap = bitmap;
            return this;
        }

        @Override
        public IMarkerOptions icon(int resId) {
            this.resId = resId;
            return this;
        }

        @Override
        public IMarkerOptions anchor(float lat, float lng) {
            anchorU = lat;
            anchorV = lng;
            return this;
        }

        @Override
        public IMarkerOptions title(String title) {
            this.title = title;
            return this;
        }

        @Override
        public IMarkerOptions snippet(String snippet) {
            this.snippet = snippet;
            return this;
        }

        @Override
        public IMarkerOptions flat(boolean flat) {
            this.flat = flat;
            return this;
        }
    }

    public static final class OSMCircleOptions implements ICircleOptions {
        GeoPoint center;
        double radius;
        int strokeColor = 0xFF000000;
        int fillColor;
        float strokeWidth = AndroidUtilities.dp(1);
        DashPathEffect dash;

        @Override
        public ICircleOptions center(LatLng latLng) {
            center = point(latLng);
            return this;
        }

        @Override
        public ICircleOptions radius(double radius) {
            this.radius = radius;
            return this;
        }

        @Override
        public ICircleOptions strokeColor(int color) {
            strokeColor = color;
            return this;
        }

        @Override
        public ICircleOptions fillColor(int color) {
            fillColor = color;
            return this;
        }

        @Override
        public ICircleOptions strokePattern(List<PatternItem> patternItems) {
            ArrayList<Float> parts = new ArrayList<>();
            for (PatternItem item : patternItems) {
                if (item instanceof PatternItem.Dash) {
                    parts.add((float) ((PatternItem.Dash) item).length);
                } else if (item instanceof PatternItem.Gap) {
                    parts.add((float) ((PatternItem.Gap) item).length);
                }
            }
            if (parts.size() >= 2) {
                float[] intervals = new float[parts.size() - parts.size() % 2];
                for (int i = 0; i < intervals.length; i++) {
                    intervals[i] = parts.get(i);
                }
                dash = new DashPathEffect(intervals, 0);
            }
            return this;
        }

        @Override
        public ICircleOptions strokeWidth(int width) {
            strokeWidth = width;
            return this;
        }
    }

    public static final class OSMLatLngBoundsBuilder implements ILatLngBoundsBuilder {
        private double north = -90;
        private double south = 90;
        private double east = -180;
        private double west = 180;

        @Override
        public ILatLngBoundsBuilder include(LatLng latLng) {
            north = Math.max(north, latLng.latitude);
            south = Math.min(south, latLng.latitude);
            east = Math.max(east, latLng.longitude);
            west = Math.min(west, latLng.longitude);
            return this;
        }

        @Override
        public ILatLngBounds build() {
            if (north < south) {
                return new OSMLatLngBounds(new BoundingBox(0, 0, 0, 0));
            }
            return new OSMLatLngBounds(new BoundingBox(north, east, south, west));
        }
    }

    public static final class OSMLatLngBounds implements ILatLngBounds {
        final BoundingBox box;

        OSMLatLngBounds(BoundingBox box) {
            this.box = box;
        }

        @Override
        public LatLng getCenter() {
            return new LatLng(box.getCenterLatitude(), box.getCenterLongitude());
        }
    }
}
