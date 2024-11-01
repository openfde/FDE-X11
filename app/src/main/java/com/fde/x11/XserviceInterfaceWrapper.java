package com.fde.x11;

import android.os.RemoteException;

import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.x11.input.InputStub;
import com.fde.x11.utils.FLog;

public class XserviceInterfaceWrapper implements InputStub {
    private static final String TAG = "XserviceWrapper";
    ICmdEntryInterface service;
    WindowAttribute mAttribute;
    private boolean isAlive = false;

    public WindowAttribute getAttribute() {
        return mCoordinate;
    }

    private WindowAttribute mCoordinate;

    public void updateCoordinate(WindowAttribute coordinate) {
        this.mCoordinate = coordinate;
    }

    public boolean isAlive() {
        if (isAlive) {
            return true;
        }
        isAlive = service != null && service.asBinder().isBinderAlive();
        return isAlive;
    }

    public boolean isAviable(){
        boolean aviable = mAttribute != null && isAlive();
        if(!aviable){
            FLog.a(TAG, "service is not aviable");
        }
        return aviable;
    }

    public XserviceInterfaceWrapper(){
    }

    public void enableService(ICmdEntryInterface service){
        this.service = service;
        isAlive = true;
    }

    public void disableService(){
        service = null;
        isAlive = false;
    }


    public XserviceInterfaceWrapper(ICmdEntryInterface service) {
        this.service = service;
    }

    public void windowChanged(android.view.Surface surface, float x,
                              float y, float w, float h, int index, long p, long window) {
        try {
            if(isAviable()){service.windowChanged(surface, x, y, w, h, index, p, window);}
        }catch (RemoteException e){
            FLog.e(TAG, "windowChanged failed" + e.getMessage());
        }
    }

    public android.os.ParcelFileDescriptor getXConnection(){
        try {
            if(isAviable()){ return service.getXConnection();}
        }catch (RemoteException e){
            FLog.e(TAG, "getXConnection failed" + e.getMessage());
        }
        return null;
    }

    public int getConnectedFD() throws android.os.RemoteException{
        return 0;
    }

    public void closeWindow(int index, long p, long window){
        try {
            if(isAviable()){service.closeWindow(index, p, window);}
        }catch (RemoteException e){
            FLog.e(TAG, "closeWindow failed" + e.getMessage());
        }
    }

    //    ParcelFileDescriptor getLogcatOutput();
    public void configureWindow(long p, long window,
                                int x, int y, int w, int h) {
        try {
            if(isAviable()){service.configureWindow(p, window, x, y, w, h);}
        }catch (RemoteException e){
            FLog.e(TAG, "configureWindow failed" + e.getMessage());
        }
    }

    public void moveWindow(long p, long window, int x, int y) throws android.os.RemoteException{

    }

    public void resizeWindow(long window, int w, int h) throws android.os.RemoteException{

    }

    public void raiseWindow(long window){
        try {
            if(isAviable()){service.raiseWindow(window);}
        }catch (RemoteException e){
            FLog.e(TAG, "raiseWindow failed" + e.getMessage());
        }
    }

    public void circulaSubWindows(long window, boolean lowest) throws android.os.RemoteException{

    }

    public void sendClipText(java.lang.String cliptext) {
        try {
            if(isAviable()){service.sendClipText(cliptext);}
        }catch (RemoteException e){
            FLog.e(TAG, "sendClipText failed" + e.getMessage());
        }
    }

    @Override
    public void sendMouseEvent(float x, float y, int whichButton, boolean buttonDown, boolean relative, int index) {
        try {
            if(isAviable()){service.sendMouseEvent(x, y, whichButton, buttonDown, relative, index);}
        }catch (RemoteException e){
            FLog.e(TAG, "sendClipText failed" + e.getMessage());
        }
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
}
