package com.fde;

import android.app.Activity;
import android.openfde.AppTaskStatusListener;
import android.os.Build;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import com.android.internal.policy.DecorView;
import com.fde.x11.utils.FLog;

import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

//todo
public class FrameworkImpl implements FrameworkOperations{
    private static final String TAG = "FrameworkImpl37";

    private WeakReference<Activity> activity;
    public static int DECOR_CAPTION_HEIGHT = 48;

    public FrameworkImpl(WeakReference<Activity> activity,
                         boolean hideDecorCaptionView) {
        this.activity = activity;
    }

    @Override
    public void exitMaxmizeWindow(Activity activity){

    }


    @Override
    public void hideDecorCaptionView(Activity activity) {
        FLog.a(TAG, "hideDecorCaptionView() called with: activity = [" + activity + "]");
        if(Build.VERSION.SDK_INT == 30){
            activity.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
    }

    @Override
    public void setDecorCaptionViewFocuseable(Activity activity, boolean focusable) {
        FLog.a(TAG, "setDecorCaptionViewFocuseable() called with: activity = [" + activity + "], focusable = [" + focusable + "]");
    }

    @Override
    public void exitFullScreenWindow(Activity activity) {
        FLog.a(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
    }

    @Override
    public void startFullScreenWindow(Activity activity) {
        FLog.a(TAG, "startFullScreenWindow() called with: activity = [" + activity + "]");
    }

    @Override
    public boolean isWindowMaximized() {
        return false;
    }

    @Override
    public boolean startDecorMovingTask(float startX, float startY) {
        return false;
    }

    @Override
    public void finisDecorMovingTask() {

    }
}
