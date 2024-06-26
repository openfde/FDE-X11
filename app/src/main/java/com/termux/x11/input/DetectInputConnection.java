package com.termux.x11.input;


import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.method.KeyListener;
import android.text.style.SuggestionSpan;
import android.util.Log;
import android.view.KeyEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputMethodManager;
import android.widget.TextView;

import com.termux.x11.BuildConfig;
import com.termux.x11.utils.Reflector;

public class DetectInputConnection extends BaseInputConnection {
    private static final boolean DEBUG = false ; // BuildConfig.DEBUG;
    private static final String TAG ="DetectConnection_ime";
    private final DetectEventEditText detectEventEditText;
    private int mBatchEditNesting;
    private final InputMethodManager mIMM;
    private int mInputModeFlag;
    private TouchInputHandler mInputHandler;

    public DetectInputConnection(DetectEventEditText textview) {
        super(textview, true);
        detectEventEditText = textview;
        mIMM = (InputMethodManager) textview.getContext()
                .getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    @Override
    public Editable getEditable() {
        TextView tv = detectEventEditText;
        if (tv != null) {
            return tv.getEditableText();
        }
        return null;
    }

    @Override
    public boolean beginBatchEdit() {
        if (DEBUG) {
            Log.d(TAG, "beginBatchEdit() called");
        }
        synchronized (this) {
            if (mBatchEditNesting >= 0) {
                detectEventEditText.beginBatchEdit();
                mBatchEditNesting++;
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean endBatchEdit() {
        if (DEBUG) {
            Log.d(TAG, "endBatchEdit() called");
        }
        synchronized (this) {
            if (mBatchEditNesting > 0) {
                // When the connection is reset by the InputMethodManager and reportFinish
                // is called, some endBatchEdit calls may still be asynchronously received from the
                // IME. Do not take these into account, thus ensuring that this IC's final
                // contribution to mTextView's nested batch edit count is zero.
                detectEventEditText.endBatchEdit();
                mBatchEditNesting--;
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean clearMetaKeyStates(int states) {
        if (DEBUG) {
            Log.d(TAG, "clearMetaKeyStates() called with: states = [" + states + "]");
        }
        Editable content = getEditable();
        if (content == null) {
            return false;
        }
        KeyListener kl = detectEventEditText.getKeyListener();
        if (kl != null) {
            try {
                kl.clearMetaKeyState(detectEventEditText, content, states);
            } catch (AbstractMethodError e) {
                // This is an old listener that doesn't implement the
                // new method.
            }
        }
        return true;
    }

    @Override
    public boolean commitCompletion(CompletionInfo text) {
        if (DEBUG) {
            Log.v(TAG, "commitCompletion " + text);
        }
        detectEventEditText.beginBatchEdit();
        detectEventEditText.onCommitCompletion(text);
        detectEventEditText.endBatchEdit();
        return true;
    }

    /**
     * Calls the {@link TextView#onCommitCorrection} method of the associated TextView.
     */
    @Override
    public boolean commitCorrection(CorrectionInfo correctionInfo) {
        if (DEBUG) {
            Log.v(TAG, "commitCorrection" + correctionInfo);
        }
        detectEventEditText.beginBatchEdit();
        detectEventEditText.onCommitCorrection(correctionInfo);
        detectEventEditText.endBatchEdit();
        return true;
    }

    @Override
    public boolean performEditorAction(int actionCode) {
        if (DEBUG) {
            Log.v(TAG, "performEditorAction " + actionCode);
        }
        detectEventEditText.onEditorAction(actionCode);
        return true;
    }

    @Override
    public boolean performContextMenuAction(int id) {
        if (DEBUG) {
            Log.v(TAG, "performContextMenuAction " + id);
        }
        detectEventEditText.beginBatchEdit();
        detectEventEditText.onTextContextMenuItem(id);
        detectEventEditText.endBatchEdit();
        return true;
    }

    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
        if (DEBUG) {
            Log.d(TAG, "getExtractedText() called with: request = [" + request + "], flags = [" + flags + "]");
        }
        if (detectEventEditText != null) {
            ExtractedText et = new ExtractedText();
            if (detectEventEditText.extractText(request, et)) {
                if ((flags & GET_EXTRACTED_TEXT_MONITOR) != 0) {
                    Reflector.invokeMethodExceptionSafe(detectEventEditText, "setExtracting",
                            new Reflector.TypedObject(request, ExtractedTextRequest.class));
                }
                return et;
            }
        }
        return null;
    }

    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        if(DEBUG){
            Log.d(TAG, "sendKeyEvent() called with: event = [" + event + "]");
        }
        return super.sendKeyEvent(event);
    }

    @Override
    public boolean performPrivateCommand(String action, Bundle data) {
        if(DEBUG){
            Log.d(TAG, "performPrivateCommand() called with: action = [" + action + "], data = [" + data + "]");
        }
        detectEventEditText.onPrivateIMECommand(action, data);
        return true;
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if(DEBUG){
            Log.d(TAG, "commitText() called with: text = [" + text + "], mTextView = [" + detectEventEditText + "]");
        }
        if (detectEventEditText == null) {
            return super.commitText(text, newCursorPosition);
        }
        if (text instanceof Spanned) {
            Spanned spanned = (Spanned) text;
            SuggestionSpan[] spans = spanned.getSpans(0, text.length(), SuggestionSpan.class);
            Reflector.invokeMethodExceptionSafe(mIMM, "registerSuggestionSpansForNotification",
                    new Reflector.TypedObject(spans, SuggestionSpan[].class));
        }
        Reflector.invokeMethodExceptionSafe(detectEventEditText, "resetErrorChangedFlag");
        Reflector.invokeMethodExceptionSafe(detectEventEditText, "hideErrorIfUnchanged");
        if(!TextUtils.isEmpty(text)){
            mInputHandler.sendKeyEvent(text);
        }
        return true;
    }

    public void setInputHandler(TouchInputHandler inputHandler) {
        this.mInputHandler = inputHandler;
    }
}
