package com.termux.x11;

import android.os.Bundle;

public class MainActivity1 extends MainActivity {


    @Override
    protected long getWindowId() {
        return 1;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }
}