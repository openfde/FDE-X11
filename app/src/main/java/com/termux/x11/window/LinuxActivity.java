package com.termux.x11.window;

import static android.view.WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS;
import static android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON;
import static android.view.WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
import static com.fde.fusionwindowmanager.Util.WINDOW_ATTRIBUTE;
import static com.termux.x11.CmdEntryPoint.ACTION_START;
import static com.termux.x11.CmdEntryPoint.ACTION_STOP;
import static com.termux.x11.LoriePreferences.ACTION_PREFERENCES_CHANGED;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.widget.FrameLayout;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.WindowManager;
import com.termux.x11.CmdEntryPoint;
import com.termux.x11.ICmdEntryInterface;
import com.termux.x11.IReceive;
import com.termux.x11.R;
import com.termux.x11.XwindowView;
import com.termux.x11.input.InputEventSender;
import com.termux.x11.input.TouchInputHandler;

import java.util.Objects;

public class LinuxActivity extends AppCompatActivity {
    private static final String TAG = "LinuxActivity";
    public static Handler handler = new Handler();
    protected WindowAttribute mWindowAttribute;
    protected long WINDOW_PTR = 0;
    protected int mIndex = 0;
    FrameLayout frm;
    private TouchInputHandler mInputHandler;
    private ICmdEntryInterface service = null;
    private BroadcastReceiver receiver;
    private XwindowView.Callback callback;

    private IReceive.Stub listener;

    private boolean mClientConnected = false;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initXwindowView();
        initCallback();
    }

    private void initCallback() {
        listener = new IReceive.Stub() {
            @Override
            public void startWindow(int offsetX, int offsetY, int width, int height, int index, long windPtr) throws RemoteException {

            }
        };
        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (ACTION_START.equals(intent.getAction())) {
                    try {
                    Log.v(TAG, "Got new ACTION_START intent");
                        IBinder b = Objects.requireNonNull(intent.getBundleExtra("")).getBinder("");
                        service = ICmdEntryInterface.Stub.asInterface(b);
                        service.registerListener(mIndex, listener);
                        Objects.requireNonNull(service).asBinder().linkToDeath(() -> {
                            service = null;
                            CmdEntryPoint.requestConnection();
                            Log.v(TAG, "Disconnected");
                            runOnUiThread(() -> clientConnectedStateChanged(false)); //recreate()); //onPreferencesChanged(""));
                        }, 0);

                        onReceiveConnection();
                    } catch (Exception e) {
                        Log.e("MainActivity", "Something went wrong while we extracted connection details from binder.", e);
                    }
                } else if (ACTION_STOP.equals(intent.getAction())) {

                } else if (ACTION_PREFERENCES_CHANGED.equals(intent.getAction())) {

                }
            }
        };
        callback = (sfc, surfaceWidth, surfaceHeight, screenWidth, screenHeight) -> {
            int frameRate = (int) ((getXwindowView().getDisplay() != null) ? getXwindowView().getDisplay().getRefreshRate() : 30);
            mInputHandler.handleHostSizeChanged(surfaceWidth, surfaceHeight);
            mInputHandler.handleClientSizeChanged(screenWidth, screenHeight);
            if (!WindowManager.ALREADY_SET_SCREEN_SIZE) {
                WindowManager.ALREADY_SET_SCREEN_SIZE = true;
                XwindowView.sendWindowChange(screenWidth, screenHeight, frameRate);
            }
            WindowAttribute attribute = (WindowAttribute) getXwindowView().getTag(R.id.WINDOW_ARRTRIBUTE);
            if (service != null) {
                try {
                    if (attribute != null) {
                        service.windowChanged(sfc, attribute.getOffsetX(), attribute.getOffsetY(),
                                attribute.getWidth(), attribute.getHeight(), attribute.getIndex(),
                                attribute.getWindowPtr());
                    } else {
                        service.windowChanged(sfc, 0, 0,
                                screenWidth, screenHeight, 0,
                                0);
                    }
                } catch (RemoteException e) {
                    e.printStackTrace();
                }
            }
        };
        getXwindowView().setCallback(callback);
        registerReceiver(receiver, new IntentFilter(ACTION_START));
    }

    private void initXwindowView() {
        mWindowAttribute = getIntent().getParcelableExtra(WINDOW_ATTRIBUTE);
        if (mWindowAttribute != null) {
            mIndex = mWindowAttribute.getIndex();
            WINDOW_PTR = mWindowAttribute.getWindowPtr();
        }
        getWindow().setFlags(FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS | FLAG_KEEP_SCREEN_ON | FLAG_TRANSLUCENT_STATUS, 0);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.linux_activity);
        frm = findViewById(R.id.frame);

        XwindowView xwindowView = findViewById(R.id.xwindowview);
        xwindowView.updateCoordinate(mWindowAttribute);
        View xwindowViewParent = (View) xwindowView.getParent();
        xwindowView.setTag(R.id.WINDOW_ARRTRIBUTE, mWindowAttribute);
        mInputHandler = new TouchInputHandler(this,  new TouchInputHandler.RenderStub.NullStub() {
            @Override
            public void swipeDown() {
            }
        }, new InputEventSender(xwindowView));
        xwindowViewParent.setOnTouchListener((v, e) -> mInputHandler.handleTouchEvent(xwindowViewParent, xwindowView, e));
        xwindowViewParent.setOnHoverListener((v, e) -> mInputHandler.handleTouchEvent(xwindowViewParent, xwindowView, e));
        xwindowViewParent.setOnGenericMotionListener((v, e) -> mInputHandler.handleTouchEvent(xwindowViewParent, xwindowView, e));
        xwindowView.setOnCapturedPointerListener((v, e) -> mInputHandler.handleTouchEvent(xwindowView, xwindowView, e));
        xwindowViewParent.setOnCapturedPointerListener((v, e) -> mInputHandler.handleTouchEvent(xwindowView, xwindowView, e));

    }

    void clientConnectedStateChanged(boolean connected) {
        runOnUiThread(() -> {
            mClientConnected = connected;
            findViewById(R.id.stub).setVisibility(connected ? View.INVISIBLE : View.VISIBLE);
            getXwindowView().setVisibility(connected ? View.VISIBLE : View.INVISIBLE);
            getXwindowView().regenerate();
            // We should recover connection in the case if file descriptor for some reason was broken...
            if (!connected) {
                tryConnect();
            }
            if (connected && mIndex != 0) {
//                getLorieView().setPointerIcon(PointerIcon.getSystemIcon(this, PointerIcon.TYPE_NULL));
            }
        });
    }

    void onReceiveConnection() {
        try {
            if (service != null && service.asBinder().isBinderAlive()) {
                tryConnect();
            }
        } catch (Exception e) {
            Log.e("MainActivity", "Something went wrong while we were establishing connection", e);
        }
    }

    void tryConnect() {
        if (mClientConnected)
            return;
        try {
            Log.v("LorieBroadcastReceiver", "Extracting X connection socket.");
            ParcelFileDescriptor fd = service == null ? null : service.getXConnection();
            if (fd != null) {
                XwindowView.connect(fd.detachFd());
                getXwindowView().triggerCallback();
                clientConnectedStateChanged(true);
                XwindowView.setClipboardSyncEnabled(PreferenceManager.getDefaultSharedPreferences(this).getBoolean("clipboardSync", false));
            } else{
                handler.postDelayed(this::tryConnect, 500);
            }
        } catch (Exception e) {
            Log.e("MainActivity", "Something went wrong while we were establishing connection", e);
            service = null;
            // We should reset the View for the case if we have sent it's surface to the client.
            getXwindowView().regenerate();
        }
    }

    private void checkXEvents() {
//        getLorieView().handleXEvents();
//        handler.postDelayed(this::checkXEvents, 300);
    }


    public XwindowView getXwindowView() {
        return findViewById(R.id.xwindowview);
    }

    protected long getWindowId() {
        return WINDOW_PTR;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        Window window = getWindow();
        View decorView = window.getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        window.addFlags(FLAG_KEEP_SCREEN_ON);
        if (hasFocus) {
            getXwindowView().regenerate();
        }
        getXwindowView().requestFocus();


    }

    public void onResume() {
        super.onResume();
        getXwindowView().requestFocus();
        String hexString = Long.toHexString(getWindowId());
        Log.d(TAG, String.format("onResume() called: 0x%s", hexString));
    }


//    @NonNull
//    @Override
//    public WindowInsets onApplyWindowInsets(@NonNull View v, @NonNull WindowInsets insets) {
//        handler.postDelayed(() -> getXwindowView().triggerCallback(), 100);
//        return insets;
//    }

    @Override
    protected void onDestroy() {
        unregisterReceiver(receiver);
        super.onDestroy();
        try {
            service.unregisterListener(mIndex, listener);
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }
}
