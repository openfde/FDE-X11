package com.termux.x11;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;
import com.termux.x11.utils.AppUtils;
import com.xwdz.http.QuietOkHttp;
import com.xwdz.http.log.HttpLog;
import com.xwdz.http.log.HttpLoggingInterceptor;

import org.greenrobot.eventbus.EventBus;

import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;


public class App extends Application {
    private static final String TAG = "lifecycle";
    private static App instance;
    public HashSet<Long> aliveActivityWindow = new HashSet<>();
    public HashSet<Long> stopingActivityWindow = new HashSet<>();
    public HashMap<Long, WindowAttribute> windowAttrMap = new HashMap<>();
    public HashMap<Long, Property> windowPropertyMap = new HashMap<>();

    public static class InstanceHolder {
        public static final App INSTANCE = new App();
    }

    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        AppUtils.init(this);
        HttpLoggingInterceptor logInterceptor = new HttpLoggingInterceptor(new HttpLog("fde"));
        logInterceptor.setLevel(HttpLoggingInterceptor.Level.BASIC);
        OkHttpClient sOkHttpClient = new OkHttpClient.Builder()
                .readTimeout(10, TimeUnit.SECONDS)
                .addInterceptor(logInterceptor)
                .writeTimeout(10, TimeUnit.SECONDS).build();
        QuietOkHttp.setOkHttpClient(sOkHttpClient);
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle savedInstanceState) {
                if(activity instanceof MainActivity){
//                    Log.d(TAG, "onActivityCreated: activity:" + activity + ", savedInstanceState:" + savedInstanceState + "");
                   WindowAttribute attribute = ((MainActivity) activity).mAttribute;
                   if(attribute != null){
                       App.getApp().aliveActivityWindow.add(attribute.getXID());
                   }
                }
            }

            @Override
            public void onActivityStarted(@NonNull Activity activity) {

            }

            @Override
            public void onActivityResumed(@NonNull Activity activity) {

            }

            @Override
            public void onActivityPaused(@NonNull Activity activity) {

            }

            @Override
            public void onActivityStopped(@NonNull Activity activity) {
                if(activity instanceof MainActivity){
                    WindowAttribute attribute = ((MainActivity) activity).mAttribute;
                    if(attribute != null){
                        App.getApp().aliveActivityWindow.remove(attribute.getXID());
                        App.getApp().windowAttrMap.remove(attribute.getXID());
                        App.getApp().windowPropertyMap.remove(attribute.getXID());
                    }
                    Property property = ((MainActivity) activity).mProperty;
//                    Log.d(TAG, "onActivityStopped: XID:" + attribute.getXID() +
//                            " getTransientfor:" + property.getTransientfor());
                    if(property != null && property.getTransientfor() != 0){
                        EventBus.getDefault().post(new EventMessage(EventType.X_UNMODAL_ACTIVITY,
                                "xserver unmodal activity", null, property));
                    }
                }
            }

            @Override
            public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle outState) {

            }

            @Override
            public void onActivityDestroyed(@NonNull Activity activity) {
                if(activity instanceof MainActivity){
                    WindowAttribute attribute = ((MainActivity) activity).mAttribute;
                    if(attribute != null){
                        App.getApp().aliveActivityWindow.remove(attribute.getXID());
                        App.getApp().stopingActivityWindow.remove(attribute.getXID());
                    }
                }
            }
        });
    }

    public static App getApp(){
        if(instance == null){
            instance = InstanceHolder.INSTANCE;
        }
        return instance;
    }
}
