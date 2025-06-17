package com.fde;

import android.app.Activity;
import android.util.Log;

import com.android.internal.policy.DecorView;

public class FrameworkImpl implements FrameworkOperations {

    private static final String TAG = "FrameworkImpl34";
    private Activity activity;

    public FrameworkImpl(Activity activity) {
        this.activity = activity;
    }

    @Override
    public void hideDecorCaptionView(Activity activity) {
        Log.d(TAG, "hideDecorCaptionView() called with: activity = [" + activity + "]");
        activity.setWindowDecorationStatus(1);
    }

    @Override
    public void setDecorCaptionViewFocuseable(Activity activity, boolean focusable) {
        Log.d(TAG, "setDecorCaptionViewFocuseable() called with: activity = [" + activity + "], focusable = [" + focusable + "]");
    }

    @Override
    public void exitFullScreenWindow(Activity activity) {
        DecorView decorView = (DecorView) activity.getWindow().getDecorView();
        decorView.exitFullScreenWindow();
        Log.d(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
    }

    @Override
    public void startFullScreenWindow(Activity activity) {
        DecorView decorView = (DecorView) activity.getWindow().getDecorView();
        decorView.startFullScreenWindow();
        Log.d(TAG, "startFullScreenWindow() called with: activity = [" + activity + "]");
    }

    @Override
    public boolean isWindowMaximized() {
        return false;
    }
}
