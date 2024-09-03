package com.termux.x11;

import static com.fde.fusionwindowmanager.eventbus.EventType.X_DISMISS_WINDOW;
import static com.fde.fusionwindowmanager.eventbus.EventType.X_START_VIEW;
import static com.fde.fusionwindowmanager.eventbus.EventType.X_UNMAP_WINDOW;
import static com.termux.x11.data.Constants.DISPLAY_GLOBAL_PARAM;

import android.app.ActivityOptions;
import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.WindowManager;
import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;
import com.termux.x11.utils.FLog;
import com.termux.x11.utils.Util;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;

import java.io.File;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;


/**
 * native xserver run on this,
 * start activity/dialog like a x window,
 * close activity/dialog,
 * update icon,
 *
 * window manager confiure window and update clipboard
 */
public class XWindowService extends Service {

    private static final String TAG = "XWindowService";

    public static final String ACTION_X_WINDOW_ATTRIBUTE = "action_x_window_attribute";
    public static final String ACTION_X_WINDOW_PROPERTY = "action_x_window_property";

    public static final String CONFIGURE_ACTIVITY_FROM_X = "com.termux.x11.Xserver.action_configure";

    public static final String START_VIEW_FROM_X = "com.termux.x11.Xserver.start_action_view";
    public static final String STOP_VIEW_FROM_X = "com.termux.x11.Xserver.stop_action_view";

    public static final String DESTROY_ACTIVITY_FROM_X = "com.termux.x11.Xserver.action_destroy";
    public static final String STOP_WINDOW_FROM_X = "com.termux.x11.Xserver.action_stop";

    public static final String START_ACTIVITY_FROM_X = "com.termux.x11.Xserver.action_start";

    public static final String MODALED_ACTION_ACTIVITY_FROM_X = "com.termux.x11.Xserver.action_modaled";
    public static final String UNMODALED_ACTION_ACTIVITY_FROM_X = "com.termux.x11.Xserver.action_unmodaled";

    public static final String X_WINDOW_ATTRIBUTE = "x_window_attribute";
    public static final String X_WINDOW_PROPERTY = "x_window_property";
    private static final boolean DWM_START_DEFAULT = true;
    private WindowManager wm;
    private final HashSet<Long> startingWindow = new HashSet<>();
    private final HashSet<Long> stopingWindow = new HashSet<>();

    private final HashMap<Long, Property> propertyHashMap = new HashMap<>();

    private final ICmdEntryInterface.Stub service = new ICmdEntryInterface.Stub() {
        @Override
        public void windowChanged(Surface surface, float x, float y, float w, float h, int index, long pWin, long XID) throws RemoteException {
            FLog.s(TAG, XID, "windowChanged: surface:" + surface + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + ", index:" + index + ", pWin:" + pWin + ", XID:" + XID + "");
            startingWindow.remove(XID);
            Xserver.getInstance().windowChanged(surface, x, y, w, h, index, pWin, XID);
        }

        @Override
        public ParcelFileDescriptor getXConnection() throws RemoteException {
//            Log.d(TAG, "getXConnection: ");
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
            startingWindow.remove(window);
            stopingWindow.remove(window);
            if(wm != null && wm.closeWindow(window) > 0){
//                Log.d(TAG, "closeWindow: index:" + index + ", winPtr:" + winPtr + ", window:" + window + "");
            }
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
//                Log.d(TAG, "moveWindow: winPtr:" + winPtr + ", window:" + window + ", x:" + x + ", y:" + y + "");
            }
        }

        @Override
        public void resizeWindow(long window, int w, int h) throws RemoteException {
            if(wm != null && wm.resizeWindow(window, w, h) > 0){
//                Log.d(TAG, "resizeWindow: window:" + window + ", w:" + w + ", h:" + h + "");
            }
        }

        @Override
        public void raiseWindow(long window) throws RemoteException {
            if(wm != null && wm.raiseWindow(window) > 0){
//                Log.d(TAG, "raiseWindow: window:" + window + "");
                Xserver.getInstance().tellFocusWindow(window);
            }
        }

        @Override
        public void circulaSubWindows(long window, boolean lowest) throws RemoteException {
            if(wm != null && wm.circulaSubWindows(window, lowest) > 0){
//                Log.d(TAG, "circulaSubWindows: window:" + window + ", lowest:" + lowest + "");
            }
        }

        @Override
        public void sendClipText(String cliptext) throws RemoteException {
            if(wm != null && wm.sendClipText(cliptext) > 0){
//                Log.d(TAG, "sendClipText: cliptext:" + cliptext + "");
            }
        }
    };
    private Handler handler = new Handler();

    @Override
    public void onCreate() {
        super.onCreate();
        EventBus.getDefault().register(this);
        Xserver.getInstance().registerContext(new WeakReference<>(this));
        Xserver.getInstance().startXserver();
        if(DWM_START_DEFAULT){
            wm = new WindowManager( new WeakReference<>(this));
            wm.startWindowManager(DISPLAY_GLOBAL_PARAM);
        }
    }

    @Subscribe(threadMode = ThreadMode.MAIN,priority = 1)
    public void onReceiveMsg(EventMessage message){
        FLog.s(TAG,  message.getType().usefor);
        switch (message.getType()){
            case X_START_ACTIVITY_MAIN_WINDOW:
                startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity1.class, 42f);
                sendBroadcastFocusableIfNeed(message.getWindowAttribute(), false);
                break;
            case X_START_ACTIVITY_WINDOW:
                startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity11.class);
                sendBroadcastFocusableIfNeed(message.getWindowAttribute(), false);
                break;
            case X_DESTROY_ACTIVITY:
            case X_UNMAP_WINDOW:
                if(message.getProperty()!= null && message.getProperty().getSupportDeleteWindow() != 0){
                    destroyActivitySafety(2, message.getWindowAttribute());
                } else {
                    stopWindow(message.getWindowAttribute());
                }
                sendBroadcastFocusableIfNeed(message.getWindowAttribute(), true);
                break;
            case X_CONFIGURE_WINDOW:
                sendBroadcastConfigureWindow(message.getWindowAttribute());
                break;
            case X_START_VIEW:
                sendBroadcastAboutView(message.getWindowAttribute(), message.getProperty(), X_START_VIEW);
                break;
            case X_DISMISS_WINDOW:
                sendBroadcastAboutView(message.getWindowAttribute(),message.getProperty(), X_DISMISS_WINDOW);
                break;
            default:
                break;
        }
    }

    private void sendBroadcastAboutView(WindowAttribute attr, Property property, EventType type) {
        FLog.s(TAG, attr.getXID(), "sendBroadcastAboutView: attr:" + attr + ", type:" + type);
        String targetPackage = getPackageName();
        Intent intent = new Intent();
        if (Objects.requireNonNull(type) == X_START_VIEW) {
            intent.setAction(START_VIEW_FROM_X);
        } else if (Objects.requireNonNull(type) == X_DISMISS_WINDOW) {
            intent.setAction(STOP_VIEW_FROM_X);
        }
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        intent.putExtra(ACTION_X_WINDOW_PROPERTY, property);
        sendBroadcast(intent);
    }

    private void sendBroadcastConfigureWindow(WindowAttribute attr) {
        String targetPackage = getPackageName();
        Intent intent = new Intent(CONFIGURE_ACTIVITY_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        sendBroadcast(intent);
    }

    private void sendBroadcastFocusableIfNeed(WindowAttribute attr, boolean isFocusable) {
        if(!isFocusable){
            Property property = attr.getProperty();
            if(property == null || property.getTransientfor() == 0 ){
                return;
            }
            attr.setFocusable(false);
            String targetPackage = getPackageName();
            Intent intent = new Intent( MODALED_ACTION_ACTIVITY_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            intent.putExtra(ACTION_X_WINDOW_PROPERTY, attr.getProperty());
            propertyHashMap.put(attr.getXID(), attr.getProperty());
            sendBroadcast(intent);
        } else {
            Property property = propertyHashMap.get(attr.getXID());
            if(property == null || property.getTransientfor() == 0 ){
                return;
            }
            attr.setFocusable(true);
            String targetPackage = getPackageName();
            Intent intent = new Intent( UNMODALED_ACTION_ACTIVITY_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            intent.putExtra(ACTION_X_WINDOW_PROPERTY, attr.getProperty());
            propertyHashMap.remove(attr.getXID(), attr.getProperty());
            sendBroadcast(intent);
        }
    }

    private void stopWindow(WindowAttribute attr) {
        if(stopingWindow.contains(attr.getXID())){
            return;
        }
        stopingWindow.add(attr.getXID());
        FLog.s(TAG, "stopActivity: attr:" + attr + "");
        String targetPackage = getPackageName();
        Intent intent = new Intent(STOP_WINDOW_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        sendStickyBroadcast(intent);
    }

    private void destroyActivitySafety(int retry, WindowAttribute attr) {
        if(retry == 0){
            return;
        }
//        Log.d(TAG, "destroyActivitySafety: retry:" + retry + ", attr:" + attr + "");
        String targetPackage = getPackageName();
        Intent intent = new Intent(DESTROY_ACTIVITY_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        sendBroadcast(intent);
        handler.postDelayed(()->{
            destroyActivitySafety(retry - 1, attr);
        }, 1000);
    }

    public void startActLikeWindow(WindowAttribute attr, Class cls) {
        startActLikeWindowWithDecorHeight(attr, cls, 0);
    }

    public void startActLikeWindowWithDecorHeight(WindowAttribute attr, Class cls, float decorHeight) {
        stopingWindow.remove(attr.getXID());
        if (startingWindow.contains(attr.getXID())) {
            return;
        }
        startingWindow.add(attr.getXID());
        FLog.s(TAG, "start act with decor: attr:" + attr + ", cls:" + cls + ", decorHeight:" + decorHeight + "");
        if(attr.getTaskTo() != 0){
            String targetPackage = getPackageName();
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
            try {
                Method method = ActivityOptions.class.getMethod("setLaunchWindowingMode", int.class);
                method.invoke(options, 5); // change to freeform mode
            } catch (Exception e) {
                e.printStackTrace();
            }
            intent.putExtra(X_WINDOW_ATTRIBUTE, attr);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent, options.toBundle());
//            Log.d(TAG, "startActLikeWindowWithDecorHeight: attr:" + attr + ", cls:" + cls + ", decorHeight:" + decorHeight + "");
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return Service.START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return service;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        EventBus.getDefault().unregister(this);
        Util.deleteRecursive(new File("/tmp/fde"));
    }
}