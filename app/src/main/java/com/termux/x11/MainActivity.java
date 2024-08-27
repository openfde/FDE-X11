package com.termux.x11;

import static android.os.Build.VERSION.SDK_INT;
import static android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC;
import static android.view.KeyEvent.*;
import static android.view.WindowManager.LayoutParams.*;
import static com.termux.x11.XWindowService.ACTION_X_WINDOW_ATTRIBUTE;
import static com.termux.x11.XWindowService.ACTION_X_WINDOW_PROPERTY;
import static com.termux.x11.XWindowService.CONFIGURE_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.DESTROY_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.MODALED_ACTION_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.START_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.START_VIEW_FROM_X;
import static com.termux.x11.XWindowService.STOP_VIEW_FROM_X;
import static com.termux.x11.XWindowService.STOP_WINDOW_FROM_X;
import static com.termux.x11.XWindowService.UNMODALED_ACTION_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.X_WINDOW_ATTRIBUTE;
import static com.termux.x11.XWindowService.X_WINDOW_PROPERTY;
import static com.termux.x11.Xserver.ACTION_START;
import static com.termux.x11.LoriePreferences.ACTION_PREFERENCES_CHANGED;
import static com.termux.x11.Xserver.ACTION_UPDATE_ICON;
import static com.termux.x11.data.Constants.APP_TITLE_PREFIX;
import static com.termux.x11.utils.Util.showXserverCloseOnDisconnect;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.ActivityTaskManager;
import android.app.AppOpsManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.PointF;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.DragEvent;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.PointerIcon;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.core.math.MathUtils;
import androidx.viewpager.widget.ViewPager;

import com.android.internal.widget.DecorCaptionView;
import com.easy.view.dialog.EasyDialog;
import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.eventbus.EventMessage;
import com.fde.fusionwindowmanager.eventbus.EventType;
import com.termux.x11.input.DetectEventEditText;
import com.termux.x11.input.InputEventSender;
import com.termux.x11.input.InputStub;
import com.termux.x11.input.TouchInputHandler.RenderStub;
import com.termux.x11.input.TouchInputHandler;
import com.termux.x11.utils.AppUtils;
import com.termux.x11.utils.FLog;
import com.termux.x11.utils.SamsungDexUtils;
import com.termux.x11.utils.TermuxX11ExtraKeys;
import com.termux.x11.utils.Util;
import com.termux.x11.utils.X11ToolbarViewPager;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@SuppressLint("ApplySharedPref")
@SuppressWarnings({"deprecation", "unused"})
public class MainActivity extends Activity implements View.OnApplyWindowInsetsListener {
    static final String ACTION_STOP = "com.termux.x11.ACTION_STOP";
    static final String REQUEST_LAUNCH_EXTERNAL_DISPLAY = "request_launch_external_display";
    public static Handler handler = new Handler();
    FrameLayout frm;
    public DetectEventEditText detectEventEditText;
    private TouchInputHandler mInputHandler;
    public ICmdEntryInterface service;
    public TermuxX11ExtraKeys mExtraKeys;
    private Notification mNotification;
    private final int mNotificationId = 7892;
    NotificationManager mNotificationManager;
    private boolean mClientConnected = false;
    private View.OnKeyListener mLorieKeyListener;
    private static final int KEY_BACK = 158;
    private EasyDialog easyDialog;
    private String TAG = "lifecycle ";
    private boolean correctMarked = false;
    protected long WindowCode = 0;
    protected int mIndex = 0;
    public WindowAttribute mAttribute;
    public Property mProperty;
    protected Rect mWindowRect = new Rect();
    ActivityManager am;
    private String title;
    private Configuration mConfiguration;
    private boolean isFullscreen = false;
    private int mDecorCaptionViewHeight = 42;

    // Used to set the contents of the clipboard.
    private android.content.ClipboardManager.OnPrimaryClipChangedListener mOnPrimaryClipChangedListener;
    private android.content.ClipboardManager mClipboardManager;
    private String mClipText = null;
    //flaot view for some window outside of activity
    private final Map<Long, View> mFloatViews = new HashMap<>();
    private WindowManager floatWindow;
    private final ServiceConnection connection = new Connection();
    protected long getWindowId() {
        return WindowCode;
    }
    private boolean killSelf;
    private Rect mConfigureRect;
    @SuppressLint("StaticFieldLeak")
    private static MainActivity instance;
    public MainActivity() {
        instance = this;
    }

    public static MainActivity getInstance() {
        return instance;
    }
    protected int getLayoutID(){
        return R.layout.main_activity;
    }


    /**
     * ============================================ oncreate ==================================================
     * @param savedInstanceState If the activity is being re-initialized after
     *     previously being shut down then this Bundle contains the data it most
     *     recently supplied in {@link #onSaveInstanceState}.  <b><i>Note: Otherwise it is null.</i></b>
     *
     */
    @Override
    @SuppressLint({"AppCompatMethod", "ObsoleteSdkInt", "ClickableViewAccessibility", "WrongConstant", "UnspecifiedRegisterReceiverFlag"})
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initXParams();
        Util.setBaseContext(this);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        initView();
        initEvent();
    }

    private void initXParams() {
        if(hideDecorCaptionView()){
            mDecorCaptionViewHeight = 0;
        }
        int measuredHeight = getWindow().getDecorView().getMeasuredHeight();
        am = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        mAttribute = getIntent().getParcelableExtra(X_WINDOW_ATTRIBUTE);
        if(mAttribute != null){
            mIndex = mAttribute.getIndex();
            WindowCode = mAttribute.getXID();
            mWindowRect.set(mAttribute.getRect());
            App.getApp().windowAttrMap.put(mAttribute.getXID(), mAttribute);
        }
        mProperty = getIntent().getParcelableExtra(X_WINDOW_PROPERTY);
        if(mProperty != null){
            String wmClass = mProperty.getWm_class();
            String netName = mProperty.getNet_name();
            this.title  = TextUtils.isEmpty(wmClass) ? (TextUtils.isEmpty(netName) ? APP_TITLE_PREFIX: APP_TITLE_PREFIX + ": "+ netName) : APP_TITLE_PREFIX + ": "+ wmClass;
            if(mProperty.getIcon() != null){
                ActivityManager.TaskDescription description = new ActivityManager.TaskDescription(title, mProperty.getIcon(), 0);
                MainActivity.this.setTaskDescription(description);
            }
            setTitle(title);
            App.getApp().windowPropertyMap.put(mAttribute.getXID(), mProperty);
        }
    }

    private void initView() {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
        int modeValue = Integer.parseInt(preferences.getString("touchMode", "1")) - 1;
        if (modeValue > 2) {
            SharedPreferences.Editor e = Objects.requireNonNull(preferences).edit();
            e.putString("touchMode", "1");
            e.apply();
        }
        setContentView(getLayoutID());
        detectEventEditText = findViewById(R.id.inputlayout);
        frm = findViewById(R.id.frame);
        findViewById(R.id.preferences_button).setOnClickListener((l) -> startActivity(new Intent(this, LoriePreferences.class) {{ setAction(Intent.ACTION_MAIN); }}));
        findViewById(R.id.help_button).setOnClickListener((l) -> startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/termux/termux-x11/blob/master/README.md#running-graphical-applications"))));
        LorieView lorieView = findViewById(R.id.lorieView);
        lorieView.updateCoordinate(mAttribute);
        View lorieParent = (View) lorieView.getParent();
        lorieView.setTag(R.id.WINDOW_ARRTRIBUTE, mAttribute);
        mInputHandler = new TouchInputHandler(this, new RenderStub.NullStub() {
            @Override
            public void swipeDown() {
                toggleExtraKeys();
            }
        }, new InputEventSender(lorieView));
        mLorieKeyListener = (v, k, e) -> {
            if (k == KEYCODE_VOLUME_DOWN && preferences.getBoolean("hideEKOnVolDown", false)) {
                if (e.getAction() == ACTION_UP)
                    toggleExtraKeys();
                return true;
            }
            if (k == KEYCODE_BACK) {
                if (e.isFromSource(InputDevice.SOURCE_MOUSE) || e.isFromSource(InputDevice.SOURCE_MOUSE_RELATIVE)) {
                    if (e.getRepeatCount() != 0) // ignore auto-repeat
                        return true;
                    if (e.getAction() == KeyEvent.ACTION_UP || e.getAction() == KeyEvent.ACTION_DOWN)
                        lorieView.sendMouseEvent(-1, -1, InputStub.BUTTON_RIGHT, e.getAction() == KeyEvent.ACTION_DOWN, true, mIndex);
                    return true;
                }

                if (e.getScanCode() == KEY_BACK && e.getDevice().getKeyboardType() != KEYBOARD_TYPE_ALPHABETIC || e.getScanCode() == 0) {
                    if (e.getAction() == ACTION_UP)
                        toggleKeyboardVisibility(MainActivity.this);

                    return true;
                }
            }
            return mInputHandler.sendKeyEvent(v, e);
        };
        lorieParent.setOnTouchListener((v, e) -> mInputHandler.handleTouchEvent(lorieParent, lorieView, e));
        lorieParent.setOnHoverListener((v, e) -> mInputHandler.handleTouchEvent(lorieParent, lorieView, e));
        lorieParent.setOnGenericMotionListener((v, e) -> mInputHandler.handleTouchEvent(lorieParent, lorieView, e));
        lorieView.setOnCapturedPointerListener((v, e) -> mInputHandler.handleTouchEvent(lorieView, lorieView, e));
        lorieParent.setOnCapturedPointerListener((v, e) -> mInputHandler.handleTouchEvent(lorieView, lorieView, e));
        lorieView.setOnKeyListener(mLorieKeyListener);
        lorieView.setCallback((sfc, surfaceWidth, surfaceHeight, screenWidth, screenHeight) -> {
            int framerate = (int) ((lorieView.getDisplay() != null) ? lorieView.getDisplay().getRefreshRate() : 30);
            mInputHandler.handleHostSizeChanged(surfaceWidth, surfaceHeight);
            mInputHandler.handleClientSizeChanged(screenWidth, screenHeight);
            LorieView.sendWindowChange(AppUtils.GLOBAL_SCREEN_WIDTH, AppUtils.GLOBAL_SCREEN_HEIGHT, framerate);
            WindowAttribute attribute = (WindowAttribute) lorieView.getTag(R.id.WINDOW_ARRTRIBUTE);
            if (!killSelf) {
                try {
                    if(attribute == null){
                        serviceWindowChange(sfc, 0, 0, AppUtils.GLOBAL_SCREEN_WIDTH, AppUtils.GLOBAL_SCREEN_HEIGHT,
                                0, 1000, 1000);
                    } else {
                        if (surfaceWidth == 0 || surfaceHeight == 0) {
                            serviceWindowChange(sfc, 0, 0,0, 0, attribute.getIndex(),attribute.getWindowPtr(), attribute.getXID());
                        } else {
                            serviceWindowChange(sfc, attribute.getOffsetX(), attribute.getOffsetY(),
                                    attribute.getWidth(), attribute.getHeight(), attribute.getIndex(),
                                    attribute.getWindowPtr(), attribute.getXID());
                        }
                    }
                } catch (Exception e) {
                    Log.e(TAG, "serviceWindowChange Exception: " + e);
                }
            }
            if(!isCaptionShowing()){
                getWindow().getDecorView().postDelayed(()->{
                    execInWindowManager();
                }, 200);
            }
        });
        getLorieView().setPointerIcon(PointerIcon.getSystemIcon(this, PointerIcon.TYPE_NULL));
        detectEventEditText.setOnKeyListener(mLorieKeyListener);
        detectEventEditText.setInputHandler(mInputHandler);
        EasyDialog.Builder builder = new EasyDialog.Builder(this);
        initStylusAuxButtons();
    }

    private void initEvent() {
        registerReceiver(receiver, new IntentFilter(ACTION_START) {{
            addAction(ACTION_PREFERENCES_CHANGED);
            addAction(ACTION_STOP);
            addAction(DESTROY_ACTIVITY_FROM_X);
            addAction(START_ACTIVITY_FROM_X);
            addAction(STOP_WINDOW_FROM_X);
            addAction(MODALED_ACTION_ACTIVITY_FROM_X);
            addAction(UNMODALED_ACTION_ACTIVITY_FROM_X);
            addAction(ACTION_UPDATE_ICON);
            addAction(CONFIGURE_ACTIVITY_FROM_X);
            addAction(START_VIEW_FROM_X);
            addAction(STOP_VIEW_FROM_X);
        }},  0);
        EventBus.getDefault().register(this);
        Xserver.requestConnection();
        bindService(new Intent(this, XWindowService.class), connection, Context.BIND_AUTO_CREATE);
        mClipboardManager = (android.content.ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        findViewById(R.id.button).setOnClickListener((v)->{
            try {
                service.configureWindow(mAttribute.getWindowPtr(), mAttribute.getXID(),
                        (int) mAttribute.getOffsetX(), (int) mAttribute.getOffsetY(),
                        mWindowRect.right - mWindowRect.left, mWindowRect.bottom - mWindowRect.top);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
    }


    /**
     *============================================ lifecycle ==================================================
     */
    @Override
    protected void onStart() {
        super.onStart();
        FLog.a("lifecycle", getWindowId(), "onstart");
    }

    @Override
    protected void onRestart() {
        super.onRestart();
        FLog.a("lifecycle", getWindowId(), "onRestart");
    }

    @Override
    public void onUserInteraction() {
        super.onUserInteraction();
//        FLog.a("lifecycle", getWindowId(), "onUserInteraction");
    }

    @Override
    public void onResume() {
        super.onResume();
        getWindow().getDecorView().postDelayed(()->{
            if(!correctMarked){
                correctMarked = true;
                try {
                    checkConfigBeforeExec(mConfiguration, true);
                } catch (RemoteException e) {
                    throw new RuntimeException(e);
                }
            }
        },1000);
//        getLorieView().requestFocus();
        FLog.a("lifecycle", getWindowId(), "onResume");
        detectViewRequestFocus();
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        FLog.a("lifecycle", getWindowId(), "onConfigurationChanged");
        if(!checkServiceExits()){
            return;
        }
        getWindow().getDecorView().postDelayed(()->{
            try {
                checkConfigBeforeExec(newConfig, true);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        },300);
    }

    @SuppressLint("WrongConstant")
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        FLog.a("lifecycle", getWindowId(), "onWindowFocusChanged");
        SharedPreferences p = PreferenceManager.getDefaultSharedPreferences(this);
        Window window = getWindow();
        View decorView = window.getDecorView();
        boolean fullscreen = p.getBoolean("fullscreen", false);
        boolean reseed = p.getBoolean("Reseed", true);
        fullscreen = fullscreen || getIntent().getBooleanExtra(REQUEST_LAUNCH_EXTERNAL_DISPLAY, false);
        int requestedOrientation = p.getBoolean("forceLandscape", false) ?
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE : ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        if (getRequestedOrientation() != requestedOrientation){
            setRequestedOrientation(requestedOrientation);
        }
        Util.set("fde.click_as_touch", hasFocus ? "false" : "true");
        if (hasFocus) {
            boolean hasFocused = true;
            if (SDK_INT >= VERSION_CODES.P) {
                if (p.getBoolean("hideCutout", false))
                    getWindow().getAttributes().layoutInDisplayCutoutMode = (SDK_INT >= VERSION_CODES.R) ?
                            LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS :
                            LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                else
                    getWindow().getAttributes().layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
            }

            window.setStatusBarColor(Color.BLACK);
            window.setNavigationBarColor(Color.BLACK);
            setDecorCaptionViewFocuseable(true);
        }
        window.setFlags(FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS | FLAG_KEEP_SCREEN_ON | FLAG_TRANSLUCENT_STATUS, 0);
        if (hasFocus) {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|
                    FLAG_NOT_TOUCHABLE);
            showFloatView();
        } else {
            if(mInputHandler != null){
                mInputHandler.mouseClick();
            }
        }
        if (p.getBoolean("keepScreenOn", true))
            window.addFlags(FLAG_KEEP_SCREEN_ON);
        else
            window.clearFlags(FLAG_KEEP_SCREEN_ON);
        SamsungDexUtils.dexMetaKeyCapture(this, hasFocus && p.getBoolean("dexMetaKeyCapture", false));
        if (hasFocus && mIndex != 0){
            getLorieView().regenerate();
            execInWindowManager();
        }
        getLorieView().requestFocus();
        detectViewRequestFocus();
        getClipText();
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        FLog.a("lifecycle", getWindowId(), "onAttachedToWindow");
    }

    @Override
    public void onPause() {
        super.onPause();
        FLog.a("lifecycle", getWindowId(), "onPause");
    }

    @Override
    protected void onStop() {
        super.onStop();
        FLog.a("lifecycle", getWindowId(), "onStop");
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        FLog.a("lifecycle", getWindowId(), "onDetachedFromWindow");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(receiver);
        unbindService(connection);
        stopFloatViews();
        service = null;
        FLog.a("lifecycle", getWindowId(), "onDestroy");
        if(mClipboardManager != null){
            mClipboardManager.removePrimaryClipChangedListener(mOnPrimaryClipChangedListener);
            mClipboardManager = null;
        }
        mOnPrimaryClipChangedListener = null;
        EventBus.getDefault().unregister(this);
    }

    public void onWindowDismissed(boolean finishTask, boolean suppressWindowTransition) {
        String hexString = Long.toHexString(getWindowId());
        if(checkServiceExits() && mProperty != null && mProperty.getSupportDeleteWindow() == 1){
            FLog.a("lifecycle", getWindowId(), "onWindowDismissed");
            closeXWindow();
        } else {
            FLog.a("lifecycle", getWindowId(), "onWindowDismissed finish");
            finish();
            App.getApp().stopingActivityWindow.add(mAttribute.getXID());
        }
    }

    @NonNull
    @Override
    public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
        FLog.a("lifecycle", getWindowId(), "onApplyWindowInsets");
        return insets;
    }

    @Override
    public void onWindowAttributesChanged(WindowManager.LayoutParams params) {
        super.onWindowAttributesChanged(params);
        FLog.a("lifecycle", getWindowId(), "onWindowAttributesChanged");
    }

    /**
     *============================================ about X Window ==================================================
     */
    private void getClipText() {
        FLog.a("window", getWindowId(), "getClipText");
        if (mClipboardManager != null && mClipboardManager.hasPrimaryClip()
                && mClipboardManager.getPrimaryClip() != null
                && mClipboardManager.getPrimaryClip().getItemCount() > 0) {
            CharSequence content =
                    mClipboardManager.getPrimaryClip().getItemAt(0).getText();
            Log.d(TAG, "clip:content:" + content);
            if(content != null && !TextUtils.isEmpty(content) &&
                    !TextUtils.equals(content, mClipText) && checkServiceExits()){
                mClipText = content.toString();
                FLog.a("window", getWindowId(), "getClipText:" + mClipText);
                try {
                    service.sendClipText(mClipText);
                } catch (RemoteException e) {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    protected void execInWindowManager()  {
        FLog.a("window", getWindowId(), "execInWindowManager");
        List<ActivityManager.RunningTaskInfo> runningTasks = am.getRunningTasks(50);
        for (ActivityManager.RunningTaskInfo info: runningTasks){
            if(TextUtils.equals(info.topActivity.getClassName(), getClass().getName())
                    && checkServiceExits()){
                Configuration configuration = info.configuration;
                try {
                    checkConfigBeforeExec(configuration, false);
                } catch (RemoteException e) {
                    e.printStackTrace();
                }
                return;
            }
        }
    }

    private boolean checkServiceExits() {
        return service != null && mAttribute != null;
    }

    private void checkConfigBeforeExec(Configuration configuration, boolean newConfig) throws RemoteException{
        if(configuration == null || !checkServiceExits()){
            return;
        }
        FLog.a("window", getWindowId(), "start checkConfigBeforeExec");
        this.mConfiguration = configuration;
//        Log.d(TAG, "checkConfigBeforeExec: configuration:" + configuration. + ", newConfig:" + newConfig + "");
        Pattern pattern = Pattern.compile("mBounds=Rect\\((-?\\d+), (-?\\d+) - (-?\\d+), (-?\\d+)\\)");
        Matcher matcher = pattern.matcher(configuration.toString());
        if(matcher.find()){
            int left = Integer.parseInt(Objects.requireNonNull(matcher.group(1)));
            int top = Integer.parseInt(Objects.requireNonNull(matcher.group(2)));
            int right = Integer.parseInt(Objects.requireNonNull(matcher.group(3)));
            int bottom = Integer.parseInt(Objects.requireNonNull(matcher.group(4)));
            float topMargin = isCaptionShowing() ? mDecorCaptionViewHeight : 0;
//            Log.d(TAG, "topMargin: " + topMargin);
            Rect rect = new Rect(left, (int) (top + topMargin), right, bottom);
            FLog.a("window", getWindowId(), "checkConfigBeforeExec configure:" + rect);
            boolean samePosition = atSamePosition(rect);
            boolean sameSize = atSameSize(rect);
            if(service == null){
                return;
            }
            if( newConfig ||  !samePosition || !sameSize ){
                updateAttribueOnly(rect);
                service.configureWindow(mAttribute.getWindowPtr(), mAttribute.getXID(),
                        (int) mAttribute.getOffsetX(), (int) mAttribute.getOffsetY(),
                        rect.right - rect.left, rect.bottom - rect.top);
            }
        }
        service.raiseWindow(mAttribute.getXID());

        pattern = Pattern.compile("mWindowingMode=([a-zA-Z0-9_]+)");
        matcher = pattern.matcher(configuration.toString());
        if (matcher.find()) {
            String windowingMode = matcher.group(1);
            isFullscreen = TextUtils.equals(windowingMode, "fullscreen");
            boolean isFreeform = TextUtils.equals(windowingMode, "freeform");
            FLog.a("window", getWindowId(), "windowingMode:" + windowingMode);
        }
        if (isFullscreen) {
            handler.postDelayed(() -> {
                try {
                    if(!checkServiceExits()){
                        return;
                    }
                    service.configureWindow(mAttribute.getWindowPtr(), mAttribute.getXID(),
                            (int) mAttribute.getOffsetX(), (int) mAttribute.getOffsetY(),
                            mWindowRect.right - mWindowRect.left, mWindowRect.bottom - mWindowRect.top);
                } catch (RemoteException e) {
                    e.printStackTrace();
                }
            },100);
        }
    }

    private void updateAttribueOnly(Rect rect) {
        FLog.a("window", getWindowId(), "updateAttribueOnly rect:" + rect);
        mWindowRect.set(rect);
        mAttribute.setRect(rect);
        getLorieView().setTag(R.id.WINDOW_ARRTRIBUTE, mAttribute);
    }

    private boolean atSameSize(Rect rect) {
        Rect r = mWindowRect;
        return ( r.right - r.left ) == ( rect.right - rect.left )
                && ( r.bottom - r.top ) == ( rect.bottom - rect.top );
    }

    private boolean atSamePosition(Rect rect) {
        Rect r = mWindowRect;
        return r.left == rect.left && r.top == rect.top ;
    }

    private boolean isCaptionShowing() {
        Window window = getWindow();
        ViewGroup decor = (ViewGroup)window.getDecorView();
        DecorCaptionView decorCaptionView = (DecorCaptionView)decor.getChildAt(0);
        try {
            Class<?> aClass = Class.forName("com.android.internal.widget.DecorCaptionView");
            Method isCaptionShowing = aClass.getMethod("isCaptionShowing");
            return (boolean) isCaptionShowing.invoke(decorCaptionView);
        } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException |
                 InvocationTargetException e) {
            throw new RuntimeException(e);
        }
    }

    private void closeXWindow() {
        FLog.a("window", getWindowId(), "closeXWindow");
        if(checkServiceExits()){
            try {
                WindowAttribute a = mAttribute;
                service.windowChanged(null, a.getOffsetX(),
                        a.getOffsetY(), a.getWidth(), a.getHeight(), a.getIndex(),
                        a.getWindowPtr(), a.getXID());
                service.closeWindow(a.getIndex(), a.getWindowPtr(), a.getXID());
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private void setDecorCaptionViewFocuseable(boolean focusable) {
        FLog.a("window", getWindowId(), "setDecorCaptionViewFocuseable:" + focusable);
        Window window = getWindow();
        ViewGroup decor = (ViewGroup) window.getDecorView();
        DecorCaptionView decorCaptionView = (DecorCaptionView) decor.getChildAt(0);
        boolean isCaptionShowing = true;
        try {
            Class<?> aClass = Class.forName("com.android.internal.widget.DecorCaptionView");
            Method method = aClass.getMethod("isCaptionShowing");
            isCaptionShowing = (boolean) method.invoke(decorCaptionView);
            if(isCaptionShowing) {
//                decorCaptionView.setOperateEnabled(focusable);
            }
        } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException |
                 InvocationTargetException e) {
            e.printStackTrace();
        }
    }

    private void serviceWindowChange(Surface sfc, float x, float y, float w, float h, int index, long pWin, long window) throws RemoteException {
        if(checkServiceExits()){
            service.windowChanged(sfc, x, y, w, h, index, pWin, window);
        }
    }

    @Subscribe(threadMode = ThreadMode.MAIN,priority = 1)
    public void onReceiveMsg(EventMessage message){
        if(mAttribute.getXID() != message.getProperty().getTransientfor()){
            return;
        }
        if (Objects.requireNonNull(message.getType()) == EventType.X_UNMODAL_ACTIVITY) {
            FLog.a("window", getWindowId(), "onReceiveMsg:" + message.getType().usefor);
            getWindow().clearFlags(FLAG_NOT_FOCUSABLE |
                    FLAG_NOT_TOUCHABLE);
        }
    }

    public void configureFromXIfNeed() {
        //todo check
        configureFromX();
    }

    public synchronized void configureFromX() {
        FLog.a("window", getWindowId(), "configureFromX mConfigureRect:" + mConfigureRect);
        if(mConfigureRect != null){
            mAttribute.setRect(mConfigureRect);
            mWindowRect = mConfigureRect;
            Rect rect = mConfigureRect;
            if(isCaptionShowing()){
                rect.top = mConfigureRect.top - mDecorCaptionViewHeight;
            }
            @SuppressLint("WrongConstant")
            ActivityTaskManager taskManager = (ActivityTaskManager)getSystemService("activity_task");
            taskManager.resizeTask(getTaskId(), rect);
            mConfigureRect = null;
        }
        if(checkServiceExits()){
            try {
                service.raiseWindow(mAttribute.getXID());
            } catch (RemoteException e) {
                Log.e(TAG, e.getMessage());
            }
        }
    }

    /**
     *============================================ about X floatview ==================================================
     */
    private void addFloatView(WindowAttribute attr) {
        FLog.a("float", getWindowId(), "addFloatView attr:" + attr);
        View floatView = LayoutInflater.from(this).inflate(R.layout.widget_floating_view,null,false);
        WindowManager.LayoutParams floatParams = createLayoutParams();
        floatWindow = createWindow( (int)attr.getOffsetX(),(int)attr.getOffsetY(),
                (int)attr.getWidth(),
                (int) attr.getHeight(),
                floatView, floatParams);
        floatWindow.updateViewLayout(floatView,floatParams);
        LorieView widgetView = floatView.findViewById(R.id.widget_view);
        widgetView.updateCoordinate(attr);
        widgetView.setCallback((sfc, surfaceWidth, surfaceHeight, screenWidth, screenHeight) ->{
            try {
                serviceWindowChange(sfc, attr.getOffsetX(), attr.getOffsetY(),attr.getWidth(), attr.getHeight(), attr.getIndex(), attr.getWindowPtr(), attr.getXID());
            } catch (Exception e) {
                Log.e(TAG, "serviceWindowChange Exception:" + e);
            }
        });
        TouchInputHandler inputHandler = new TouchInputHandler(this, new RenderStub.NullStub() {
            @Override
            public void swipeDown() {
                toggleExtraKeys();
            }
        }, new InputEventSender(widgetView));
        floatView.setOnTouchListener((v, e) -> inputHandler.handleTouchEvent(floatView, widgetView, e));
        floatView.setOnHoverListener((v, e) -> inputHandler.handleTouchEvent(floatView, widgetView, e));
        floatView.setOnGenericMotionListener((v, e) -> inputHandler.handleTouchEvent(floatView, widgetView, e));
        widgetView.setOnCapturedPointerListener((v, e) -> inputHandler.handleTouchEvent(widgetView, widgetView, e));
        floatView.setOnCapturedPointerListener((v, e) -> inputHandler.handleTouchEvent(widgetView, widgetView, e));
        widgetView.setOnKeyListener(mLorieKeyListener);
        mFloatViews.put(attr.getXID(), floatView);
    }

    public WindowManager createWindow(int x, int y, int width, int height, View view,
                                      WindowManager.LayoutParams params){
        WindowManager windowManager = (WindowManager) this.getSystemService(WINDOW_SERVICE);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        windowManager.getDefaultDisplay().getMetrics(displayMetrics);
        params.width = width;
        params.height = height;
        params.x = x;
        params.y = y;
        params.type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        params.gravity = Gravity.TOP | Gravity.START;
        windowManager.addView(view,params);
        return windowManager;
    }

    public WindowManager.LayoutParams createLayoutParams() {
        WindowManager.LayoutParams params = new WindowManager.LayoutParams();
        params.flags = WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | WindowManager.LayoutParams.FLAG_SCALED
                | WindowManager.LayoutParams.FLAG_LAYOUT_INSET_DECOR
                | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN;
        if (Build.VERSION.SDK_INT >= 26) {
            params.type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        } else {
            params.type = WindowManager.LayoutParams.TYPE_SYSTEM_ALERT;
        }
        params.format = PixelFormat.RGBA_8888;
        return params;
    }


    private void stopFloatView(WindowAttribute attr) {
        FLog.a("float", getWindowId(), "stopFloatView attr:" + attr);
        if(attr == null || mFloatViews.isEmpty()){
            return;
        }
        View floatView = mFloatViews.get(attr.getXID());
        if(floatWindow != null && floatView != null){
            floatWindow.removeView(floatView);
        }
        mFloatViews.remove(floatView);
    }

    private void stopFloatViews() {
        FLog.a("float", getWindowId(), "stopFloatViews");
        for(Map.Entry set: mFloatViews.entrySet()){
            View floatView = (View) set.getValue();
            if(floatView.isAttachedToWindow()){
                if(floatWindow != null){
                    floatWindow.removeView(floatView);
                }
                mFloatViews.remove(floatView);
            }
        }
    }


    private void showFloatView() {
        FLog.a("float", getWindowId(), "showFloatView");
        for(Map.Entry set: mFloatViews.entrySet()){
            View view = (View) set.getValue();
            if(view.isAttachedToWindow()){
                view.setVisibility(View.VISIBLE);
            }
        }
    }

    /**
     *============================================ about event  ==================================================
     */
    @SuppressLint("ClickableViewAccessibility")
    private void initStylusAuxButtons() {
        SharedPreferences p = PreferenceManager.getDefaultSharedPreferences(this);
        boolean stylusMenuEnabled = p.getBoolean("showStylusClickOverride", false);
        final float menuUnselectedTrasparency = 0.66f;
        final float menuSelectedTrasparency = 1.0f;
        Button left = findViewById(R.id.button_left_click);
        Button right = findViewById(R.id.button_right_click);
        Button middle = findViewById(R.id.button_middle_click);
        Button visibility = findViewById(R.id.button_visibility);
        LinearLayout overlay = findViewById(R.id.mouse_helper_visibility);
        LinearLayout buttons = findViewById(R.id.mouse_helper_secondary_layer);
        overlay.setOnTouchListener((v, e) -> true);
        overlay.setOnHoverListener((v, e) -> true);
        overlay.setOnGenericMotionListener((v, e) -> true);
        overlay.setOnCapturedPointerListener((v, e) -> true);
        overlay.setVisibility(stylusMenuEnabled ? View.VISIBLE : View.GONE);
        View.OnClickListener listener = view -> {
            TouchInputHandler.STYLUS_INPUT_HELPER_MODE = (view.equals(left) ? 1 : (view.equals(middle) ? 2 : (view.equals(right) ? 3 : 0)));
            left.setAlpha((TouchInputHandler.STYLUS_INPUT_HELPER_MODE == 1) ? menuSelectedTrasparency : menuUnselectedTrasparency);
            middle.setAlpha((TouchInputHandler.STYLUS_INPUT_HELPER_MODE == 2) ? menuSelectedTrasparency : menuUnselectedTrasparency);
            right.setAlpha((TouchInputHandler.STYLUS_INPUT_HELPER_MODE == 3) ? menuSelectedTrasparency : menuUnselectedTrasparency);
            visibility.setAlpha(menuUnselectedTrasparency);
        };

        left.setOnClickListener(listener);
        middle.setOnClickListener(listener);
        right.setOnClickListener(listener);

        visibility.setOnClickListener(view -> {
            if (buttons.getVisibility() == View.VISIBLE) {
                buttons.setVisibility(View.GONE);
                visibility.setAlpha(menuUnselectedTrasparency);
                int m = TouchInputHandler.STYLUS_INPUT_HELPER_MODE;
                visibility.setText(m == 1 ? "L" : (m == 2 ? "M" : (m == 3 ? "R" : "U")));
            } else {
                buttons.setVisibility(View.VISIBLE);
                visibility.setAlpha(menuUnselectedTrasparency);
                visibility.setText("X");

                //Calculate screen border making sure btn is fully inside the view
                float maxX = frm.getWidth() - 4 * left.getWidth();
                float maxY = frm.getHeight() - 4 * left.getHeight();

                //Make sure the Stylus menu is fully inside the screen
                overlay.setX(MathUtils.clamp(overlay.getX(), 0, maxX));
                overlay.setY(MathUtils.clamp(overlay.getY(), 0, maxY));

                int m = TouchInputHandler.STYLUS_INPUT_HELPER_MODE;
                listener.onClick(m == 1 ? left : (m == 2 ? middle : (m == 3 ? right : left)));
            }
        });
        //Simulated mouse click 1 = left , 2 = middle , 3 = right
        TouchInputHandler.STYLUS_INPUT_HELPER_MODE = 1;
        listener.onClick(left);

        visibility.setOnLongClickListener(v -> {
            v.startDragAndDrop(ClipData.newPlainText("", ""), new View.DragShadowBuilder(visibility) {
                public void onDrawShadow(Canvas canvas) {}
            }, null, View.DRAG_FLAG_GLOBAL);

            frm.setOnDragListener((v2, event) -> {
                //Calculate screen border making sure btn is fully inside the view
                float maxX = frm.getWidth() - visibility.getWidth();
                float maxY = frm.getHeight() - visibility.getHeight();

                switch (event.getAction()) {
                    case DragEvent.ACTION_DRAG_LOCATION:
                        //Center touch location with btn icon
                        float dX = event.getX() - visibility.getWidth() / 2.0f;
                        float dY = event.getY() - visibility.getHeight() / 2.0f;

                        //Make sure the dragged btn is inside the view with clamp
                        overlay.setX(MathUtils.clamp(dX, 0, maxX));
                        overlay.setY(MathUtils.clamp(dY, 0, maxY));
                        break;
                    case DragEvent.ACTION_DRAG_ENDED:
                        //Make sure the dragged btn is inside the view
                        overlay.setX(MathUtils.clamp(overlay.getX(), 0, maxX));
                        overlay.setY(MathUtils.clamp(overlay.getY(), 0, maxY));
                        break;
                }
                return true;
            });

            return true;
        });
    }

    void onReceiveConnection() {
        try {
            if (service != null && service.asBinder().isBinderAlive()) {
                FLog.a("event", getWindowId(), "Extracting logcat fd.");
                tryConnect();
            }
        } catch (Exception e) {
            Log.e(TAG, "Something went wrong while we were establishing connection", e);
        }
    }

    void tryConnect() {
        if (mClientConnected)
            return;
        try {
            FLog.a("event", getWindowId(), "tryConnect");
            ParcelFileDescriptor fd = service == null ? null : service.getXConnection();
            if (fd != null) {
                LorieView.connect(fd.detachFd());
                getLorieView().triggerCallback();
                clientConnectedStateChanged(true);
                LorieView.setClipboardSyncEnabled(PreferenceManager.getDefaultSharedPreferences(this).getBoolean("clipboardSync", true));
            }
        } catch (Exception e) {
            Log.e(TAG, "Something went wrong while we were establishing connection", e);
            service = null;

            // We should reset the View for the case if we have sent it's surface to the client.
            getLorieView().regenerate();
        }
    }

    private void detectViewRequestFocus() {
        detectEventEditText.setFocusable(true);
        detectEventEditText.setFocusableInTouchMode(true);
        detectEventEditText.requestFocus();
    }

    public void onTextViewClicked(View view) {
        detectEventEditText.requestFocus();
        detectEventEditText.setFocusableInTouchMode(true);
    }

    public LorieView getLorieView() {
        return findViewById(R.id.lorieView);
    }

    public ViewPager getTerminalToolbarViewPager() {
        return findViewById(R.id.terminal_toolbar_view_pager);
    }

    public void toggleExtraKeys(boolean visible, boolean saveState) {
        runOnUiThread(() -> {
            SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
            boolean enabled = preferences.getBoolean("showAdditionalKbd", true);
            ViewPager pager = getTerminalToolbarViewPager();
            ViewGroup parent = (ViewGroup) pager.getParent();
            boolean show = enabled && mClientConnected && visible;

            if (show) {

            } else {
                parent.removeView(pager);
                parent.addView(pager, 0);
            }

            if (enabled && saveState) {
                SharedPreferences.Editor edit = preferences.edit();
                edit.putBoolean("additionalKbdVisible", show);
                edit.commit();
            }

            pager.setVisibility(View.GONE);

//            getLorieView().requestFocus();
        });
    }

    public void toggleExtraKeys() {
        int visibility = getTerminalToolbarViewPager().getVisibility();
        toggleExtraKeys(visibility != View.VISIBLE, true);
//        getLorieView().requestFocus();
    }

    public boolean handleKey(KeyEvent e) {
        FLog.a("event", getWindowId(), "handleKey: e:" + e);
        boolean filterOutWinKey = false;
        if (filterOutWinKey && (e.getKeyCode() == KEYCODE_META_LEFT || e.getKeyCode() == KEYCODE_META_RIGHT || e.isMetaPressed()))
            return false;
        mLorieKeyListener.onKey(getLorieView(), e.getKeyCode(), e);
        return true;
    }


    private final BroadcastReceiver receiver = new BroadcastReceiver() {
        @SuppressLint("UnspecifiedRegisterReceiverFlag")
        @Override
        public void onReceive(Context context, Intent intent) {
            if (ACTION_START.equals(intent.getAction()) && service != null &&!mClientConnected) {
                try {
                    Objects.requireNonNull(service).asBinder().linkToDeath(() -> {
                        service = null;
                        Xserver.requestConnection();
                        FLog.a("event", getWindowId(), "disconnect");
                        runOnUiThread(() -> clientConnectedStateChanged(false)); //recreate()); //onPreferencesChanged(""));
                    }, 0);
                    onReceiveConnection();
                } catch (Exception e) {
                    Log.e(TAG, "Something went wrong while we extracted connection details from binder.", e);
                }
            } else if (ACTION_STOP.equals(intent.getAction())) {
                finishAffinity();
            } else if (ACTION_PREFERENCES_CHANGED.equals(intent.getAction())) {
                Log.v(TAG, "preference: " + intent.getStringExtra("key"));
            } else if (DESTROY_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                if(mAttribute != null && attr != null && mAttribute.getXID() == attr.getXID()){
                    FLog.a("event", getWindowId(), "onReceive: "  + "DESTROY_ACTIVITY_FROM_X"  + " attr:" + attr);
                    killSelf = true;
                    finish();
                    App.getApp().stopingActivityWindow.add(mAttribute.getXID());
                }
            } else if (START_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                if(mAttribute != null && attr != null && mAttribute.getXID() == attr.getTaskTo()){
                    ActivityOptions options = ActivityOptions.makeBasic();
                    Intent startAct = new Intent(MainActivity.this, MainActivity.MainActivity11.class);
                    startAct.putExtra(X_WINDOW_ATTRIBUTE, attr);
                    options.setLaunchBounds(new Rect(100, 100, 200 , 200));
                    startActivity(startAct, options.toBundle());
                }
            } else if(STOP_WINDOW_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                if(mAttribute != null && attr != null && mAttribute.getXID() == attr.getXID()
                        && mAttribute.getWindowPtr() == attr.getWindowPtr()
                        && mProperty!=null && mProperty.getSupportDeleteWindow() == 1
//                        && App.getApp().stopingActivityWindow.contains(attr.getWindowPtr())
                ){
                    FLog.a("event", getWindowId(), "onReceive: "  +
                            "STOP_WINDOW_FROM_X"  + " attr:" + attr);
                    finish();
                }
            } else if(MODALED_ACTION_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                Property property = intent.getParcelableExtra(ACTION_X_WINDOW_PROPERTY);
                if(attr != null && property != null && property.getTransientfor() == mAttribute.getXID()){
                    FLog.a("event", getWindowId(), "onReceive: "  +
                            "MODALED_ACTION_ACTIVITY_FROM_X"  + " property:" + property);
                    getWindow().addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
//                            |FLAG_NOT_TOUCHABLE
                    );
                    setDecorCaptionViewFocuseable(false);
                }
            } else if(UNMODALED_ACTION_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                if(attr != null ){
                    FLog.a("event", getWindowId(), "onReceive: "  +
                            "UNMODALED_ACTION_ACTIVITY_FROM_X"  + " attr:" + attr);
                    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
//                            |FLAG_NOT_TOUCHABLE
                    );
                    setDecorCaptionViewFocuseable(true);
                }
            } else if(ACTION_UPDATE_ICON.equals(intent.getAction())){
                long windowId = intent.getLongExtra("window_id", 0);
                if(mAttribute !=  null && windowId == mAttribute.getXID()){
//                    Log.d(TAG, "onReceive: " + title + ", windowId:" + windowId + " mAttribute:" + mAttribute) ;
                    Bitmap windowIcon = intent.getParcelableExtra("window_icon");
                    ActivityManager.TaskDescription description = new ActivityManager.TaskDescription(title , windowIcon, 0);
                    MainActivity.this.setTaskDescription(description);

                }
            } else if(CONFIGURE_ACTIVITY_FROM_X.equals(intent.getAction())) {
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                FLog.a("event", getWindowId(), "onReceive: "  +
                        "CONFIGURE_ACTIVITY_FROM_X"  + " attr:" + attr);
                if (mAttribute != null && mAttribute.getXID() == attr.getXID() && !isFullscreen)
                {
                    mConfigureRect = attr.getRect();
                    if(!mInputHandler.isTouching()){
                        configureFromX();
                    }
                }
            } else if(START_VIEW_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                Property prop = intent.getParcelableExtra(ACTION_X_WINDOW_PROPERTY);
                long transientfor = Objects.requireNonNull(prop).getTransientfor();
                long taskTo = Objects.requireNonNull(attr).getTaskTo();
                FLog.a("event", getWindowId(), "onReceive: "  +
                        "START_VIEW_FROM_X"  + " attr:" + attr + " prop:" + prop);
                if(transientfor == Objects.requireNonNull(mAttribute).getXID()
                 || taskTo == Objects.requireNonNull(mAttribute).getXID()){
                    addFloatView(attr);
                }
            } else if(STOP_VIEW_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                Property prop = intent.getParcelableExtra(ACTION_X_WINDOW_PROPERTY);
                FLog.a("event", getWindowId(), "onReceive: "  +
                        "STOP_VIEW_FROM_X"  + " attr:" + attr + " prop:" + prop);
                stopFloatView(attr);
            }
        }
    };

    public class Connection implements ServiceConnection {

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            if(!killSelf){
                MainActivity.this.service = ICmdEntryInterface.Stub.asInterface(service);
                try {
                    Objects.requireNonNull(MainActivity.this.service).asBinder().linkToDeath(() -> {
                        MainActivity.this.service = null;
                    }, 0);
                } catch (RemoteException e) {
                    e.printStackTrace();
                }
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            showXserverCloseOnDisconnect(MainActivity.this);
            finish();
        }
    }

    /**
     *============================================ other activity  ==================================================
     */
    protected boolean hideDecorCaptionView() {
        return false;
    }
    public static class MainActivity1 extends MainActivity {
    }

    public static class MainActivity11 extends MainActivity {

        protected int getLayoutID() {
            return R.layout.main_activity;
        }

        protected boolean hideDecorCaptionView() {
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);
            Log.d("TAG", "hideDecorCaptionView");
            return true;
        }
    }

    public static void toggleKeyboardVisibility(Context context) {
        InputMethodManager inputMethodManager = (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
//        Log.v(TAG, "Toggling keyboard visibility");
        if(inputMethodManager != null)
            inputMethodManager.toggleSoftInput(InputMethodManager.SHOW_FORCED, 0);
    }

    @SuppressWarnings("SameParameterValue")
    void clientConnectedStateChanged(boolean connected) {
        runOnUiThread(()-> {
            SharedPreferences p = PreferenceManager.getDefaultSharedPreferences(this);
            mClientConnected = connected;
            toggleExtraKeys(connected && p.getBoolean("additionalKbdVisible", true), true);
            findViewById(R.id.mouse_buttons).setVisibility(p.getBoolean("showMouseHelper", false) && "1".equals(p.getString("touchMode", "1")) && mClientConnected ? View.VISIBLE : View.GONE);
            findViewById(R.id.stub).setVisibility(connected?View.INVISIBLE:View.VISIBLE);
            getLorieView().setVisibility(connected?View.VISIBLE:View.INVISIBLE);
            getLorieView().regenerate();

            // We should recover connection in the case if file descriptor for some reason was broken...
            if (!connected)
                tryConnect();
            if (connected && mIndex != 0){
                getLorieView().setPointerIcon(PointerIcon.getSystemIcon(this, PointerIcon.TYPE_NULL));
            }
        });
    }
}
