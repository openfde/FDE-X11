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

    @Override
    public void hideDecorCaptionView(Activity activity) {
        Log.d(TAG, "hideDecorCaptionView() called with: activity = [" + activity + "]");
        if(Build.VERSION.SDK_INT == 30  ){
            activity.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
    }

    @Override
    public void setDecorCaptionViewFocuseable(Activity activity, boolean focusable) {
        Log.d(TAG, "setDecorCaptionViewFocuseable() called with: activity = [" + activity + "], focusable = [" + focusable + "]");
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
        Log.d(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
        DecorCaptionView captionView = getCaptionView(activity);
        if(captionView == null){
            return;
        }
        captionView.exitFullScreenWindow();
        captionView.toggleFreeformWindowingMode();
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
}
