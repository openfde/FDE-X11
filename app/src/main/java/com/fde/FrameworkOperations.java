package com.fde;

import android.app.Activity;

public interface FrameworkOperations {

    void hideDecorCaptionView(Activity activity);

    void setDecorCaptionViewFocuseable(Activity activity, boolean focusable);

    void exitFullScreenWindow(Activity activity);
    void startFullScreenWindow(Activity activity);

    void exitMaxmizeWindow(Activity activity);

    boolean isWindowMaximized();

    boolean startDecorMovingTask(float startX, float startY);

    void finisDecorMovingTask();
}