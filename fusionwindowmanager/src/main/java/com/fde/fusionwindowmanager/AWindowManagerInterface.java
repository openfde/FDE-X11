package com.fde.fusionwindowmanager;


/**
 * define action for android window(activity/dialog/floatview)
 */
public interface AndroidWindowInterface {

    void startActivityForXAPP(Coordinate coordinate);

    void startActivityForXWindow(Coordinate coordinate);

    void startDialogForXWindow(Coordinate coordinate);

    void startViewForXWindow(Coordinate coordinate);

}
