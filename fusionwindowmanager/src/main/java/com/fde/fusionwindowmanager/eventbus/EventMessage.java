package com.fde.fusionwindowmanager.eventbus;

import com.fde.fusionwindowmanager.WindowAttribute;

public class EventMessage {


    private WindowAttribute windowAttribute;

    private EventType type;
    private String message;

    public EventMessage(EventType type, String message, WindowAttribute windowAttribute) {
        this.type = type;
        this.message = message;
        this.windowAttribute = windowAttribute;
    }

    public EventType getType() {
        return type;
    }

    public void setType(EventType type) {
        this.type = type;
    }

    public String getMessage() {
        return message;
    }

    public void setMessage(String message) {
        this.message = message;
    }

    public WindowAttribute getWindowAttribute() {
        return windowAttribute;
    }

    public void setWindowAttribute(WindowAttribute windowAttribute) {
        this.windowAttribute = windowAttribute;
    }

    @Override
    public String toString() {
        return "type=" + type + "--message= " + message + "--windowAttribute= " + windowAttribute;
    }
}
