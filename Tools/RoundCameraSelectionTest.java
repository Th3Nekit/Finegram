import com.sun.source.tree.MethodTree;
import com.sun.source.util.JavacTask;
import com.sun.source.util.TreeScanner;
import com.sun.source.util.Trees;
import java.net.URI;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import javax.tools.JavaFileObject;
import javax.tools.SimpleJavaFileObject;
import javax.tools.ToolProvider;

public class RoundCameraSelectionTest {
    private static JavaFileObject source(String name, String text) {
        return new SimpleJavaFileObject(URI.create("string:///" + name + ".java"), JavaFileObject.Kind.SOURCE) {
            @Override public CharSequence getCharContent(boolean ignoreEncodingErrors) { return text; }
        };
    }

    public static void main(String[] args) throws Exception {
        var compiler = ToolProvider.getSystemJavaCompiler();
        String original = Files.readString(Path.of("TMessagesProj/src/main/java/org/telegram/ui/Components/InstantCameraView2.java"));
        var task = (JavacTask) compiler.getTask(null, null, null, List.of("-proc:none"), null,
                List.of(source("InstantCameraView2", original)));
        var unit = task.parse().iterator().next();
        var positions = Trees.instance(task).getSourcePositions();
        StringBuilder methods = new StringBuilder();
        new TreeScanner<Void, Void>() {
            @Override public Void visitMethod(MethodTree method, Void unused) {
                if (List.of("showCamera", "onCameraCapabilitiesChanged").contains(method.getName().toString())) {
                    methods.append(original, (int) positions.getStartPosition(unit, method),
                            (int) positions.getEndPosition(unit, method));
                }
                return super.visitMethod(method, unused);
            }
        }.scan(unit, null);
        String fixture = """
                import java.io.File;
                public class RoundCameraFixture {
                    @interface NonNull {}
                    static final int VISIBLE = 0;
                    static class Setting<T> {
                        T value;
                        Setting(T value) { this.value = value; }
                        T get() { return value; }
                        void set(T value) { this.value = value; }
                    }
                    static class SharedSettings {
                        static Setting<RoundVideoSession.CameraFacing> roundVideoLastCamera = new Setting<>(RoundVideoSession.CameraFacing.FRONT);
                        static Setting<Object> roundVideoOutputResolution = new Setting<>(null), roundVideoVideoBitrate = new Setting<>(null),
                                roundVideoCameraResolution = new Setting<>(null), roundVideoFrameRate = new Setting<>(null), roundVideoComposition = new Setting<>(null);
                    }
                    static class FinegramCameraConfig {
                        static final FinegramCameraConfig INSTANCE = new FinegramCameraConfig();
                        Boolean pendingRear;
                        boolean preferredRear, lastRear;
                        Boolean takePendingRoundRear() { Boolean value = pendingRear; pendingRear = null; return value; }
                        boolean startWithRear() { return preferredRear; }
                        void setRoundLastRear(boolean value) { lastRear = value; }
                    }
                    static class RoundVideoSession {
                        enum CameraFacing { FRONT, BACK }
                        CameraFacing initialFacing;
                        void start() {}
                        static class CameraCapabilities {
                            CameraFacing facing;
                            CameraCapabilities(CameraFacing facing) { this.facing = facing; }
                            CameraFacing getActiveFacing() { return facing; }
                        }
                        static class Builder {
                            RoundVideoSession result = new RoundVideoSession();
                            Builder(Object context, Object texture) {}
                            Builder setOutputDirectory(File value) { return this; }
                            Builder setInitialFacing(CameraFacing value) { result.initialFacing = value; return this; }
                            Builder setOutputResolution(Object value) { return this; }
                            Builder setVideoBitrate(Object value) { return this; }
                            Builder setCameraResolution(Object value) { return this; }
                            Builder setFrameRate(Object value) { return this; }
                            Builder setCompositionEnabled(Object value) { return this; }
                            Builder setListener(Object value) { return this; }
                            Builder setOutputListener(Object value) { return this; }
                            Builder setScreenFlashController(java.util.function.Consumer<Boolean> value) { return this; }
                            RoundVideoSession build() { return result; }
                        }
                    }
                    static class ApplicationLoader { static File getFilesDirFixed() { return new File("."); } }
                    static class TelegramRoundVideoUpload { TelegramRoundVideoUpload(int account, boolean secret) {} }
                    static class MediaController {
                        static MediaController getInstance() { return new MediaController(); }
                        void requestRecordAudioFocus(boolean value) {}
                    }
                    static class Progress {
                        Progress getPaint() { return this; }
                        void setAlpha(int value) {}
                        void setProgress(float value) {}
                    }
                    RoundVideoSession session;
                    RoundVideoSession.CameraCapabilities capabilities;
                    Progress progressView = new Progress();
                    boolean setVisibilityFromPause, sent, muted, startedNotificationSent, stopNotificationSent, secretChat;
                    long lastVisibleDurationMs;
                    int currentAccount;
                    Object textureView, activeOutputResolution, sessionListener;
                    TelegramRoundVideoUpload upload;
                    Object getContext() { return null; }
                    void setVisibility(int value) {}
                    void showInitialPlaceholder() {}
                    void setScreenFlashEnabled(boolean value) {}
                    void startAnimation(boolean open, boolean fromPaused) {}
                    void updateButtons() {}
                    static int checks;
                    static void check(boolean condition, String message) {
                        checks++;
                        if (!condition) throw new AssertionError(message);
                    }
                    public static void test() {
                        FinegramCameraConfig config = FinegramCameraConfig.INSTANCE;
                        SharedSettings.roundVideoLastCamera.set(RoundVideoSession.CameraFacing.BACK);
                        config.preferredRear = true;
                        config.pendingRear = false;
                        RoundCameraFixture view = new RoundCameraFixture();
                        view.showCamera(false);
                        check(view.session.initialFacing == RoundVideoSession.CameraFacing.FRONT, "Front camera selected in the recording menu must override last camera and settings");
                        check(config.pendingRear == null, "Menu selection must be consumed by this recording");
                        view.session = null;
                        view.showCamera(false);
                        check(view.session.initialFacing == RoundVideoSession.CameraFacing.BACK, "The next recording must use settings after the one-time selection");
                        SharedSettings.roundVideoLastCamera.set(RoundVideoSession.CameraFacing.FRONT);
                        config.preferredRear = false;
                        config.pendingRear = true;
                        view.session = null;
                        view.showCamera(false);
                        check(view.session.initialFacing == RoundVideoSession.CameraFacing.BACK, "Rear camera selected in the recording menu must override front-camera settings");
                        config.pendingRear = true;
                        view.showCamera(false);
                        check(config.pendingRear == null, "A duplicate opening request must not leave a selection for a future recording");
                        view.session = null;
                        view.showCamera(false);
                        check(view.session.initialFacing == RoundVideoSession.CameraFacing.FRONT, "Front-camera settings must work without a menu selection");
                        config.pendingRear = true;
                        view.showCamera(true);
                        check(Boolean.TRUE.equals(config.pendingRear), "Resuming an existing recording must not consume a future selection");
                        config.pendingRear = null;
                        view.onCameraCapabilitiesChanged(new RoundVideoSession.CameraCapabilities(RoundVideoSession.CameraFacing.BACK));
                        check(config.lastRear, "The camera actually opened must be remembered for the Last used setting");
                        view.onCameraCapabilitiesChanged(new RoundVideoSession.CameraCapabilities(null));
                        check(config.lastRear, "An unknown active camera must not overwrite the last used camera");
                        view.onCameraCapabilitiesChanged(new RoundVideoSession.CameraCapabilities(RoundVideoSession.CameraFacing.FRONT));
                        check(!config.lastRear, "Switching to the front camera must update the shared last-camera preference");
                        System.out.println("PASS: " + checks + " round-camera selection checks");
                    }
                """ + methods.toString().replace("@Override", "") + "}";
        Path output = Files.createTempDirectory("finegram-round-camera-test-");
        if (!compiler.getTask(null, null, null, List.of("-d", output.toString()), null,
                List.of(source("RoundCameraFixture", fixture))).call()) {
            throw new AssertionError("Could not compile actual camera-opening methods");
        }
        try (var loader = new URLClassLoader(new java.net.URL[]{output.toUri().toURL()})) {
            loader.loadClass("RoundCameraFixture").getMethod("test").invoke(null);
        }
    }
}
