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
import android.os.Build;
import com.fde.x11.utils.FLog;
import com.android.internal.policy.ITaskCaptionOperationService;
import java.lang.ref.WeakReference;
import java.util.List;
import android.util.Log;
import android.os.RemoteException;
import android.os.ServiceManager;

public class FrameworkImpl implements FrameworkOperations{
    private static final String TAG = "FrameworkImpl37";

    private final WeakReference<Activity> activity;
    private boolean mSystemBarsVisible = true;
    private ITaskCaptionOperationService mTaskCaptionService;
    public static int DECOR_CAPTION_HEIGHT = 48;

    public FrameworkImpl(WeakReference<Activity> activity,
                         boolean hideDecorCaptionView) {
        this.activity = activity;
        mTaskCaptionService = ITaskCaptionOperationService.Stub.asInterface(
                ServiceManager.getService("TASK_CAPTION_OPERATION"));
    }

    @Override
    public void exitMaxmizeWindow(Activity activity) {
        Log.d(TAG, "exitMaxmizeWindow() called with: activity = [" + activity + "]");
        try {
            mTaskCaptionService.executeTaskOperation(activity.getTaskId(), 2);
            Log.i(TAG, "Task operation executed successfully");
        } catch (RemoteException e) {
//            Log.e(TAG, "Failed to execute task operation, taskId: " + activity.getTaskId() + ", opCode: " + opCode, e);
        } catch (NullPointerException e) {
            Log.e(TAG, "Operation service is null, taskId: " + activity.getTaskId(), e);
        }
    }

    @Override
    public void hideDecorCaptionView(Activity activity) {
        Log.d(TAG, "hideDecorCaptionView() called with: activity = [" + activity + "]");
        if (Build.VERSION.SDK_INT >= 36) {
            activity.getWindow().getInsetsController().setSystemBarsAppearance(WindowInsetsController
                            .APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND,
                    WindowInsetsController
                            .APPEARANCE_TRANSPARENT_CAPTION_BAR_BACKGROUND);
        }
    }

    @Override
    public void setDecorCaptionViewFocuseable(Activity activity, boolean focusable) {
//        Log.a(TAG, "setDecorCaptionViewFocuseable() called with: activity = [" + activity + "], focusable = [" + focusable + "]");
    }

    @Override
    public void exitFullScreenWindow(Activity activity) {
        Log.d(TAG, "exitFullScreenWindow() called with: activity = [" + activity + "]");
        try {
            mTaskCaptionService.executeTaskOperation(activity.getTaskId(), 4);
            Log.e(TAG, "Task operation executed successfully");
        } catch (RemoteException e) {
//            Log.e(TAG, "Failed to execute task operation, taskId: " + activity.getTaskId() + ", opCode: " + opCode, e);
        } catch (NullPointerException e) {
            Log.e(TAG, "Operation service is null, taskId: " + activity.getTaskId(), e);
        }
    }

    @Override
    public void startFullScreenWindow(Activity activity) {
        Log.d(TAG, "startFullScreenWindow() called with: activity = [" + activity + "]");
        try {
            mTaskCaptionService.executeTaskOperation(activity.getTaskId(), 4);
            Log.i(TAG, "Task operation executed successfully");
        } catch (RemoteException e) {
//            Log.e(TAG, "Failed to execute task operation, taskId: " + activity.getTaskId() + ", opCode: " + opCode, e);
        } catch (NullPointerException e) {
            Log.e(TAG, "Operation service is null, taskId: " + activity.getTaskId(), e);
        }
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

    @Override
    public int getTaskState() {
        Log.d(TAG, "getTaskState()");
        try {
            return mTaskCaptionService.getTaskState(activity.get().getTaskId());
        } catch (RemoteException e) {
//            Log.e(TAG, "Failed to execute task operation, taskId: " + activity.getTaskId() + ", opCode: " + opCode, e);
        } catch (NullPointerException e) {
            Log.e(TAG, "Operation service is null, taskId: " + activity.get().getTaskId(), e);
        }
        return 0;
    }

    private int getWindowingMode(Activity activity) {
        return activity.getResources().getConfiguration()
                .windowConfiguration.getWindowingMode();
    }

    private void setWindowingMode(Activity activity, int windowingMode) {
        WindowContainerToken token = getTaskToken(activity);
        if (token == null) {
            Log.e(TAG, "setWindowingMode: task token is null");
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
            Log.e(TAG, "getTaskToken error: " + e.getMessage());
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
