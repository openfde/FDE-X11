// IActivityCallback.aidl
package com.fde.x11;

// Declare any non-default types here with import statements

interface IActivityCallback {

    boolean startDecorMovingTask(float startX, float startY, long window);

    void finisDecorMovingTask(long window);

}