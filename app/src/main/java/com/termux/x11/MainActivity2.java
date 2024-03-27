package com.termux.x11;

import android.os.Bundle;

public class MainActivity2 extends MainActivity {


    @Override
    protected long getWindowId() {
        return 2;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }
}