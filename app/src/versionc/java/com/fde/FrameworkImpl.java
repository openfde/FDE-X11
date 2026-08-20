package com.fde;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityTaskManager;
import android.app.WindowConfiguration;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.window.WindowContainerToken;
import android.window.WindowContainerTransaction;
import android.window.WindowOrganizer;

import com.fde.x11.utils.FLog;

import java.lang.ref.WeakReference;
import java.util.List;

public class FrameworkImpl implements FrameworkOperations{
    private static final String TAG = "FrameworkImpl37";

    private final WeakReference<Activity> activity;
    private boolean mSystemBarsVisible = true;
    public static int DECOR_CAPTION_HEIGHT = 48;

    public FrameworkImpl(WeakReference<Activity> activity,
                         boolean hideDecorCaptionView) {
        this.activity = activity;
    }

    @Override
    public void exitMaxmizeWindow(Activity activity) {
        FLog.a(TAG, "exitMaxmizeWindow() called with: activity = [" + activity + "]");
        setWindowingMode(activity, WindowConfiguration.WINDOWING_MODE_FREEFORM);
    }

    @Override
    public void hideDecorCaptionView(Activity activity) {
        FLog.a(TAG, "hideDecorCaptionView() called with: activity = [" + activity + "]");
    }

    @Override
    public void setDecorCaptionViewFocuseable(Activity activity, boolean focusable) {
        FLog.a(TAG, "setDecorCaptionViewFocuseable() called with: activity = [" + activity + "], focusable = [" + focusable + "]");
    }

    @Override
    public void exitFullScreenWindow(Activity activity) {
        FLog.a(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
        int windowingMode = getWindowingMode(activity);
        if (windowingMode == WindowConfiguration.WINDOWING_MODE_FULLSCREEN && !mSystemBarsVisible) {
            setSystemBarsVisible(activity, true);
            setWindowingMode(activity, WindowConfiguration.WINDOWING_MODE_FREEFORM);
        } else if (windowingMode == WindowConfiguration.WINDOWING_MODE_FREEFORM) {
            setWindowingMode(activity, WindowConfiguration.WINDOWING_MODE_FULLSCREEN);
            setSystemBarsVisible(activity, false);
        } else {
            setSystemBarsVisible(activity, false);
        }
    }

    @Override
    public void startFullScreenWindow(Activity activity) {
        FLog.a(TAG, "startFullScreenWindow() called with: activity = [" + activity + "]");
        setWindowingMode(activity, WindowConfiguration.WINDOWING_MODE_FULLSCREEN);
    }

    @Override
    public boolean isWindowMaximized() {
        Activity a = activity.get();
        return a != null && getWindowingMode(a) == WindowConfiguration.WINDOWING_MODE_FULLSCREEN;
    }

    @Override
    public boolean startDecorMovingTask(float startX, float startY) {
        return false;
    }

    @Override
    public void finisDecorMovingTask() {
    }

    private int getWindowingMode(Activity activity) {
        return activity.getResources().getConfiguration()
                .windowConfiguration.getWindowingMode();
    }

    private void setWindowingMode(Activity activity, int windowingMode) {
        WindowContainerToken token = getTaskToken(activity);
        if (token == null) {
            FLog.e(TAG, "setWindowingMode: task token is null");
            return;
        }
        WindowContainerTransaction wct = new WindowContainerTransaction();
        wct.setWindowingMode(token, windowingMode);
        if (windowingMode == WindowConfiguration.WINDOWING_MODE_FULLSCREEN) {
            wct.setBounds(token, null);
        }
        new WindowOrganizer().applyTransaction(wct);
    }

    private WindowContainerToken getTaskToken(Activity activity) {
        try {
            ActivityTaskManager atm =
                    (ActivityTaskManager) activity.getSystemService("activity_task");
            int taskId = activity.getTaskId();
            List<ActivityManager.RunningTaskInfo> tasks = atm.getTasks(100);
            for (ActivityManager.RunningTaskInfo info : tasks) {
                if (info.taskId == taskId) {
                    return info.token;
                }
            }
        } catch (Exception e) {
            FLog.e(TAG, "getTaskToken error: " + e.getMessage());
        }
        return null;
    }

    private void setSystemBarsVisible(Activity activity, boolean visible) {
        WindowInsetsController controller = activity.getWindow().getInsetsController();
        if (controller == null) {
            return;
        }
        if (visible) {
            controller.show(WindowInsets.Type.systemBars());
        } else {
            controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            controller.hide(WindowInsets.Type.systemBars());
        }
        mSystemBarsVisible = visible;
    }
}
