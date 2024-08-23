package com.termux.x11.utils;

import android.util.Log;

public class FLog {

    public static final int VERBOSE = 2;

    /**
     * Priority constant for the println method; use Log.d.
     */
    public static final int DEBUG = 3;

    /**
     * Priority constant for the println method; use Log.i.
     */
    public static final int INFO = 4;

    /**
     * Priority constant for the println method; use Log.w.
     */
    public static final int WARN = 5;

    /**
     * Priority constant for the println method; use Log.e.
     */
    public static final int ERROR = 6;

    /**
     * Priority constant for the println method.
     */
    public static final int ASSERT = 7;

    private static final boolean LogEnable = true;

    //for activity
    public static void a(String tagsuffix, long window, String content, int level){
        String hexString = Long.toHexString(window);
        String TAG = "activity" + "_" + tagsuffix + "_"+ hexString;
        normal(TAG, content, level);
    }

    public static void a(String tagsuffix, long window, String content){
        String hexString = Long.toHexString(window);
        String TAG = "activity" + "_" + tagsuffix + "_"+ hexString;
        normal(TAG, content, DEBUG);
    }

    public static void normal(String tag, String content, int level){
        if(!LogEnable && level < ERROR){
            return;
        }
        switch (level){
            case DEBUG:
                Log.d(tag, content);
                break;
            case WARN:
                Log.w(tag, content);
                break;
            case ERROR:
                Log.e(tag, content);
                break;
            default:
                break;
        }
    }



}
