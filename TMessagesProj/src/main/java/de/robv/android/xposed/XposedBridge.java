package de.robv.android.xposed;

import android.util.Log;

import com.th3nekit.finegram.hook.ArtHook;
import com.th3nekit.finegram.hook.HookCallback;
import com.th3nekit.finegram.hook.HookFrame;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import de.robv.android.xposed.XC_MethodHook.MethodHookParam;

/**
 * Xposed API для плагинов клиента.
 *
 * Плагины пишутся под Xposed: ставят XC_MethodHook на методы клиента и зовут оригинал через
 * invokeOriginalMethod. Сами перехваты делает движок клиента ({@link ArtHook}); здесь только
 * порядок обработчиков Xposed: before — по убыванию приоритета, after — в обратном порядке,
 * ранний выход из before пропускает оригинал и обработчики с меньшим приоритетом.
 */
public final class XposedBridge {

	private static String[] sSupportedFeatures = new String[0];

	/**
	 * The system class loader which can be used to locate Android framework classes.
	 * Application classes cannot be retrieved from it.
	 *
	 * @see ClassLoader#getSystemClassLoader
	 */
	public static final ClassLoader BOOTCLASSLOADER = ClassLoader.getSystemClassLoader();

	/** @hide */
	public static final String TAG = "Xposed";

	/** @deprecated Use {@link #getXposedVersion()} instead. */
	@Deprecated
	public static int XPOSED_BRIDGE_VERSION = 90;

	/** Все обработчики Xposed временно в стороне: методы работают как без них. */
	public static volatile boolean disableHooks = false;

	private static final Map<Member, CopyOnWriteSortedSet<XC_MethodHook>> sHookedMethodCallbacks = new HashMap<>();

	private static HookProvider hookProvider = HookProvider.ART;

	public interface HookProvider {
		HookProvider ART = new HookProvider() {
			@Override
			public void hook(Member method, CopyOnWriteSortedSet<XC_MethodHook> callbacks) {
				ArtHook.hook(method, new Handler(callbacks));
			}

			@Override
			public Object invokeOriginal(Member method, Object thisObject, Object[] args) throws NullPointerException, IllegalAccessException, IllegalArgumentException, InvocationTargetException {
				return ArtHook.invokeOriginal(method, thisObject, args);
			}
		};

		void hook(Member method, CopyOnWriteSortedSet<XC_MethodHook> callbacks);

		Object invokeOriginal(Member method, Object thisObject, Object[] args) throws
				NullPointerException, IllegalAccessException, IllegalArgumentException, InvocationTargetException;
	}

	private XposedBridge() {}

	/**
	 * Returns the currently installed version of the Xposed framework.
	 */
	public static int getXposedVersion() {
		return XPOSED_BRIDGE_VERSION;
	}

	public static void setXposedVersion(int version) {
		XPOSED_BRIDGE_VERSION = version;
	}

	public static HookProvider getHookProvider() {
		return hookProvider;
	}

	public static void setHookProvider(HookProvider provider) {
		hookProvider = provider;
	}

	public static boolean isFeatureSupported(String featureName) {
		for (String f : sSupportedFeatures) {
			if (f.equalsIgnoreCase(featureName)) return true;
		}
		return false;
	}

	public static String[] getSupportedFeatures() {
		return sSupportedFeatures;
	}

	public static void setSupportedFeatures(String[] features) {
		sSupportedFeatures = features;
	}

	/**
	 * Writes a message to the Xposed error log.
	 *
	 * @param text The log message.
	 */
	public static synchronized void log(String text) {
		Log.i(TAG, text);
	}

	/**
	 * Logs a stack trace to the Xposed error log.
	 *
	 * @param t The Throwable object for the stack trace.
	 */
	public static synchronized void log(Throwable t) {
		Log.e(TAG, Log.getStackTraceString(t));
	}

	/**
	 * Deoptimize a method to avoid callee being inlined.
	 *
	 * @param method The method to deoptmize. Generally it should be a caller of a method that is inlined.
	 */
	public static void deoptimizeMethod(Member method) {
		ArtHook.deoptimize(method);
	}

	/**
	 * Hook any method (or constructor) with the specified callback.
	 *
	 * @param hookMethod The method to be hooked.
	 * @param callback The callback to be executed when the hooked method is called.
	 * @return An object that can be used to remove the hook.
	 */
	public static XC_MethodHook.Unhook hookMethod(Member hookMethod, XC_MethodHook callback) {
		if (!(hookMethod instanceof Method) && !(hookMethod instanceof Constructor<?>)) {
			throw new IllegalArgumentException("Only methods and constructors can be hooked: " + hookMethod.toString());
		} else if (Modifier.isAbstract(hookMethod.getModifiers())) {
			throw new IllegalArgumentException("Cannot hook abstract methods: " + hookMethod.toString());
		}

		CopyOnWriteSortedSet<XC_MethodHook> callbacks;
		synchronized (sHookedMethodCallbacks) {
			callbacks = sHookedMethodCallbacks.get(hookMethod);
			if (callbacks == null) {
				callbacks = new CopyOnWriteSortedSet<>();
				// сначала перехват: не встал — метод не числится перехваченным,
				// и следующая попытка поставит его заново
				hookProvider.hook(hookMethod, callbacks);
				sHookedMethodCallbacks.put(hookMethod, callbacks);
			}
		}
		callbacks.add(callback);

		return callback.new Unhook(hookMethod);
	}

	/**
	 * Removes the callback for a hooked method/constructor.
	 *
	 * @deprecated Use {@link XC_MethodHook.Unhook#unhook} instead.
	 */
	@Deprecated
	public static void unhookMethod(Member hookMethod, XC_MethodHook callback) {
		CopyOnWriteSortedSet<XC_MethodHook> callbacks;
		synchronized (sHookedMethodCallbacks) {
			callbacks = sHookedMethodCallbacks.get(hookMethod);
			if (callbacks == null)
				return;
		}
		callbacks.remove(callback);
	}

	/**
	 * Hooks all methods with a certain name that were declared in the specified class.
	 */
	@SuppressWarnings("UnusedReturnValue")
	public static Set<XC_MethodHook.Unhook> hookAllMethods(Class<?> hookClass, String methodName, XC_MethodHook callback) {
		Set<XC_MethodHook.Unhook> unhooks = new HashSet<>();
		for (Member method : hookClass.getDeclaredMethods())
			if (method.getName().equals(methodName))
				unhooks.add(hookMethod(method, callback));
		return unhooks;
	}

	/**
	 * Hook all constructors of the specified class.
	 */
	@SuppressWarnings("UnusedReturnValue")
	public static Set<XC_MethodHook.Unhook> hookAllConstructors(Class<?> hookClass, XC_MethodHook callback) {
		Set<XC_MethodHook.Unhook> unhooks = new HashSet<>();
		for (Member constructor : hookClass.getDeclaredConstructors())
			unhooks.add(hookMethod(constructor, callback));
		return unhooks;
	}

	/**
	 * Basically the same as {@link Method#invoke}, but calls the original method
	 * as it was before the interception by Xposed.
	 */
	public static Object invokeOriginalMethod(Member method, Object thisObject, Object[] args)
			throws NullPointerException, IllegalAccessException, IllegalArgumentException, InvocationTargetException {
		return hookProvider.invokeOriginal(method, thisObject, args);
	}

	/**
	 * Все обработчики Xposed одного метода.
	 *
	 * Что узнали в before — параметр вызова и докуда дошли, — хранится в самом вызове, а не
	 * в потоке: иначе рекурсивный вызов того же метода затирал бы данные внешнего.
	 */
	static final class Handler extends HookCallback {
		private final CopyOnWriteSortedSet<XC_MethodHook> callbacks;

		Handler(CopyOnWriteSortedSet<XC_MethodHook> callbacks) {
			this.callbacks = callbacks;
		}

		private static final class CallState {
			final Object[] callbacks;
			final MethodHookParam param;
			final int afterIdx;

			CallState(Object[] callbacks, MethodHookParam param, int afterIdx) {
				this.callbacks = callbacks;
				this.param = param;
				this.afterIdx = afterIdx;
			}
		}

		@Override
		public void before(HookFrame frame) {
			if (disableHooks) return;

			Object[] callbacksSnapshot = callbacks.getSnapshot();
			final int callbacksLength = callbacksSnapshot.length;
			if (callbacksLength == 0) return;

			MethodHookParam param = new MethodHookParam();
			param.method = frame.method;
			param.thisObject = frame.thisObject;
			param.args = frame.args;

			int beforeIdx = 0;
			do {
				try {
					((XC_MethodHook) callbacksSnapshot[beforeIdx]).beforeHookedMethod(param);
				} catch (Throwable t) {
					XposedBridge.log(t);

					// reset result (ignoring what the unexpectedly exiting callback did)
					param.setResult(null);
					param.returnEarly = false;
					continue;
				}

				if (param.returnEarly) {
					// skip remaining "before" callbacks and corresponding "after" callbacks
					beforeIdx++;
					break;
				}
			} while (++beforeIdx < callbacksLength);

			frame.thisObject = param.thisObject;
			frame.args = param.args;
			if (param.returnEarly) {
				if (param.hasThrowable())
					frame.setThrowable(param.getThrowable());
				else
					frame.setResult(param.getResult());
			}

			frame.setState(new CallState(callbacksSnapshot, param, beforeIdx - 1));
		}

		@Override
		public void after(HookFrame frame) {
			Object stored = frame.getState();
			if (!(stored instanceof CallState)) return;
			CallState state = (CallState) stored;
			MethodHookParam param = state.param;
			Object[] callbacksSnapshot = state.callbacks;
			int afterIdx = state.afterIdx;

			param.thisObject = frame.thisObject;
			param.args = frame.args;
			if (frame.hasThrowable())
				param.setThrowable(frame.getThrowable());
			else
				param.setResult(frame.getResult());

			do {
				Object lastResult = param.getResult();
				Throwable lastThrowable = param.getThrowable();

				try {
					((XC_MethodHook) callbacksSnapshot[afterIdx]).afterHookedMethod(param);
				} catch (Throwable t) {
					XposedBridge.log(t);

					// reset to last result (ignoring what the unexpectedly exiting callback did)
					if (lastThrowable == null)
						param.setResult(lastResult);
					else
						param.setThrowable(lastThrowable);
				}
			} while (--afterIdx >= 0);

			frame.thisObject = param.thisObject;
			frame.args = param.args;
			if (param.hasThrowable())
				frame.setThrowable(param.getThrowable());
			else
				frame.setResult(param.getResult());
		}
	}

	public static final class CopyOnWriteSortedSet<E> {
		private static final Object[] EMPTY = new Object[0];

		private transient volatile Object[] elements = EMPTY;

		@SuppressWarnings("UnusedReturnValue")
		public synchronized boolean add(E e) {
			int index = indexOf(e);
			if (index >= 0)
				return false;

			Object[] newElements = new Object[elements.length + 1];
			System.arraycopy(elements, 0, newElements, 0, elements.length);
			newElements[elements.length] = e;
			Arrays.sort(newElements);
			elements = newElements;
			return true;
		}

		@SuppressWarnings("UnusedReturnValue")
		public synchronized boolean remove(E e) {
			int index = indexOf(e);
			if (index == -1)
				return false;

			Object[] newElements = new Object[elements.length - 1];
			System.arraycopy(elements, 0, newElements, 0, index);
			System.arraycopy(elements, index + 1, newElements, index, elements.length - index - 1);
			elements = newElements;
			return true;
		}

		private int indexOf(Object o) {
			for (int i = 0; i < elements.length; i++) {
				if (o.equals(elements[i]))
					return i;
			}
			return -1;
		}

		public Object[] getSnapshot() {
			return elements;
		}
	}
}
