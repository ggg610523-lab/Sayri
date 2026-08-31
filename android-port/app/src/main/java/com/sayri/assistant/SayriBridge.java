package com.sayri.assistant;

import android.content.Context;
import android.content.Intent;

/**
 * Static bridge between the native SDL main loop and the Java
 * QR scanner activity.
 *
 * The native side (main_android.c) calls launchScanner() to open
 * the camera scanner, then polls takeScannedUri() each frame. A
 * successful scan leaves the decoded pairing URI here for the
 * native code to parse and auto-connect with.
 */
public final class SayriBridge {

    /** Decoded sayri://<ip>:<port>?code=<code> URI, or null. */
    public static volatile String sScannedUri = null;

    /** Open the QR scanner activity (fullscreen camera view). */
    public static void launchScanner(Context context) {
        context.startActivity(new Intent(context, QrScannerActivity.class));
    }

    /** Return and clear any scanned URI from last time. */
    public static String takeScannedUri() {
        String s = sScannedUri;
        sScannedUri = null;
        return s;
    }
}