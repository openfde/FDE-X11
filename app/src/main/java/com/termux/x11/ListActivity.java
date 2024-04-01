package com.termux.x11;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.View;
import android.app.ActivityOptions;

import androidx.annotation.Nullable;

public class ListActivity extends Activity {

    private static final int DECORCATIONVIEW_HEIGHT = 42;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.list_activity);

        findViewById(R.id.button2).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startActivityForXserver(0, 0 , 1920, 989, 0 , 0);
            }
        });
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
