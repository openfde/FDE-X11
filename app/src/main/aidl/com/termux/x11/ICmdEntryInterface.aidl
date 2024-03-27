package com.termux.x11;

// This interface is used by utility on termux side.
interface ICmdEntryInterface {
    void windowChanged(in Surface surface, long id);
    ParcelFileDescriptor getXConnection();
//    ParcelFileDescriptor getLogcatOutput();
}