package com.fde;

import android.app.Activity;
import android.util.Log;

public class FrameworkImpl implements FrameworkOperations {

    private static final String TAG = "FrameworkImpl34";

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
        Log.d(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
    }
}
