package de.robv.android.xposed;

import java.lang.reflect.Member;

import de.robv.android.xposed.callbacks.IXUnhook;
import de.robv.android.xposed.callbacks.XCallback;

public abstract class XC_MethodHook extends XCallback {

	@SuppressWarnings("deprecation")
	public XC_MethodHook() {
		super();
	}

	public XC_MethodHook(int priority) {
		super(priority);
	}

	protected void beforeHookedMethod(MethodHookParam param) throws Throwable {}

	protected void afterHookedMethod(MethodHookParam param) throws Throwable {}

	public static final class MethodHookParam extends Param {

		@SuppressWarnings("deprecation")
		public MethodHookParam() {
			super();
		}

		public Member method;

		public Object thisObject;

		public Object[] args;

		private Object result = null;
		private Throwable throwable = null;
		              boolean returnEarly = false;

		public Object getResult() {
			return result;
		}

		public void setResult(Object result) {
			this.result = result;
			this.throwable = null;
			this.returnEarly = true;
		}

		public Throwable getThrowable() {
			return throwable;
		}

		public boolean hasThrowable() {
			return throwable != null;
		}

		public void setThrowable(Throwable throwable) {
			this.throwable = throwable;
			this.result = null;
			this.returnEarly = true;
		}

		public Object getResultOrThrowable() throws Throwable {
			if (throwable != null)
				throw throwable;
			return result;
		}
	}

	public class Unhook implements IXUnhook<XC_MethodHook> {
		private final Member hookMethod;

		            Unhook(Member hookMethod) {
			this.hookMethod = hookMethod;
		}

		public Member getHookedMethod() {
			return hookMethod;
		}

		@Override
		public XC_MethodHook getCallback() {
			return XC_MethodHook.this;
		}

		@SuppressWarnings("deprecation")
		@Override
		public void unhook() {
			XposedBridge.unhookMethod(hookMethod, XC_MethodHook.this);
		}

	}
}
