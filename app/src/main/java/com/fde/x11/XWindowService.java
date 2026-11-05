package com.fde.x11;

import static android.os.Build.VERSION.SDK_INT;
import static com.fde.FrameworkImpl.DECOR_CAPTION_HEIGHT;
import static com.fde.fusionwindowmanager.eventbus.EventType.X_DISMISS_WINDOW;
import static com.fde.fusionwindowmanager.eventbus.EventType.X_START_VIEW;
import static com.fde.x11.Xserver._WM_WINDOW_TYPE_SYSTIP;
import static com.fde.x11.data.Constants.DISPLAY_GLOBAL;
import static com.fde.x11.utils.AppUtils.NAVIGATION_BAR_HEIGHT_U;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.ActivityTaskManager;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
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
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
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

    // Activity 宿主类型：带标题栏装饰 / 无装饰
    private static final int HOST_ACTIVITY_DECOR = 1;
    private static final int HOST_ACTIVITY_NODECOR = 2;
    private static final int RESTART_ACTIVITY_DELAY = 600;

    private final Map<Long, Integer> windowHostKind = new ConcurrentHashMap<>();

    private final Map<Long, View> mFloatTrays = new ConcurrentHashMap<>();
    private final Map<Long, View> mFloatTips = new ConcurrentHashMap<>();

    private WindowAttribute rightAttr;

    /** 窗口角色诊断广播接收器（见 ACTION_DUMP_WINDOW_ROLES）。 */
    private BroadcastReceiver roleDumpReceiver;

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
    public static final String UPDATE_ACTIVITY_FROM_X = "com.fde.x11.Xserver.action_update_activity";

    public static final String ACTION_X_MAIN_WINDOW_SIZE = "action_x_main_window_size";
    public static final String X_MAIN_WINDOW_SIZE = "x_main_window_size";
    public static final String X_CLIENT_SIZE = "x_client_size";

    /**
     * 诊断入口：打印所有已登记窗口的"证据 + 策略建议 + 实际宿主"。
     * <pre>adb shell am broadcast -a com.fde.x11.DUMP_WINDOW_ROLES -p com.fde.x11</pre>
     */
    public static final String ACTION_DUMP_WINDOW_ROLES = "com.fde.x11.DUMP_WINDOW_ROLES";

    /** 诊断日志 tag，固定不随构建类型变化（release 包也要能看到）。 */
    public static final String ROLE_DUMP_TAG = "winrole";

    public static final String X_WINDOW_RECT = "x_window_rect";
    public static final String X_WINDOW_INDEX = "x_window_index";
    public static final String X_WINDOW_PWIN = "x_window_pwin";
    public static final String X_WINDOW_WINDOW = "x_window_window";

    public Handler mainHandler = new Handler(Looper.getMainLooper());
    public static final String X_WINDOW_ATTRIBUTE = "x_window_attribute";
    public static final String X_WINDOW_PROPERTY = "x_window_property";
    /** 该 X 窗口是否要求不上 dock / 不进最近任务（SKIP_TASKBAR 或 KDE override）。 */
    public static final String X_WINDOW_HIDE_FROM_DOCK = "x_window_hide_from_dock";
    private static final int DESTROY_ACTIVITY_RETRY = 1;
    private static final int DESTROY_ACTIVITY_DELAY = 0;
    private static final int CREATE_ACTIVITY_DELAY = 300;
    private static final boolean DWM_START_DEFAULT = true;
    private WindowManager fusionWindowManager;
    private ActivityManager am;
    // 这些集合会被 binder 线程（windowChanged/registerActivityCallback 等）和主线程同时访问
    private final Set<Long> startingWindow = ConcurrentHashMap.newKeySet();
    private final Set<Long> stopingWindow = ConcurrentHashMap.newKeySet();
    private final Set<Long> runningMainWindow = ConcurrentHashMap.newKeySet();
    private boolean mBound = false;

    private ActivityTaskManager taskManager;
    // FDE-X11: 几何事务序号，随 resizeTask 传给 framework，用于识别"这次变化是自己请求的回声"
    private final java.util.concurrent.atomic.AtomicLong geometrySeq = new java.util.concurrent.atomic.AtomicLong(1);
    private android.view.WindowManager systemWindowManager;
    private final Map<Long, Property> propertyHashMap = new ConcurrentHashMap<>();

    // 由 binder 线程（register/unregisterActivityCallback）和主线程共同访问
    private final Map<Long, IActivityCallback> activityCallbackMap = new ConcurrentHashMap<>();
    public final Map<Long, WindowAttribute> shouldDestroyMap = new ConcurrentHashMap<>();
    public final Map<Long, WindowAttribute> shouldResizeMap = new ConcurrentHashMap<>();
    private final Map<Long, Long> transientForMap = new ConcurrentHashMap<>();
    private int mWidth = 1920;
    private int mHeight = 1080;

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
            if(whichButton != 0){
                FLog.k(TAG, "sendMouseEvent() called with: x = [" + x + "], y = [" + y + "], whichButton = [" + whichButton + "], buttonDown = [" + buttonDown + "], relative = [" + relative + "], index = [" + index + "]");
            }
            Xserver.getInstance().sendMouseEvent(x, y, whichButton, buttonDown, relative, index);
        }

        @Override
        public void registerActivityCallback(long window, IActivityCallback callback) throws RemoteException {
            activityCallbackMap.put(window, callback);
            // 窗口在这个宿主连上来之前就已经没了：直接收掉，别让它变成空壳窗口
            WindowAttribute pendingDestroy = shouldDestroyMap.remove(window);
            if (pendingDestroy != null) {
                FLog.s(TAG, window, "registerActivityCallback: window already destroyed, finish host");
                try {
                    callback.finishActivity(window);
                } catch (RemoteException e) {
                    Log.e(TAG, "finishActivity for pending destroy failed", e);
                }
                return;
            }
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

    // ---------------------------------------------------------------------
    // X ↔ Android 通信统计（P3-11）：每 5 秒一行，优化前后可以直接对比。
    // 只统计我们改过的那几条几何通道，渲染/内容刷新不在内。
    // 全部在主线程累加（EventBus 是 MAIN，service handler 也是主线程）。
    // ---------------------------------------------------------------------
    private static final long COMM_STATS_INTERVAL_MS = 5000;
    private int statGeometryReports;      // X→Android 几何回传（X_RESIZE_TASK / X_CONFIGURE_WINDOW）
    private int statEchoSkips;            // 其中判定为「Android 自己请求的回声」被跳过
    private int statResizeTasks;          // 真正下发到 framework 的 resizeTask
    private int statConfigureCallbacks;   // 定向回调 configureActivity
    private int statConfigureBroadcasts;  // 宿主没注册回调时的兜底广播

    private final Runnable commStatsTicker = new Runnable() {
        @Override
        public void run() {
            if (statGeometryReports > 0 || statResizeTasks > 0 || statConfigureCallbacks > 0
                    || statConfigureBroadcasts > 0 || statEchoSkips > 0) {
                FLog.s(TAG, "comm-stats/5s: geometry=" + statGeometryReports
                        + " echoSkip=" + statEchoSkips
                        + " resizeTask=" + statResizeTasks
                        + " callback=" + statConfigureCallbacks
                        + " broadcast=" + statConfigureBroadcasts);
                statGeometryReports = 0;
                statEchoSkips = 0;
                statResizeTasks = 0;
                statConfigureCallbacks = 0;
                statConfigureBroadcasts = 0;
            }
            handler.postDelayed(this, COMM_STATS_INTERVAL_MS);
        }
    };

    @SuppressLint("WrongConstant")
    @Override
    public void onCreate() {
        super.onCreate();
        handler.postDelayed(commStatsTicker, COMM_STATS_INTERVAL_MS);
        taskManager = (ActivityTaskManager) getSystemService("activity_task");
        systemWindowManager = (android.view.WindowManager) getSystemService(WINDOW_SERVICE);
        am = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        Util.copyAssetsToFiles(this, "xkb", "xkb");
//        Util.checkX11FdPermission(this);
        EventBus.getDefault().register(this);
        Xserver.getInstance().registerContext(new WeakReference<>(this));
        String height = AppUtils.getProperty("openfde.display_height", "1080");
        String width = AppUtils.getProperty("openfde.display_width", "1920");
        int density = getSystemDensity();
        Xserver.getInstance().startXserver(width, height);
        Xserver.X_ClientNum = 0;
        if (DWM_START_DEFAULT) {
            fusionWindowManager = new WindowManager(new WeakReference<>(this),
                    mWidth, mHeight, density);
            fusionWindowManager.startWindowManager(DISPLAY_GLOBAL + "");
        }
        // 窗口角色诊断入口（只读，不改变任何行为）
        roleDumpReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                // 无条件打一行，用于区分"广播没到"和"广播到了但没数据"
                Log.i(ROLE_DUMP_TAG, "received " + ACTION_DUMP_WINDOW_ROLES);
                Xserver.dumpWindowRoles();
            }
        };
        // 必须用 RECEIVER_EXPORTED(0x2)：0x4 是 RECEIVER_NOT_EXPORTED，
        // 只接受同 UID 发送者，adb shell 发的广播会被静默丢弃。
        registerReceiver(roleDumpReceiver, new IntentFilter(ACTION_DUMP_WINDOW_ROLES),
                Context.RECEIVER_EXPORTED);
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
        if (message == null || message.getWindowAttribute() == null) {
            Log.e(TAG, "onReceiveMsg: invalid message " + message);
            return;
        }
        FLog.k(TAG, message.getWindowAttribute().getXID(), "message:" + message);
        int windowSize = runningMainWindow.size();
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
                // 内嵌 widget（画在父窗口 Surface 里的菜单/工具条）没有独立宿主：
                // native 已经把它从父窗口里摘掉了，这里不能再走 stopWindow——
                // 否则 closeOrDestroyWindow 带的 taskTo 会把父窗口的宿主一起收掉
                // （WPS 的登录框就是这样被子工具条"连坐"销毁的）。
                if (!isHostedWindow(destroyAttr.getXID())) {
                    FLog.s(TAG, destroyAttr.getXID(), "skip destroy for non-hosted window");
                    break;
                }
                // Activity 还没连上来就收到销毁：广播会丢在启动窗口期，等它注册回调时再收尾，
                // 否则那个宿主会一直留在屏幕上，变成一个空壳窗口（kylin-os-manager 的
                // 那批 716x36 临时窗就是这样留下 4 个幽灵 Activity 的）。
                if (!activityCallbackMap.containsKey(destroyAttr.getXID())) {
                    shouldDestroyMap.put(destroyAttr.getXID(), destroyAttr);
                }
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
            case X_UPDATE_WINDOW_ATTRIBUTE:
                handleWindowAttributeUpdate(message.getWindowAttribute(), message.getProperty());
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
                if (!isHostedWindow(message.getWindowAttribute().getXID())) {
                    // 同上：内嵌 widget 的 dismiss 不该外溢到父窗口
                    FLog.s(TAG, message.getWindowAttribute().getXID(),
                            "skip dismiss for non-hosted window");
                    break;
                }
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

    private void shouldResizeActivity(long xid){
        if (shouldResizeMap.get(xid) != null) {
            WindowAttribute attr = shouldResizeMap.get(xid);
            shouldResizeActivity(attr);
        }
    }

    private void shouldResizeActivity(WindowAttribute attr) {
        android.util.Log.d("GeometryToken", "msg RESIZE_TASK isMoving=" + attr.getIsMoving()
                + " req=" + attr.getGeometryRequestId()
                + " rect=" + attr.getRect());
        statGeometryReports++;
        if (attr.getGeometryRequestId() != 0) {
            // 这是 Android 自己那次 configureWindow 的回声（WM 原样应用了请求）。
            // task 本来就在我们请求的几何上，再 resizeTask 只会抖动，直接跳过。
            statEchoSkips++;
            android.util.Log.d("GeometryToken", "geometry echo req=" + attr.getGeometryRequestId()
                    + " -> skip resizeTask, rect=" + attr.getRect());
            return;
        }
        FLog.k(TAG,  attr.getXID(), "shouldResizeActivity attr: " +  attr);
        WindowAttribute resize = WindowManager.existTaskMap.get(attr.getXID());
        if (resize != null && resize.getTaskId() != 0) {
            Rect rect = new Rect(attr.getRect().left,
                    attr.getRect().top - resize.getCaptionHeight(),
                    attr.getRect().right,
                    attr.getRect().bottom);
            long token = geometrySeq.incrementAndGet();
                        statResizeTasks++;
                        android.util.Log.d("GeometryToken", "send task=" + resize.getTaskId()
                                + " rect=" + rect + " token=" + token + " isMoving=" + attr.getIsMoving());
                        taskManager.resizeTask(resize.getTaskId(), rect, token);
        } else {
            shouldResizeMap.put(attr.getXID(), attr);
        }
    }

    private void shouldMoveActivity(WindowAttribute attr) {
        android.util.Log.d("GeometryToken", "msg MOVE_TASK isMoving=" + attr.getIsMoving()
                + " rect=" + attr.getRect());
        statGeometryReports++;
        WindowAttribute resize = WindowManager.existTaskMap.get(attr.getXID());
        if (resize != null && resize.getTaskId() != 0) {
            Rect rect = new Rect(attr.getRect().left,
                    attr.getRect().top - resize.getCaptionHeight(),
                    attr.getRect().right,
                    attr.getRect().bottom);
            FLog.k(TAG,  attr.getXID(), "shouldConfigureActivity: " + " " + resize + " " + rect);
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
                if (attr.getGeometryRequestId() != 0) {
                    // 同上：Android 自己那次 configureWindow 的回声，不用再 resizeTask
                    statEchoSkips++;
                    android.util.Log.d("GeometryToken", "geometry echo req=" + attr.getGeometryRequestId()
                            + " -> skip resizeTask, rect=" + attr.getRect());
                } else {
                    long token = geometrySeq.incrementAndGet();
                        statResizeTasks++;
                        android.util.Log.d("GeometryToken", "send task=" + resize.getTaskId()
                                + " rect=" + rect + " token=" + token + " isMoving=" + attr.getIsMoving());
                        taskManager.resizeTask(resize.getTaskId(), rect, token);
                }
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
//            shouldDestroyMap.put(attr.getXID(), attr);
        } else {
//            shouldDestroyMap.put(attr.getXID(), attr);
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
        FLog.k(TAG, attr.getXID(), "updateSystrayAndTip() called with: attr = [" + attr + "], type = [" + type + "]");

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
        // 托盘/提示也是浮层：Surface 尺寸跟着布局走，不要按全局分辨率裁剪
        widgetView.setFloatingPopup(true);
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

    /**
     * 把 X 侧的窗口几何同步给宿主 Activity。
     *
     * <p>优先走已经注册的 {@link IActivityCallback#configureActivity}：一次 oneway 定向
     * Binder 直达宿主进程，不再经过 AMS 广播，也不需要每个 MainActivity 各自按 XID 过滤。
     * 只有宿主还没注册回调（窗口刚创建 / 正在重启）时才退回原来的包内广播。
     */
    private void sendBroadcastConfigureWindow(WindowAttribute attr) {
        handler.postDelayed(() -> {
            FLog.s(TAG, "sendBroadcastConfigureWindow: attr:" + attr + "");
            IActivityCallback callback = activityCallbackMap.get(attr.getXID());
            if (callback != null) {
                try {
                    Rect rect = attr.getRect();
                    callback.configureActivity(attr.getXID(), rect.left, rect.top,
                            rect.width(), rect.height());
                    statConfigureCallbacks++;
                    return;
                } catch (RemoteException e) {
                    Log.e(TAG, "configureActivity failed, fallback to broadcast: " + e.getMessage());
                }
            }
            statConfigureBroadcasts++;
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

    /**
     * 这个 xid 是不是一个"独立宿主"（Activity 或浮层窗口）。
     *
     * <p>内嵌 widget（画在父窗口 Surface 里的菜单/工具条）不是宿主：它们的销毁只该由
     * native 从父窗口里摘掉，不能走 stopWindow——否则会把父窗口的宿主一起收掉。
     */
    private boolean isHostedWindow(long xid) {
        return runningMainWindow.contains(xid)
                || mFloatTrays.containsKey(xid)
                || mFloatTips.containsKey(xid)
                || activityCallbackMap.containsKey(xid)
                || stopingWindow.contains(xid);
    }

    private void stopWindow(WindowAttribute attr) {
        if (stopingWindow.contains(attr.getXID())) {
            return;
        }
        windowHostKind.remove(attr.getXID());
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
        windowHostKind.remove(attr.getXID());
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

    /**
     * 窗口类型/装饰属性变化的同步入口：
     * 宿主类型不变 -> 广播给 Activity 原地更新；
     * 宿主类型变化 -> 结束现有 Activity 并以新的宿主类型重启（保留 X 窗口）
     */
    private void handleWindowAttributeUpdate(WindowAttribute attr, Property property) {
        long xid = attr.getXID();
        FLog.s(TAG, xid, "handleWindowAttributeUpdate attr:" + attr + ", property:" + property);
        if (property == null || !runningMainWindow.contains(xid)) {
            return;
        }
        int desiredKind = computeHostKind(property);
        Integer currentKind = windowHostKind.get(xid);
        if (currentKind == null || currentKind == desiredKind) {
            sendBroadcastUpdateAttribute(attr, property);
            return;
        }
        Class cls = desiredKind == HOST_ACTIVITY_DECOR
                ? MainActivity.MainActivity1.class : MainActivity.MainActivity11.class;
        float decorHeight = desiredKind == HOST_ACTIVITY_DECOR ? DECOR_CAPTION_HEIGHT : 0;
        restartWindowForTypeChange(attr, property, cls, decorHeight);
    }

    private static int computeHostKind(Property property) {
        int type = property.getType();
        if (type == 0 || type == Xserver._NET_WM_WINDOW_TYPE_NORMAL) {
            return property.getSupportMotif() > 0 ? HOST_ACTIVITY_NODECOR : HOST_ACTIVITY_DECOR;
        }
        return HOST_ACTIVITY_NODECOR;
    }

    private void sendBroadcastUpdateAttribute(WindowAttribute attr, Property property) {
        FLog.s(TAG, attr.getXID(), "sendBroadcastUpdateAttribute attr:" + attr);
        Intent intent = new Intent(UPDATE_ACTIVITY_FROM_X);
        intent.setPackage(getPackageName());
        intent.putExtra(ACTION_X_WINDOW_ATTRIBUTE, attr);
        intent.putExtra(ACTION_X_WINDOW_PROPERTY, property);
        sendBroadcast(intent);
    }

    private void restartWindowForTypeChange(WindowAttribute attr, Property property, Class cls, float decorHeight) {
        long xid = attr.getXID();
        IActivityCallback callback = activityCallbackMap.get(xid);
        if (callback == null) {
            // 宿主 Activity 还没注册回调（启动窗口期），此时重启会重复创建宿主，
            // 延迟重试等待宿主就绪
            FLog.s(TAG, xid, "restartWindowForTypeChange: callback not ready, retry later");
            mainHandler.postDelayed(() -> handleWindowAttributeUpdate(attr, property), 1000);
            return;
        }
        FLog.s(TAG, xid, "restartWindowForTypeChange cls:" + cls.getSimpleName() + ", decorHeight:" + decorHeight);
        try {
            callback.finishActivityForRestart(xid);
        } catch (RemoteException e) {
            Log.e(TAG, "finishActivityForRestart RemoteException: " + e.getMessage());
        }
        runningMainWindow.remove(xid);
        startingWindow.remove(xid);
        windowHostKind.put(xid, cls == MainActivity.MainActivity1.class ? HOST_ACTIVITY_DECOR : HOST_ACTIVITY_NODECOR);
        final WindowAttribute restartAttr = new WindowAttribute((int) attr.getOffsetX(), (int) attr.getOffsetY(),
                (int) attr.getWidth(), (int) attr.getHeight(), attr.getIndex(), attr.getWindowPtr(),
                attr.getXID(), attr.getWindow(), attr.getTaskTo(), property);
        mainHandler.postDelayed(() -> startActLikeWindowWithDecorHeight(restartAttr, cls, decorHeight), RESTART_ACTIVITY_DELAY);
    }

    public void startActLikeWindow(WindowAttribute attr, Class cls) {
        startActLikeWindowWithDecorHeight(attr, cls, 0);
    }

    public void startActLikeWindowWithDecorHeight(WindowAttribute attr, Class cls, float decorHeight) {
        stopingWindow.remove(attr.getXID());
        // XID 会被复用：同一个 id 上来了新窗口，不能继承上一个窗口的"待销毁"标记
        shouldDestroyMap.remove(attr.getXID());
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
        windowHostKind.put(attr.getXID(), cls == MainActivity.MainActivity1.class
                ? HOST_ACTIVITY_DECOR : HOST_ACTIVITY_NODECOR);
        FLog.k(TAG, attr.getXID(), "startActLikeWindowWithDecorHeight: attr:" + attr + ", cls:" + cls + ", decorHeight:" + decorHeight + "");
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
        // _NET_WM_STATE_SKIP_TASKBAR / _KDE_NET_WM_WINDOW_TYPE_OVERRIDE：宿主仍是
        // Activity（要能拿焦点/输入法），但不进最近任务，也不上 dock。必须在这里算：
        // 只有本进程（xserver）能读到 native 的窗口登记表，MainActivity 在另一个进程里读不到。
        boolean hideFromDock = Xserver.isHiddenFromDockWindow(attr.getXID());
        intent.putExtra(X_WINDOW_HIDE_FROM_DOCK, hideFromDock);
        if (hideFromDock) {
            intent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
        }
        startActivity(intent, options.toBundle());
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
        handler.removeCallbacks(commStatsTicker);
        if (roleDumpReceiver != null) {
            try {
                unregisterReceiver(roleDumpReceiver);
            } catch (IllegalArgumentException ignored) {
                // 未注册或已注销
            }
            roleDumpReceiver = null;
        }
        Util.deleteRecursive(new File("/tmp/fde"));
        if (DWM_START_DEFAULT) {
            fusionWindowManager.stopWindowManager();
        }
    }
}