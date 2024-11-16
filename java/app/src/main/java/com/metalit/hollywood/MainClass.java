package com.metalit.hollywood;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import com.unity3d.player.UnityPlayer;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.IBinder;
import android.view.Surface;
import android.view.WindowManager;

public class MainClass {
    public static int DBG = 3;
    public static int INF = 4;
    public static int WRN = 5;
    public static int ERR = 6;
    public static int CRIT = 7;

    public static native void log(int level, String message);

    public static void logE(String message, Throwable e) {
        log(ERR, message + e.toString());
        Throwable cause = e;
        while (cause != null) {
            if (cause != e)
                log(ERR, "Caused by: " + e.toString());
            StringWriter stringWriter = new StringWriter();
            PrintWriter printWriter = new PrintWriter(stringWriter);
            cause.printStackTrace(printWriter);
            log(ERR, stringWriter.toString());
            cause = cause.getCause();
        }
    }

    private Activity activity;
    private Context context;

    private static native Method nativeGetDeclaredMethod(Object recv, String name, Class<?>[] parameterTypes);

    private class Bypass {
        public static Method getDeclaredMethod(Object clazz, String name, Class<?>... args) {
            return nativeGetDeclaredMethod(clazz, name, args);
        }

        public static void unseal() throws Exception {
            Method getRuntime = getDeclaredMethod(
                    Class.forName("dalvik.system.VMRuntime"),
                    "getRuntime");
            getRuntime.setAccessible(true);
            Object vmRuntime = getRuntime.invoke(null);

            Method setHiddenApiExemptions = getDeclaredMethod(
                    vmRuntime.getClass(),
                    "setHiddenApiExemptions",
                    String[].class);
            setHiddenApiExemptions.setAccessible(true);

            setHiddenApiExemptions.invoke(vmRuntime, new Object[] { new String[] { "L" } });
        }
    }

    public MainClass() {
        log(DBG, "Initializing MainClass");

        activity = UnityPlayer.currentActivity;
        context = getUnityPlayer().getContext();

        try {
            Bypass.unseal();
        } catch (Throwable e) {
            logE("Error bypassing reflection restrictions: ", e);
        }
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

    // needs to be called by system or shell (adb)
    private boolean setCallerInfo(Intent intent, String name) {
        intent.setExtrasClassLoader(context.getClassLoader());
        Bundle extras = intent.getExtras();
        if (extras == null)
            extras = new Bundle();
        extras.setClassLoader(context.getClassLoader());

        PendingIntent info;

        try {
            Method getServiceMethod = ActivityManager.class.getDeclaredMethod("getService");
            getServiceMethod.setAccessible(true);
            Object activityManagerService = getServiceMethod.invoke(null);

            // make target
            String resolvedType = intent.resolveTypeIfNeeded(context.getContentResolver());

            @SuppressLint("BlockedPrivateApi")
            Method getUserMethod = Context.class.getDeclaredMethod("getUser");
            getUserMethod.setAccessible(true);
            Object user = getUserMethod.invoke(context);

            Method getIdentifierMethod = user.getClass().getDeclaredMethod("getIdentifier");
            getIdentifierMethod.setAccessible(true);
            Object identifier = getIdentifierMethod.invoke(user);

            Method getSenderMethod = activityManagerService.getClass().getDeclaredMethod(
                    "getIntentSenderWithFeature",
                    int.class, String.class, String.class, IBinder.class, String.class, int.class, Intent[].class,
                    String[].class,
                    int.class, Bundle.class, int.class);
            getSenderMethod.setAccessible(true);
            Object target = getSenderMethod.invoke(activityManagerService, 1, name, context.getAttributionTag(), null,
                    null, 0,
                    new Intent[] { intent },
                    resolvedType != null ? new String[] { resolvedType } : null,
                    PendingIntent.FLAG_IMMUTABLE, null, identifier);

            Constructor<?> pendingIntentConstructor = PendingIntent.class.getConstructor(target.getClass());
            pendingIntentConstructor.setAccessible(true);
            info = (PendingIntent) pendingIntentConstructor.newInstance(target);
        } catch (Throwable e) {
            logE("Failed to create PendingIntent: ", e);
            return false;
        }

        extras.putParcelable("_ci_", info);
        intent.putExtras(extras);

        return true;
    }

    public void sendSurfaceCapture(Surface surface, float fovAdjustment, int selectedEye, int frameRate,
            boolean showInhibitedLayers) {
        // try permission: com.oculus.vrruntimeservice.permission.SEND_PROTECTED_CAPTURE_COMMAND
        // with intent: com.oculus.systemactivities.PROTECTED_BEGIN_VIDEO_CAPTURE_WITH_SURFACE
        Intent intent = new Intent("com.oculus.systemactivities.BEGIN_VIDEO_CAPTURE_WITH_SURFACE");
        intent.setPackage("com.oculus.systemdriver");
        intent.putExtra("surface", surface);
        intent.putExtra("lift_inhibit", showInhibitedLayers);
        intent.putExtra("audio_only", false);
        intent.putExtra("show_capture_indicator", false);
        intent.putExtra("video_capture_aspect_ratio_fov", fovAdjustment);
        intent.putExtra("video_capture_eye_selection", selectedEye);
        intent.putExtra("video_capture_frame_rate", frameRate);
        intent.putExtra("new_audio_capture", true);

        // intent.putExtra("rotation_3_dof_stabil_enabled", true);
        // intent.putExtra("stabil_slerp_speed", 1.0f);
        // intent.putExtra("stabil_dead_zone_speed_perc", 1.0f);
        // intent.putExtra("stabil_dead_zone_angular_radius", 1.0f);
        // intent.putExtra("stabil_fov_crop_degrees", 1.0f);

        if (!setCallerInfo(intent, "com.oculus.horizon"))
            return;

        log(DBG, "Broadcasting intent " + intent);
        // context.sendBroadcast(intent);
    }

    public void stopSurfaceCapture() {
        Intent intent = new Intent("com.oculus.systemactivities.STOP_VIDEO_CAPTURE");

        if (!setCallerInfo(intent, "com.oculus.horizon"))
            return;

        log(DBG, "Broadcasting intent " + intent);
        // context.sendBroadcast(intent);
    }
}
