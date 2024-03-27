package com.termux.x11;

import com.termux.x11.IReceive;

// This interface is used by utility on termux side.
interface ICmdEntryInterface {
    void windowChanged(in Surface surface, long id);
    ParcelFileDescriptor getXConnection();
    void registerListener(IReceive receiver);
    void unregisterListener(IReceive receiver);
//    ParcelFileDescriptor getLogcatOutput();
}