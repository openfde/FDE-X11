package com.termux.x11;

import static com.termux.x11.MainActivity.ACTION_STOP;
import static com.termux.x11.data.Constants.DISPLAY_GLOBAL_PARAM;

import android.app.ActivityOptions;
import android.app.Service;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.WindowManager;
import com.fde.fusionwindowmanager.eventbus.EventMessage;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashSet;

public class XWindowService extends Service {

    private static final String TAG = "XWindowService";

    public static final String ACTION_X_WINDOW_ATTRIBUTE = "action_x_window_attribute";

    public static final String DESTROY_ACTIVITY_FROM_X = "com.termux.x11.Xserver.ACTION_DESTROY";
    public static final String STOP_ANR_FROM_X = "com.termux.x11.Xserver.ACTION_STOP";

    public static final String START_ACTIVITY_FROM_X = "com.termux.x11.Xserver.ACTION_start";

    public static final String X_WINDOW_ATTRIBUTE = "x_window_attribute";
    public static final String X_WINDOW_PROPERTY = "x_window_property";
    private WindowManager wm;

    private Handler handler = new Handler();
    private HashSet<Long> pendingDiscardWindow = new HashSet<>();

    private final ICmdEntryInterface.Stub service = new ICmdEntryInterface.Stub() {
        @Override
        public void windowChanged(Surface surface, float x, float y, float w, float h, int index, long window, long id) throws RemoteException {
            Log.d(TAG, "windowChanged: surface:" + surface + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + ", index:" + index + ", window:" + window + ", id:" + id + "");
            if(!pendingDiscardWindow.contains(id)){
                Log.d(TAG, "windowChanged: pendingDiscardWindow" + id);
                Xserver.getInstance().windowChanged(surface, x, y, w, h, index, window, id);
            }
        }

        @Override
        public ParcelFileDescriptor getXConnection() throws RemoteException {
            Log.d(TAG, "getXConnection: ");
            return Xserver.getInstance().getXConnection();
        }

        @Override
        public void registerListener(int index, IReceive receiver) throws RemoteException {

        }

        @Override
        public void unregisterListener(int index, IReceive receiver) throws RemoteException {

        }

        @Override
        public void closeWindow(int index, long winPtr, long window) throws RemoteException {
            if(wm != null && wm.closeWindow(window) > 0){
                Log.d(TAG, "closeWindow: index:" + index + ", p:" + winPtr + ", window:" + window + "");
            }
            pendingDiscardWindow.remove(window);
            wm.WINDOW_XIDS.remove(window);
        }

        @Override
        public void configureWindow(long winPtr, long window, int x, int y, int w, int h) throws RemoteException {
            if(wm != null && wm.configureWindow(window, x, y, w, h) > 0){
                Log.d(TAG, "configureWindow: winPtr:" + winPtr + ", window:" + window + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + "");
            }
        }

        @Override
        public void moveWindow(long winPtr, long window, int x, int y) throws RemoteException {
            if(wm != null && wm.moveWindow(window, x, y) > 0){
                Log.d(TAG, "moveWindow: winPtr:" + winPtr + ", window:" + window + ", x:" + x + ", y:" + y + "");
            }
        }

        @Override
        public void resizeWindow(long window, int w, int h) throws RemoteException {
            if(wm != null && wm.resizeWindow(window, w, h) > 0){
                Log.d(TAG, "resizeWindow: window:" + window + ", w:" + w + ", h:" + h + "");
            }
        }

        @Override
        public void raiseWindow(long window) throws RemoteException {
            if(wm != null && wm.raiseWindow(window) > 0){
                Log.d(TAG, "raiseWindow: window:" + window + "");
                Xserver.getInstance().tellFocusWindow(window);
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        EventBus.getDefault().register(this);
        Xserver.getInstance().registerContext(new WeakReference<>(this));
        Xserver.getInstance().startXserver();
        Log.d(TAG, "onCreate: ");
        wm = new WindowManager( new WeakReference<>(this));
        wm.startWindowManager(DISPLAY_GLOBAL_PARAM);
    }

    @Subscribe(threadMode = ThreadMode.MAIN,priority = 1)
    public void onReceiveMsg(EventMessage message){
        Log.e(TAG, message.toString());
        switch (message.getType()){
            case X_START_ACTIVITY_MAIN_WINDOW:
                if(message.getWindowAttribute().getIndex() == 1){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity1.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 2){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity2.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 3){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity3.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 4){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity4.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 5){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity5.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 6){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity6.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 7){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity7.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 8){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity8.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 9){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity9.class, 42f);
                } else if (message.getWindowAttribute().getIndex() == 10){
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity0.class, 42f);
                }
                break;
            case X_START_ACTIVITY_WINDOW:
                if(message.getWindowAttribute().getIndex() == 11){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity11.class);
                } else if (message.getWindowAttribute().getIndex() == 12){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity12.class);
                } else if (message.getWindowAttribute().getIndex() == 13){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity13.class);
                } else if (message.getWindowAttribute().getIndex() == 14){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity14.class);
                } else if (message.getWindowAttribute().getIndex() == 15){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity15.class);
                } else if (message.getWindowAttribute().getIndex() == 16){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity16.class);
                } else if (message.getWindowAttribute().getIndex() == 17){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity17.class);
                } else if (message.getWindowAttribute().getIndex() == 18){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity18.class);
                } else if (message.getWindowAttribute().getIndex() == 19){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity19.class);
                }  else if (message.getWindowAttribute().getIndex() == 20){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity10.class);
                }
                break;
            case X_DESTROY_ACTIVITY:
                Log.d(TAG, "pendingDiscardWindow add window:" + message.getWindowAttribute().getXID());
                pendingDiscardWindow.add(message.getWindowAttribute().getXID());
                destroyActivitySafety(5, message.getWindowAttribute());
                break;
            case X_UNMAP_WINDOW:
                Log.d(TAG, "pendingDiscardWindow add window:" + message.getWindowAttribute().getXID());
                pendingDiscardWindow.add(message.getWindowAttribute().getXID());
                stopActivity(message.getWindowAttribute());
                break;
            default:
                break;
        }
    }

    private void stopActivity(WindowAttribute attr) {
        String targetPackage = "com.termux.x11";
        Intent intent = new Intent(STOP_ANR_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        sendBroadcast(intent);
        Log.d(TAG, "stopActivity: attr:" + attr + "");
    }

    private void destroyActivitySafety(int retry, WindowAttribute attr) {
        if(retry == 0){
            return;
        }
        String targetPackage = "com.termux.x11";
        Intent intent = new Intent(DESTROY_ACTIVITY_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        sendBroadcast(intent);
        handler.postDelayed(()->{
            destroyActivitySafety(retry - 1, attr);
        }, 2000);
    }

    public void startActLikeWindow(WindowAttribute attr, Class cls) {
        startActLikeWindowWithDecorHeight(attr, cls, 0);
    }

    public void startActLikeWindowWithDecorHeight(WindowAttribute attr, Class cls, float decorHeight) {
        if (wm.WINDOW_XIDS.contains(attr.getXID())) {
            return;
        }
        Log.d(TAG, "startActLikeWindowWithDecorHeight: attr:" + attr + ", cls:" + cls + ", decorHeight:" + decorHeight + "");
        wm.WINDOW_XIDS.add(attr.getXID());
        if(attr.getTaskTo() != 0){
            String targetPackage = "com.termux.x11";
            Intent intent = new Intent(START_ACTIVITY_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            sendBroadcast(intent);
        } else {
            ActivityOptions options = ActivityOptions.makeBasic();
            options.setLaunchBounds(new Rect((int)attr.getOffsetX(),
                    (int)(attr.getOffsetY()),
                    (int)(attr.getWidth() + attr.getOffsetX()),
                    (int)(attr.getHeight() + decorHeight + attr.getOffsetY())));
            Intent intent = new Intent(this, cls);
            if(attr.getProperty() != null){
                intent.putExtra(X_WINDOW_PROPERTY, attr.getProperty());
            }
            intent.putExtra(X_WINDOW_ATTRIBUTE, attr);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent, options.toBundle());
            Log.d(TAG, "startActLikeWindowWithDecorHeight: attr:" + attr + ", cls:" + cls + ", decorHeight:" + decorHeight + "");
        }
    }




    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStartCommand: ");
        return Service.START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind: ");
        return service;
    }
}