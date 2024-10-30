package com.fde.x11.utils;

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

    private static final boolean LogEnable = false;
    private static final boolean LogAppListEnable = true;
    private static final boolean LogMainEnable = true;
    private static final boolean LogServerEnable = true;
    private static final boolean LogFileEnable = true;

    public static final boolean LogXserverNativeEnable = true;

    public static final boolean SHOW_DEBUG_TITLE = false;

    //for activity
    public static void a(String tagsuffix, long window, String content, int level){
        if(!LogMainEnable){
            return;
        }
        String hexString = Long.toHexString(window);
        String TAG = "activity" + "_" + tagsuffix + "_"+ hexString;
        normal(TAG, content, level);
    }

    public static void a(String tagsuffix, long window, String content){
        if(!LogMainEnable){
            return;
        }
        String hexString = Long.toHexString(window);
        String TAG = "activity" + "_" + tagsuffix + "_"+ hexString;
        normal(TAG, content, DEBUG);
    }

    //for xserver java
    public static void s(String tag, long window, String content , int level){
        if(!LogServerEnable){
            return;
        }
        String hexString = Long.toHexString(window);
        String TAG = tag + "_" + hexString;
        normal(TAG, content, level);
    }

    public static void s(String tag, String content , int level){
        if(!LogServerEnable){
            return;
        }
        normal(tag, content, level);
    }

    public static void s(String tag, String content){
        if(!LogServerEnable){
            return;
        }
        normal(tag, content, DEBUG);
    }

    public static void s(String tag, long window, String content){
        if(!LogServerEnable){
            return;
        }
        String hexString = Long.toHexString(window);
        String TAG = tag + "_" + hexString;
        normal(TAG, content, DEBUG);
    }

    //for app list
    public static void l(String tag, String content){
        if(LogAppListEnable){
            normal(tag, content, DEBUG);
        }
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


    public static void f(String tag, String content) {
        if(LogFileEnable){
            normal(tag, content, DEBUG);
        }
    }
}
