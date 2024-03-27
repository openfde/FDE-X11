package com.termux.x11.utils;

import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Log;

import com.termux.x11.MainActivity1;

import java.io.IOException;
import java.lang.reflect.Field;
import java.util.HashMap;

public class Util {
    private static final String TAG = "Util";
    private static Context baseContext;

    public static void setBaseContext(Context context){
        baseContext = context;
    }

    /**
     * Count the number of bits in an integer.
     *
     * @param n The integer containing the bits.
     * @return The number of bits in the integer.
     */
    public static int bitcount(int n) {
        int c = 0;

        while (n != 0) {
            c += n & 1;
            n >>= 1;
        }

        return c;
    }

    public static void startActivityForWindow(long windowPtr) {
        Log.d(TAG, "startActivityForWindow() called with: windowPtr = [" + windowPtr + "]");
        Intent intent = new Intent(baseContext, MainActivity1.class);
        intent.putExtra("KEY_WindowPtr", windowPtr);
        intent.setFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT | Intent.FLAG_ACTIVITY_NEW_TASK);
        baseContext.startActivity(intent);
    }


    public static Class<?> getClassByName(String name) {
        Class<?> xwindowActivityClass = null;
        try {
            xwindowActivityClass = Class.forName(name);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        return xwindowActivityClass;
    }
}