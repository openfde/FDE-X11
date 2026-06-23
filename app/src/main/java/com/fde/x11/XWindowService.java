package com.fde.x11;

import static android.os.Build.VERSION.SDK_INT;
import static com.fde.fusionwindowmanager.eventbus.EventType.X_DISMISS_WINDOW;
import static com.fde.fusionwindowmanager.eventbus.EventType.X_START_VIEW;
import static com.fde.x11.Xserver._WM_WINDOW_TYPE_SYSTIP;
import static com.fde.x11.data.Constants.DISPLAY_GLOBAL;
import static com.fde.x11.utils.AppUtils.DECOR_CAPTION_HEIGHT;
import static com.fde.x11.utils.AppUtils.NAVIGATION_BAR_HEIGHT_U;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.ActivityTaskManager;
import android.app.Service;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.View;

import androidx.annotation.NonNull;

import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.WindowManager;
import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;
import com.fde.fusionwindowmanager.HolderActivityPool;
import com.fde.x11.activity.HolderActivity;
import com.fde.x11.utils.AppUtils;
import com.fde.x11.input.InputEventSender;
import com.fde.x11.input.InputStub;
import com.fde.x11.input.TouchInputHandler;
import com.fde.x11.utils.FLog;
import com.fde.x11.utils.Util;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;

import java.io.File;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;


/**
 * native xserver run on this,
 * start activity/dialog like a x window,
 * close activity/dialog,
 * update icon,
 * <p>
 * window manager configure window and update clipboard
 */
public class XWindowService extends Service {

    private static final String TAG = "XWindowService";

    public static final String ACTION_X_WINDOW_ATTRIBUTE = "action_x_window_attribute";
    public static final String ACTION_X_WINDOW_PROPERTY = "action_x_window_property";
    private static final int TYPE_TRAY = 1;
    private static final int TYPE_TIP = 2;

    private final Map<Long, View> mFloatTrays = new HashMap<>();
    private final Map<Long, View> mFloatTips = new HashMap<>();

    private WindowAttribute rightAttr;

    public static final String CONFIGURE_ACTIVITY_FROM_X = "com.fde.x11.Xserver.action_configure";
    public static final String CONFIGURE_WIDGET_FROM_X = "com.fde.x11.Xserver.action_configure_widget";

    public static final String START_VIEW_FROM_X = "com.fde.x11.Xserver.start_action_view";
    public static final String STOP_VIEW_FROM_X = "com.fde.x11.Xserver.stop_action_view";
    public static final String START_SYSTRAY_FROM_X = "com.fde.x11.Xserver.start_systray_from_x";
    public static final String STOP_SYSTRAY_FROM_X = "com.fde.x11.Xserver.stop_systray_from_x";

    public static final String DESTROY_ACTIVITY_FROM_X = "com.fde.x11.Xserver.action_destroy";
    public static final String STOP_WINDOW_FROM_X = "com.fde.x11.Xserver.action_stop";
    public static final String HIDE_WINDOW_FROM_X = "com.fde.x11.Xserver.action_hide";
    public static final String SHOW_WINDOW_FROM_X = "com.fde.x11.Xserver.action_show";

    public static final String START_ACTIVITY_FROM_X = "com.fde.x11.Xserver.action_start";

    public static final String MODALED_ACTION_ACTIVITY_FROM_X = "com.fde.x11.Xserver.action_modaled";
    public static final String UNMODALED_ACTION_ACTIVITY_FROM_X = "com.fde.x11.Xserver.action_unmodaled";

    public static final String ACTION_X_MAIN_WINDOW_SIZE = "action_x_main_window_size";
    public static final String X_MAIN_WINDOW_SIZE = "x_main_window_size";
    public static final String X_CLIENT_SIZE = "x_client_size";

    public static final String X_WINDOW_RECT = "x_window_rect";
    public static final String X_WINDOW_INDEX = "x_window_index";
    public static final String X_WINDOW_PWIN = "x_window_pwin";
    public static final String X_WINDOW_WINDOW = "x_window_window";

    public Handler mainHandler = new Handler(Looper.getMainLooper());
    public static final String X_WINDOW_ATTRIBUTE = "x_window_attribute";
    public static final String X_WINDOW_PROPERTY = "x_window_property";
    private static final int DESTROY_ACTIVITY_RETRY = 1;
    private static final int DESTROY_ACTIVITY_DELAY = 0;
    private static final int CREATE_ACTIVITY_DELAY = 1000;
    private static final int CREATE_ACTIVTIY_POOL = 1;
    private static final int CREATE_ACTIVTIY_POOL_SIZE = 3;
    private HolderActivityPool mHolderActivityPool;
    private static final boolean DWM_START_DEFAULT = true;
    private WindowManager fusionWindowManager;
    private ActivityManager am;
    private final HashSet<Long> startingWindow = new HashSet<>();
    private final HashSet<Long> stopingWindow = new HashSet<>();
    private final HashSet<Long> runningMainWindow = new HashSet<>();
    private boolean mBound = false;

    private ActivityTaskManager taskManager;
    private android.view.WindowManager systemWindowManager;
    private final HashMap<Long, Property> propertyHashMap = new HashMap<>();

    private final HashMap<Long, IActivityCallback> activityCallbackMap = new HashMap<>();
    public final HashMap<Long, WindowAttribute> shouldDestroyMap = new HashMap<>();
    public final HashMap<Long, WindowAttribute> shouldResizeMap = new HashMap<>();
    private int mWidth = 1920;
    private int mHeight = 1080;
    private long mLastFocusWindow;

    private final ICmdEntryInterface.Stub service = new ICmdEntryInterface.Stub() {
        @Override
        public void windowChanged(Surface surface, float x, float y, float w, float h, int index, long pWin, long XID) throws RemoteException {
            FLog.s(TAG, XID, "windowChanged: surface:" + surface + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + ", index:" + index + ", pWin:" + pWin + ", XID:" + Long.toHexString(XID) + "");
            startingWindow.remove(XID);
            Xserver.getInstance().windowChanged(surface, x, y, w, h, index, pWin, XID);
        }

        @Override
        public ParcelFileDescriptor getXConnection() throws RemoteException {
//            FLog.s(TAG, "getXConnection: ");
            return Xserver.getInstance().getXConnection();
        }

        @Override
        public int getConnectedFD() throws RemoteException {
            return 0;
        }


        @Override
        public void closeWindow(int index, long winPtr, long window) throws RemoteException {
            startingWindow.remove(window);
            stopingWindow.remove(window);
            if (fusionWindowManager != null && fusionWindowManager.closeWindow(window) > 0) {
//                FLog.s(TAG, "closeWindow: index:" + index + ", winPtr:" + winPtr + ", window:" + window + "");
            }
        }

        @Override
        public void unmapWindow(int index, long p, long window) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.unmapWindow(window) > 0) {
//                FLog.s(TAG, "unmapWindow: index:" + index + ", winPtr:" + winPtr + ", window:" + window + "");
            }
        }

        @Override
        public void mapWindow(int index, long p, long window) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.mapWindow(window) > 0) {
//                FLog.s(TAG, "unmapWindow: index:" + index + ", winPtr:" + winPtr + ", window:" + window + "");
            }
        }

        @Override
        public void configureWindow(long winPtr, long window, int x, int y, int w, int h) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.configureWindow(window, x, y, w, h) > 0) {
                FLog.s(TAG, "configureWindow: winPtr:" + winPtr + ", window:" + window + ", x:" + x + ", y:" + y + ", w:" + w + ", h:" + h + "");
            }
        }

        @Override
        public void setWindowingMode(long frame, long window, int mode) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.setWindowingMode(frame, window, mode) > 0) {
                FLog.s(TAG, "setWindowingMode: frame:" + frame + ", window:" + window + ", mode:" + mode + "");
            }
        }

        @Override
        public void moveWindow(long winPtr, long window, int x, int y) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.moveWindow(window, x, y) > 0) {
//                FLog.s(TAG, "moveWindow: winPtr:" + winPtr + ", window:" + window + ", x:" + x + ", y:" + y + "");
            }
        }

        @Override
        public void resizeWindow(long window, int w, int h) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.resizeWindow(window, w, h) > 0) {
//                FLog.s(TAG, "resizeWindow: window:" + window + ", w:" + w + ", h:" + h + "");
            }
        }

        @Override
        public void raiseWindow(long window) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.raiseWindow(window) > 0) {
//                FLog.s(TAG, "raiseWindow: window:" + window + "");
                Xserver.getInstance().tellFocusWindow(window);
                mLastFocusWindow = window;
            }
        }

        @Override
        public void circulaSubWindows(long window, boolean lowest) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.circulaSubWindows(window, lowest) > 0) {
//                FLog.s(TAG, "circulaSubWindows: window:" + window + ", lowest:" + lowest + "");
            }
        }

        @Override
        public void sendClipText(String cliptext) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.sendClipText(cliptext) > 0) {
                FLog.s(TAG, "sendClipText: cliptext:" + cliptext + "");
            }
        }

        @Override
        public void sendClipFile(String file) throws RemoteException {
            if (fusionWindowManager != null && fusionWindowManager.sendClipFile(file) > 0) {
                FLog.s(TAG, "sendClipFile: file:" + file + "");
            }
        }

        @Override
        public void sendMouseEvent(float x, float y, int whichButton, boolean buttonDown, boolean relative, int index) throws RemoteException {
//            FLog.s(TAG, "sendMouseEvent() called with: x = [" + x + "], y = [" + y + "], whichButton = [" + whichButton + "], buttonDown = [" + buttonDown + "], relative = [" + relative + "], index = [" + index + "]");
            Xserver.getInstance().sendMouseEvent(x, y, whichButton, buttonDown, relative, index);
        }

        @Override
        public void registerActivityCallback(long window, IActivityCallback callback) throws RemoteException {
            activityCallbackMap.put(window, callback);
            shouldWindowManagerFinishActivity(window);
            shouldResizeActivity(window);
        }

        @Override
        public void unregisterActivityCallback(long window, IActivityCallback callback) throws RemoteException {
            activityCallbackMap.remove(window);
            Xserver.getInstance().removeWindow(window);
        }

        @Override
        public void updateSystemViewVisible(boolean visible) throws RemoteException {
            serviceUpdateSystemViewVisible(visible);
        }

        @Override
        public void onWindowFocusChanged(long window, boolean hasFocus) throws RemoteException {
            activityOnWindowFocusChanged(window, hasFocus);
        }

    };

    private void activityOnWindowFocusChanged(long xid, boolean hasFocus) {
//        FLog.s(TAG, "activityOnWindowFocusChanged() called with: xid = [" + xid + "], hasFocus = [" + hasFocus + "]");
//        if(!hasFocus){
//            View floatView = mFloatTips.get(xid);
//            if (systemWindowManager != null && floatView != null && floatView.isAttachedToWindow()) {
//                systemWindowManager.removeView(floatView);
//                FLog.s(TAG, "stopFloatTrayAndTip: successful");
//                mFloatTips.remove(xid);
//            }
//        }
    }

    private void serviceUpdateSystemViewVisible(boolean visible) {
        FLog.s(TAG, "serviceUpdateSystemViewVisible() called with: visible = [" + visible + "]");
        mainHandler.post(() -> {
            for (Map.Entry set : mFloatTrays.entrySet()) {
                View view = (View) set.getValue();
                if (view.isAttachedToWindow()) {
                    view.setVisibility(visible ? View.VISIBLE : View.GONE);
                }
            }
            for (Map.Entry set : mFloatTips.entrySet()) {
                View view = (View) set.getValue();
                if (view.isAttachedToWindow()) {
                    view.setVisibility(visible ? View.VISIBLE : View.GONE);
                }
            }
        });

    }

    private Handler handler = new Handler();

    @SuppressLint("WrongConstant")
    @Override
    public void onCreate() {
        super.onCreate();
        taskManager = (ActivityTaskManager) getSystemService("activity_task");
        systemWindowManager = (android.view.WindowManager) getSystemService(WINDOW_SERVICE);
        am = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        Util.copyAssetsToFiles(this, "xkb", "xkb");
        int density = getSystemDensity();
        mHolderActivityPool = new HolderActivityPool(CREATE_ACTIVTIY_POOL_SIZE, handler);
        if (DWM_START_DEFAULT) {
            fusionWindowManager = new WindowManager(new WeakReference<>(this),
                    mWidth, mHeight, density);
            fusionWindowManager.startWindowManager(DISPLAY_GLOBAL + "");
            fusionWindowManager.setPool(mHolderActivityPool);
        }
//        Util.checkX11FdPermission(this);
        EventBus.getDefault().register(this);
        Xserver.getInstance().registerContext(new WeakReference<>(this), fusionWindowManager);
        String height = AppUtils.getProperty("openfde.display_height", "1080");
        String width = AppUtils.getProperty("openfde.display_width", "1920");
        Xserver.getInstance().startXserver(width, height);
        Xserver.X_ClientNum = 0;

    }

    private int getSystemDensity() {
//        String pDensity = AppUtils.getProperty("ro.sf.lcd_density", "160");
//        int lcd_density = Integer.parseInt(pDensity);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        if (SDK_INT >= Build.VERSION_CODES.R) {
            getDisplay().getRealMetrics(displayMetrics);
        }
        mWidth = displayMetrics.widthPixels;
        mHeight = displayMetrics.heightPixels;
        int densityDpi = displayMetrics.densityDpi;
        float d = (float) (densityDpi * 96 / 160);
        float xFactor = 1.f;
//        float xFactor = mWidth == 1920 ? 1.f : 1.75f; //TODO for d3000M
        return (int) (d * xFactor);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Subscribe(threadMode = ThreadMode.MAIN, priority = 1)
    public void onReceiveMsg(EventMessage message) {
        FLog.s(TAG, message.getMessage() + " ID:" +
                Long.toHexString(message.getWindowAttribute().getXID())
                + "   prop:" + message.getProperty());
        int windowSize = runningMainWindow.size();
        handler.postDelayed(()->{
            if(mHolderActivityPool.isFullOrNearly()){
                boolean decorFull = mHolderActivityPool.goingInrease();
                startHolderActivity(decorFull? HolderActivity.NoDecorHolderActivity.class :
                        HolderActivity.DecorHolderActivity.class);
            }
        }, CREATE_ACTIVITY_DELAY);

//        FLog.s(TAG, "before: size:" + windowSize);
        switch (message.getType()) {
            case X_START_ACTIVITY_MAIN_WINDOW:
                if (message.getWindowAttribute().getProperty() != null
                        && message.getWindowAttribute().getProperty().getSupportMotif() > 0) {
                    startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity11.class);
                } else {
                    startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity1.class, DECOR_CAPTION_HEIGHT);
                }
                sendBroadcastFocusableIfNeed(message.getWindowAttribute(), false);
                break;
            case X_START_ACTIVITY_WINDOW:
                startActLikeWindow(message.getWindowAttribute(), MainActivity.MainActivity11.class);
                sendBroadcastFocusableIfNeed(message.getWindowAttribute(), false);
                break;
            case X_UNMAP_WINDOW:
                if (mFloatTrays.get(message.getWindowAttribute().getXID()) != null) {
                    stopFloatTrayAndTip(message.getWindowAttribute(), TYPE_TRAY);
                } else if (mFloatTips.get(message.getWindowAttribute().getXID()) != null) {
                    stopFloatTrayAndTip(message.getWindowAttribute(), TYPE_TIP);
                } else {
//                    sendBroadcastHide(message.getWindowAttribute());
                    WindowAttribute unmap = WindowManager.existTaskMap.get(message.getWindowAttribute().getXID());
                    FLog.s(TAG, "onReceiveMsg: unmapId:" + unmap);
                    if (unmap != null && unmap.getTaskId() != 0) {
                        am.moveTaskToBack(true, unmap.getTaskId());
                    }
                }
                break;
            case X_MAP_ACTIVITY: {
//                WindowAttribute mapAttr = WindowManager.taskIdMap.get( message.getWindowAttribute().getXID());
//                FLog.s(TAG, "X_MAP_ACTIVITY: map:" + mapAttr);
//                if(mapAttr != null && mapAttr.getTaskId() != 0){
//                    am.moveTaskToFront(mapAttr.getTaskId(), MOVE_TASK_NO_USER_ACTION);
//                }
                sendBroadcastMapWindow(message.getWindowAttribute());
            }
            break;
            case X_DESTROY_ACTIVITY: {
                WindowAttribute destroyAttr = message.getWindowAttribute();
//                Log.d(TAG, "onReceiveMsg() called with: destroyAttr = [" + destroyAttr + "]");
                if (mFloatTrays.get(destroyAttr.getXID()) != null) {
                    stopFloatTrayAndTip(destroyAttr, TYPE_TRAY);
                } else if (mFloatTips.get(destroyAttr.getXID()) != null) {
                    stopFloatTrayAndTip(destroyAttr, TYPE_TIP);
                } else if (message.getProperty() != null && message.getProperty().getSupportDeleteWindow() != 0) {
                    destroyActivitySafety(DESTROY_ACTIVITY_RETRY, destroyAttr);
                } else {
                    stopWindow(destroyAttr);
                }
                shouldWindowManagerFinishActivity(destroyAttr);
                sendBroadcastFocusableIfNeed(destroyAttr, true);
            }
            break;
            case X_CONFIGURE_WINDOW: {
                sendBroadcastConfigureWindow(message.getWindowAttribute());
                WindowAttribute attr = message.getWindowAttribute();
                shouldResizeActivity(attr);
            }
            break;
            case X_RESIZE_TASK: {
                WindowAttribute attr = message.getWindowAttribute();
                shouldMoveActivity(attr);
            }
            break;
            case X_CONFIGURE_WIDGET:
                sendBroadcastConfigureWidget(message.getWindowAttribute());
                break;
            case X_START_VIEW:
                if (message.getProperty() != null && message.getProperty().getType() == _WM_WINDOW_TYPE_SYSTIP) {
                    updateSystrayAndTip(message.getWindowAttribute(), TYPE_TIP);
                } else {
                    sendBroadcastAboutView(message.getWindowAttribute(), message.getProperty(), X_START_VIEW);
                }
//   TODO for test             startActLikeWindowWithDecorHeight(message.getWindowAttribute(), MainActivity.MainActivity1.class, 42f);
                break;
            case X_DISMISS_WINDOW:
                if (mFloatTrays.get(message.getWindowAttribute().getXID()) != null) {
                    stopFloatTrayAndTip(message.getWindowAttribute(), TYPE_TRAY);
                } else if (mFloatTips.get(message.getWindowAttribute().getXID()) != null) {
                    stopFloatTrayAndTip(message.getWindowAttribute(), TYPE_TIP);
                } else {
                    sendBroadcastAboutView(message.getWindowAttribute(), message.getProperty(), X_DISMISS_WINDOW);
                }
                break;
            case X_START_SYSTRAY:
//                sendBroadcastSystray(message.getWindowAttribute(),message.getProperty(), X_START_SYSTRAY);
                updateSystrayAndTip(message.getWindowAttribute(), TYPE_TRAY);
                break;
            default:
                break;
        }
        int size = runningMainWindow.size();
//        FLog.s(TAG, "after: size:" + size);
        if (windowSize != size) {
            sendBroadcastSize(size);
        }
    }

    public void startHolderActivity(Class cls) {
        Intent intent = new Intent(this, cls);
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchBounds(new Rect(0,0,1,1));
        try {
            Method method = ActivityOptions.class.getMethod("setLaunchWindowingMode", int.class);
            method.invoke(options, 5); // change to freeform mode
        } catch (Exception e) {
            e.printStackTrace();
        }
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent, options.toBundle());
    }

    private void shouldResizeActivity(long xid){
        if (shouldResizeMap.get(xid) != null) {
            WindowAttribute attr = shouldResizeMap.get(xid);
            shouldResizeActivity(attr);
        }
    }

    private void shouldResizeActivity(WindowAttribute attr) {
        FLog.s(TAG, "shouldResizeActivity: " + " " + attr + " " + attr);
        WindowAttribute resize = fusionWindowManager.existTaskMap.get(attr.getXID());
        if (resize != null && resize.getTaskId() != 0) {
            Rect rect = new Rect(attr.getRect().left,
                    attr.getRect().top - resize.getCaptionHeight(),
//                            attr.getRect().top,
                    attr.getRect().right,
                    attr.getRect().bottom);
            taskManager.resizeTask(resize.getTaskId(), rect);
        } else {
            shouldResizeMap.put(attr.getXID(), attr);
        }
    }

    private void shouldMoveActivity(WindowAttribute attr) {
        WindowAttribute resize = fusionWindowManager.existTaskMap.get(attr.getXID());
        if (resize != null && resize.getTaskId() != 0) {
            Rect rect = new Rect(attr.getRect().left,
                    attr.getRect().top - resize.getCaptionHeight(),
//                            attr.getRect().top,
                    attr.getRect().right,
                    attr.getRect().bottom);
            FLog.s(TAG, "shouldConfigureActivity: " + " " + resize + " " + rect);
            IActivityCallback callback = activityCallbackMap.get(attr.getXID());
            if (callback != null && attr.getIsMoving() != 2) {
                try {
                    if (attr.getIsMoving() == 1) {
                        callback.startDecorMovingTask(rect.left, rect.top, attr.getXID());
                    } else if (attr.getIsMoving() == 0) {
                        callback.finisDecorMovingTask(attr.getXID());
                    }
                } catch (RemoteException e) {
                    Log.e(TAG, "RemoteException: " + e.getMessage());
                }
            }
            if (attr.getIsMoving() == 2) {
                taskManager.resizeTask(resize.getTaskId(), rect);
            }
        }
    }

    private void shouldWindowManagerFinishActivity(WindowAttribute attr) {
        Log.d(TAG, "shouldWindowManagerFinishActivity() called with: attr = [" + attr + "]");
        if (attr.getWindowPtr() == 0) {
            return;
        }
        if (fusionWindowManager != null && fusionWindowManager.shouldFinishWindow(attr)) {
            IActivityCallback callback = activityCallbackMap.get(attr.getXID());
            if (callback != null) {
                try {
                    boolean success = callback.finishActivity(attr.getXID());
                    if (success && shouldDestroyMap.get(attr.getXID()) != null) {
                        shouldDestroyMap.remove(attr.getXID());
                    }
                } catch (RemoteException e) {
                    Log.e(TAG, "RemoteException: " + e.getMessage());
                }
            }
            shouldDestroyMap.put(attr.getXID(), attr);
        } else {
            shouldDestroyMap.put(attr.getXID(), attr);
        }
    }

    private void shouldWindowManagerFinishActivity(long xid) {
        Log.d(TAG, "shouldWindowManagerFinishActivity() called with: xid = [" + Long.toHexString(xid) + "]");
        if (shouldDestroyMap.get(xid) != null) {
            WindowAttribute attr = shouldDestroyMap.get(xid);
            shouldWindowManagerFinishActivity(attr);
        }
    }

    private boolean stopFloatTrayAndTip(WindowAttribute attr, int type) {
        FLog.s(TAG, "stopFloatTrayAndTip() called with: attr = [" + attr + "], type = [" + type + "]");
        Map<Long, View> floatViews;
        if (type == TYPE_TRAY) {
            floatViews = mFloatTrays;
        } else {
            floatViews = mFloatTips;
        }

        if (attr == null || floatViews.isEmpty()) {
            FLog.s(TAG, "stopFloatTrayAndTip: isEmpty");
            return false;
        }
        View floatView = floatViews.get(attr.getXID());
        if (systemWindowManager != null && floatView != null && floatView.isAttachedToWindow()) {
            systemWindowManager.removeView(floatView);
            FLog.s(TAG, "stopFloatTrayAndTip: successful");
            if (type == TYPE_TIP) {
                mFloatTips.remove(attr.getXID());
                return true;
            }
        }
//        mFloatViews.remove(attr.getXID());
        if (type == TYPE_TRAY) {
            rearrangeWindowAttributes(attr.getXID());
        }
        return false;
//        }
    }

    private void rearrangeWindowAttributes(long removedViewId) {
        // 1. 移除指定的View
        mFloatTrays.remove(removedViewId);
        // 2. 获取剩余的所有WindowAttribute并按当前offsetX排序
        List<WindowAttribute> attributes = mFloatTrays.values().stream()
                .map(view -> (WindowAttribute) view.getTag())
                .sorted((attr1, attr2) -> Float.compare(attr1.getOffsetX(), attr2.getOffsetX()))
                .collect(Collectors.toList());

        // 3. 重新设置offsetX，从右向左排列
        float currentOffsetX = rightAttr.getOffsetX();
        for (WindowAttribute attr : attributes) {
            attr.setOffsetX(currentOffsetX);
            updateSystrayAndTip(attr, TYPE_TRAY);
            currentOffsetX -= 30;
        }
    }

    private void sendBroadcastSystray(WindowAttribute attr, Property property, EventType type) {
        FLog.s(TAG, attr.getXID(), "sendBroadcastSystray: attr:" + attr + ", type:" + type);
        Intent intent = new Intent();
        intent.setAction(START_SYSTRAY_FROM_X);
        intent.setPackage("com.android.systemui");
        intent.putExtra(X_WINDOW_RECT, attr.getRect());
        intent.putExtra(X_WINDOW_INDEX, attr.getIndex());
        intent.putExtra(X_WINDOW_PWIN, attr.getWindowPtr());
        intent.putExtra(X_WINDOW_WINDOW, attr.getXID());
        sendBroadcast(intent);
    }

    private void updateSystrayAndTip(WindowAttribute attr, int type) {
        FLog.s(TAG, "updateSystrayAndTip() called with: attr = [" + attr + "], type = [" + type + "]");

        if(outOfScreen(attr)){
            insetsIntoScreenWithHeight(attr, 0, false);
            if(fusionWindowManager != null){
                fusionWindowManager.configureWindow(attr.getXID(), (int) attr.getOffsetX(), (int) attr.getOffsetY(),
                        (int) attr.getWidth(), (int) attr.getHeight());
            }
        }

        if (type == TYPE_TRAY && rightAttr == null) {
            rightAttr = attr;
        }

        View floatView;
        Map<Long, View> floatViews;
        if (type == TYPE_TRAY) {
            floatViews = mFloatTrays;
        } else {
            floatViews = mFloatTips;
        }

        android.view.WindowManager.LayoutParams floatParams = createLayoutParams(attr);
        if (floatViews.get(attr.getXID()) == null) {
            floatView = LayoutInflater.from(this).inflate(R.layout.widget_floating_view, null, false);
            systemWindowManager.addView(floatView, floatParams);
        } else {
            floatView = floatViews.get(attr.getXID());
            systemWindowManager.updateViewLayout(floatView, floatParams);
        }
//        systemWindowManager.updateViewLayout(floatView,floatParams);
        LorieView widgetView = floatView.findViewById(R.id.widget_view);
        widgetView.updateCoordinate(attr);
        mainHandler.post(() -> widgetView.setCallback(new LorieView.Callback() {
            @Override
            public void changed(Surface sfc, int surfaceWidth, int surfaceHeight, int screenWidth, int screenHeight) {
//                Xserver.getInstance().windowChanged(sfc, attr.getOffsetX(), attr.getOffsetY(),attr.getWidth(), attr.getHeight(), attr.getIndex(), attr.getWindowPtr(), attr.getXID());
            }

            @Override
            public void realSizeChanged(Surface sfc, int width, int height) {
                if (fusionWindowManager != null) {
                    fusionWindowManager.configureWindow(attr.getXID(), (int) attr.getOffsetX(),
                            (int) attr.getOffsetY(), width, height);
                }
                Xserver.getInstance().windowChanged(sfc, attr.getOffsetX(), attr.getOffsetY(), attr.getWidth(), attr.getHeight(), attr.getIndex(), attr.getWindowPtr(), attr.getXID());
            }

            @Override
            public void onSurfaceDestroy(Surface sfc) {

            }
        }));

        InputEventSender inputEventSender = getInputEventSender(attr, widgetView);
        TouchInputHandler inputHandler = new TouchInputHandler(this, new TouchInputHandler.RenderStub.NullStub() {}, inputEventSender);
        floatView.setOnTouchListener((v, e) -> inputHandler.handleTouchEvent(floatView, widgetView, e));
        floatView.setOnHoverListener((v, e) -> inputHandler.handleTouchEvent(floatView, widgetView, e));
        floatView.setOnGenericMotionListener((v, e) -> inputHandler.handleTouchEvent(floatView, widgetView, e));
        widgetView.setOnCapturedPointerListener((v, e) -> inputHandler.handleTouchEvent(widgetView, widgetView, e));
        floatView.setOnCapturedPointerListener((v, e) -> inputHandler.handleTouchEvent(widgetView, widgetView, e));
        floatView.setTag(attr);
        floatViews.put(attr.getXID(), floatView);
        FLog.s(TAG, "updateSystrayAndTip: " + floatViews.size());
    }

    @NonNull
    private static InputEventSender getInputEventSender(WindowAttribute attr, LorieView widgetView) {
        InputEventSender inputEventSender = new InputEventSender(widgetView);
        inputEventSender.setEventInterface(new InputStub() {
            @Override
            public void sendMouseEvent(float x, float y, int whichButton, boolean buttonDown, boolean relative, int index) {
                Xserver.getInstance().sendMouseEvent(x, y, whichButton, buttonDown, relative, index);
            }

            @Override
            public void sendMouseWheelEvent(float deltaX, float deltaY) {

            }

            @Override
            public boolean sendKeyEvent(int scanCode, int keyCode, boolean keyDown) {
                return false;
            }

            @Override
            public void sendTextEvent(byte[] utf8Bytes) {

            }

            @Override
            public void sendUnicodeEvent(int code) {

            }

            @Override
            public void sendTouchEvent(int action, int pointerId, int x, int y) {

            }

            @Override
            public WindowAttribute getAttribute() {
                return attr;
            }
        });
        return inputEventSender;
    }

    public android.view.WindowManager.LayoutParams createLayoutParams(WindowAttribute attr) {
        android.view.WindowManager.LayoutParams params = new android.view.WindowManager.LayoutParams();
        params.flags = android.view.WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                | android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                | android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | android.view.WindowManager.LayoutParams.FLAG_SCALED
                | android.view.WindowManager.LayoutParams.FLAG_LAYOUT_INSET_DECOR
                | android.view.WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN;
//        if (Build.VERSION.SDK_INT >= 26) {
        params.type = 2024;//android.view.WindowManager.LayoutParams.TYPE_SYSTEM_ALERT;
//        } else {
//            params.type = android.view.WindowManager.LayoutParams.TYPE_SYSTEM_DIALOG;
//        }
        if (SDK_INT >= Build.VERSION_CODES.R) {
            params.setFitInsetsTypes(0);
        }
        params.gravity = Gravity.TOP | Gravity.START;
        params.width = (int) attr.getWidth();
        params.height = (int) attr.getHeight();
        params.x = (int) attr.getOffsetX();
        params.y = (int) attr.getOffsetY();
//        params.format = PixelFormat.TRANSPARENT;
        return params;
    }

    private void sendBroadcastMapWindow(WindowAttribute attr) {
        handler.postDelayed(() -> {
            FLog.s(TAG, "sendBroadcastMapWindow: attr:" + attr + "");
            String targetPackage = getPackageName();
            Intent intent = new Intent(SHOW_WINDOW_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            sendBroadcastAsUser(intent, UserHandle.ALL);
        }, 1000);

    }

    private void sendBroadcastHide(WindowAttribute attr) {
        FLog.s(TAG, "sendBroadcastHide: attr:" + attr + "");
        String targetPackage = getPackageName();
        Intent intent = new Intent(HIDE_WINDOW_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
//        sendStickyBroadcast(intent);
        sendBroadcastAsUser(intent, UserHandle.ALL);
    }

    private void sendBroadcastSize(int size) {
//        String targetPackage = getPackageName();
        Intent intent = new Intent(ACTION_X_MAIN_WINDOW_SIZE);
//        intent.setPackage(targetPackage);
        intent.putExtra(X_MAIN_WINDOW_SIZE, size);
        intent.putExtra(X_CLIENT_SIZE, Xserver.X_ClientNum > 0 ? Xserver.X_ClientNum - 1 : 0);
        sendBroadcast(intent);
    }

    private void sendBroadcastConfigureWidget(WindowAttribute attr) {
//        if(outOfScreen(attr)){
//            insetsIntoScreen(attr);
//            if(fusionWindowManager != null){
//                fusionWindowManager.configureWindow(attr.getXID(), (int) attr.getOffsetX(), (int) attr.getOffsetY(),
//                        (int) attr.getWidth(), (int) attr.getHeight());
//            }
//            return;
//        }
        String targetPackage = getPackageName();
        Intent intent = new Intent(CONFIGURE_WIDGET_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        sendBroadcast(intent);
    }

    private void insetsIntoScreenWithHeight(WindowAttribute attr, int decorHeight, boolean withNavi) {
        int x = (int) attr.getOffsetX();
        int y = (int) attr.getOffsetY();
        int w = (int) attr.getWidth();
        int h = (int) attr.getHeight();
        if(x < 0){
            attr.setOffsetX(0);
        }
        if(y < decorHeight){
            attr.setOffsetY(decorHeight);
        }
        if(x + w > mWidth){
            attr.setOffsetX(mWidth - w);
        }
        int height = withNavi? (mHeight - NAVIGATION_BAR_HEIGHT_U) :  mHeight;
        if(y + h > height){
            attr.setOffsetY(height - h);
        }
    }

    private void insetsIntoScreen(WindowAttribute attr) {
        int x = (int) attr.getOffsetX();
        int y = (int) attr.getOffsetY();
        int w = (int) attr.getWidth();
        int h = (int) attr.getHeight();
        if(x < 0){
            attr.setOffsetX(0);
        }
        if(y < 0){
            attr.setOffsetY(0);
        }
        if(x + w > mWidth){
            attr.setOffsetX(mWidth - w);
        }
        if(y + h > mHeight){
            attr.setOffsetY(mHeight - h);
        }
    }

    private boolean outOfScreen(WindowAttribute attr) {
        int x = (int) attr.getOffsetX();
        int y = (int) attr.getOffsetY();
        int w = (int) attr.getWidth();
        int h = (int) attr.getHeight();
        return x < 0 || y < 0 || x + w > mWidth || y + h > mHeight;
    }

    private void sendBroadcastAboutView(WindowAttribute attr, Property property, EventType type) {
        FLog.s(TAG, attr.getXID(), "sendBroadcastAboutView: attr:" + attr + ", type:" + type +
                ", property:" + property);
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
        handler.postDelayed(() -> {
            FLog.s(TAG, "sendBroadcastConfigureWindow: attr:" + attr + "");
            String targetPackage = getPackageName();
            Intent intent = new Intent(CONFIGURE_ACTIVITY_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            sendBroadcast(intent);
        }, DESTROY_ACTIVITY_DELAY);
    }

    private void sendBroadcastFocusableIfNeed(WindowAttribute attr, boolean isFocusable) {
        if (!isFocusable) {
            Property property = attr.getProperty();
            if (property == null || property.getTransientfor() == 0) {
                return;
            }
            attr.setFocusable(false);
            String targetPackage = getPackageName();
            Intent intent = new Intent(MODALED_ACTION_ACTIVITY_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            intent.putExtra(ACTION_X_WINDOW_PROPERTY, attr.getProperty());
            propertyHashMap.put(attr.getXID(), attr.getProperty());
            sendBroadcast(intent);
        } else {
            Property property = propertyHashMap.get(attr.getXID());
            if (property == null || property.getTransientfor() == 0) {
                return;
            }
            attr.setFocusable(true);
            String targetPackage = getPackageName();
            Intent intent = new Intent(UNMODALED_ACTION_ACTIVITY_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            intent.putExtra(ACTION_X_WINDOW_PROPERTY, attr.getProperty());
            propertyHashMap.remove(attr.getXID(), attr.getProperty());
            sendBroadcast(intent);
        }
    }

    private void stopWindow(WindowAttribute attr) {
        if (stopingWindow.contains(attr.getXID())) {
            return;
        }
        runningMainWindow.remove(attr.getXID());
        stopingWindow.add(attr.getXID());
        FLog.s(TAG, "stopActivity: attr:" + attr + "");
        String targetPackage = getPackageName();
        Intent intent = new Intent(STOP_WINDOW_FROM_X);
        intent.setPackage(targetPackage);
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
//        sendStickyBroadcast(intent);
        sendBroadcastAsUser(intent, UserHandle.ALL);

    }

    private void destroyActivitySafety(int retry, WindowAttribute attr) {
        runningMainWindow.remove(attr.getXID());
        mainHandler.postDelayed(() -> {
            FLog.s(TAG, "destroyActivitySafety: retry:" + retry + ", attr:" + attr + "");
            String targetPackage = getPackageName();
            Intent intent = new Intent(DESTROY_ACTIVITY_FROM_X);
            intent.setPackage(targetPackage);
            intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
            sendBroadcast(intent);
        }, DESTROY_ACTIVITY_DELAY);
    }

    public void startActLikeWindow(WindowAttribute attr, Class cls) {
        startActLikeWindowWithDecorHeight(attr, cls, 0);
    }

    public void startActLikeWindowWithDecorHeight(WindowAttribute attr, Class cls, float decorHeight) {
        stopingWindow.remove(attr.getXID());
        if (startingWindow.contains(attr.getXID())) {
            return;
        }
        if(outOfScreen(attr)){
            insetsIntoScreenWithHeight(attr, (int)decorHeight, true);
            if(fusionWindowManager != null){
                fusionWindowManager.configureWindow(attr.getXID(), (int) attr.getOffsetX(), (int) attr.getOffsetY(),
                        (int) attr.getWidth(), (int) attr.getHeight());
            }
//            return;
        }

        runningMainWindow.add(attr.getXID());
        startingWindow.add(attr.getXID());
        FLog.s(TAG, "startActLikeWindowWithDecorHeight: attr:" + attr + ", cls:" + cls + ", decorHeight:" + decorHeight + "");
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchBounds(new Rect((int) attr.getOffsetX(),
                (int) (attr.getOffsetY() - decorHeight),
                (int) (attr.getWidth() + attr.getOffsetX()),
                (int) (attr.getHeight() + attr.getOffsetY())));
        Intent intent = new Intent(this, cls);
        if (attr.getProperty() != null) {
            intent.putExtra(X_WINDOW_PROPERTY, attr.getProperty());
            intent.putExtra("X11_titile", attr.getProperty().getNet_name());
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
        FLog.s(TAG, "startActLikeWindowWithDecorHeight: attr:" + attr + ", cls:" + cls + ", decorHeight:" + decorHeight + "");
//        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return Service.START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        FLog.s(TAG, "onBind:" + intent);
        mBound = true;
        return service;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        mBound = false;
        FLog.s(TAG, "onUnbind:" + intent);
        return true;
    }

    private void checkIfShouldStopSelf() {
        if (!mBound) {
            stopSelf();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        FLog.s(TAG, "onDestroy");
        Util.deleteRecursive(new File("/tmp/fde"));
        if (DWM_START_DEFAULT) {
            fusionWindowManager.stopWindowManager();
        }
    }
}