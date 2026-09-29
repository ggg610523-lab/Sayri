package com.sayri.assistant;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import android.Manifest;

import com.google.zxing.BinaryBitmap;
import com.google.zxing.MultiFormatReader;
import com.google.zxing.RGBLuminanceSource;
import com.google.zxing.Result;
import com.google.zxing.common.HybridBinarizer;

/**
 * Fullscreen camera QR scanner.
 *
 * Opens the back camera through the legacy android.hardware.Camera API
 * (the classic ZXing path), shows a live landscape preview, and decodes
 * each NV21 frame with ZXing. On a successful scan it stores the decoded
 * identify URI into SayriBridge and closes; the native SDL loop then
 * auto-fills the pairing sheet and connects.
 *
 * Requires android.permission.CAMERA; on Android 11+ (API 23+) a runtime
 * prompt is shown, so we request it explicitly before opening the camera.
 */
public class QrScannerActivity extends Activity implements Camera.PreviewCallback {

    private static final int REQ_CAMERA = 100;
    private static final int PREVIEW_W = 640;
    private static final int PREVIEW_H = 480;

    private Camera mCamera;
    private SurfaceHolder mHolder;
    private boolean mDecoded = false;
    private boolean mStarting = false;

    private final int[] mRgb = new int[PREVIEW_W * PREVIEW_H];
    private final MultiFormatReader mReader = new MultiFormatReader();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Landscape so the NV21 preview buffer's orientation matches
        // the display: no rotation handling needed for the decoder.
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(0xFF000000);

        SurfaceView preview = new SurfaceView(this);
        root.addView(preview,
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                        0, 1.0f));

        TextView hint = new TextView(this);
        hint.setText("Point the camera at the Sayri QR code");
        hint.setTextColor(0xFFDDDDDD);
        hint.setTextSize(16);
        root.addView(hint);

        Button cancel = new Button(this);
        cancel.setText("Cancel");
        cancel.setBackgroundColor(0xFF303030);
        cancel.setTextColor(0xFFFFFFFF);
        cancel.setOnClickListener(v -> finish());
        root.addView(cancel,
                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                        56));

        setContentView(root);

        preview.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                mHolder = holder;
                tryStart();
            }
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format,
                                       int width, int height) {
            }
            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                mHolder = null;
            }
        });
    }

    /** Open the camera once the CAMERA permission is granted. */
    private void tryStart() {
        if (mStarting || mHolder == null) return;

        if (checkSelfPermission(Manifest.permission.CAMERA)
                != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.CAMERA},
                    REQ_CAMERA);
            return;
        }

        mStarting = true;
        try {
            mCamera = Camera.open(0);
            Camera.Parameters p = mCamera.getParameters();
            p.setPreviewSize(PREVIEW_W, PREVIEW_H);
            p.setPreviewFormat(ImageFormat.NV21);
            p.setFocusMode("continuous-picture");
            mCamera.setParameters(p);
            mCamera.setPreviewDisplay(mHolder);
            mCamera.setPreviewCallbackWithBuffer(this);
            mCamera.addCallbackBuffer(new byte[PREVIEW_W * PREVIEW_H * 3 / 2]);
            mCamera.startPreview();
        } catch (Exception e) {
            closeCamera();
            mStarting = false;
            finish();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        if (requestCode != REQ_CAMERA) return;
        boolean granted = grantResults != null && grantResults.length > 0
                && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        if (!granted) {
            finish();
            return;
        }
        tryStart();
    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        if (mDecoded || mCamera == null) return;
        mDecoded = true;

        // Copy the Y (luma) plane into a grayscale ARGB array for ZXing.
        int[] rgb = mRgb;
        for (int i = 0; i < PREVIEW_W * PREVIEW_H; i++) {
            int y = data[i] & 0xFF;
            rgb[i] = 0xFF000000 | (y << 16) | (y << 8) | y;
        }

        try {
            Result r = mReader.decodeWithState(new BinaryBitmap(
                    new HybridBinarizer(
                            new RGBLuminanceSource(PREVIEW_W, PREVIEW_H, rgb))));
            mReader.reset();
            if (r != null && !TextUtils.isEmpty(r.getText())) {
                SayriBridge.sScannedUri = r.getText();
                closeCamera();
                finish();
                return;
            }
        } catch (Exception ignore) {
            // No code in this frame; keep scanning.
        }

        mDecoded = false;
        try {
            camera.addCallbackBuffer(data);
        } catch (Exception ignore) {
        }
    }

    private void closeCamera() {
        if (mCamera != null) {
            try {
                mCamera.setPreviewCallbackWithBuffer(null);
            } catch (Exception ignore) {
            }
            mCamera = null;
        }
    }

    @Override
    public void onDestroy() {
        closeCamera();
        super.onDestroy();
    }
}