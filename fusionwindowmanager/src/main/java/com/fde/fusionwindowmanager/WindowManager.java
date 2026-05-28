package com.fde.fusionwindowmanager;

import android.app.ActivityOptions;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;

import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;

import org.greenrobot.eventbus.EventBus;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.ref.WeakReference;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;


public class WindowManager  {

    private static final String TAG = "fusionwm";
    public static boolean ALREADY_SET_SCREEN_SIZE;
    public static final int ACTION_UNMAP =      1;
    public static final int ACTION_DESTORY =    2;
    public static final int ACTION_DISMISS =    3;

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
    public static final String WINDOW_ACTION_FULLSCREEN_ACTION =
            "com.fdex.x11.Xserver.action.fullscreen";
    public static final int WINDOW_ACTION_MAXIMIZED_HORZ = 1;
    public static final int WINDOW_ACTION_MAXIMIZED_VERT = 2;
    public static final int WINDOW_ACTION_DELETE = 1007;
    public static final int WINDOW_ACTION_FULLSCREEN = 4;
    public static final String WINDOW_ACTION_KEY_WINDOWID = "window_id";

    public static final String TASK_ID_FROM_ACTIVITY_ADD = "task_id_from_activity_add";
    public static final String TASK_ID_FROM_ACTIVITY_REMOVE = "task_id_from_activity_remove";

    public static final String ATTR_ABOUT_WINDOW = "attr_from_activity";
    public static final String WINDOW_ABOUT_TASK_ID = "window_about_task_id";
    private int mWidth = 1920;
    private int mHeight = 1080;
    private int density = 96;

    public static final String ACTION_X_UPDATE_SYSTEMTRAY_ICON = "com.fde.x11.update_systemtray_icon";
    public static final String KEY_ICON = "icon";
    public static final String KEY_WINDOW = "window";
    public static final String KEY_ACTION = "action";
    public static final String KEY_TITLE = "title";
    public static final long SYSTEM_TRAY_REQUEST_DOCK = 0;
    public static final long SYSTEM_TRAY_BEGIN_MESSAGE = 1;
    public static final long SYSTEM_TRAY_CANCEL_MESSAGE = 2;
    public static final long SYSTEM_TRAY_UNDOCK = 3;

    public static final long SYSTEM_TRAY_CLICK = 4;


    public static HashMap<Long, WindowAttribute> existTaskMap = new HashMap<>();
    IntentFilter intentFilter;
    public WindowManager() {
        mThread = new HandlerThread("WM");
        mThread.start();
        mHandler = new TaskHandler(mThread.getLooper());
    }

    public WindowManager(WeakReference<Context> activityWeakReference, int width, int height, int density) {
        this.mWidth = width;
        this.mHeight = height;
        this.density = density;
        contextReference = activityWeakReference;
        mThread = new HandlerThread("WM");
        mThread.start();
        mHandler = new TaskHandler(mThread.getLooper());
        intentFilter = new IntentFilter();
        intentFilter.addAction(TASK_ID_FROM_ACTIVITY_ADD);
        intentFilter.addAction(TASK_ID_FROM_ACTIVITY_REMOVE);
        intentFilter.addAction(ACTION_X_UPDATE_SYSTEMTRAY_ICON);
        contextReference.get().registerReceiver(receiver, intentFilter, 0X4);
    }

    BroadcastReceiver receiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            long window = 0;
            if(TextUtils.equals(intent.getAction(), TASK_ID_FROM_ACTIVITY_ADD)){
                window= intent.getLongExtra(WINDOW_ABOUT_TASK_ID, -1);
                WindowAttribute attr= intent.getParcelableExtra(ATTR_ABOUT_WINDOW);
                Log.d(TAG, "onReceive: window:" + window  + " attr:" + attr);
                existTaskMap.put(window, attr);
            } else if(TextUtils.equals(intent.getAction(), TASK_ID_FROM_ACTIVITY_REMOVE)){
                window= intent.getLongExtra(WINDOW_ABOUT_TASK_ID, -1);
                WindowAttribute attr= intent.getParcelableExtra(ATTR_ABOUT_WINDOW);
//                Log.d(TAG, "onReceive: window:" + window  + " attr:" + attr);
                existTaskMap.remove(window);
            } else if(TextUtils.equals(intent.getAction(), ACTION_X_UPDATE_SYSTEMTRAY_ICON)){
                window = intent.getLongExtra(KEY_WINDOW, -1);
                long action = intent.getLongExtra(KEY_ACTION, -1);
            }
//            Log.d(TAG, "onReceive() called with: window = [" + Long.toHexString(window) + "], intent = [" + intent.getAction() + "]");
        }
    };


    public static void unmapWindowFromX(int index, long pWin, long taskTo,long window,
                                        int action, int support_wm_delete, int clientNum) {
        Log.d(TAG, "unmapWindowFromX() called with: index = [" + index + "], " +
                "pWin = [" + Long.toHexString(pWin) + "], taskTo = [" + Long.toHexString(taskTo) + "], " +
                "window = [" + Long.toHexString(window) + "], action = [" + action + "], " +
                "support_wm_delete = [" + support_wm_delete + "], clientNum = [" + clientNum + "]");
        Property property = new Property();
        property.setSupportDeleteWindow(support_wm_delete);
        property.setTransientfor(taskTo);
        switch (action){
            case ACTION_DESTORY:
                EventBus.getDefault().post(new EventMessage(EventType.X_DESTROY_ACTIVITY,
                        "wm finish activity", new WindowAttribute(index, pWin, window), property));
                break;
            case ACTION_UNMAP:
                EventBus.getDefault().post(new EventMessage(EventType.X_UNMAP_WINDOW,
                        "wm hide any window", new WindowAttribute(index, pWin, window), property));
                break;
            case ACTION_DISMISS:
                EventBus.getDefault().post(new EventMessage(EventType.X_DISMISS_WINDOW,
                        "wm dismiss any window", new WindowAttribute(index, pWin, window), property));

                break;
            default:
                break;
        }


        EventBus.getDefault().post(new EventMessage(EventType.X_UNMAP_WINDOW,
                        "xserver unmap any window",
                new WindowAttribute(index, pWin, window), property));
    }

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

    public static native int connect2Server(String display, String cliptext, String filepath,
        int width, int height, int dpi);

    public native int configureWindow(long window, int x, int y, int width, int height);

    public native int setWindowingMode(long frame, long window, int mode);

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
    public static void  syncConfigureRequest(int x, int y, int width, int height, long window, int isMoving){
        Log.d(TAG, "syncConfigureRequest() called with: x = [" + x + "], y = [" + y + "], width = [" + width + "], height = [" + height + "], window = [" + window + "], isMoving = [" + isMoving + "]");
        if(existTaskMap.get(window) != null  && existTaskMap.get(window).getTaskId() != -1){
            EventMessage message = new EventMessage(EventType.X_RESIZE_TASK, "configure_window", new WindowAttribute(x, y, width, height, 0, 0, window, isMoving), null);
            EventBus.getDefault().post(message);
        } else {
            EventMessage message = new EventMessage(EventType.X_CONFIGURE_WINDOW, "configure_window", new WindowAttribute(x, y, width, height, 0, 0, window, isMoving), null);
            EventBus.getDefault().post(message);
        }
    }

    //called from native code
    public static void  updateSystemTrayIcon(Bitmap bitmap, long window, long action){
//        Log.d(TAG, "updateSystemTrayIcon() called with: bitmap = [" + bitmap + "], window = [" + window + "], action = [" + action + "]");
        Context context = contextReference.get();
        if(context != null){
            Intent intent = new Intent("com.fde.x11.update_systemtray_icon");
            intent.putExtra("icon", bitmap);
            intent.putExtra("window", window);
            intent.putExtra("action", action);
            intent.setPackage("com.android.systemui"); // 指定接收应用的包名
            context.sendBroadcast(intent);
        }
    }

        //called from native code
    public static void updateWmStateClient(int action, long window){
        Log.d(TAG, "updateWmStateClient action = [" + action + "], window = [" + window + "]");
        Context context = contextReference.get();

        if (action ==  WINDOW_ACTION_MAXIMIZED_REMOVE) {
        } if((action & WINDOW_ACTION_MAXIMIZED_HORZ) > 0
                && (action & WINDOW_ACTION_MAXIMIZED_VERT) > 0){
            action = WINDOW_ACTION_MAXIMIZED;
        } else {
//            action = WINDOW_ACTION_MAXIMIZED_REMOVE;
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
            case WINDOW_ACTION_FULLSCREEN:
                sendBroadcastWmState(WINDOW_ACTION_FULLSCREEN_ACTION, window, context);
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
//        Log.d(TAG, "startActivityForXMainWindow: attribute:" + attribute + ", activityClass:" + activityClass + "");
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

    public boolean shouldFinishWindow(WindowAttribute attr) {
//        Log.d(TAG, "shouldFinishWindow() called with: attr = [" + attr + "]");
        WindowAttribute finishAttr = existTaskMap.get(attr.getXID());
        return finishAttr != null && finishAttr.getWindowPtr() == attr.getWindowPtr();
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
//                    Log.d(TAG, "MSG_START_WM isConnected:" + isConnected + " display:" + display);
                    isConnected = connect2Server(display, clipText, filePath,
                            mWidth, mHeight, density) > 0;
                    break;
                default:
                    break;
            }
        }
    }

}