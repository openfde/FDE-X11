package com.fde.x11;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

//import com.fde.DynamicConfigImplDemo;
import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;
import com.fde.x11.utils.AppUtils;
import com.kwai.koom.base.DefaultInitTask;
import com.kwai.koom.base.MonitorManager;
import com.kwai.koom.nativeoom.leakmonitor.LeakMonitor;
import com.kwai.koom.nativeoom.leakmonitor.LeakMonitorConfig;
//import com.tencent.matrix.Matrix;
//import com.tencent.matrix.iocanary.IOCanaryPlugin;
//import com.tencent.matrix.iocanary.config.IOConfig;
import com.kwai.koom.nativeoom.leakmonitor.LeakRecord;
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
        initMatrix();
        initKoom();
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

    private void initKoom() {
        DefaultInitTask.INSTANCE.init(this);
        LeakMonitorConfig config = new LeakMonitorConfig.Builder()
                .setLoopInterval(50000) // 设置轮训的间隔，单位：毫秒
                .setMonitorThreshold(16) // 设置监听的最小内存值，单位：字节
                .setNativeHeapAllocatedThreshold(0) // 设置native heap分配的内存达到多少阈值开始监控，单位：字节
                .setSelectedSoList(new String[]{"libXlorie"}) // 不设置是监控所有， 设置是监听特定的so,  比如监控libcore.so 填写 libcore 不带.so
//                .setIgnoredSoList(new String[0]) // 设置需要忽略监控的so
                .setEnableLocalSymbolic(false) // 设置使能本地符号化，仅在 debuggable apk 下有用，release 请关闭
                .setLeakListener(leaks -> {
                    if (leaks.isEmpty()) {
                        return;
                    }
                    StringBuilder builder = new StringBuilder();
                    for (LeakRecord leak : leaks) {
                        builder.append(leak.toString());
                    }
                    Log.d(TAG, "initKoom builder:" + builder);
                    Toast.makeText(this, builder.toString(), Toast.LENGTH_SHORT).show();
                }) // 设置泄漏监听器
                .build();
        MonitorManager.addMonitorConfig(config);
        LeakMonitor.INSTANCE.start();
    }

    private void initMatrix() {
//        Matrix.Builder builder = new Matrix.Builder(this); // build matrix
//        builder.pluginListener(new TestPluginListener(this)); // add general pluginListener
//        DynamicConfigImplDemo dynamicConfig = new DynamicConfigImplDemo(); // dynamic config
//
//        // init plugin
//        IOCanaryPlugin ioCanaryPlugin = new IOCanaryPlugin(new IOConfig.Builder()
//                .dynamicConfig(dynamicConfig)
//                .build());
//        //add to matrix
//        builder.plugin(ioCanaryPlugin);
//
//        //init matrix
//        Matrix.init(builder.build());
//
//        // start plugin
//        ioCanaryPlugin.start();
    }

    public static App getApp(){
        if(instance == null){
            instance = InstanceHolder.INSTANCE;
        }
        return instance;
    }
}
