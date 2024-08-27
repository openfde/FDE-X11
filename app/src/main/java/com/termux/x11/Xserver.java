package com.termux.x11;

import static android.system.Os.getuid;
import static android.system.Os.getenv;

import static com.termux.x11.data.Constants.DISPLAY_GLOBAL_PARAM;

import android.annotation.SuppressLint;
import android.app.IActivityManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.IIntentReceiver;
import android.content.IIntentSender;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Log;
import android.view.Surface;

import androidx.annotation.Keep;

import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.WindowManager;
import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;
import com.termux.x11.utils.FLog;
import com.termux.x11.utils.Util;

import org.greenrobot.eventbus.EventBus;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.net.ConnectException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.URL;
import java.util.Arrays;

@Keep @SuppressLint({"StaticFieldLeak", "UnsafeDynamicallyLoadedCode"})
public class Xserver {
    public static final String ACTION_START = "com.termux.x11.Xserver.action_start";
    private final Handler handler = new Handler();
    public static final int PORT = 7892;
    public static final byte[] MAGIC = "0xDEADBEEF".getBytes();
    private static final String TAG = "Xserver";
    public static final String ACTION_UPDATE_ICON = "update_icon";
    // listen TCP, for remote and local X client
    // private static final String[] ARGS_DEFAULT = { DISPLAY_GLOBAL_PARAM, "-listen", "tcp","-ac"};

    // listen unix or tcp socket, only for local X client
    private static final String[] ARGS_DEFAULT = { DISPLAY_GLOBAL_PARAM};

    private  static final int _NET_WM_WINDOW_TYPE = 267;
    private  static final int _NET_WM_WINDOW_TYPE_COMBO = 268;
    private  static final int _NET_WM_WINDOW_TYPE_DIALOG = 269;
    private  static final int _NET_WM_WINDOW_TYPE_DND = 270;
    private  static final int _NET_WM_WINDOW_TYPE_DROPDOWN_MENU = 271;
    private  static final int _NET_WM_WINDOW_TYPE_MENU = 272;
    private  static final int _NET_WM_WINDOW_TYPE_NORMAL = 273;
    private  static final int _NET_WM_WINDOW_TYPE_POPUP_MENU = 274;
    private  static final int _NET_WM_WINDOW_TYPE_TOOLTIP = 275;
    private  static final int _NET_WM_WINDOW_TYPE_UTILITY = 276;

    private static final int ACTION_UNMAP = 1;
    private static final int ACTION_DESTORY = 2;
    private static final int ACTION_DISMISS_VIEW = 3;


    private static WeakReference<Service> context;

    public void startXserver() {
        if (!start(ARGS_DEFAULT)) {
            FLog.s(TAG, "startXserver: failed", FLog.ERROR);
        }
        spawnListeningThread();
        sendBroadcastDelayed();
    }

    public void registerContext(WeakReference<Service> context) {
        this.context = context;
    }

    public native void tellFocusWindow(long window);

    private static class SingletonHolder {
        private static final Xserver INSTANCE = new Xserver();
    }

    public static Xserver getInstance() {
        return SingletonHolder.INSTANCE;
    }

    /**
     * Start or update activity:  call from jni
     * @param aid                   Property window XID
     * @param transientfor          ICCCM transient for window XID
     * @param leader                leader window XID
     * @param type                  window type
     * @param net_name              window name
     * @param wm_class              window name -1
     * @param x                     offset x
     * @param y                     offset y
     * @param w                     width
     * @param h                     height
     * @param index                 index for ecah window by myself
     * @param p                     window pointer
     * @param window                window XID
     * @param taskTo                in which activity task
     * @param support_wm_delete     close action
     */
    public static void startOrUpdateActivity(long aid, long transientfor, long leader,
                                             int type, String net_name, String wm_class,
                                             int x, int y, int w, int h, int index, long p,
                                             long window, long taskTo, int support_wm_delete, Bitmap bitmap, boolean inbound) {
        FLog.s(TAG, aid,"start Activity: aid:" + Long.toHexString(aid) + ", transientfor:" + Long.toHexString(transientfor) + ", leader:" + Long.toHexString(leader)
                + ", type:" + type + ", net_name:" + net_name + ", wm_class:" + wm_class + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + ", index:" + index + ", p:" + p
                + ", window:" + Long.toHexString(window) + ", taskTo:" + Long.toHexString(taskTo) +
                ", support_wm_delete:" + support_wm_delete + ", bitmap:" + bitmap +  " inbound:" + inbound, FLog.WARN);
        EventMessage message = null;
        if(bitmap != null){
            bitmap = Util.scaleBitmapIfneed(bitmap);
            FLog.s(TAG, "scaled bitmap: " + bitmap.getWidth() + " X " + bitmap.getHeight());
        }
        if( type == _NET_WM_WINDOW_TYPE_NORMAL || type ==  _NET_WM_WINDOW_TYPE_DIALOG ){
            type = convert2AndroidType(type, x, y, w, h);
        }

        if( type == _NET_WM_WINDOW_TYPE_UTILITY || type ==  _NET_WM_WINDOW_TYPE_MENU ||
                type ==  _NET_WM_WINDOW_TYPE_POPUP_MENU){
            transientfor = transientfor == 0 ? taskTo:transientfor;
        }
        switch (type) {
            case _NET_WM_WINDOW_TYPE_NORMAL:
                message = new EventMessage(EventType.X_START_ACTIVITY_MAIN_WINDOW,
                        "xserver start activity as main window", new WindowAttribute(x, y, w, h, index, p, window, taskTo, new Property(aid, transientfor, leader, type, net_name, wm_class, support_wm_delete, bitmap)));
                break;
            case _NET_WM_WINDOW_TYPE_DIALOG:
                message = new EventMessage(EventType.X_START_ACTIVITY_WINDOW,
                        "xserver open activity as dialog", new WindowAttribute(x, y, w, h, index, p, window, taskTo, new Property(aid, transientfor, leader, type, net_name, wm_class, support_wm_delete)));
                break;
            case _NET_WM_WINDOW_TYPE_UTILITY:
            case _NET_WM_WINDOW_TYPE_MENU:
            case _NET_WM_WINDOW_TYPE_TOOLTIP:
            case _NET_WM_WINDOW_TYPE_POPUP_MENU:
                message = new EventMessage(EventType.X_START_VIEW,
                        "xserver show floatview as window", new WindowAttribute(x, y, w, h, index, p, window, taskTo), new Property(aid, transientfor, leader, type, net_name, wm_class, support_wm_delete));
                break;
        }
        if (message != null) {
            EventBus.getDefault().post(message);
        }
    }

    private static int convert2AndroidType(int type, int x, int y, int w, int h) {
        //case 1:
        if(type == _NET_WM_WINDOW_TYPE_NORMAL){
            if(w < 400 || h < 250){
                FLog.s(TAG, "change to type:dialog" +  ", w:" + w + ", h:" + h + "");
                return _NET_WM_WINDOW_TYPE_DIALOG;
            } else {
                FLog.s(TAG, "change to type:normal" +  ", w:" + w + ", h:" + h + "");
                return _NET_WM_WINDOW_TYPE_NORMAL;
            }
        }
        return type;
    }


    /**
     * close or destory a activity: call from jni
     * @param index                 window index
     * @param pWin                  window pointer
     * @param window                window XID
     * @param action                action to window
     * @param support_wm_delete     close action
     */
    public static void closeOrDestroyWindow(int index, long pWin, long taskTo,long window, int action, int support_wm_delete) {
        FLog.s(TAG, window,"close window: index:" + index + ", pWin:" + pWin +
                ", taskTo:" + Long.toHexString(taskTo) + ", window:" + Long.toHexString(window) +
                ", action:" + action + ", support_wm_delete:" + support_wm_delete + "", FLog.WARN);
        Property property = new Property();
        property.setSupportDeleteWindow(support_wm_delete);
        property.setTransientfor(taskTo);
        switch (action){
            case ACTION_DESTORY:
                EventBus.getDefault().post(new EventMessage(EventType.X_DESTROY_ACTIVITY,
                        "xserver finish activity", new WindowAttribute(index, pWin, window), property));
                break;
            case ACTION_UNMAP:
                EventBus.getDefault().post(new EventMessage(EventType.X_UNMAP_WINDOW,
                        "xserver hide any window", new WindowAttribute(index, pWin, window), property));
                break;
            case ACTION_DISMISS_VIEW:
                EventBus.getDefault().post(new EventMessage(EventType.X_DISMISS_WINDOW,
                        "xserver dismiss any window", new WindowAttribute(index, pWin, window), property));

                break;
            default:
                break;
        }
    }


    /**
     * call from jni
     * @param text
     */
    public static void updateXserverCliptext(String text){
//        Log.d(TAG, "updateXserverCliptext: text:" + text + "");
        if(context.get() != null && !TextUtils.isEmpty(text)){
            ClipData mClipData = ClipData.newPlainText("x11", text);
            android.content.ClipboardManager mClipboardManager = (ClipboardManager) context.get().getSystemService(Context.CLIPBOARD_SERVICE);
            mClipboardManager.setPrimaryClip(mClipData);
        }
    }


    /**
     * update icon: call from jni
     * @param bitmap
     * @param window
     */
    public static void  getWindowIconFromManager(Bitmap bitmap, long window){
        Context ctx = context.get();
        if(ctx == null){
            Log.d(TAG, "context  == null ");
            return;
        }
        FLog.s(TAG, window, "getWindowIconFromManager: bitmap:" + bitmap + ", window:" + window + "");
        Bitmap newBitmap = Util.scaleBitmapIfneed(bitmap);
        String targetPackage = context.get().getPackageName();
        Intent intent = new Intent(ACTION_UPDATE_ICON);
        intent.setPackage(targetPackage);
        intent.putExtra("window_id", window);
        intent.putExtra("window_icon", newBitmap);
        ctx.sendStickyBroadcast(intent);
    }

    private void sendBroadcastDelayed() {
        sendBroadcast();
//        handler.postDelayed(this::sendBroadcastDelayed, 1000);
    }

    void spawnListeningThread() {
        new Thread(() -> {
//            Log.e("Xserver", "Listening port " + PORT);
            try (ServerSocket listeningSocket =
                         new ServerSocket(PORT, 0, InetAddress.getByName("127.0.0.1"))) {
                listeningSocket.setReuseAddress(true);
                while(true) {
                    try (Socket client = listeningSocket.accept()) {
                        Log.e("Xserver", "Somebody connected!");
                        byte[] b = new byte[MAGIC.length];
                        DataInputStream reader = new DataInputStream(client.getInputStream());
                        reader.readFully(b);
                        if (Arrays.equals(MAGIC, b)) {
                            Log.e("Xserver", "New client connection!");
                            sendBroadcast();
                        }
                    } catch (Exception e) {
                        e.printStackTrace(System.err);
                    }
                }
            } catch (Exception e) {
                e.printStackTrace(System.err);
            }
        }).start();
    }

    private void sendBroadcast() {
        String  targetPackage = context.get().getPackageName();
        Bundle bundle = new Bundle();
        Intent intent = new Intent(ACTION_START);
        intent.putExtra("", bundle);
        intent.setPackage(targetPackage);
        if(context.get() != null){
            context.get().sendBroadcast(intent);
        }
    }

    public static void requestConnection() {
        System.err.println("Requesting connection...");
        new Thread(() -> { // New thread is needed to avoid android.os.NetworkOnMainThreadException
            try (Socket socket = new Socket("127.0.0.1", Xserver.PORT)) {
                socket.getOutputStream().write(Xserver.MAGIC);
            } catch (ConnectException e) {
                if (e.getMessage() != null && e.getMessage().contains("Connection refused")) {
                    Log.e("Xserver", "ECONNREFUSED: Connection has been refused by the server");
                } else
                    Log.e("Xserver", "Something went wrong when we requested connection", e);
            } catch (Exception e) {
                Log.e("Xserver", "Something went wrong when we requested connection", e);
            }
        }).start();
    }

    public static native boolean start(String[] args);

    public native void windowChanged(Surface surface, float offsetX, float offsetY,
                                     float width, float height, int index, long windPtr, long window);

    public native ParcelFileDescriptor getXConnection();


    public native ParcelFileDescriptor getLogcatOutput();
    public static native boolean connected();

    static {
        String path = "lib/" + Build.SUPPORTED_ABIS[0] + "/libXlorie.so";
        ClassLoader loader = Xserver.class.getClassLoader();
        URL res = loader != null ? loader.getResource(path) : null;
        String libPath = res != null ? res.getFile().replace("file:", "") : null;
        if (libPath != null) {
            try {
                System.load(libPath);
            } catch (Exception e) {
                Log.e("Xserver", "Failed to dlopen " + libPath, e);
                System.err.println("Failed to load native library. Did you install the right apk? Try the universal one.");
                System.exit(134);
            }
        } else {
            Log.e(TAG, "static initializer: libPath == null");
        }
    }
}
