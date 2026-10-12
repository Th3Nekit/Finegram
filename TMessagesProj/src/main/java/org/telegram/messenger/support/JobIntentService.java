/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.telegram.messenger.support;

import android.app.Service;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobServiceEngine;
import android.app.job.JobWorkItem;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import android.util.Log;

import org.telegram.messenger.FileLog;

import java.util.ArrayList;
import java.util.HashMap;

public abstract class JobIntentService extends Service {
    static final String TAG = "JobIntentService";

    static final boolean DEBUG = false;

    CompatJobEngine mJobImpl;
    WorkEnqueuer mCompatWorkEnqueuer;
    CommandProcessor mCurProcessor;
    boolean mInterruptIfStopped = false;
    boolean mStopped = false;
    boolean mDestroyed = false;

    final ArrayList<CompatWorkItem> mCompatQueue;

    static final Object sLock = new Object();
    static final HashMap<ComponentName, WorkEnqueuer> sClassWorkEnqueuer = new HashMap<>();

    abstract static class WorkEnqueuer {
        final ComponentName mComponentName;

        boolean mHasJobId;
        int mJobId;

        WorkEnqueuer(Context context, ComponentName cn) {
            mComponentName = cn;
        }

        void ensureJobId(int jobId) {
            if (!mHasJobId) {
                mHasJobId = true;
                mJobId = jobId;
            } else if (mJobId != jobId) {
                throw new IllegalArgumentException("Given job ID " + jobId
                        + " is different than previous " + mJobId);
            }
        }

        abstract void enqueueWork(Intent work);

        public void serviceStartReceived() {
        }

        public void serviceProcessingStarted() {
        }

        public void serviceProcessingFinished() {
        }
    }

    interface CompatJobEngine {
        IBinder compatGetBinder();
        GenericWorkItem dequeueWork();
    }

    static final class CompatWorkEnqueuer extends WorkEnqueuer {
        private final Context mContext;
        private final PowerManager.WakeLock mLaunchWakeLock;
        private final PowerManager.WakeLock mRunWakeLock;
        boolean mLaunchingService;
        boolean mServiceProcessing;

        CompatWorkEnqueuer(Context context, ComponentName cn) {
            super(context, cn);
            mContext = context.getApplicationContext();

            PowerManager pm = ((PowerManager) context.getSystemService(Context.POWER_SERVICE));
            mLaunchWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                    cn.getClassName() + ":launch");
            mLaunchWakeLock.setReferenceCounted(false);
            mRunWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                    cn.getClassName() + ":run");
            mRunWakeLock.setReferenceCounted(false);
        }

        @Override
        void enqueueWork(Intent work) {
            Intent intent = new Intent(work);
            intent.setComponent(mComponentName);
            if (DEBUG) Log.d(TAG, "Starting service for work: " + work);
            if (mContext.startService(intent) != null) {
                synchronized (this) {
                    if (!mLaunchingService) {
                        mLaunchingService = true;
                        if (!mServiceProcessing) {

                            mLaunchWakeLock.acquire(60 * 1000);
                        }
                    }
                }
            }
        }

        @Override
        public void serviceStartReceived() {
            synchronized (this) {

                mLaunchingService = false;
            }
        }

        @Override
        public void serviceProcessingStarted() {
            synchronized (this) {

                if (!mServiceProcessing) {
                    mServiceProcessing = true;

                    try {
                        mRunWakeLock.acquire(2 * 60 * 1000L);
                        mLaunchWakeLock.release();
                    } catch (Throwable e) {
                        FileLog.e(e);
                        mServiceProcessing = false;
                    }
                }
            }
        }

        @Override
        public void serviceProcessingFinished() {
            synchronized (this) {
                if (mServiceProcessing) {

                    if (mLaunchingService) {
                        mLaunchWakeLock.acquire(60 * 1000);
                    }
                    mServiceProcessing = false;
                    mRunWakeLock.release();
                }
            }
        }
    }

    @RequiresApi(26)
    static final class JobServiceEngineImpl extends JobServiceEngine
            implements CompatJobEngine {
        static final String TAG = "JobServiceEngineImpl";

        static final boolean DEBUG = false;

        final JobIntentService mService;
        final Object mLock = new Object();
        JobParameters mParams;

        final class WrapperWorkItem implements GenericWorkItem {
            final JobWorkItem mJobWork;

            WrapperWorkItem(JobWorkItem jobWork) {
                mJobWork = jobWork;
            }

            @Override
            public Intent getIntent() {
                return mJobWork.getIntent();
            }

            @Override
            public void complete() {
                synchronized (mLock) {
                    if (mParams != null) {
                        mParams.completeWork(mJobWork);
                    }
                }
            }
        }

        JobServiceEngineImpl(JobIntentService service) {
            super(service);
            mService = service;
        }

        @Override
        public IBinder compatGetBinder() {
            return getBinder();
        }

        @Override
        public boolean onStartJob(JobParameters params) {
            if (DEBUG) Log.d(TAG, "onStartJob: " + params);
            mParams = params;

            mService.ensureProcessorRunningLocked(false);
            return true;
        }

        @Override
        public boolean onStopJob(JobParameters params) {
            if (DEBUG) Log.d(TAG, "onStartJob: " + params);
            boolean result = mService.doStopCurrentWork();
            synchronized (mLock) {

                mParams = null;
            }
            return result;
        }

        @Override
        public GenericWorkItem dequeueWork() {
            JobWorkItem work = null;
            synchronized (mLock) {
                if (mParams == null) {
                    return null;
                }
                try {
                    work = mParams.dequeueWork();
                } catch (Throwable ignore) {

                }
            }
            if (work != null) {
                work.getIntent().setExtrasClassLoader(mService.getClassLoader());
                return new JobServiceEngineImpl.WrapperWorkItem(work);
            } else {
                return null;
            }
        }
    }

    @RequiresApi(26)
    static final class JobWorkEnqueuer extends WorkEnqueuer {
        private final JobInfo mJobInfo;
        private final JobScheduler mJobScheduler;

        JobWorkEnqueuer(Context context, ComponentName cn, int jobId) {
            super(context, cn);
            ensureJobId(jobId);
            JobInfo.Builder b = new JobInfo.Builder(jobId, mComponentName);
            mJobInfo = b.setOverrideDeadline(0).setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY).build();
            mJobScheduler = (JobScheduler) context.getApplicationContext().getSystemService(
                    Context.JOB_SCHEDULER_SERVICE);
        }

        @Override
        void enqueueWork(Intent work) {
            if (DEBUG) Log.d(TAG, "Enqueueing work: " + work);
            mJobScheduler.enqueue(mJobInfo, new JobWorkItem(work));
        }
    }

    interface GenericWorkItem {
        Intent getIntent();
        void complete();
    }

    final class CompatWorkItem implements GenericWorkItem {
        final Intent mIntent;
        final int mStartId;

        CompatWorkItem(Intent intent, int startId) {
            mIntent = intent;
            mStartId = startId;
        }

        @Override
        public Intent getIntent() {
            return mIntent;
        }

        @Override
        public void complete() {
            if (DEBUG) Log.d(TAG, "Stopping self: #" + mStartId);
            stopSelf(mStartId);
        }
    }

    final class CommandProcessor extends AsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... params) {
            GenericWorkItem work;

            if (DEBUG) Log.d(TAG, "Starting to dequeue work...");

            while ((work = dequeueWork()) != null) {
                if (DEBUG) Log.d(TAG, "Processing next work: " + work);
                onHandleWork(work.getIntent());
                if (DEBUG) Log.d(TAG, "Completing work: " + work);
                try {
                    work.complete();
                } catch (Throwable ignore) {

                }
            }

            if (DEBUG) Log.d(TAG, "Done processing work!");

            return null;
        }

        @Override
        protected void onCancelled(Void aVoid) {
            processorFinished();
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            processorFinished();
        }
    }

    public JobIntentService() {
        if (Build.VERSION.SDK_INT >= 26) {
            mCompatQueue = null;
        } else {
            mCompatQueue = new ArrayList<>();
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) Log.d(TAG, "CREATING: " + this);
        if (Build.VERSION.SDK_INT >= 26) {
            mJobImpl = new JobServiceEngineImpl(this);
            mCompatWorkEnqueuer = null;
        } else {
            mJobImpl = null;
            ComponentName cn = new ComponentName(this, this.getClass());
            mCompatWorkEnqueuer = getWorkEnqueuer(this, cn, false, 0);
        }
    }

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        if (mCompatQueue != null) {
            mCompatWorkEnqueuer.serviceStartReceived();
            if (DEBUG) Log.d(TAG, "Received compat start command #" + startId + ": " + intent);
            synchronized (mCompatQueue) {
                mCompatQueue.add(new CompatWorkItem(intent != null ? intent : new Intent(),
                        startId));
                ensureProcessorRunningLocked(true);
            }
            return START_REDELIVER_INTENT;
        } else {
            if (DEBUG) Log.d(TAG, "Ignoring start command: " + intent);
            return START_NOT_STICKY;
        }
    }

    @Override
    public IBinder onBind(@NonNull Intent intent) {
        if (mJobImpl != null) {
            IBinder engine = mJobImpl.compatGetBinder();
            if (DEBUG) Log.d(TAG, "Returning engine: " + engine);
            return engine;
        } else {
            return null;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mCompatQueue != null) {
            synchronized (mCompatQueue) {
                mDestroyed = true;
                mCompatWorkEnqueuer.serviceProcessingFinished();
            }
        }
    }

    public static void enqueueWork(@NonNull Context context, @NonNull Class cls, int jobId,
                                   @NonNull Intent work) {
        enqueueWork(context, new ComponentName(context, cls), jobId, work);
    }

    public static void enqueueWork(@NonNull Context context, @NonNull ComponentName component,
                                   int jobId, @NonNull Intent work) {
        if (work == null) {
            throw new IllegalArgumentException("work must not be null");
        }
        synchronized (sLock) {
            WorkEnqueuer we = getWorkEnqueuer(context, component, true, jobId);
            we.ensureJobId(jobId);
            we.enqueueWork(work);
        }
    }

    static WorkEnqueuer getWorkEnqueuer(Context context, ComponentName cn, boolean hasJobId,
                                                                                int jobId) {
        WorkEnqueuer we = sClassWorkEnqueuer.get(cn);
        if (we == null) {
            if (Build.VERSION.SDK_INT >= 26) {
                if (!hasJobId) {
                    throw new IllegalArgumentException("Can't be here without a job id");
                }
                we = new JobWorkEnqueuer(context, cn, jobId);
            } else {
                we = new CompatWorkEnqueuer(context, cn);
            }
            sClassWorkEnqueuer.put(cn, we);
        }
        return we;
    }

    protected abstract void onHandleWork(@NonNull Intent intent);

    public void setInterruptIfStopped(boolean interruptIfStopped) {
        mInterruptIfStopped = interruptIfStopped;
    }

    public boolean isStopped() {
        return mStopped;
    }

    public boolean onStopCurrentWork() {
        return true;
    }

    boolean doStopCurrentWork() {
        if (mCurProcessor != null) {
            mCurProcessor.cancel(mInterruptIfStopped);
        }
        mStopped = true;
        return onStopCurrentWork();
    }

    void ensureProcessorRunningLocked(boolean reportStarted) {
        if (mCurProcessor == null) {
            mCurProcessor = new CommandProcessor();
            if (mCompatWorkEnqueuer != null && reportStarted) {
                mCompatWorkEnqueuer.serviceProcessingStarted();
            }
            if (DEBUG) Log.d(TAG, "Starting processor: " + mCurProcessor);
            mCurProcessor.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    void processorFinished() {
        if (mCompatQueue != null) {
            synchronized (mCompatQueue) {
                mCurProcessor = null;

                if (mCompatQueue != null && mCompatQueue.size() > 0) {
                    ensureProcessorRunningLocked(false);
                } else if (!mDestroyed) {
                    mCompatWorkEnqueuer.serviceProcessingFinished();
                }
            }
        }
    }

    GenericWorkItem dequeueWork() {
        if (mJobImpl != null) {
            return mJobImpl.dequeueWork();
        } else {
            synchronized (mCompatQueue) {
                if (mCompatQueue.size() > 0) {
                    return mCompatQueue.remove(0);
                } else {
                    return null;
                }
            }
        }
    }
}