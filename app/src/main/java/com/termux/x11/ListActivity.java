package com.termux.x11;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.app.ActivityOptions;
import android.widget.Button;

import androidx.annotation.Nullable;

import com.fde.fusionwindowmanager.NativeLib;

public class ListActivity extends Activity implements View.OnClickListener {

    private static final int DECORCATIONVIEW_HEIGHT = 42;
    private Button btCreateWindow, btStartScreen, btBindWindowManager;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.list_activity);
        btCreateWindow = findViewById(R.id.button_create);
        btStartScreen = findViewById(R.id.button_startscreen);
        btBindWindowManager = findViewById(R.id.button_manager);
        btCreateWindow.setOnClickListener(this);
        btStartScreen.setOnClickListener(this);
        btBindWindowManager.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        if(v == btCreateWindow){
            new Thread(new Runnable() {
                @Override
                public void run() {
                    String s = NativeLib.stringFromJNI();
                }
            }).start();
        } else if( v == btStartScreen){
            startActivityForXserver(0, 0 , 1920, 989, 0 , 0);
        } else if( v == btBindWindowManager){

        }
    }

    void startActivityForXserver(int offsetX, int offsetY, int width, int height, int index, int windPtr){
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchBounds(new Rect(offsetX, offsetY - DECORCATIONVIEW_HEIGHT, width + offsetX, height + offsetY));
        Intent intent = new Intent(this, MainActivity.class);
        intent.putExtra("index", index);
        intent.putExtra("KEY_WindowPtr", windPtr);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent, options.toBundle());
    }


}
