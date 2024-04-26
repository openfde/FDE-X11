// IReceive.aidl
package com.termux.x11;

// Declare any non-default types here with import statements

interface IReceive {
    void startWindow(int offsetX, int offsetY, int width, int height, int index, long windPtr, long window);
}