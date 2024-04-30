package com.termux.x11;

import static android.system.Os.getuid;
import static android.system.Os.getenv;

import android.annotation.SuppressLint;
import android.app.IActivityManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.IIntentReceiver;
import android.content.IIntentSender;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import androidx.annotation.Keep;

import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.WindowManager;
import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;

import org.greenrobot.eventbus.EventBus;

import java.io.DataInputStream;
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
    private static final String[] ARGS_DEFAULT = {":1", "-legacy-drawing", "-listen", "tcp"};

    private WeakReference<XWindowService> contextRef;

    public void startXserver() {
        if (!start(ARGS_DEFAULT)) {
            Log.e(TAG, "startXserver: failed");
        }
        spawnListeningThread();
        sendBroadcastDelayed();
    }

    public void registerContext(WeakReference<XWindowService> contextRef) {
        this.contextRef = contextRef;
    }

    private static class SingletonHolder {
        private static final Xserver INSTANCE = new Xserver();
    }

    public static Xserver getInstance() {
        Log.d(TAG, "getInstance: " + SingletonHolder.INSTANCE);
        return SingletonHolder.INSTANCE;
    }

    public static void startOrUpdateActivity(int x, int y, int w, int h, int index, long p, long window) {
        Log.d(TAG, "startOrUpdateActivity: x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + ", index:" + index + ", p:" + p + ", window:" + window + "");
        EventBus.getDefault().post(new EventMessage(EventType.X_START_ACTIVITY_MAIN_WINDOW,
                " ", new WindowAttribute(x, y, w, h, index, p, window)));
    }


    private void sendBroadcastDelayed() {
//        if (!connected()){
            sendBroadcast();
//        }
        handler.postDelayed(this::sendBroadcastDelayed, 1000);
    }

    void spawnListeningThread() {
        new Thread(() -> {
            Log.e("Xserver", "Listening port " + PORT);
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
//        bundle.putBinder("", this);
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

//    @Override
//    public void registerListener(int index, IReceive receiver) throws RemoteException {
//    }
//
//    @Override
//    public void unregisterListener(int index, IReceive receiver) throws RemoteException {
//    }
//
//    @Override
//    public void closeWindow(int index, long p, long window) throws RemoteException {
//
//    }
//
//    @Override
//    public void moveWindow(long winPtr, long xid, int x, int y) throws RemoteException {
//    }

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
