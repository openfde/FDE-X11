package com.termux.x11.data;

import java.io.Serializable;
import java.util.List;

public class VncResult implements Serializable {

    public int code;
    public String message;

    public static class AppResult extends VncResult implements Serializable {

        @Override
        public String toString() {
            return "AppResult{" +
                    "data=" + data +
                    '}';
        }



        public List<App> data;

        public static class App {
            public String Type;
            public String Path;
            //base64
            public String Icon;
            public String IconPath;
            public String IconType;
            //显示名称，请求名称
            public String Name;
            public String ZhName;

// ------------------------------------- 自已定义
            //端口
            public int port;
            //序号
            public int id;

            @Override
            public String toString() {
                return "App{" +
                        "Type='" + Type + '\'' +
                        ", Path='" + Path + '\'' +
                        ", Icon='" + Icon + '\'' +
                        ", IconPath='" + IconPath + '\'' +
                        ", IconType='" + IconType + '\'' +
                        ", Name='" + Name + '\'' +
                        ", ZhName='" + ZhName + '\'' +
                        ", id='" + id + '\'' +
                        ", port='" + port + '\'' +
                        '}';
            }
        }
    }

    public static class GetPortResult extends VncResult implements Serializable {

        public PortResult Data;

        public static class PortResult {
            public String Port;
        }

    }
}
