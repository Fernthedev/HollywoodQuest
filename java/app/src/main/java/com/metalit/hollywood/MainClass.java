package com.metalit.hollywood;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Field;

import com.unity3d.player.UnityPlayer;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.view.WindowManager;

public class MainClass {
    public static int DBG = 3;
    public static int INF = 0;
    public static int WRN = 1;
    public static int ERR = 2;
    public static int CRIT = 4;

    public static native void log(int level, String message);

    public static void logE(String message, Throwable e) {
        StringWriter stringWriter = new StringWriter();
        PrintWriter printWriter = new PrintWriter(stringWriter);
        e.printStackTrace(printWriter);
        log(ERR, message + stringWriter.toString());
    }

    private Activity activity;
    private Context context;

    public MainClass() {
        log(DBG, "Initializing MainClass");

        // activity = UnityPlayer.currentActivity;
        // context = getUnityPlayer().getContext();
    }

    private static UnityPlayer getUnityPlayer() {
        Activity activity = UnityPlayer.currentActivity;
        Class<?> cls = activity.getClass();
        try {
            Field field = cls.getDeclaredField("mUnityPlayer");
            field.setAccessible(true);
            return (UnityPlayer) field.get(activity);
        } catch (Throwable e) {
            throw new AssertionError(e);
        }
    }

    public void setScreenOn(boolean value) {
        String action = value ? "com.oculus.vrpowermanager.prox_close" : "com.oculus.vrpowermanager.automation_disable";
        context.sendBroadcast(new Intent(action));

        log(INF, "Set proximity sensor enabled: " + !value);

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (value)
                    activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else
                    activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                log(INF, "Set screen on flag: " + value);
            }
        });
    }

    public void setMute(boolean value) {
        AudioManager audioManager = context.getSystemService(AudioManager.class);
        int adjust = value ? AudioManager.ADJUST_MUTE : AudioManager.ADJUST_UNMUTE;
        audioManager.adjustStreamVolume(AudioManager.STREAM_MUSIC, adjust, AudioManager.FLAG_REMOVE_SOUND_AND_VIBRATE);

        log(INF, "Adjusted mute: " + value);
    }
}
