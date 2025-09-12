package com.fde.fusionwindowmanager;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityOptions;
import android.app.Dialog;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.core.content.FileProvider;

import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;

import org.greenrobot.eventbus.EventBus;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.ref.WeakReference;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;


public class WindowManager  {

    private static final String TAG = "WindowManager";
    public static boolean ALREADY_SET_SCREEN_SIZE;

    // Used to load the 'fusionwindowmanager' library on application startup.
    static {
        System.loadLibrary("glib-2.0");
        System.loadLibrary("fusionwindowmanager");
    }

    HandlerThread mThread;
    static Handler mHandler;

    private static WeakReference<Context> contextReference;
    public static boolean isConnected;
    public static final float DECORCATIONVIEW_HEIGHT = 42;
    public static final int MSG_START_WM = 1;
    public static final int MSG_STOP_WM = 2;
    public static List<WindowAttribute> PERFORM_WINDOW_LIST = new ArrayList<>();
    public static Set<Long> WINDOW_XIDS = new HashSet<>();
    private String display;

    public static final int WINDOW_ACTION_UNDEFINED = 0;
    public static final int WINDOW_ACTION_MAXIMIZED = 1000;
    public static final String WINDOW_ACTION_MAXIMIZED_ACTION =
            "com.fdex.x11.Xserver.action.maximized";
    public static final int WINDOW_ACTION_MAXIMIZED_REMOVE = 1001;
    public static final String WINDOW_ACTION_MAXIMIZED_REMOVE_ACTION =
            "com.fdex.x11.Xserver.action.maximized_remove";
    public static final int WINDOW_ACTION_MINIMIZE = 1003;
    public static final String WINDOW_ACTION_MINIMIZE_ACTION =
            "com.fdex.x11.Xserver.action.minize";
    public static final int WINDOW_ACTION_MINIMIZE_REMOVE = 1004;
    public static final String WINDOW_ACTION_MINIMIZE_REMOVE_ACTION =
            "com.fdex.x11.Xserver.action.minize_remove";
    public static final int WINDOW_ACTION_MAXIMIZED_HORZ = 1;
    public static final int WINDOW_ACTION_MAXIMIZED_VERT = 2;
    public static final int WINDOW_ACTION_DELETE = 1007;
    public static final String WINDOW_ACTION_KEY_WINDOWID = "window_id";

    public static final String TASK_ID_FROM_ACTIVITY_ADD = "task_id_from_activity_add";
    public static final String TASK_ID_FROM_ACTIVITY_REMOVE = "task_id_from_activity_remove";

    public static final String TASK_ID_ABOUT_WINDOW = "task_id_from_activity";
    public static final String WINDOW_ABOUT_TASK_ID = "window_about_task_id";

    public static HashMap<Long, Integer> taskIdMap = new HashMap<>();
    IntentFilter intentFilter;
    public WindowManager() {
        mThread = new HandlerThread("WM");
        mThread.start();
        mHandler = new TaskHandler(mThread.getLooper());
    }

    public WindowManager(WeakReference<Context> activityWeakReference) {
        contextReference = activityWeakReference;
        mThread = new HandlerThread("WM");
        mThread.start();
        mHandler = new TaskHandler(mThread.getLooper());
        intentFilter = new IntentFilter();
        intentFilter.addAction(TASK_ID_FROM_ACTIVITY_ADD);
        intentFilter.addAction(TASK_ID_FROM_ACTIVITY_REMOVE);
        contextReference.get().registerReceiver(receiver, intentFilter, 0X4);
    }

    BroadcastReceiver receiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.d(TAG, "onReceive() called with: context = [" + context + "], intent = [" + intent.getAction() + "]");
            if(TextUtils.equals(intent.getAction(), TASK_ID_FROM_ACTIVITY_ADD)){
                long window= intent.getLongExtra(WINDOW_ABOUT_TASK_ID, -1);
                int taskId= intent.getIntExtra(TASK_ID_ABOUT_WINDOW, -1);
                Log.d(TAG, "onReceive: window:" + window  + " taskId:" + taskId);
                taskIdMap.put(window, taskId);
            } else if(TextUtils.equals(intent.getAction(), TASK_ID_FROM_ACTIVITY_REMOVE)){
                long window= intent.getLongExtra(WINDOW_ABOUT_TASK_ID, -1);
                int taskId= intent.getIntExtra(TASK_ID_ABOUT_WINDOW, -1);
                Log.d(TAG, "onReceive: window:" + window  + " taskId:" + taskId);
                taskIdMap.remove(window);
            }
        }
    };

    public void startWindowManager(String displayGlobalParam) {
        this.display = displayGlobalParam;
        Message msg = Message.obtain();
        msg.what = MSG_START_WM;
        mHandler.sendMessage(msg);
    }

    public static boolean isConnected() {
        return isConnected;
    }

    public void stopWindowManager() {
        disconnect2Server();
        contextReference.get().unregisterReceiver(receiver);
    }

    /**
     * these native methods that is implemented by the 'fusionwindowmanager' native library,
     * which is packaged with this application.
     */
    public native void createXWindow();

    public static native int connect2Server(String display, String cliptext, String filepath);

    public native int configureWindow(long window, int x, int y, int width, int height);

    public native int moveWindow(long window, int x, int y);

    public native int resizeWindow(long window, int width, int height);

    public native int closeWindow(long window);
    public native int unmapWindow(long window);
    public native int mapWindow(long window);
    public native int raiseWindow(long window);

    public native int circulaSubWindows(long window, boolean lowest);

    public native int sendClipText(String cliptext);

    public native int sendClipFile(String file);
    public native int disconnect2Server();

    //called from native code
    public static void  syncConfigureRequest(int x, int y, int width, int height, long window){
        Log.d(TAG, "syncConfigureRequest: x:" + x + ", y:" + y + ", width:" + width + ", height:" + height + ", window:" + window + "");
        if(taskIdMap.get(window) != null  && taskIdMap.get(window) != -1){
            EventMessage message = new EventMessage(EventType.X_RESIZE_TASK, "configure_window", new WindowAttribute(x, y, width, height, 0, 0, window), null);
            EventBus.getDefault().post(message);
        } else {
            EventMessage message = new EventMessage(EventType.X_CONFIGURE_WINDOW, "configure_window", new WindowAttribute(x, y, width, height, 0, 0, window), null);
            EventBus.getDefault().post(message);
        }
    }

    //called from native code
    public static void updateWmStateClient(int action, long window){
        Log.d(TAG, "updateWmStateClient action = [" + action + "], window = [" + window + "]");
        Context context = contextReference.get();
        if((action & WINDOW_ACTION_MAXIMIZED_HORZ) > 0
                && (action & WINDOW_ACTION_MAXIMIZED_VERT) > 0){
            action = WINDOW_ACTION_MAXIMIZED;
        } else {
            action = WINDOW_ACTION_MAXIMIZED_REMOVE;
        }

        if(context == null){
            return;
        }
        switch (action){
            case WINDOW_ACTION_MAXIMIZED:
                sendBroadcastWmState(WINDOW_ACTION_MAXIMIZED_ACTION, window, context);
                break;
            case WINDOW_ACTION_MAXIMIZED_REMOVE:
                sendBroadcastWmState(WINDOW_ACTION_MAXIMIZED_REMOVE_ACTION, window, context);
                break;
            case WINDOW_ACTION_MINIMIZE:
                sendBroadcastWmState(WINDOW_ACTION_MINIMIZE_ACTION, window, context);
                break;
            case WINDOW_ACTION_MINIMIZE_REMOVE:
                break;
            case WINDOW_ACTION_DELETE:
                break;
            default:
                break;
        }
    }

    public static void sendBroadcastWmState(String action, long window, Context context) {
        String targetPackage = context.getPackageName();
        Intent intent = new Intent(action);
        intent.setPackage(targetPackage);
        intent.putExtra(WINDOW_ACTION_KEY_WINDOWID, window);
        context.sendBroadcast(intent);
    }

    //called from native code
    public static void updateXserverCliptext(String text){
        Log.d(TAG, "updateXserverCliptext: text:" + text + "");
        if(contextReference.get() != null && !TextUtils.isEmpty(text)){
            ClipData mClipData = ClipData.newPlainText("x11", text);
            android.content.ClipboardManager mClipboardManager = (ClipboardManager) contextReference.get().getSystemService(Context.CLIPBOARD_SERVICE);
            mClipboardManager.setPrimaryClip(mClipData);
        }
    }

    //called from native code
    public static void updateXserverClipFile(String text){
        Log.d(TAG, "updateXserverClipFile: text:" + text + "");
        if(contextReference.get() != null && !TextUtils.isEmpty(text)){
            try {
                String decodedPath = URLDecoder.decode(text, StandardCharsets.UTF_8.toString());
                Log.d(TAG, "updateXserverClipFile: " + decodedPath);
                Util.copyFileUriToClipboard(contextReference.get(), decodedPath);
            } catch (UnsupportedEncodingException e) {
                Log.e(TAG, "updateXserverClipFile: " + e );
            }
        }
    }

    public static void saveBitmapToFile(Bitmap bitmap, String filePath) {
        File file = new File(filePath);
        try (FileOutputStream out = new FileOutputStream(file)) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }


    public void startActivityForXMainWindow(WindowAttribute attribute, Class activityClass) {
        Log.d(TAG, "startActivityForXMainWindow: attribute:" + attribute + ", activityClass:" + activityClass + "");
        Context context = contextReference.get();
        if (context == null){
            return;
        }
        if (!WINDOW_XIDS.contains(attribute.getWindowPtr())) {
            Log.d(TAG, "startActivityForXMainWindow: attribute:" + attribute + " activityClass:" + activityClass);
            WINDOW_XIDS.add(attribute.getWindowPtr());
            ActivityOptions options = ActivityOptions.makeBasic();
            options.setLaunchBounds(new Rect((int)attribute.getOffsetX(),
                    (int)(attribute.getOffsetY()),
                    (int)(attribute.getWidth() + attribute.getOffsetX()),
                    (int)(attribute.getHeight() + DECORCATIONVIEW_HEIGHT + attribute.getOffsetY())));
            Intent intent = new Intent(context, activityClass);
            intent.putExtra("linux_window_attribute", attribute);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent, options.toBundle());
        }
    }

    private class TaskHandler extends Handler {

        public TaskHandler(@NonNull Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(@NonNull Message msg) {
            super.handleMessage(msg);
            switch (msg.what) {
                case MSG_START_WM:
                    Context context = contextReference.get();
                    ClipboardManager clipboardManager = (ClipboardManager) context.getApplicationContext().getSystemService(Context.CLIPBOARD_SERVICE);
                    String filePath = Util.getClipFilePath(clipboardManager, context);
                    String clipText = Util.getClipText(clipboardManager, context);
                    if(!TextUtils.isEmpty(filePath)){
                        filePath = filePath.replace(" ", "%20");
                    }
                    isConnected = connect2Server(display, clipText, filePath) > 0;
                    Log.d(TAG, "MSG_START_WM isConnected:" + isConnected + " display:" + display);
                    break;
                default:
                    break;
            }
        }
    }

}