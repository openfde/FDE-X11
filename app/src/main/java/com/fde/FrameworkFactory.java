package com.fde;

import android.app.Activity;

import com.fde.x11.BuildConfig;


public class FrameworkFactory {

    public static FrameworkOperations create(Activity activity){
        return new FrameworkImpl(activity);
    }
}
