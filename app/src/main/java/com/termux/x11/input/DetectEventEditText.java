package com.termux.x11.input;

import static android.text.InputType.TYPE_TEXT_FLAG_CAP_WORDS;
import static android.view.KeyEvent.KEYCODE_0;
import static android.view.KeyEvent.KEYCODE_1;
import static android.view.KeyEvent.KEYCODE_9;
import static android.view.KeyEvent.KEYCODE_NUMPAD_0;
import static android.view.KeyEvent.KEYCODE_NUMPAD_1;
import static android.view.KeyEvent.KEYCODE_NUMPAD_9;


import android.app.Activity;
import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.method.KeyListener;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.EditText;

import com.termux.x11.BuildConfig;


public class DetectEventEditText extends EditText {
    private static final String TAG = "DetectText_ime";
    private static final boolean DEBUG  = false; // BuildConfig.DEBUG;
    private DetectInputConnection detectInputConnection;
    private TouchInputHandler mInputHandler;

    public DetectEventEditText(Context context) {
        super(context);
        setKeyListener(getDefaultKeyListener());
    }

    public DetectEventEditText(Context context,
                               AttributeSet attrs) {
        super(context, attrs);
        setKeyListener(getDefaultKeyListener());
    }

    public DetectEventEditText(Context context,
                               AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setKeyListener(getDefaultKeyListener());
    }


    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        super.onCreateInputConnection(outAttrs);
        detectInputConnection = new DetectInputConnection(this);
        outAttrs.initialSelStart = getSelectionStart();
        outAttrs.initialSelEnd = getSelectionEnd();
        outAttrs.initialCapsMode = detectInputConnection.getCursorCapsMode(getInputType());
        if(mInputHandler != null){
            setInputHandler(mInputHandler);
        }
        return detectInputConnection;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if(DEBUG){
            Log.d(TAG, "onKeyPreIme() called with: keyCode = [" + keyCode + "], event = [" + event + "]");
        }
        return super.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean dispatchKeyEventPreIme(KeyEvent event) {
        Log.d(TAG, "dispatchKeyEventPreIme() called with: event = [" + event + "]");
        return super.dispatchKeyEventPreIme(event);
    }

    private KeyListener getDefaultKeyListener() {
        return new KeyListener() {
            @Override
            public int getInputType() {
                return TYPE_TEXT_FLAG_CAP_WORDS;
            }

            @Override
            public boolean onKeyDown(View view, Editable text, int keyCode, KeyEvent event) {
                Log.d(TAG, "onKeyDown text = [" + text + "], keyCode = [" + keyCode + "], event = [" + event + "]");
                return false;
            }

            //FIXME never called
            @Override
            public boolean onKeyUp(View view, Editable text, int keyCode, KeyEvent event) {
                return false;
            }

            @Override
            public boolean onKeyOther(View view, Editable text, KeyEvent event) {
                Log.d(TAG, "onKeyOther text = [" + text + "], event = [" + event + "]");
                return false;
            }

            @Override
            public void clearMetaKeyState(View view, Editable content, int states) {
            }
        };
    }

    public void setInputHandler(TouchInputHandler inputHandler) {
        this.mInputHandler = inputHandler;
        if(detectInputConnection != null ){
            detectInputConnection.setInputHandler(inputHandler);
        }
    }
}