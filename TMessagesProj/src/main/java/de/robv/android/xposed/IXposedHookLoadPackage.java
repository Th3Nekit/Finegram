package de.robv.android.xposed;

import android.app.Application;

import de.robv.android.xposed.callbacks.XC_LoadPackage;
import de.robv.android.xposed.callbacks.XC_LoadPackage.LoadPackageParam;

public interface IXposedHookLoadPackage extends IXposedMod {

	void handleLoadPackage(LoadPackageParam lpparam) throws Throwable;

	final class Wrapper extends XC_LoadPackage {
		private final IXposedHookLoadPackage instance;
		public Wrapper(IXposedHookLoadPackage instance) {
			this.instance = instance;
		}
		@Override
		public void handleLoadPackage(LoadPackageParam lpparam) throws Throwable {
			instance.handleLoadPackage(lpparam);
		}
	}
}
