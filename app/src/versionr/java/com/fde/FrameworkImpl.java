package com.fde;

import android.app.Activity;
import android.os.Build;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import com.android.internal.policy.DecorView;
import com.android.internal.widget.DecorCaptionView;
import com.fde.x11.utils.FLog;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class FrameworkImpl implements FrameworkOperations{
    private static final String TAG = "FrameworkImpl30";

    private Activity activity;

    public FrameworkImpl(Activity activity) {
        this.activity = activity;
    }


    @Override
    public void hideDecorCaptionView(Activity activity) {
        FLog.e(TAG, "hideDecorCaptionView() called with: activity = [" + activity + "]");
        if(Build.VERSION.SDK_INT == 30  ){
            activity.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
    }

    @Override
    public void setDecorCaptionViewFocuseable(Activity activity, boolean focusable) {
        FLog.e(TAG, "setDecorCaptionViewFocuseable() called with: activity = [" + activity + "], focusable = [" + focusable + "]");
        Window window = activity.getWindow();
        ViewGroup decor = (ViewGroup) window.getDecorView();
        DecorCaptionView decorCaptionView = (DecorCaptionView) decor.getChildAt(0);
        boolean isCaptionShowing = true;
        try {
            Class<?> aClass = Class.forName("com.android.internal.widget.DecorCaptionView");
            Method method = aClass.getMethod("isCaptionShowing");
            isCaptionShowing = (boolean) method.invoke(decorCaptionView);
            if(isCaptionShowing) {
//                    decorCaptionView.setOperateEnabled(focusable);
            }
        } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException |
                 InvocationTargetException e) {
            FLog.e(TAG, e.getMessage());
        }
    }

    @Override
    public void exitFullScreenWindow(Activity activity) {
        FLog.e(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
        DecorCaptionView captionView = getCaptionView(activity);
        if(captionView == null){
            return;
        }
        captionView.exitFullScreenWindow();
        captionView.toggleFreeformWindowingMode();
    }

    @Override
    public void startFullScreenWindow(Activity activity) {
        DecorCaptionView captionView = getCaptionView(activity);
        if(captionView == null){
            return;
        }
        if(Build.VERSION.SDK_INT == 30 ){
            captionView.exitFullScreenWindow();
            captionView.toggleFreeformWindowingMode();
        }
        FLog.e(TAG, "startFullScreenWindow() called with: activity = [" + activity + "]");
    }


    private DecorCaptionView getCaptionView(Activity activity) {
        DecorView decorView = (DecorView) activity.getWindow().getDecorView();
        if(decorView.getChildCount() > 0){
            View childAt = decorView.getChildAt(0);
            if(childAt instanceof DecorCaptionView){
                return (DecorCaptionView)childAt;
            }
        }
        return null;
    }

    @Override
    public boolean isWindowMaximized() {
        DecorView decorView = (DecorView)activity.getWindow().getDecorView();
        return decorView.isWindowMaximized();
    }

    @Override
    public boolean startDecorMovingTask(float startX, float startY) {
        return false;
    }

    @Override
    public void finisDecorMovingTask() {

    }
}
