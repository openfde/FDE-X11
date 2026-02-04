package com.fde;

import android.app.Activity;
import android.openfde.AppTaskControllerProxy;
import android.openfde.AppTaskStatusListener;
import com.fde.x11.BuildConfig;

import java.lang.ref.WeakReference;


public class FrameworkFactory {

    public static FrameworkOperations create(WeakReference<Activity> activity,
                                             boolean hideDecorCaptionView,
                                             AppTaskStatusListener listener
                                             ){
        return new FrameworkImpl(activity, hideDecorCaptionView, listener);
    }
}
