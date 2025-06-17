package com.fde;

import android.app.Activity;

public interface FrameworkOperations {

    void hideDecorCaptionView(Activity activity);

    void setDecorCaptionViewFocuseable(Activity activity, boolean focusable);

    void exitFullScreenWindow(Activity activity);
    void startFullScreenWindow(Activity activity);

    boolean isWindowMaximized();
}