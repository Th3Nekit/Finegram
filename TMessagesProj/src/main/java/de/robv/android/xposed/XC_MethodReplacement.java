package de.robv.android.xposed;

import de.robv.android.xposed.callbacks.XCallback;

public abstract class XC_MethodReplacement extends XC_MethodHook {

	public XC_MethodReplacement() {
		super();
	}

	public XC_MethodReplacement(int priority) {
		super(priority);
	}

	@Override
	protected final void beforeHookedMethod(MethodHookParam param) throws Throwable {
		try {
			Object result = replaceHookedMethod(param);
			param.setResult(result);
		} catch (Throwable t) {
			param.setThrowable(t);
		}
	}

	@Override
	@SuppressWarnings("EmptyMethod")
	protected final void afterHookedMethod(MethodHookParam param) throws Throwable {}

	@SuppressWarnings("UnusedParameters")
	protected abstract Object replaceHookedMethod(MethodHookParam param) throws Throwable;

	public static final XC_MethodReplacement DO_NOTHING = new XC_MethodReplacement(PRIORITY_HIGHEST*2) {
		@Override
		protected Object replaceHookedMethod(MethodHookParam param) throws Throwable {
			return null;
		}
	};

	public static XC_MethodReplacement returnConstant(final Object result) {
		return returnConstant(PRIORITY_DEFAULT, result);
	}

	public static XC_MethodReplacement returnConstant(int priority, final Object result) {
		return new XC_MethodReplacement(priority) {
			@Override
			protected Object replaceHookedMethod(MethodHookParam param) throws Throwable {
				return result;
			}
		};
	}

}
