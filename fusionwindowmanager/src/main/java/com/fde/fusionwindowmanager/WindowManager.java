package com.fde.fusionwindowmanager;

public class FusionWindowManager {

    // Used to load the 'fusionwindowmanager' library on application startup.
    static {
        System.loadLibrary("fusionwindowmanager");
    }

    /**
     * A native method that is implemented by the 'fusionwindowmanager' native library,
     * which is packaged with this application.
     */
    public static native String stringFromJNI();
}