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

import com.fde.fusionwindowmanager.fusionview.FusionActivity;
import com.fde.fusionwindowmanager.WindowManager;
import com.fde.fusionwindowmanager.service.WMService;
import com.fde.fusionwindowmanager.service.WMServiceConnection;

public class ControlActivity extends Activity implements View.OnClickListener {

    private static final int DECORCATIONVIEW_HEIGHT = 42;
    private Button btCreateWindow, btStartScreen, btBindWindowManager,
            btCreateFromWM, btMoveNative, btResizeNative, btCloseWindow, btRaiseWindow;
    private WMServiceConnection connection;
    private WindowManager windowManager;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.list_activity);
        btCreateWindow = findViewById(R.id.button_create);
        btStartScreen = findViewById(R.id.button_startscreen);
        btBindWindowManager = findViewById(R.id.button_manager);
        btCreateFromWM = findViewById(R.id.btcreateinwm);
        btMoveNative = findViewById(R.id.button_movenative);
        btResizeNative = findViewById(R.id.button_resize);
        btCloseWindow = findViewById(R.id.button_closewindow);
        btRaiseWindow = findViewById(R.id.button_raisewindow);

        btCreateWindow.setOnClickListener(this);
        btStartScreen.setOnClickListener(this);
        btBindWindowManager.setOnClickListener(this);
        btCreateFromWM.setOnClickListener(this);
        btMoveNative.setOnClickListener(this);
        btResizeNative.setOnClickListener(this);
        btCloseWindow.setOnClickListener(this);
        btRaiseWindow.setOnClickListener(this);

        if(connection == null ){
            connection = new WMServiceConnection();
        }
        Intent intent = new Intent(this, WMService.class);
        bindService(intent, connection, BIND_AUTO_CREATE);
        windowManager = new WindowManager();
    }

    @Override
    public void onClick(View v) {
        if(v == btCreateWindow){
            new Thread(new Runnable() {
                @Override
                public void run() {
                    windowManager.createXWindow();
                }
            }).start();
        } else if( v == btStartScreen){
            startActivityForXserver(0, 0 , 1920, 989, 0 , 0);
//            startActivityForXserver(0, 0 , 960, 720, 0 , 0);
        } else if( v == btBindWindowManager){
            new Thread(new Runnable() {
                @Override
                public void run() {
                    int result = windowManager.connect2Server();
                    Log.d("TAG", "onClick() called with: result = [" + result + "]");
                }
            }).start();
        } else if ( v == btCreateFromWM){
            connection.startActivityForXserver(0, 0 , 300, 600, 0 , 0);
        } else if ( v == btMoveNative){
            int ret = windowManager.moveWindow(1000, 50, 50);
            Log.d("TAG", "moveWindow: ret = [" + ret + "]");
        } else if ( v == btResizeNative){
            int ret = windowManager.resizeWindow(1000, 800, 600);
            Log.d("TAG", "resizeWindow: ret = [" + ret + "]");
        } else if ( v == btCloseWindow){
            int ret = windowManager.closeWindow(1000);
            Log.d("TAG", "closeWindow: ret = [" + ret + "]");
        } else if ( v == btRaiseWindow){
            int ret = windowManager.raiseWindow(1000);
            Log.d("TAG", "raiseWindow: ret = [" + ret + "]");
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
