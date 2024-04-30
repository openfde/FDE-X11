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
import java.net.ConnectException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.URL;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;

@Keep @SuppressLint({"StaticFieldLeak", "UnsafeDynamicallyLoadedCode"})
public class CmdEntryPoint {
    public static final String ACTION_START = "com.termux.x11.CmdEntryPoint.ACTION_START";
    public static final String ACTION_STOP = "com.termux.x11.ACTION_STOP";

    public static final String ACTION_PREFERENCES_CHANGED = "com.termux.x11.ACTION_PREFERENCES_CHANGED";

    public static final int PORT = 7892;
    public static final byte[] MAGIC = "0xDEADBEEF".getBytes();
    private static final Handler handler;
    private static final String TAG = "CmdEntryPoint";
    public static Context ctx;
    public static HashMap<Integer, IReceive> receiverMap = new HashMap<>();

    static {
        try {
            ctx = createContext();
        } catch (Throwable e) {
            Log.e("CmdEntryPoint", "Failed to instantiate context:", e);
            ctx = null;
        }
    }

    private static HashSet<Integer> Ptrs = new HashSet<>();

    public static void startActivityOrUpdateOffsetForWindow(int offsetX, int offsetY,
               int width, int height, int index, long windowPtr, long window) {
        Log.d(TAG, "startActivityOrUpdateOffsetForWindow() called with: offsetX = [" + offsetX + "], offsetY = [" + offsetY + "], width = [" + width + "], height = [" + height + "], index = [" + index + "], windowPtr = [" + windowPtr + "]");
        EventBus.getDefault().post(new EventMessage(EventType.X_START_ACTIVITY_MAIN_WINDOW,
                " ", new WindowAttribute(offsetX, offsetY, width, height, index, windowPtr, window)));
//        IReceive receiver = receiverMap.get(0);
//        if(Ptrs.contains(index)){
//            receiver = receiverMap.get(index);
//        } else {
//            receiver = receiverMap.get(0);
//        }
//        Ptrs.add(index);
//        if(receiver != null){
//            try {
//                receiver.startWindow(offsetX, offsetY , width, height, index , windowPtr);
////                receiver.startWindow(0, 0 , 1920, 989, 1 , 0);
//            } catch (RemoteException e) {
//                throw new RuntimeException(e);
//            }
//        }

    }


    /**
     * Command-line entry point.
     *
     * @param args The command-line arguments
     */
    public static void main(String[] args) {
        Log.d(TAG, "main() called with: args = [" + Arrays.toString(args) + "]");
        android.util.Log.i("CmdEntryPoint", "commit " + BuildConfig.COMMIT);
        handler.post(() -> new CmdEntryPoint(args));
        Looper.loop();
    }

    CmdEntryPoint(String[] args) {
        if (!start(args))
            System.exit(1);

        spawnListeningThread();
        sendBroadcastDelayed();
    }

    @SuppressLint({"WrongConstant", "PrivateApi"})
    void sendBroadcast() {

//        String targetPackage = getenv("TERMUX_X11_OVERRIDE_PACKAGE");
//        if (targetPackage == null)
//            targetPackage = "com.termux.x11";
//        // We should not care about multiple instances, it should be called only by `Termux:X11` app
//        // which is single instance...
//        Bundle bundle = new Bundle();
//        bundle.putBinder("", this);
//
//        Intent intent = new Intent(ACTION_START);
//        intent.putExtra("", bundle);
//        intent.setPackage(targetPackage);
//
//        if (getuid() == 0 || getuid() == 2000)
//            intent.setFlags(0x00400000 /* FLAG_RECEIVER_FROM_SHELL */);
//
//
//        try {
//            ctx.sendBroadcast(intent);
//        } catch (Exception e) {
//            if (e instanceof NullPointerException && ctx == null)
//                Log.e("CmdEntryPoint", "Context is null, falling back to manual broadcasting");
//            else
//                Log.e("CmdEntryPoint", "Falling back to manual broadcasting, failed to broadcast intent through Context:", e);
//
//
//
//            String packageName;
//            try {
//                packageName = android.app.ActivityThread.getPackageManager().getPackagesForUid(getuid())[0];
//            } catch (RemoteException ex) {
//                throw new RuntimeException(ex);
//            }
//            IActivityManager am;
//            try {
//                //noinspection JavaReflectionMemberAccess
//                am = (IActivityManager) android.app.ActivityManager.class
//                        .getMethod("getService")
//                        .invoke(null);
//
//
//            } catch (Exception e2) {
//
//
//                try {
//                    am = (IActivityManager) Class.forName("android.app.ActivityManagerNative")
//                            .getMethod("getDefault")
//                            .invoke(null);
//                } catch (Exception e3) {
//                    throw new RuntimeException(e3);
//                }
//            }
//
//            assert am != null;
//            IIntentSender sender = am.getIntentSender(1, packageName, null, null, 0, new Intent[] { intent },
//                    null, PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_ONE_SHOT, null, 0);
//            try {
//                //noinspection JavaReflectionMemberAccess
//                IIntentSender.class
//                        .getMethod("send", int.class, Intent.class, String.class, IBinder.class, IIntentReceiver.class, String.class, Bundle.class)
//                        .invoke(sender, 0, intent, null, null, new IIntentReceiver.Stub() {
//                            @Override public void performReceive(Intent i, int r, String d, Bundle e, boolean o, boolean s, int a) {}
//                        }, null, null);
//
//                Log.d("CmdEntryPoint", "sendBroadcast() send");
//            } catch (Exception ex) {
//                throw new RuntimeException(ex);
//            }
//        }
    }

    // In some cases Android Activity part can not connect opened port.
    // In this case opened port works like a lock file.
    private void sendBroadcastDelayed() {

        if (!connected()){
            sendBroadcast();
        }

        handler.postDelayed(this::sendBroadcastDelayed, 1000);
    }

    void spawnListeningThread() {
        new Thread(() -> { // New thread is needed to avoid android.os.NetworkOnMainThreadException
            /*
                The purpose of this function is simple. If the application has not been launched
                before running termux-x11, the initial sendBroadcast had no effect because no one
                received the intent. To allow the application to reconnect freely, we will listen on
                port `PORT` and when receiving a magic phrase, we will send another intent.
             */
            Log.e("CmdEntryPoint", "Listening port " + PORT);
            try (ServerSocket listeningSocket =
                         new ServerSocket(PORT, 0, InetAddress.getByName("127.0.0.1"))) {
                listeningSocket.setReuseAddress(true);
                while(true) {
                    try (Socket client = listeningSocket.accept()) {
                        Log.e("CmdEntryPoint", "Somebody connected!");
                        // We should ensure that it is some
                        byte[] b = new byte[MAGIC.length];
                        DataInputStream reader = new DataInputStream(client.getInputStream());
                        reader.readFully(b);
                        if (Arrays.equals(MAGIC, b)) {
                            Log.e("CmdEntryPoint", "New client connection!");
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

    public static void requestConnection() {
        System.err.println("Requesting connection...");
        new Thread(() -> { // New thread is needed to avoid android.os.NetworkOnMainThreadException
            try (Socket socket = new Socket("127.0.0.1", CmdEntryPoint.PORT)) {
                socket.getOutputStream().write(CmdEntryPoint.MAGIC);
            } catch (ConnectException e) {
                if (e.getMessage() != null && e.getMessage().contains("Connection refused")) {
                    Log.e("CmdEntryPoint", "ECONNREFUSED: Connection has been refused by the server");
                } else
                    Log.e("CmdEntryPoint", "Something went wrong when we requested connection", e);
            } catch (Exception e) {
                Log.e("CmdEntryPoint", "Something went wrong when we requested connection", e);
            }
        }).start();
    }

    /** @noinspection DataFlowIssue*/
    @SuppressLint("DiscouragedPrivateApi")
    public static Context createContext() {
        try {
            java.lang.reflect.Field f = Class.forName("sun.misc.Unsafe").getDeclaredField("theUnsafe");
            f.setAccessible(true);
            Object unsafe = f.get(null);
            return ((android.app.ActivityThread) Class.
                    forName("sun.misc.Unsafe").
                    getMethod("allocateInstance", Class.class).
                    invoke(unsafe, android.app.ActivityThread.class))
                    .getSystemContext();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static native boolean start(String[] args);
//    public native void windowChanged(Surface surface, long id);

    public native void windowChanged(Surface surface, float offsetX, float offsetY,
                                     float width, float height, int index, long windPtr, long window);

    public native ParcelFileDescriptor getXConnection();

//    @Override
//    public void registerListener(int index, IReceive receiver) throws RemoteException {
//        receiverMap.put(index, receiver);
//    }
//
//    @Override
//    public void unregisterListener(int index, IReceive receiver) throws RemoteException {
//        receiverMap.remove(index);
//    }
//
//    @Override
//    public void closeWindow(int index, long p, long window) throws RemoteException {
//
//    }

    public native ParcelFileDescriptor getLogcatOutput();
    public static native boolean connected();

    static {
        String path = "lib/" + Build.SUPPORTED_ABIS[0] + "/libXlorie.so";
        ClassLoader loader = CmdEntryPoint.class.getClassLoader();
        URL res = loader != null ? loader.getResource(path) : null;
        String libPath = res != null ? res.getFile().replace("file:", "") : null;
        if (libPath != null) {
            try {
                System.load(libPath);
            } catch (Exception e) {
                Log.e("CmdEntryPoint", "Failed to dlopen " + libPath, e);
                System.err.println("Failed to load native library. Did you install the right apk? Try the universal one.");
                System.exit(134);
            }
        } else {
            // It is critical only when it is not running in Android application process
            if (MainActivity.getInstance() == null) {
                System.err.println("Failed to acquire native library. Did you install the right apk? Try the universal one.");
                System.exit(134);
            }
        }

        try {
            if (Looper.getMainLooper() == null)
                Looper.prepareMainLooper();
        } catch (Exception e) {
            Log.e("CmdEntryPoint", "Something went wrong when preparing MainLooper", e);
        }
        handler = new Handler();
    }
}
