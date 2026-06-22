// IActivityCallback.aidl
package com.fde.x11;

import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.fusionwindowmanager.Property;

interface IActivityCallback {

    boolean startDecorMovingTask(float startX, float startY, long window);

    void finisDecorMovingTask(long window);

    boolean finishActivity(long window);

    boolean configureActivity(long window);

    boolean fillAndResize(in WindowAttribute attr, in Property prop);
}
