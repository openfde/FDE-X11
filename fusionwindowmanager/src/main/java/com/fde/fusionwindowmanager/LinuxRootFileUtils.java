package com.fde.fusionwindowmanager;

import android.content.Context;
import android.util.Log;

import com.google.gson.Gson;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;

public class LinuxRootFileUtils {
    private static String linuxRootPath;

    public static String getLinuxRootFileName(Context context) {
        try {
            File file = new File("/volumes/.fde_path_key");
            if (!file.exists()) {
                return null;
            }

            // 读取文件内容
            String jsonString = new String(Files.readAllBytes(Paths.get(file.getPath())));

            // 解析 JSON
            Gson gson = new Gson();
            VolumeInfo[] volumes = gson.fromJson(jsonString, VolumeInfo[].class);

            // 查找根路径 "/" 对应的 UUID
            for (VolumeInfo volume : volumes) {
                if ("/".equals(volume.Path)) {
                    linuxRootPath = "/volumes/" + volume.UUID;
                    return linuxRootPath;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    public static String getUserName(Context context){
        String homeDir = getLinuxRootFileName(context) + File.separator + "home";
        Log.d("TAG", "getUserName: " + homeDir);
        File homeFile = new File(homeDir);
        if(!homeFile.exists()){
            return null;
        }
        File[] files = new File(homeDir).listFiles();
        if(files.length == 0){
            return null;
        }
        for (File file: files){
            if(!file.isHidden()){
                return file.getName();
            }
        }
        return null;
    }
}


// VolumeInfo 类（假设结构）
class VolumeInfo {
    public String Path;
    public String UUID;
}
