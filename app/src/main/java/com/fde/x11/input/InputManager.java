package com.fde.x11.input;
import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.util.Log;
import android.view.InputEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public class InputManager {

    public static final int INJECT_INPUT_EVENT_MODE_ASYNC = 0;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT = 1;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH = 2;
    private static final String TAG = "InputManager";

    private final Object manager;
    private Method injectInputEventMethod;

    private static Method setDisplayIdMethod;
    private static Method setActionButtonMethod;

    public static InputManager create() {
        try {
            Class<?> inputManagerClass = getInputManagerClass();
            Method getInstanceMethod = inputManagerClass.getDeclaredMethod("getInstance");
            Object im = getInstanceMethod.invoke(null);
            return new InputManager(im);
        } catch (ReflectiveOperationException e) {
            throw new AssertionError(e);
        }
    }

    public void setPointerIcon(Bitmap bitmap, int xhot, int yhot){
        Log.d(TAG, "setPointerIcon() called with: bitmap = [" + bitmap + "], xhot = [" + xhot + "], yhot = [" + yhot + "]");
        PointerIcon pointerIcon = PointerIcon.create(bitmap, xhot, yhot);
        try {
            Method setCustomPointerIcon = manager.getClass().getMethod("setCustomPointerIcon", PointerIcon.class);
            setCustomPointerIcon.invoke(manager, pointerIcon);
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(e);
        } catch (InvocationTargetException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }

    }

    private static Class<?> getInputManagerClass() {
        try {
            // Parts of the InputManager class have been moved to a new InputManagerGlobal class in Android 14 preview
            return Class.forName("android.hardware.input.InputManagerGlobal");
        } catch (ClassNotFoundException e) {
            return android.hardware.input.InputManager.class;
        }
    }

    private InputManager(Object manager) {
        this.manager = manager;
    }

    private Method getInjectInputEventMethod() throws NoSuchMethodException {
        if (injectInputEventMethod == null) {
            injectInputEventMethod = manager.getClass().getMethod("injectInputEvent", InputEvent.class, int.class);
        }
        return injectInputEventMethod;
    }

    public boolean injectInputEvent(InputEvent inputEvent, int mode) {
        try {
            Method method = getInjectInputEventMethod();
            return (boolean) method.invoke(manager, inputEvent, mode);
        } catch (ReflectiveOperationException e) {
            return false;
        }
    }

    private static Method getSetDisplayIdMethod() throws NoSuchMethodException {
        if (setDisplayIdMethod == null) {
            setDisplayIdMethod = InputEvent.class.getMethod("setDisplayId", int.class);
        }
        return setDisplayIdMethod;
    }

    public static boolean setDisplayId(InputEvent inputEvent, int displayId) {
        try {
            Method method = getSetDisplayIdMethod();
            method.invoke(inputEvent, displayId);
            return true;
        } catch (ReflectiveOperationException e) {
            return false;
        }
    }

    private static Method getSetActionButtonMethod() throws NoSuchMethodException {
        if (setActionButtonMethod == null) {
            setActionButtonMethod = MotionEvent.class.getMethod("setActionButton", int.class);
        }
        return setActionButtonMethod;
    }

    public static boolean setActionButton(MotionEvent motionEvent, int actionButton) {
        try {
            Method method = getSetActionButtonMethod();
            method.invoke(motionEvent, actionButton);
            return true;
        } catch (ReflectiveOperationException e) {
            return false;
        }
    }
}
