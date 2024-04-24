package com.termux.x11;

import android.os.Bundle;

public class MainActivity1 extends MainActivity {

    private String TAG = "Xevent_MainActivity1";

    @Override
    protected long getWindowId() {
        return 0;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }
}