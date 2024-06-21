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
    public static final String ACTION_START = "com.termux.x11.Xserver.ACTION_START";
    private final Handler handler = new Handler();
    public static final int PORT = 7892;
    public static final byte[] MAGIC = "0xDEADBEEF".getBytes();
    private static final String TAG = "Xserver";
    public static final String ACTION_UPDATE_ICON = "UPDATE_ICON";
    private static final String[] ARGS_DEFAULT = { DISPLAY_GLOBAL_PARAM, "-listen", "tcp", "-ac"};

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


    private static WeakReference<Service> contextRef;

    public void startXserver() {
        if (!start(ARGS_DEFAULT)) {
//            Log.e(TAG, "startXserver: failed");
        }
        spawnListeningThread();
        sendBroadcastDelayed();
    }

    public void registerContext(WeakReference<Service> context) {
        contextRef = context;
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
    public static void startOrUpdateActivity(long aid, long transientfor, long leader, int type, String net_name, String wm_class,
    int x, int y, int w, int h, int index, long p, long window, long taskTo, int support_wm_delete) {
        Log.d(TAG, "startOrUpdateActivity: aid:" + aid + ", transientfor:" + transientfor + ", leader:" + leader + ", type:" + type + ", net_name:" + net_name + ", wm_class:" + wm_class + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + ", index:" + index + ", p:" + p +
                ", window:" + window + ", taskTo:" + taskTo + ", support_wm_delete:" + support_wm_delete + "");
        EventMessage message = null;
        switch (type) {
            case _NET_WM_WINDOW_TYPE_NORMAL:
                message = new EventMessage(EventType.X_START_ACTIVITY_MAIN_WINDOW,
                        "xserver start activity as main window", new WindowAttribute(x, y, w, h, index, p, window, taskTo, new Property(aid, transientfor, leader, type, net_name, wm_class, support_wm_delete)));
                break;
            case _NET_WM_WINDOW_TYPE_DIALOG:
                message = new EventMessage(EventType.X_START_ACTIVITY_WINDOW,
                        "xserver open activity as dialog", new WindowAttribute(x, y, w, h, index, p, window, taskTo, new Property(aid, transientfor, leader, type, net_name, wm_class, support_wm_delete)));
                break;
            default:
                break;
        }
        if (message != null) {
            EventBus.getDefault().post(message);
        }
    }


    /**
     * close or destory a activity: call from jni
     * @param index                 window index
     * @param pWin                  window pointer
     * @param window                window XID
     * @param action                action to window
     * @param support_wm_delete     close action
     */
    public static void closeOrDestroyActivity(int index, long pWin, long window, int action, int support_wm_delete) {
        Log.d(TAG, "closeOrDestroyActivity: index:" + index + ", p:" + pWin + ", window:" + window + ", action:" + action + "");
        Property property = new Property();
        property.setSupportDeleteWindow(support_wm_delete);
        switch (action){
            case ACTION_DESTORY:
                EventBus.getDefault().post(new EventMessage(EventType.X_DESTROY_ACTIVITY,
                        "xserver finish activity", new WindowAttribute(index, pWin, window), property));
                break;
            case ACTION_UNMAP:
                EventBus.getDefault().post(new EventMessage(EventType.X_UNMAP_WINDOW,
                        "xserver hide any window", new WindowAttribute(index, pWin, window), property));
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
        if(contextRef.get() != null && !TextUtils.isEmpty(text)){
            ClipData mClipData = ClipData.newPlainText("x11", text);
            android.content.ClipboardManager mClipboardManager = (ClipboardManager) contextRef.get().getSystemService(Context.CLIPBOARD_SERVICE);
            mClipboardManager.setPrimaryClip(mClipData);
        }
    }


    /**
     * update icon: call from jni
     * @param bitmap
     * @param window
     */
    public static void  getWindowIconFromManager(Bitmap bitmap, long window){
//        Log.d(TAG, "getWindowIconFromManager: bitmap:" + bitmap + ", window:" + window + "");
        Context context = contextRef.get();
        if(context == null){
            Log.d(TAG, "context  == null ");
            return;
        }
        Bitmap newBitmap = Util.scaleBitmapIfneed(bitmap);
        String targetPackage = "com.termux.x11";
        Intent intent = new Intent(ACTION_UPDATE_ICON);
        intent.setPackage(targetPackage);
        intent.putExtra("window_id", window);
        intent.putExtra("window_icon", newBitmap);
        context.sendStickyBroadcast(intent);
    }

    private void sendBroadcastDelayed() {
        sendBroadcast();
        handler.postDelayed(this::sendBroadcastDelayed, 1000);
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
        String   targetPackage = "com.termux.x11";
        Bundle bundle = new Bundle();
        Intent intent = new Intent(ACTION_START);
        intent.putExtra("", bundle);
        intent.setPackage(targetPackage);
        if(contextRef.get() != null){
            contextRef.get().sendBroadcast(intent);
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
