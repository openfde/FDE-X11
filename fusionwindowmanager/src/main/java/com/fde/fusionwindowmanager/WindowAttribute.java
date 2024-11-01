package com.fde.fusionwindowmanager;

import android.graphics.Rect;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class WindowAttribute implements Parcelable {
    
    float offsetX;
    float offsetY;
    float width;
    float height;
    int index;
    long windowPtr;

    long XID;

    long taskTo = 0;

    boolean focusable = true;
    Property property;


    public boolean isFocusable() {
        return focusable;
    }

    public void setFocusable(boolean focusable) {
        this.focusable = focusable;
    }

    public WindowAttribute(int index, long p, long window) {
        this.offsetX = 0;
        this.offsetY = 0;
        this.width = 0;
        this.height = 0;
        this.index = index;
        this.windowPtr = p;
        this.XID = window;
    }

    public WindowAttribute(int x, int y, int w, int h, int index, long p, long window, long taskTo) {
        this.offsetX = x;
        this.offsetY = y;
        this.width = w;
        this.height = h;
        this.index = index;
        this.windowPtr = p;
        this.XID = window;
        this.taskTo = taskTo;
    }

    public WindowAttribute(int x, int y, int w, int h, int index, long p, long window, long taskTo, Property property) {
        this.offsetX = x;
        this.offsetY = y;
        this.width = w;
        this.height = h;
        this.index = index;
        this.windowPtr = p;
        this.XID = window;
        this.taskTo = taskTo;
        this.property = property;
    }

    public Property getProperty() {
        return property;
    }

    public void setProperty(Property property) {
        this.property = property;
    }

    public long getXID() {
        return XID;
    }

    public void setXID(long XID) {
        this.XID = XID;
    }

    public float getOffsetX() {
        return offsetX;
    }

    public void setOffsetX(float offsetX) {
        this.offsetX = offsetX;
    }

    public float getOffsetY() {
        return offsetY;
    }

    public void setOffsetY(float offsetY) {
        this.offsetY = offsetY;
    }

    public float getWidth() {
        return width;
    }

    public void setWidth(float width) {
        this.width = width;
    }

    public float getHeight() {
        return height;
    }

    public void setHeight(float height) {
        this.height = height;
    }

    public int getIndex() {
        return index;
    }

    public void setIndex(int index) {
        this.index = index;
    }

    public long getWindowPtr() {
        return windowPtr;
    }

    public void setWindowPtr(long windowPtr) {
        this.windowPtr = windowPtr;
    }

    public WindowAttribute(int offsetX, int offsetY, int width, int height, int index, long windowPtr, long window){
        this.offsetX = offsetX;
        this.offsetY = offsetY;
        this.width = width;
        this.height = height;
        this.index = index;
        this.windowPtr = windowPtr;
        this.XID = window;
    }


    protected WindowAttribute(Parcel in) {
        offsetX = in.readFloat();
        offsetY = in.readFloat();
        width = in.readFloat();
        height = in.readFloat();
        index = in.readInt();
        windowPtr = in.readLong();
        XID = in.readLong();
        taskTo = in.readLong();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            focusable = in.readBoolean();
        }
    }

    public long getTaskTo() {
        return taskTo;
    }

    public void setTaskTo(long taskTo) {
        this.taskTo = taskTo;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
            dest.writeFloat(offsetX);
            dest.writeFloat(offsetY);
            dest.writeFloat(width);
            dest.writeFloat(height);
            dest.writeInt(index);
            dest.writeLong(windowPtr);
            dest.writeLong(XID);
            dest.writeLong(taskTo);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            dest.writeBoolean(focusable);
        }
    }

    public static final Creator<WindowAttribute> CREATOR = new Creator<WindowAttribute>() {
        @Override
        public WindowAttribute createFromParcel(Parcel in) {
            return new WindowAttribute(in);
        }

        @Override
        public WindowAttribute[] newArray(int size) {
            return new WindowAttribute[size];
        }
    };


    @NonNull
    @Override
    public String toString() {
        return " -->" +
                " x:" + offsetX +
                " , y:" + offsetY +
                " , w:" + width +
                " , h:" + height +
                " , index:" + index +
//                "\n, windowPtr=" + windowPtr +
                " XID:" + Long.toHexString(XID) +
                " taskTo:" + Long.toHexString(taskTo) +
//                "\n, focusable=" + focusable +
//                "\n, property=" + property +
                "";
    }

    public Rect getRect() {
        return new Rect((int) offsetX, (int) offsetY, (int) (offsetX + width), (int) (offsetY + height));
    }

    public void setRect(Rect rect) {
        this.offsetX = rect.left;
        this.offsetY = rect.top;
        this.width = rect.right - rect.left;
        this.height = rect.bottom - rect.top;
    }
}
