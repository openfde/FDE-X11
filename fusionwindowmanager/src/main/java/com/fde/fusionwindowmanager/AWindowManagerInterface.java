package com.fde.fusionwindowmanager;


import android.app.Activity;
import android.app.Dialog;
import android.view.View;

/**
 * define action for start android window(activity/dialog/floatview)
 * start/destory
 */
public interface AWindowManagerInterface {

    void startActivityForXAPP(Coordinate coordinate);

    void startActivityForXWindow(Coordinate coordinate);

    void destroyActivity(Activity activity);

    void startDialogForXWindow(Coordinate coordinate);

    void destroyDialog(Dialog dialog);

    void startViewForXWindow(Coordinate coordinate);

    void destroyView(View view);

}
