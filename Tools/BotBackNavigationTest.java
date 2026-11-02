import com.sun.source.tree.ClassTree;
import com.sun.source.tree.MethodTree;
import com.sun.source.util.JavacTask;
import com.sun.source.util.Trees;
import java.net.URI;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import javax.tools.JavaFileObject;
import javax.tools.SimpleJavaFileObject;
import javax.tools.ToolProvider;

public class BotBackNavigationTest {
    private static JavaFileObject source(String name, String text) {
        return new SimpleJavaFileObject(URI.create("string:///" + name + ".java"), JavaFileObject.Kind.SOURCE) {
            @Override public CharSequence getCharContent(boolean ignoreEncodingErrors) { return text; }
        };
    }

    public static void main(String[] args) throws Exception {
        var compiler = ToolProvider.getSystemJavaCompiler();
        String original = Files.readString(Path.of("TMessagesProj/src/main/java/org/telegram/ui/Components/ChatActivityEnterView.java"));
        var task = (JavacTask) compiler.getTask(null, null, null, List.of("-proc:none"), null,
                List.of(source("ChatActivityEnterView", original)));
        var unit = task.parse().iterator().next();
        var positions = Trees.instance(task).getSourcePositions();
        StringBuilder methods = new StringBuilder();
        for (var declaration : unit.getTypeDecls()) {
            if (!(declaration instanceof ClassTree type)) continue;
            for (var member : type.getMembers()) {
                if (member instanceof MethodTree method && List.of("swipeToBackEnabled", "botCommandsMenuIsVisible").contains(method.getName().toString())) {
                    methods.append(original, (int) positions.getStartPosition(unit, member), (int) positions.getEndPosition(unit, member));
                }
            }
        }
        String fixture = """
                public class ComposerBackFixture {
                    static final int VISIBLE = 0;
                    static class View {
                        static final int VISIBLE = 0;
                        int visibility = 8;
                        int getVisibility() { return visibility; }
                    }
                    static class MenuButton {
                        boolean opened;
                        boolean isOpened() { return opened; }
                        void setOpened(boolean value) { opened = value; }
                    }
                    static class BaseFragment {
                        boolean sheets;
                        static boolean hasSheets(BaseFragment fragment) { return fragment.sheets; }
                    }
                    boolean recordingAudioVideo, videoMode, webView;
                    View recordedAudioPanel, botCommandsMenuContainer;
                    MenuButton botCommandsMenuButton;
                    BaseFragment parentFragment = new BaseFragment();
                    boolean isInVideoMode() { return videoMode; }
                    boolean hasBotWebView() { return webView; }
                    static void check(boolean condition, String message) {
                        if (!condition) throw new AssertionError(message);
                    }
                    public static void test() {
                        ComposerBackFixture composer = new ComposerBackFixture();
                        composer.webView = true;
                        composer.botCommandsMenuButton = new MenuButton();
                        composer.botCommandsMenuButton.opened = true;
                        check(composer.swipeToBackEnabled(), "An invisible bot menu must not trap the back gesture");
                        check(!composer.botCommandsMenuIsVisible(), "A stale button is not a visible menu");
                        check(composer.botCommandsMenuButton.opened, "Visibility queries must not mutate predictive-back state");
                        composer.botCommandsMenuButton = null;
                        check(composer.swipeToBackEnabled(), "A missing menu button must not crash back navigation");
                        composer.botCommandsMenuContainer = new View();
                        composer.botCommandsMenuContainer.visibility = 0;
                        check(composer.botCommandsMenuIsVisible(), "The dismiss animation remains visible after the button closes");
                        check(!composer.swipeToBackEnabled(), "A visible menu owns the back gesture");
                        composer.botCommandsMenuContainer.visibility = 8;
                        composer.parentFragment.sheets = true;
                        check(!composer.swipeToBackEnabled(), "A real mini app sheet owns the back gesture");
                        composer.parentFragment.sheets = false;
                        composer.recordingAudioVideo = true;
                        check(!composer.swipeToBackEnabled(), "Recording must keep its back protection");
                        composer.recordingAudioVideo = false;
                        composer.videoMode = true;
                        composer.recordedAudioPanel = new View();
                        composer.recordedAudioPanel.visibility = 0;
                        check(!composer.swipeToBackEnabled(), "A recorded round video keeps its back protection");
                        System.out.println("PASS: 9 bot/composer navigation checks");
                    }
                """ + methods + "}";
        Path output = Files.createTempDirectory("finegram-bot-back-test-");
        if (!compiler.getTask(null, null, null, List.of("-d", output.toString()), null,
                List.of(source("ComposerBackFixture", fixture))).call()) {
            throw new AssertionError("Could not compile actual composer navigation methods");
        }
        try (var loader = new URLClassLoader(new java.net.URL[]{output.toUri().toURL()})) {
            loader.loadClass("ComposerBackFixture").getMethod("test").invoke(null);
        }
    }
}
