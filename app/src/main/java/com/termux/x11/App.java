package com.termux.x11;

import android.app.Application;
import android.content.Context;

import com.termux.x11.utils.AppUtils;
import com.xwdz.http.QuietOkHttp;
import com.xwdz.http.log.HttpLog;
import com.xwdz.http.log.HttpLoggingInterceptor;

import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;


public class App extends Application {
    private static App instance;

    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        instance = this;
        AppUtils.init(this);
        HttpLoggingInterceptor logInterceptor = new HttpLoggingInterceptor(new HttpLog("fde"));
        logInterceptor.setLevel(HttpLoggingInterceptor.Level.BODY);
        OkHttpClient sOkHttpClient = new OkHttpClient.Builder()
                .readTimeout(10, TimeUnit.SECONDS)
                .addInterceptor(logInterceptor)
                .writeTimeout(10, TimeUnit.SECONDS).build();
        QuietOkHttp.setOkHttpClient(sOkHttpClient);

    }

    public static App getApp(){
        return instance;
    }
}
