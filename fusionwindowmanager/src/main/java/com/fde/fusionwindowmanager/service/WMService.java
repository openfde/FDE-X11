package com.fde.fusionwindowmanager.service;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

public class WMService extends Service {


    private static final String TAG = "WMService";

    private IBinder service = new MyBinder();

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "WMService -> onStartCommand, startId: " + startId + ", Thread: " + Thread.currentThread().getName());
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "WMService -> onBind, Thread: " + Thread.currentThread().getName());
        return service;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "WMService -> onUnbind, from:" + intent.getStringExtra("from"));
        return false;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "WMService -> onDestroy, Thread: " + Thread.currentThread().getName());
        super.onDestroy();
    }

    public class MyBinder extends Binder {

        public WMService getService(){
            return WMService.this;
        }
    }



}
