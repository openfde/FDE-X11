package com.fde.fusionwindowmanager.eventbus;

public enum EventType {

    //send to WindowManager
    X_START_ACTIVITY_MAIN_WINDOW("main window"),
    X_START_ACTIVITY_WINDOW("other window"),
    X_START_DIALOG("dialog window"),
    X_START_VIEW("tip window"),

    //May or not send to XSERVER
    X_DESTROY_ACTIVITY("destroy_activity"),
    X_DESTROY_DIALOG("destroy_dialog"),
    X_DESTROY_VIEW("destroy_view");

    private final String usefor;
    EventType(final String usefor){
        this.usefor = usefor;
    }
}
