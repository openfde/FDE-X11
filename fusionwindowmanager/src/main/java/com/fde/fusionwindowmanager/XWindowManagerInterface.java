package com.fde.fusionwindowmanager;

/**
 * define action for manager linux window
 * all create action from xserver, so X window follow action after android input
 * these fuction will send to fusionwindow manager then to xserver
 */
public interface XWindowManagerInterface {


    void moveWindow(long windowPtr, int x, int y);

    void resizeWindow(long windowPtr, int x, int y);

    void closeWindow(long windowPtr);

    /**
     * resver interface
     */
    void hideWindow(long windowPtr);

    /**
     * resver interface
     */
    void showWindow(long windowPtr);

    void raiseWindow(long windowPtr);

    void closeClient(int XID);

    /**
     * int
     * XConfigureWindow(
     *     register Display *dpy,
     *     Window w,
     *     unsigned int mask,
     *     XWindowChanges *changes)
     */
}
