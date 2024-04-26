package com.termux.x11;

import android.os.Bundle;

public class MainActivity2 extends MainActivity {

    private String TAG = "Xevent_MainActivity2";

    @Override
    protected long getWindowId() {
        return 2;
    }

    protected int getLayoutID() {
        return R.layout.main_activity;
    }

    protected void goback(){
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }
}