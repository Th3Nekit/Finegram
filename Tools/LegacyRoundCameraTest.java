import com.sun.source.tree.IfTree;
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

public class LegacyRoundCameraTest {
    static JavaFileObject source(String name, String text) {
        return new SimpleJavaFileObject(URI.create("string:///" + name + ".java"), JavaFileObject.Kind.SOURCE) {
            public CharSequence getCharContent(boolean ignoreErrors) { return text; }
        };
    }

    static String extract(Path path, boolean cameraSetup) throws Exception {
        String original = Files.readString(path);
        var compiler = ToolProvider.getSystemJavaCompiler();
        var task = (JavacTask) compiler.getTask(null, null, null, List.of("-proc:none"), null,
                List.of(source("Original", original)));
        var unit = task.parse().iterator().next();
        var positions = Trees.instance(task).getSourcePositions();
        var result = new StringBuilder();
        new TreeScanner<Void, Void>() {
            boolean inShowCamera;
            public Void visitMethod(MethodTree method, Void unused) {
                if (!cameraSetup && List.of("fgCommitCameraChooser", "fgCommitCameraChooserPick")
                        .contains(method.getName().toString())) {
                    result.append(original, (int) positions.getStartPosition(unit, method),
                            (int) positions.getEndPosition(unit, method));
                }
                boolean previous = inShowCamera;
                inShowCamera = cameraSetup && method.getName().contentEquals("showCamera");
                super.visitMethod(method, unused);
                inShowCamera = previous;
                return null;
            }
            public Void visitIf(IfTree statement, Void unused) {
                if (inShowCamera && result.isEmpty() && statement.getCondition().toString().equals("(useCamera2)")) {
                    result.append(original, (int) positions.getStartPosition(unit, statement),
                            (int) positions.getEndPosition(unit, statement));
                }
                return super.visitIf(statement, unused);
            }
        }.scan(unit, null);
        if (result.isEmpty()) throw new AssertionError("Production camera code not found");
        return result.toString();
    }

    public static void main(String[] args) throws Exception {
        String root = "TMessagesProj/src/main/java/org/telegram/ui/Components/";
        String camera = extract(Path.of(root + "InstantCameraView.java"), true);
        String chooser = extract(Path.of(root + "ChatActivityEnterView.java"), false);
        String fixture = """
                public class LegacyCameraFixture {
                    boolean useCamera2 = true, bothCameras, isFrontface;
                    int surfaceIndex;
                    Camera2Session[] camera2Sessions = new Camera2Session[2];
                    Camera2Session camera2SessionCurrent;
                    Size[] previewSize = new Size[2];
                    Object getContext() { return null; }
                    void updateFlash() {}
                    Camera2Session nmCreateRoundSession(boolean front) { return new Camera2Session(front); }
                    void nmHandleRoundCamError(int index, Camera2Session session, int error) {}
                    void nmArmRoundFrameWatchdog() {}
                    void setPreviewSize(Size size) { previewSize[0] = size; }
                    static class Size { Size(int w, int h) {} }
                    static class Camera2Session {
                        final boolean front;
                        Camera2Session(boolean front) { this.front = front; }
                        static Camera2Session create(boolean front, int w, int h) { return new Camera2Session(front); }
                        void setRecordingVideo(boolean value) {}
                        void whenError(java.util.function.Consumer<Integer> callback) {}
                        void destroy(boolean async) {}
                        int getPreviewWidth() { return 640; }
                        int getPreviewHeight() { return 480; }
                    }
                    static class DualCameraView { static boolean roundDualAvailableStatic(Object context) { return true; } }
                    static class FinegramCameraConfig {
                        static final FinegramCameraConfig INSTANCE = new FinegramCameraConfig();
                        boolean dual;
                        boolean getUseDualCamera() { return dual; }
                    }
                    static class UserConfig { static int selectedAccount; }
                    static class MessagesController {
                        int roundVideoSize = 512;
                        static MessagesController getInstance(int account) { return new MessagesController(); }
                    }
                    void setup() { %s }
                    boolean fgChooserActive = true;
                    int fgChooserHover = -1, opened = -1;
                    ActionBarPopupWindow fgCameraChooser = new ActionBarPopupWindow();
                    class ActionBarPopupWindow {
                        boolean dismissed, focusable, outside;
                        void dismiss() { dismissed = true; fgChooserActive = false; fgChooserHover = -1; fgCameraChooser = null; }
                        void setFocusable(boolean value) { focusable = value; }
                        void setOutsideTouchable(boolean value) { outside = value; }
                        void update() {}
                    }
                    void fgOpenRoundCamera(boolean rear) { opened = rear ? 1 : 0; }
                    %s
                    static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
                    static int passed, failed;
                    static void test(String name, Runnable run) {
                        try { run.run(); passed++; }
                        catch (AssertionError error) { failed++; System.err.println(name + ": " + error.getMessage()); }
                    }
                    public static void run() {
                        test("dual rear", () -> {
                            var f = new LegacyCameraFixture(); FinegramCameraConfig.INSTANCE.dual = true; f.setup();
                            check(!f.camera2SessionCurrent.front && f.surfaceIndex == 1, "rear session must render rear surface");
                        });
                        test("dual front", () -> {
                            var f = new LegacyCameraFixture(); FinegramCameraConfig.INSTANCE.dual = true; f.isFrontface = true; f.surfaceIndex = 1; f.setup();
                            check(f.camera2SessionCurrent.front && f.surfaceIndex == 0, "front must reset the previous surface");
                        });
                        test("single rear", () -> {
                            var f = new LegacyCameraFixture(); FinegramCameraConfig.INSTANCE.dual = false; f.surfaceIndex = 1; f.setup();
                            check(!f.camera2SessionCurrent.front && f.surfaceIndex == 0, "single camera always uses surface zero");
                        });
                        test("release without hover", () -> {
                            var f = new LegacyCameraFixture(); var popup = f.fgCameraChooser; f.fgCommitCameraChooser(true);
                            check(!popup.dismissed && f.fgChooserActive && f.opened == -1, "menu must wait for a tap");
                            check(popup.focusable && popup.outside, "menu must close on Back or an outside tap");
                            f.fgCommitCameraChooserPick(true);
                            check(popup.dismissed && f.opened == 1, "tap must open the rear camera");
                        });
                        test("drag to rear", () -> {
                            var f = new LegacyCameraFixture(); var popup = f.fgCameraChooser; f.fgChooserHover = 1; f.fgCommitCameraChooser(true);
                            check(popup.dismissed && f.opened == 1, "drag release must open rear");
                        });
                        test("cancel gesture", () -> {
                            var f = new LegacyCameraFixture(); var popup = f.fgCameraChooser; f.fgChooserHover = 1; f.fgCommitCameraChooser(false);
                            check(popup.dismissed && f.opened == -1, "cancel must not start recording");
                        });
                        System.out.println("Legacy camera: " + passed + " passed, " + failed + " failed");
                        if (failed > 0) throw new AssertionError("Camera regressions");
                    }
                }
                """.formatted(camera, chooser);
        fixture = fixture.replace("com.th3nekit.finegram.core.configs.FinegramCameraConfig.INSTANCE", "FinegramCameraConfig.INSTANCE");
        var dir = Files.createTempDirectory("legacy-camera-test-");
        var compiler = ToolProvider.getSystemJavaCompiler();
        boolean compiled = compiler.getTask(null, null, null, List.of("-d", dir.toString(), "-proc:none"), null,
                List.of(source("LegacyCameraFixture", fixture))).call();
        if (!compiled) throw new AssertionError("Fixture compilation failed");
        try (var loader = new URLClassLoader(new java.net.URL[]{dir.toUri().toURL()})) {
            loader.loadClass("LegacyCameraFixture").getMethod("run").invoke(null);
        }
    }
}
