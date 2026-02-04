package com.fde;

import android.app.Activity;
import android.util.Log;
import com.fde.x11.utils.FLog;
import android.openfde.AppTaskControllerProxy;
import android.openfde.AppTaskStatusListener;
import com.android.internal.policy.DecorView;
import java.lang.ref.WeakReference;

public class FrameworkImpl implements FrameworkOperations {

    private static final String TAG = "FrameworkImpl34";
    private WeakReference<Activity> activity;
    private AppTaskControllerProxy proxy;

    public FrameworkImpl(WeakReference<Activity> activity,
                         boolean hideDecorCaptionView,
                         AppTaskStatusListener listener) {
        this.activity = activity;
        this.proxy = AppTaskControllerProxy.create();
        this.proxy.initCustomCaption(activity, hideDecorCaptionView, listener);
    }

    @Override
    public void hideDecorCaptionView(Activity activity) {
        FLog.e(TAG, "hideDecorCaptionView() called with: activity = [" + activity + "]");
        activity.setWindowDecorationStatus(1);
    }

    @Override
    public void setDecorCaptionViewFocuseable(Activity activity, boolean focusable) {
        FLog.e(TAG, "setDecorCaptionViewFocuseable() called with: activity = [" + activity + "], focusable = [" + focusable + "]");
    }

    @Override
    public void exitFullScreenWindow(Activity activity) {
//        DecorView decorView = (DecorView) activity.getWindow().getDecorView();
//        decorView.exitFullScreenWindow();
        proxy.maximizeOrNot();
        FLog.e(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
    }

    @Override
    public void startFullScreenWindow(Activity activity) {
//        DecorView decorView = (DecorView) activity.getWindow().getDecorView();
//        decorView.startFullScreenWindow(false);
        proxy.maximizeOrNot();
        FLog.e(TAG, "startFullScreenWindow() called with: activity = [" + activity + "]");
    }

    @Override
    public boolean isWindowMaximized() {
        return false;
    }

    @Override
    public boolean startDecorMovingTask(float startX, float startY) {
        DecorView decorView = (DecorView) activity.get().getWindow().getDecorView();
        return decorView.startDecorMovingTask(startX, startY);
    }

    public void finisDecorMovingTask() {
        DecorView decorView = (DecorView) activity.get().getWindow().getDecorView();
        decorView.finisDecorMovingTask();
    }
}
