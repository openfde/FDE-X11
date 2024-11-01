package com.fde.x11;


// This interface is used by utility on termux side.
interface ICmdEntryInterface {
    void windowChanged(in Surface surface, float offsetX, float offsetY, float width, float height, int index, long windPtr, long window);
    ParcelFileDescriptor getXConnection();

    int getConnectedFD();

    void closeWindow(int index, long p, long window);
//    ParcelFileDescriptor getLogcatOutput();

    void configureWindow(long winPtr, long window, int x, int y, int w, int h);

    void moveWindow(long winPtr, long window, int x, int y);

    void resizeWindow(long window, int w, int h);

    void raiseWindow(long window);

    void circulaSubWindows(long window, boolean lowest);

    void sendClipText(String cliptext);

    void sendMouseEvent(float x, float y, int whichButton, boolean buttonDown, boolean relative, int index);
}