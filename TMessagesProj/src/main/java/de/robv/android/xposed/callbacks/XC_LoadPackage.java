package de.robv.android.xposed.callbacks;

import android.content.pm.ApplicationInfo;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XposedBridge.CopyOnWriteSortedSet;

public abstract class XC_LoadPackage extends XCallback implements IXposedHookLoadPackage {

	@SuppressWarnings("deprecation")
	public XC_LoadPackage() {
		super();
	}

	public XC_LoadPackage(int priority) {
		super(priority);
	}

	public static final class LoadPackageParam extends Param {

		public LoadPackageParam(CopyOnWriteSortedSet<XC_LoadPackage> callbacks) {
			super(callbacks);
		}

		public String packageName;

		public String processName;

		public ClassLoader classLoader;

		public ApplicationInfo appInfo;

		public boolean isFirstApplication;
	}

	@Override
	protected void call(Param param) throws Throwable {
		if (param instanceof LoadPackageParam)
			handleLoadPackage((LoadPackageParam) param);
	}
}
