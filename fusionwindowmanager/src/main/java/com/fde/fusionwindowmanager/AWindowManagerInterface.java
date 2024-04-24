package com.fde.fusionwindowmanager;


import android.app.Activity;
import android.app.Dialog;
import android.view.View;

/**
 * define action for start android window(activity/dialog/floatview)
 * start/destory
 */
public interface AWindowManagerInterface <ActivityType extends Activity, DialogType extends Dialog, ViewType extends View>{

    //start use evnent bus
    void startActivityForXMainWindow(WindowAttribute windowAttribute, Class<ActivityType> activityClass);

    void startActivityForXWindow(WindowAttribute windowAttribute, Class<ActivityType> activityClass);

    void startDialogForXWindow(WindowAttribute windowAttribute, Class<DialogType> activityClass);

    void startViewForXWindow(WindowAttribute windowAttribute, Class<ViewType> activityClass);


    //destroy use broadcast
    void destroyXActivity(Activity activity);

    void destroyXDialog(Dialog dialog);

    void destroyXView(View view);

}
