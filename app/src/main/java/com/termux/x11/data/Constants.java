/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.termux.x11.data;

/**
 * Keys for intent values
 */
public class Constants {

    public static String BASIP = "127.0.0.1";
    public static String BASEURL = "http://" + BASIP + ":18080";
    public static final String URL_GETALLAPP = "/api/v1/apps";
    public static final String URL_STARTAPP = "/api/v1/vnc";
    public static final String URL_STOPAPP = "/api/v1/vnc";

    public static final String URL_STARTAPP_X = "/api/v1/xserver";

    public static final int DISPLAY_GLOBAL =  1000;
    public static final String DISPLAY_GLOBAL_PARAM = ":" + DISPLAY_GLOBAL;



    public static final String URL_KILLAPP = "/api/v1/stop_vnc";

    public static String app = null;

    public static final String SURFFIX_SVG = ".svg";
    public static final String SURFFIX_SVGZ = ".svgz";
    public static final String SURFFIX_PNG = ".png";

    public static final String APP_TITLE_PREFIX = "Fusion Linux Application";
}
