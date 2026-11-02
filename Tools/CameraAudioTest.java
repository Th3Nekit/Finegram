import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.net.URLClassLoader;
import javax.tools.ToolProvider;

public class CameraAudioTest {
    public static void main(String[] args) throws Exception {
        Path dir = Files.createTempDirectory("camera-audio-test-");
        Map<String, String> sources = Map.ofEntries(
            Map.entry("android/content/Context.java", """
                package android.content;
                public class Context {
                    public static final String AUDIO_SERVICE = "audio";
                    public boolean permitted = true;
                    public android.media.AudioManager manager = new android.media.AudioManager();
                    public Object getSystemService(String key) { return manager; }
                }
                """),
            Map.entry("android/Manifest.java", "package android; public class Manifest { public static class permission { public static final String RECORD_AUDIO = \"audio\"; } }"),
            Map.entry("android/content/pm/PackageManager.java", "package android.content.pm; public class PackageManager { public static final int PERMISSION_GRANTED = 0; }"),
            Map.entry("androidx/core/content/ContextCompat.java", "package androidx.core.content; public class ContextCompat { public static int checkSelfPermission(android.content.Context c, String p) { return c.permitted ? 0 : -1; } }"),
            Map.entry("android/media/AudioDeviceInfo.java", """
                package android.media;
                public class AudioDeviceInfo {
                    public static final int TYPE_WIRED_HEADSET=1, TYPE_BLUETOOTH_SCO=2, TYPE_USB_DEVICE=3, TYPE_USB_HEADSET=4, TYPE_BLE_HEADSET=5;
                    final int type;
                    public AudioDeviceInfo(int type) { this.type=type; }
                    public int getType() { return type; }
                }
                """),
            Map.entry("android/media/AudioManager.java", "package android.media; public class AudioManager { public static final int GET_DEVICES_INPUTS=1; public AudioDeviceInfo[] devices=new AudioDeviceInfo[0]; public AudioDeviceInfo[] getDevices(int flags) { return devices; } }"),
            Map.entry("android/media/AudioFormat.java", "package android.media; public class AudioFormat { public static final int CHANNEL_IN_STEREO=2, CHANNEL_IN_MONO=1, ENCODING_PCM_16BIT=16; }"),
            Map.entry("android/media/MediaRecorder.java", "package android.media; public class MediaRecorder { public static class AudioSource { public static final int DEFAULT=0, MIC=1, CAMCORDER=5; } }"),
            Map.entry("android/media/AudioRecord.java", """
                package android.media;
                import java.util.*;
                public class AudioRecord {
                    public static final int STATE_INITIALIZED=1, RECORDSTATE_RECORDING=3;
                    public static List<AudioRecord> attempts = new ArrayList<>();
                    public static Map<Integer,Integer> failures = new HashMap<>();
                    public static boolean monoOnly, failAll;
                    public final int source, channels;
                    public boolean released, recording;
                    final int failure;
                    public AudioRecord(int source, int rate, int mask, int encoding, int buffer) {
                        this.source=source; channels=mask;
                        failure=failAll || monoOnly && mask==2 ? 1 : failures.getOrDefault(source,0);
                        attempts.add(this);
                    }
                    public static int getMinBufferSize(int rate,int mask,int encoding) { return 256; }
                    public int getState() { return failure==1 ? 0 : STATE_INITIALIZED; }
                    public void startRecording() {
                        if(failure==2) throw new IllegalStateException("start failed");
                        if(failure==4) throw new SecurityException("source denied");
                        recording = failure != 3;
                    }
                    public int getRecordingState() { return recording ? RECORDSTATE_RECORDING : 0; }
                    public void release() { released=true; recording=false; }
                    public static void reset() { attempts.clear(); failures.clear(); monoOnly=false; failAll=false; }
                }
                """),
            Map.entry("com/th3nekit/finegram/camera/CameraXAudioCapture.java", "package com.th3nekit.finegram.camera; public class CameraXAudioCapture { public static int requests; public static void requestWideCapture(android.media.AudioRecord record) { requests++; } }"),
            Map.entry("com/th3nekit/finegram/core/configs/FinegramCameraConfig.java", "package com.th3nekit.finegram.core.configs; public class FinegramCameraConfig { public static final FinegramCameraConfig INSTANCE=new FinegramCameraConfig(); public boolean modern; public boolean getNewCameraAudio() { return modern; } }"),
            Map.entry("com/th3nekit/finegram/core/configs/FinegramChatsConfig.java", "package com.th3nekit.finegram.core.configs; public class FinegramChatsConfig { public static final FinegramChatsConfig INSTANCE=new FinegramChatsConfig(); public boolean wide; public boolean getRecordMultiMic() { return wide; } }"),
            Map.entry("com/th3nekit/finegram/chats/AudioEnhance.java", "package com.th3nekit.finegram.chats; public class AudioEnhance { public static final AudioEnhance INSTANCE=new AudioEnhance(); public int source=9; public int getAudioSource() { return source; } }"),
            Map.entry("AudioFixture.java", """
                import android.content.Context;
                import android.media.*;
                import com.th3nekit.finegram.camera.*;
                import com.th3nekit.finegram.core.configs.*;
                public class AudioFixture {
                    static int passed;
                    static void check(boolean value,String message) { if(!value) throw new AssertionError(message); }
                    static void cleanFailed(AudioRecord kept) {
                        for(AudioRecord attempt:AudioRecord.attempts) check(attempt==kept || attempt.released,"failed input leaked");
                    }
                    static void reset() {
                        AudioRecord.reset(); CameraXAudioCapture.requests=0;
                        FinegramCameraConfig.INSTANCE.modern=true;
                        FinegramChatsConfig.INSTANCE.wide=true;
                    }
                    public static void run() {
                        reset(); FinegramCameraConfig.INSTANCE.modern=false;
                        AudioRecord old=VoiceAudioCapture.createCameraStarted(new Context(),48000,1024,true);
                        check(old.source==0 && old.channels==1 && old.recording,"old must use default mono");
                        check(CameraXAudioCapture.requests==0,"old must not request wide capture"); passed++;
                        reset(); AudioRecord chosen=VoiceAudioCapture.createCameraStarted(new Context(),48000,1024,false);
                        check(chosen.source==9 && chosen.recording,"new must honor the selected source"); passed++;
                        for(int failure=1;failure<=4;failure++) {
                            reset(); AudioRecord.failures.put(9,failure);
                            AudioRecord fallback=VoiceAudioCapture.createCameraStarted(new Context(),48000,1024,false);
                            check(fallback.source==1 && fallback.recording,"failed source must fall back to MIC");
                            cleanFailed(fallback); passed++;
                        }
                        reset(); AudioRecord stereo=VoiceAudioCapture.createCameraStarted(new Context(),48000,1024,true);
                        check(stereo.channels==2 && CameraXAudioCapture.requests==1,"new wide capture must request stereo"); passed++;
                        reset(); AudioRecord.monoOnly=true;
                        AudioRecord mono=VoiceAudioCapture.createCameraStarted(new Context(),48000,1024,true);
                        check(mono.channels==1 && mono.recording,"unsupported stereo must fall back to mono"); cleanFailed(mono); passed++;
                        reset(); Context external=new Context();
                        external.manager.devices=new AudioDeviceInfo[]{new AudioDeviceInfo(AudioDeviceInfo.TYPE_BLUETOOTH_SCO)};
                        AudioRecord headset=VoiceAudioCapture.createCameraStarted(external,48000,1024,true);
                        check(headset.channels==1 && headset.source==9 && CameraXAudioCapture.requests==0,"external input must keep routing without wide capture"); passed++;
                        reset(); Context denied=new Context(); denied.permitted=false; AudioRecord.failures.put(9,4);
                        try { VoiceAudioCapture.createCameraStarted(denied,48000,1024,false); throw new AssertionError("permission error swallowed"); }
                        catch(SecurityException expected) { check(AudioRecord.attempts.size()==1,"denied permission must not retry"); cleanFailed(null); } passed++;
                        reset(); AudioRecord.failAll=true;
                        try { VoiceAudioCapture.createCameraStarted(new Context(),48000,1024,true); throw new AssertionError("all failures swallowed"); }
                        catch(IllegalStateException expected) { cleanFailed(null); } passed++;
                        System.out.println("PASS: "+passed+" camera-audio checks");
                    }
                }
                """));
        var compiler = ToolProvider.getSystemJavaCompiler();
        List<String> arguments = new ArrayList<>(List.of("-d", dir.toString(), "-encoding", "UTF-8"));
        for (var entry : sources.entrySet()) {
            Path file = dir.resolve(entry.getKey());
            Files.createDirectories(file.getParent());
            Files.writeString(file, entry.getValue());
            arguments.add(file.toString());
        }
        arguments.add("TMessagesProj/src/main/java/com/th3nekit/finegram/camera/VoiceAudioCapture.java");
        if (compiler.run(null, null, null, arguments.toArray(String[]::new)) != 0) throw new AssertionError("Audio fixture compilation failed");
        try (var loader = new URLClassLoader(new java.net.URL[]{dir.toUri().toURL()})) {
            loader.loadClass("AudioFixture").getMethod("run").invoke(null);
        }
    }
}
