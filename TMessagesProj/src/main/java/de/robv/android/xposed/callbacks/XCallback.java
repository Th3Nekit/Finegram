package de.robv.android.xposed.callbacks;

import android.os.Bundle;

import java.io.Serializable;

import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedBridge.CopyOnWriteSortedSet;

public abstract class XCallback implements Comparable<XCallback> {

	public final int priority;

	@Deprecated
	public XCallback() {
		this.priority = PRIORITY_DEFAULT;
	}

	public XCallback(int priority) {
		this.priority = priority;
	}

	public static abstract class Param {

		public final Object[] callbacks;
		private Bundle extra;

		@Deprecated
		protected Param() {
			callbacks = null;
		}

		protected Param(CopyOnWriteSortedSet<? extends XCallback> callbacks) {
			this.callbacks = callbacks.getSnapshot();
		}

		public synchronized Bundle getExtra() {
			if (extra == null)
				extra = new Bundle();
			return extra;
		}

		public Object getObjectExtra(String key) {
			Serializable o = getExtra().getSerializable(key);
			if (o instanceof SerializeWrapper)
				return ((SerializeWrapper) o).object;
			return null;
		}

		public void setObjectExtra(String key, Object o) {
			getExtra().putSerializable(key, new SerializeWrapper(o));
		}

		private static class SerializeWrapper implements Serializable {
			private static final long serialVersionUID = 1L;
			private final Object object;
			public SerializeWrapper(Object o) {
				object = o;
			}
		}
	}

	public static void callAll(Param param) {
		if (param.callbacks == null)
			throw new IllegalStateException("This object was not created for use with callAll");

		for (int i = 0; i < param.callbacks.length; i++) {
			try {
				((XCallback) param.callbacks[i]).call(param);
			} catch (Throwable t) { XposedBridge.log(t); }
		}
	}

	protected void call(Param param) throws Throwable {}

	@Override
	public int compareTo(XCallback other) {
		if (this == other)
			return 0;

		if (other.priority != this.priority)
			return other.priority - this.priority;

		else if (System.identityHashCode(this) < System.identityHashCode(other))
			return -1;
		else
			return 1;
	}

	public static final int PRIORITY_DEFAULT = 50;

	public static final int PRIORITY_LOWEST = -10000;

	public static final int PRIORITY_HIGHEST = 10000;
}
