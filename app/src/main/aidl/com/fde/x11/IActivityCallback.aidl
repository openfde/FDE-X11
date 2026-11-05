// IActivityCallback.aidl
package com.fde.x11;

// Declare any non-default types here with import statements

interface IActivityCallback {

    boolean startDecorMovingTask(float startX, float startY, long window);

    void finisDecorMovingTask(long window);

    boolean finishActivity(long window);

    // 因窗口类型/装饰变化需要以新的宿主类型重启 Activity 时调用，
    // 与 finishActivity 的区别是：Activity 销毁时保留 X 窗口，不发送 close
    boolean finishActivityForRestart(long window);

    // X 侧窗口几何变化，直接回调给宿主 Activity。
    // 以前这条是包内广播（CONFIGURE_ACTIVITY_FROM_X）：要过 AMS，而且所有 MainActivity
    // 都要收一遍再按 XID 过滤。改成 oneway 定向回调后只有宿主收到，也不会阻塞
    // xserver 主线程；宿主还没注册回调（窗口刚创建/重启中）时 Service 会退回广播。
    oneway void configureActivity(long window, int x, int y, int width, int height);


}