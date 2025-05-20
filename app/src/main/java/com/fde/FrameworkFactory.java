package com.fde;

import com.fde.x11.BuildConfig;


public class FrameworkFactory {

    public static FrameworkOperations create(){
        return new FrameworkImpl();
    }
}
