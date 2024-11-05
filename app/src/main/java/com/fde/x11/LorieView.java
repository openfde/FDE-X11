package com.fde.x11;

import static com.fde.x11.utils.AppUtils.CONTENT_HEIGHT;
import static com.fde.x11.utils.AppUtils.GLOBAL_SCREEN_WIDTH;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.opengl.GLES10;
import android.opengl.GLES20;
import android.opengl.GLES30;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.x11.input.InputStub;

import java.util.regex.PatternSyntaxException;

@SuppressLint("WrongConstant")
@SuppressWarnings("deprecation")
public class LorieView extends SurfaceView implements InputStub {

    private static final String TAG = "LorieView";

    public WindowAttribute getAttribute() {
        return mCoordinate;
    }

    private WindowAttribute mCoordinate;

    public void updateCoordinate(WindowAttribute coordinate) {
        this.mCoordinate = coordinate;
    }

    interface Callback {
        void changed(Surface sfc, int surfaceWidth, int surfaceHeight, int screenWidth, int screenHeight);
    }

//    interface PixelFormat {
//        int BGRA_8888 = 5; // Stands for HAL_PIXEL_FORMAT_BGRA_8888
//    }

    private Callback mCallback;
    private final Point p = new Point();
    private final SurfaceHolder.Callback mSurfaceCallback = new SurfaceHolder.Callback() {
        @Override public void surfaceCreated(@NonNull SurfaceHolder holder) {
            holder.setFormat(PixelFormat.TRANSLUCENT);
//            Log.d(TAG, "surfaceCreated: holder:" + holder + "");
            String version = GLES10.glGetString(GLES10.GL_VERSION);
            Log.d(TAG, "egl version: " + version);

        }

        @Override public void surfaceChanged(@NonNull SurfaceHolder holder, int f, int width, int height) {
            width = getMeasuredWidth();
            height = getMeasuredHeight();

            Log.d(TAG, "surfaceChanged: holder:" + holder + ", f:" + f + ", width:" + width + ", height:" + height + "");

            if (mCallback == null || width == 0 || height == 0)
                return;

            getDimensionsFromSettings();
            mCallback.changed(holder.getSurface(), GLOBAL_SCREEN_WIDTH,
                    CONTENT_HEIGHT, GLOBAL_SCREEN_WIDTH , CONTENT_HEIGHT);
        }

        @Override public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
//            Log.d(TAG, "surfaceDestroyed: holder:" + holder + "");
            if (mCallback != null){
//                mCallback.changed(holder.getSurface(), 0, 0, 0, 0);
            }
        }
    };

    public LorieView(Context context) { super(context); init(); }
    public LorieView(Context context, AttributeSet attrs) { super(context, attrs); init(); }
    public LorieView(Context context, AttributeSet attrs, int defStyleAttr) { super(context, attrs, defStyleAttr); init(); }
    @SuppressWarnings("unused")
    public LorieView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) { super(context, attrs, defStyleAttr, defStyleRes); init(); }

    private void init() {
        getHolder().addCallback(mSurfaceCallback);
    }

    public void setCallback(Callback callback) {
        mCallback = callback;
        triggerCallback();
    }

    public void regenerate() {
        Callback callback = mCallback;
        mCallback = null;
        getHolder().setFormat(android.graphics.PixelFormat.TRANSLUCENT);
        mCallback = callback;

        triggerCallback();
    }

    public void triggerCallback() {
//        setFocusable(true);
//        setFocusableInTouchMode(true);
//        requestFocus();

        setBackground(new ColorDrawable(Color.TRANSPARENT) {
            public boolean isStateful() {
                return true;
            }
            public boolean hasFocusStateSpecified() {
                return true;
            }
        });

        Rect r = getHolder().getSurfaceFrame();
        getActivity().runOnUiThread(() -> mSurfaceCallback.surfaceChanged(getHolder(), PixelFormat.TRANSLUCENT, r.width(), r.height()));
    }

    private Activity getActivity() {
        Context context = getContext();
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity) context;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }

        throw new NullPointerException();
    }

    void getDimensionsFromSettings() {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(getContext());
        int width = getMeasuredWidth();
        int height = getMeasuredHeight();
        int w = width;
        int h = height;
        switch(preferences.getString("displayResolutionMode", "native")) {
            case "scaled": {
                int scale = preferences.getInt("displayScale", 100);
                w = width * 100 / scale;
                h = height * 100 / scale;
                break;
            }
            case "exact": {
                String[] resolution = preferences.getString("displayResolutionExact", "1280x1024").split("x");
                w = Integer.parseInt(resolution[0]);
                h = Integer.parseInt(resolution[1]);
                break;
            }
            case "custom": {
                try {
                    String[] resolution = preferences.getString("displayResolutionCustom", "1280x1024").split("x");
                    w = Integer.parseInt(resolution[0]);
                    h = Integer.parseInt(resolution[1]);
                } catch (NumberFormatException | PatternSyntaxException ignored) {
                    w = 1280;
                    h = 1024;
                }
                break;
            }
        }

        if ((width < height && w > h) || (width > height && w < h))
            p.set(h, w);
        else
            p.set(w, h);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(getContext());
        if (preferences.getBoolean("displayStretch", false)
              || "native".equals(preferences.getString("displayResolutionMode", "native"))
              || "scaled".equals(preferences.getString("displayResolutionMode", "native"))) {
            getHolder().setSizeFromLayout();
            return;
        }

        getDimensionsFromSettings();

        if (p.x <= 0 || p.y <= 0)
            return;

        int width = getMeasuredWidth();
        int height = getMeasuredHeight();

        if ((width < height && p.x > p.y) || (width > height && p.x < p.y))
            //noinspection SuspiciousNameCombination
            p.set(p.y, p.x);

        if (width > height * p.x / p.y)
            width = height * p.x / p.y;
        else
            height = width * p.y / p.x;

        getHolder().setFixedSize(p.x, p.y);
        setMeasuredDimension(width, height);
    }

    @Override
    public void sendMouseWheelEvent(float deltaX, float deltaY) {
        sendMouseEvent(deltaX, deltaY, BUTTON_SCROLL, false, true, getAttribute() == null ? 0 : getAttribute().getIndex());
    }

    @Override
    public boolean dispatchKeyEventPreIme(KeyEvent event) {
        Log.d(TAG, "dispatchKeyEventPreIme: event:" + event + "");
        Activity a = getActivity();
        return (a instanceof MainActivity) && ((MainActivity) a).handleKey(event);
    }

    // It is used in native code
    void setClipboardText(String text) {
        ClipboardManager clipboard = (ClipboardManager) getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newPlainText("X11 clipboard", text));
    }

    static native void connect(int fd);
    native void handleXEvents();
    static native void startLogcat(int fd);
    static native void setClipboardSyncEnabled(boolean enabled);
    static native void sendWindowChange(int width, int height, int framerate);
    public native void sendMouseEvent(float x, float y, int whichButton, boolean buttonDown, boolean relative, int index);
    public native void sendTouchEvent(int action, int id, int x, int y);
    public native boolean sendKeyEvent(int scanCode, int keyCode, boolean keyDown);
    public native void sendTextEvent(byte[] text);
    public native void sendUnicodeEvent(int code);

    public native void sendClipText(String cliptext);


    static {
        System.loadLibrary("Xlorie");
    }
}
