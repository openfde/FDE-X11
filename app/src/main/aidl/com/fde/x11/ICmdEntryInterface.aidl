package com.fde.x11;

import com.fde.x11.IActivityCallback;

// This interface is used by utility on termux side.
interface ICmdEntryInterface {
    void windowChanged(in Surface surface, float offsetX, float offsetY, float width, float height, int index, long windPtr, long window);
    ParcelFileDescriptor getXConnection();

    int getConnectedFD();

    void closeWindow(int index, long p, long window);

    void unmapWindow(int index, long p, long window);

    void mapWindow(int index, long p, long window);

//    ParcelFileDescriptor getLogcatOutput();

    // 高频几何/焦点类调用改成 oneway：它们都在 Activity 主线程上调用，同步 Binder 会让
    // UI 线程等 xserver 进程返回（配置变更、拖动、全屏切换时尤其明显）。oneway 在同一
    // 线程内仍然保序，所以 "configureWindow 之后立刻 raiseWindow" 的顺序不受影响。
    // 生命周期类（close/unmap/map/windowChanged）保持同步，避免销毁顺序被打乱。
    oneway void configureWindow(long winPtr, long window, int x, int y, int w, int h);

    oneway void setWindowingMode(long frame, long window, int mode);

    oneway void moveWindow(long winPtr, long window, int x, int y);

    oneway void resizeWindow(long window, int w, int h);

    oneway void raiseWindow(long window);

    void circulaSubWindows(long window, boolean lowest);

    void sendClipText(String cliptext);

    void sendClipFile(String file);

    void sendMouseEvent(float x, float y, int whichButton, boolean buttonDown, boolean relative, int index);

    void registerActivityCallback(long window, IActivityCallback callback);

    void unregisterActivityCallback(long window, IActivityCallback callback);

    void updateSystemViewVisible(boolean visible);

//unused  interface
    void onWindowFocusChanged(long window, boolean hasFocus);

}