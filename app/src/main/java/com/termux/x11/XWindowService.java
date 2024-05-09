package com.termux.x11;

import static com.termux.x11.MainActivity.DECORCATIONVIEW_HEIGHT;

import android.app.ActivityOptions;
import android.app.Service;
import android.content.Intent;
import android.graphics.Rect;
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

public class XWindowService extends Service {

    private static final String TAG = "XWindowService";
    private WindowManager wm;

    private final ICmdEntryInterface.Stub service = new ICmdEntryInterface.Stub() {
        @Override
        public void windowChanged(Surface surface, float x, float y, float w, float h, int index, long window, long id) throws RemoteException {
            Log.d(TAG, "windowChanged: surface:" + surface + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + ", index:" + index + ", window:" + window + ", id:" + id + "");
            Xserver.getInstance().windowChanged(surface, x, y, w, h, index, window, id);
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
        wm.startWindowManager();
    }

    @Subscribe(threadMode = ThreadMode.MAIN,priority = 1)
    public void onReceiveMsg(EventMessage message){
        Log.e("EventBus_Subscriber", "onReceiveMsg_MAIN: " + message.toString());
        switch (message.getType()){
            case X_START_ACTIVITY_MAIN_WINDOW:
                if(message.getWindowAttribute().getIndex() == 1){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity1.class);
                } else if (message.getWindowAttribute().getIndex() == 2){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity2.class);
                } else if (message.getWindowAttribute().getIndex() == 3){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity3.class);
                } else if (message.getWindowAttribute().getIndex() == 4){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity4.class);
                } else if (message.getWindowAttribute().getIndex() == 5){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity5.class);
                } else if (message.getWindowAttribute().getIndex() == 6){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity6.class);
                } else if (message.getWindowAttribute().getIndex() == 7){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity7.class);
                } else if (message.getWindowAttribute().getIndex() == 8){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity8.class);
                } else if (message.getWindowAttribute().getIndex() == 9){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity9.class);
                } else if (message.getWindowAttribute().getIndex() == 10){
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity0.class);
                }
                break;
            default:
                break;
        }
    }

    public void startActLikeWindow(WindowAttribute attr, Class cls) {
        Log.d(TAG, "startActLikeWindow: attr:" + attr + ", cls:" + cls + "");
        if (!wm.WINDOW_XIDS.contains(attr.getWindowPtr())) {
            wm.WINDOW_XIDS.add(attr.getWindowPtr());
            ActivityOptions options = ActivityOptions.makeBasic();
            options.setLaunchBounds(new Rect((int)attr.getOffsetX(),
                    (int)(attr.getOffsetY()),
                    (int)(attr.getWidth() + attr.getOffsetX()),
                    (int)(attr.getHeight() + DECORCATIONVIEW_HEIGHT + attr.getOffsetY())));
            Intent intent = new Intent(this, cls);
            intent.putExtra("linux_window_attribute", attr);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent, options.toBundle());
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