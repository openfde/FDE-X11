package com.termux.x11;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

public class RightClickView extends LinearLayout {


    private RightClickListener listener;

    public void setListener(RightClickListener listener) {
        this.listener = listener;
    }

    private static final String TAG = "RightClickView";

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        Log.d(TAG, "onTouchEvent() called with: event = [" + event + "]");
        return super.onTouchEvent(event);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            if (event.getAction() == MotionEvent.ACTION_CANCEL && listener != null && event.getButtonState() != MotionEvent.BUTTON_PRIMARY) {
                listener.onRightClick(true, event);
            } else if (event.getAction() == MotionEvent.ACTION_UP && event.getButtonState() == 0) {
                listener.onRightClick(false, event);
            } else {
            }
        } else if (event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER) {
            if (event.getAction() == MotionEvent.ACTION_UP && event.getButtonState() == 0) {
                listener.onRightClick(false, event);
            }
        }
        Log.d(TAG, "dispatchTouchEvent() called with: event = [" + event + "]");
        return super.dispatchTouchEvent(event);
    }

    public RightClickView(Context context) {
        super(context);
    }

    public RightClickView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public RightClickView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public RightClickView(Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    public interface RightClickListener {
        void onRightClick(boolean b, MotionEvent event);
    }
}
