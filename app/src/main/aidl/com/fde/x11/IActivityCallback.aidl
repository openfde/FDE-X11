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

    boolean configureActivity(long window);


}