package com.th3nekit.finegram.maps;

import android.graphics.ColorFilter;
import android.graphics.ColorMatrixColorFilter;

import org.osmdroid.tileprovider.tilesource.ITileSource;
import org.osmdroid.tileprovider.tilesource.XYTileSource;
import org.osmdroid.util.MapTileIndex;
import org.telegram.messenger.ApplicationLoader;
import org.telegram.messenger.IMapsProvider;
import org.telegram.messenger.LocaleController;
import org.telegram.messenger.MessagesController;
import org.telegram.messenger.OSMDroidMapsProvider;
import org.telegram.messenger.R;
import org.telegram.ui.ActionBar.ActionBarMenuItem;
import org.telegram.ui.ActionBar.ActionBarMenuSubItem;
import org.telegram.ui.ActionBar.Theme;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

public final class FGMapTiles {

    public static final int SOURCE_GOOGLE = 0;
    public static final int SOURCE_2GIS = 1;
    public static final int SOURCE_OSM = 2;

    public static final int MENU_GOOGLE = 7001;
    public static final int MENU_2GIS = 7002;
    public static final int MENU_OSM = 7003;

    private static final String PREF = "FG_MapTileSource";

    private static final String COPYRIGHT_GOOGLE = "© Google";
    private static final String COPYRIGHT_2GIS = "© 2GIS";
    private static final String COPYRIGHT_OSM = "© OpenStreetMap contributors";
    private static final String COPYRIGHT_ESRI = "© Esri, Maxar, Earthstar Geographics";

    private static final String GOOGLE_NIGHT = "s.e%3Ag%7Cp.c%3A%23ff1d2c4d%2Cs.e%3Al.t.f%7Cp.c%3A%23ff8ec3b9%2Cs.e%3Al.t.s%7Cp.c%3A%23ff1a3646"
            + "%2Cs.t%3A17%7Cs.e%3Ag.s%7Cp.c%3A%23ff4b6878%2Cs.t%3A21%7Cs.e%3Al.t.f%7Cp.c%3A%23ff64779e%2Cs.t%3A18%7Cs.e%3Ag.s%7Cp.c%3A%23ff4b6878"
            + "%2Cs.t%3A81%7Cs.e%3Ag.s%7Cp.c%3A%23ff334e87%2Cs.t%3A82%7Cs.e%3Ag%7Cp.c%3A%23ff023e58%2Cs.t%3A2%7Cs.e%3Ag%7Cp.c%3A%23ff283d6a"
            + "%2Cs.t%3A2%7Cs.e%3Al.t.f%7Cp.c%3A%23ff6f9ba5%2Cs.t%3A2%7Cs.e%3Al.t.s%7Cp.c%3A%23ff1d2c4d%2Cs.t%3A40%7Cs.e%3Ag.f%7Cp.c%3A%23ff023e58"
            + "%2Cs.t%3A40%7Cs.e%3Al.t.f%7Cp.c%3A%23ff3c7680%2Cs.t%3A3%7Cs.e%3Ag%7Cp.c%3A%23ff304a7d%2Cs.t%3A3%7Cs.e%3Al.t.f%7Cp.c%3A%23ff98a5be"
            + "%2Cs.t%3A3%7Cs.e%3Al.t.s%7Cp.c%3A%23ff1d2c4d%2Cs.t%3A49%7Cs.e%3Ag%7Cp.c%3A%23ff2c6675%2Cs.t%3A49%7Cs.e%3Ag.s%7Cp.c%3A%23ff255763"
            + "%2Cs.t%3A49%7Cs.e%3Al.t.f%7Cp.c%3A%23ffb0d5ce%2Cs.t%3A49%7Cs.e%3Al.t.s%7Cp.c%3A%23ff023e58%2Cs.t%3A4%7Cs.e%3Al.t.f%7Cp.c%3A%23ff98a5be"
            + "%2Cs.t%3A4%7Cs.e%3Al.t.s%7Cp.c%3A%23ff1d2c4d%2Cs.t%3A65%7Cs.e%3Ag.f%7Cp.c%3A%23ff283d6a%2Cs.t%3A66%7Cs.e%3Ag%7Cp.c%3A%23ff3a4762"
            + "%2Cs.t%3A6%7Cs.e%3Ag%7Cp.c%3A%23ff0e1626%2Cs.t%3A6%7Cs.e%3Al.t.f%7Cp.c%3A%23ff4e6d70";

    private static final ColorFilter NIGHT_FILTER = new ColorMatrixColorFilter(new float[]{
            0.2616f, -0.8418f, -0.0758f, 0f, 182.58f,
            -0.2529f, -0.3839f, -0.0832f, 0f, 204.0f,
            -0.3035f, -1.1087f, 0.5482f, 0f, 253.47f,
            0f, 0f, 0f, 1f, 0f
    });

    private static final ITileSource TWO_GIS = new XYTileSource("FG2GisHd", 2, 18, 256, "", new String[]{
            "https://tile0.maps.2gis.com/tiles",
            "https://tile1.maps.2gis.com/tiles",
            "https://tile2.maps.2gis.com/tiles",
            "https://tile3.maps.2gis.com/tiles"}, COPYRIGHT_2GIS) {
        @Override
        public String getTileURLString(long index) {
            return getBaseUrl() + "?x=" + MapTileIndex.getX(index) + "&y=" + MapTileIndex.getY(index)
                    + "&z=" + MapTileIndex.getZoom(index) + "&v=1&ts=online_hd";
        }
    };

    private static final ITileSource OSM = new XYTileSource("FGOsmMapnik", 1, 19, 256, ".png", new String[]{
            "https://tile.openstreetmap.org/"}, COPYRIGHT_OSM);

    private static final ITileSource ESRI_IMAGERY = esri("FGEsriImagery", "World_Imagery");
    private static final ITileSource ESRI_ROADS = esri("FGEsriRoads", "Reference/World_Transportation");
    private static final ITileSource ESRI_PLACES = esri("FGEsriPlaces", "Reference/World_Boundaries_and_Places");

    private static final HashMap<String, ITileSource> google = new HashMap<>();

    private FGMapTiles() {
    }

    private static ITileSource esri(String name, String service) {
        return new XYTileSource(name, 1, 19, 256, "", new String[]{
                "https://server.arcgisonline.com/ArcGIS/rest/services/" + service + "/MapServer/tile/"}, COPYRIGHT_ESRI) {
            @Override
            public String getTileURLString(long index) {
                return getBaseUrl() + MapTileIndex.getZoom(index) + "/" + MapTileIndex.getY(index) + "/" + MapTileIndex.getX(index);
            }
        };
    }

    public static boolean available() {
        return ApplicationLoader.getMapsProvider() instanceof OSMDroidMapsProvider;
    }

    public static int source() {
        int value = MessagesController.getGlobalMainSettings().getInt(PREF, SOURCE_GOOGLE);
        return value < SOURCE_GOOGLE || value > SOURCE_OSM ? SOURCE_GOOGLE : value;
    }

    public static void setSource(int value) {
        MessagesController.getGlobalMainSettings().edit().putInt(PREF, value).apply();
    }

    private static String language() {
        Locale locale = null;
        try {
            locale = LocaleController.getInstance().getCurrentLocale();
        } catch (Throwable ignored) {
        }
        String lang = (locale == null ? Locale.getDefault() : locale).getLanguage();
        if (lang == null || !lang.matches("[a-zA-Z]{2,3}")) {
            return "en";
        }
        return lang.toLowerCase(Locale.ROOT);
    }

    private static ITileSource google(String layer, boolean night) {
        String lang = language();
        String key = layer + "_" + lang + (night ? "_n" : "");
        synchronized (google) {
            ITileSource cached = google.get(key);
            if (cached != null) {
                return cached;
            }
            final String tail = "&hl=" + lang + "&scale=2" + (night ? "&apistyle=" + GOOGLE_NIGHT : "");
            ITileSource source = new XYTileSource("FGGoogle_" + key, 1, 20, 256, "", new String[]{
                    "https://mt0.google.com/vt/",
                    "https://mt1.google.com/vt/",
                    "https://mt2.google.com/vt/",
                    "https://mt3.google.com/vt/"}, COPYRIGHT_GOOGLE) {
                @Override
                public String getTileURLString(long index) {
                    return getBaseUrl() + "lyrs=" + layer + "&x=" + MapTileIndex.getX(index) + "&y=" + MapTileIndex.getY(index)
                            + "&z=" + MapTileIndex.getZoom(index) + tail;
                }
            };
            google.put(key, source);
            return source;
        }
    }

    public static ITileSource base(int mapType, boolean dark) {
        int source = source();
        if (source == SOURCE_GOOGLE) {
            if (mapType == IMapsProvider.MAP_TYPE_SATELLITE) {
                return google("s", false);
            }
            if (mapType == IMapsProvider.MAP_TYPE_HYBRID) {
                return google("y", false);
            }
            return google("m", dark);
        }
        if (mapType == IMapsProvider.MAP_TYPE_SATELLITE || mapType == IMapsProvider.MAP_TYPE_HYBRID) {
            return ESRI_IMAGERY;
        }
        return source == SOURCE_2GIS ? TWO_GIS : OSM;
    }

    public static List<ITileSource> overlays(int mapType) {
        ArrayList<ITileSource> list = new ArrayList<>();
        if (mapType == IMapsProvider.MAP_TYPE_HYBRID && source() != SOURCE_GOOGLE) {
            list.add(ESRI_ROADS);
            list.add(ESRI_PLACES);
        }
        return list;
    }

    public static ColorFilter filter(int mapType, boolean dark) {
        if (!dark || mapType != IMapsProvider.MAP_TYPE_NORMAL || source() == SOURCE_GOOGLE) {
            return null;
        }
        return NIGHT_FILTER;
    }

    public static void addMenu(ActionBarMenuItem item, Theme.ResourcesProvider resourcesProvider) {
        if (!available()) {
            return;
        }
        item.addColoredGap();
        item.addSubItem(MENU_GOOGLE, 0, null, LocaleController.getString(R.string.FG_MapSourceGoogle), true, true, resourcesProvider);
        item.addSubItem(MENU_2GIS, 0, null, LocaleController.getString(R.string.FG_MapSource2Gis), true, true, resourcesProvider);
        item.addSubItem(MENU_OSM, 0, null, LocaleController.getString(R.string.FG_MapSourceOsm), true, true, resourcesProvider);
        syncMenu(item);
    }

    private static void syncMenu(ActionBarMenuItem item) {
        int source = source();
        check(item, MENU_GOOGLE, source == SOURCE_GOOGLE);
        check(item, MENU_2GIS, source == SOURCE_2GIS);
        check(item, MENU_OSM, source == SOURCE_OSM);
    }

    private static void check(ActionBarMenuItem item, int id, boolean checked) {
        try {
            if (item.getSubItem(id) instanceof ActionBarMenuSubItem) {
                ((ActionBarMenuSubItem) item.getSubItem(id)).setChecked(checked);
            }
        } catch (Throwable ignored) {
        }
    }

    public static boolean onMenu(int id, ActionBarMenuItem item, IMapsProvider.IMap map) {
        int source;
        if (id == MENU_GOOGLE) {
            source = SOURCE_GOOGLE;
        } else if (id == MENU_2GIS) {
            source = SOURCE_2GIS;
        } else if (id == MENU_OSM) {
            source = SOURCE_OSM;
        } else {
            return false;
        }
        if (source != source()) {
            setSource(source);
            if (map instanceof OSMDroidMapsProvider.OSMMapImpl) {
                ((OSMDroidMapsProvider.OSMMapImpl) map).reloadTiles();
            }
        }
        syncMenu(item);
        return true;
    }
}
