package com.fde.fusionwindowmanager;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import androidx.annotation.NonNull;

public class WindowManager {

    private static final String TAG = "WindowManager";

    // Used to load the 'fusionwindowmanager' library on application startup.
    static {
        System.loadLibrary("fusionwindowmanager");
    }

    HandlerThread mThread;
    Handler mHandler;

    public static final int MSG_START_WM = 1;
    public static final int MSG_STOP_WM = 2;

    public WindowManager(){
    }


    public void startWindowManager(){
        Log.d(TAG, "startWindowManager() called");
        Message msg = Message.obtain();
        msg.what = MSG_START_WM;
        mHandler.sendMessage(msg);
    }

    public void stopWindowManager(){
        disconnect2Server();
    }

    /**
     * these native methods that is implemented by the 'fusionwindowmanager' native library,
     * which is packaged with this application.
     */
    public native void createXWindow();

    public static native int connect2Server();

    public native int moveWindow(long winPtr, int x, int y);

    public native int resizeWindow(long winPtr, int width, int height);

    public native int closeWindow(long winPtr);

    public native int raiseWindow(long winPtr);

    public native int disconnect2Server();

    private static class TaskHandler extends Handler{

        public TaskHandler(@NonNull Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(@NonNull Message msg) {
            super.handleMessage(msg);
            Log.d(TAG, "handleMessage() called with: msg = [" + msg + "]");
            switch (msg.what){
                case MSG_START_WM:
                    connect2Server();
                    break;
                default:
                    break;
            }
        }
    }

    public void init() {
        mThread = new HandlerThread("WM");
        mThread.start();
        mHandler = new TaskHandler(mThread.getLooper());
    }
}