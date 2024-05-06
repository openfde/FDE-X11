package com.termux.x11;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.app.ActivityOptions;
import android.widget.Button;
import androidx.annotation.Nullable;

import com.fde.fusionwindowmanager.fusionview.FusionActivity;
import com.fde.fusionwindowmanager.WindowManager;
import com.fde.fusionwindowmanager.service.WMService;
import com.fde.fusionwindowmanager.service.WMServiceConnection;
import com.termux.x11.utils.Util;

public class ControlActivity extends Activity implements View.OnClickListener {

    private static final int DECORCATIONVIEW_HEIGHT = 42;
    private Button btCreateWindow, btstartXserver,
            btStartScreen, btBindWindowManager,
            btCreateFromWM, btMoveNative, btResizeNative, btCloseWindowNative, btRaiseNative,
            btStopWindowManager;
    private WMServiceConnection connection;
    private WindowManager windowManager;
    public ICmdEntryInterface service;
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
        btCloseWindowNative = findViewById(R.id.button_closewindow);
        btRaiseNative = findViewById(R.id.button_raisewindow);
        btStopWindowManager = findViewById(R.id.button_stopwm);
        btstartXserver = findViewById(R.id.button_startServer);

        btCreateWindow.setOnClickListener(this);
        btStartScreen.setOnClickListener(this);
        btBindWindowManager.setOnClickListener(this);
        btCreateFromWM.setOnClickListener(this);
        btMoveNative.setOnClickListener(this);
        btResizeNative.setOnClickListener(this);
        btCloseWindowNative.setOnClickListener(this);
        btRaiseNative.setOnClickListener(this);
        btStopWindowManager.setOnClickListener(this);
        btstartXserver.setOnClickListener(this);

        Util.copyAssetsToFiles(this, "xkb", "xkb");

//        if(connection == null ){
//            connection = new WMServiceConnection();
//        }
        Intent intent = new Intent(this, XWindowService.class);
        bindService(intent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder s) {
                service = ICmdEntryInterface.Stub.asInterface(s);
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {

            }
        }, BIND_AUTO_CREATE);


//        windowManager = new WindowManager();



    }

    @Override
    public void onClick(View v) {
        if(v == btCreateWindow){
            windowManager.createXWindow();
        } else if( v == btStartScreen){
            startActivityForXserver(0, 0 , 1920, 989, 0 , 0);
//            startActivityForXserver(0, 0 , 960, 720, 0 , 0);
        } else if( v == btBindWindowManager){
            windowManager.startWindowManager();
        } else if ( v == btCreateFromWM){
            connection.startActivityForXserver(0, 0 , 300, 600, 0 , 0);
        } else if ( v == btMoveNative){
            int ret = windowManager.moveWindow(1000, 50, 50);
            Log.d("TAG", "moveWindow: ret = [" + ret + "]");
        } else if ( v == btResizeNative){
            try {
                service.resizeWindow(1000, 800, 600);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        } else if ( v == btCloseWindowNative){
            try {
                service.closeWindow(1000, 100, 100);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        } else if ( v == btRaiseNative){
            try {
                service.raiseWindow(2097153);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        } else if ( v == btStopWindowManager){
            windowManager.stopWindowManager();
        } else if ( v == btstartXserver){
            CmdEntryPoint.main(new String[]{":1", "-legacy-drawing", "-listen", "tcp"});
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
