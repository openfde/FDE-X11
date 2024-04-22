package com.fde.fusionwindowmanager.service;

import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Rect;
import android.os.IBinder;
import android.util.Log;

import com.fde.fusionwindowmanager.fusionview.FusionActivity;

public class WMServiceConnection implements ServiceConnection {

    private static final String TAG = "WMServiceConnection";
    private WMService mWMSerivce = null;

    private boolean isBound = false;
    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        isBound = true;
        mWMSerivce = ((WMService.MyBinder) service).getService();
        Log.d(TAG, "onServiceConnected() called with: name = [" + name + "], service = [" + service + "]");
//        startActivityForXserver(0, 0 , 300, 600, 0 , 0);
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        isBound = false;
    }

    public void startActivityForXserver(int offsetX, int offsetY, int width, int height, int index, int windPtr){
        if(mWMSerivce == null){
            return;
        }
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchBounds(new Rect(offsetX, offsetY - 42, width + offsetX, height + offsetY));
        Intent intent = new Intent(mWMSerivce, FusionActivity.class);
        intent.putExtra("index", index);
        intent.putExtra("KEY_WindowPtr", windPtr);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mWMSerivce.startActivity(intent, options.toBundle());
    }

}
