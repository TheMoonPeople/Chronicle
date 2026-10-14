package org.themoonpeople.chronicle;

import android.content.Intent;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class ChronicleActivity extends SDLActivity {
    private static final int REQUEST_DISC_IMAGE = 7101;
    private final ExecutorService copyExecutor = Executors.newSingleThreadExecutor();
    private ProgressDialog importDialog;
    private ChronicleTouchOverlay touchOverlay;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        touchOverlay = new ChronicleTouchOverlay(this);
        FrameLayout root = findViewById(android.R.id.content);
        root.addView(touchOverlay, new FrameLayout.LayoutParams(-1, -1));
    }

    public void setTouchControlsVisible(boolean visible) {
        runOnUiThread(() -> {
            if (touchOverlay == null) return;
            touchOverlay.setControlsVisible(visible);
            touchOverlay.setVisibility(visible ? View.VISIBLE : View.GONE);
        });
    }

    @Override
    protected String[] getArguments() {
        // SDL calls this before SDL_main. Keep the title test hook available through the launcher
        // activity without changing SDL's shared Java implementation or the normal retail boot.
        String jump = getIntent().getStringExtra("chronicle.jump");
        if (jump == null || jump.isEmpty()) return new String[0];
        if ("options".equals(jump)) {
            return new String[]{"--jump", "options", "--fast-load"};
        }
        return new String[]{"--jump", jump};
    }

    public void showImportStatus(String message) {
        runOnUiThread(() -> {
            if (isFinishing()) return;
            if (importDialog == null) {
                importDialog = new ProgressDialog(this);
                importDialog.setTitle("Dark Cloud setup");
                importDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
                importDialog.setCancelable(false);
            }
            importDialog.setMessage(message);
            if (!importDialog.isShowing()) importDialog.show();
        });
    }

    public void hideImportStatus() {
        runOnUiThread(() -> {
            if (importDialog != null && importDialog.isShowing()) importDialog.dismiss();
            importDialog = null;
        });
    }

    public void showImportError(String message) {
        runOnUiThread(() -> new AlertDialog.Builder(this)
                .setTitle("Disc image could not be imported")
                .setMessage(message)
                .setPositiveButton("Choose another image", (dialog, which) -> requestDiscImage())
                .setNegativeButton("Close", (dialog, which) -> finish())
                .setOnCancelListener(dialog -> requestDiscImage())
                .show());
    }

    public void requestDiscImage() {
        runOnUiThread(() -> {
            showImportStatus("Choose your Dark Cloud disc image.");
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, REQUEST_DISC_IMAGE);
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        Log.i("Chronicle", "onActivityResult request=" + requestCode + " result=" + resultCode);
        if (requestCode != REQUEST_DISC_IMAGE) {
            return;
        }
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            hideImportStatus();
            nativeDiscImageResult(null);
            return;
        }
        Uri uri = data.getData();
        showImportStatus("Copying the disc image to app storage…");
        copyExecutor.execute(() -> {
            File image = new File(getCacheDir(), "darkcloud-user-disc.iso");
            String result = null;
            try (InputStream input = getContentResolver().openInputStream(uri);
                 FileOutputStream output = new FileOutputStream(image, false)) {
                if (input == null) {
                    throw new IllegalStateException("The selected document could not be opened.");
                }
                byte[] buffer = new byte[256 * 1024];
                int read;
                long copied = 0;
                long expected = -1;
                try (android.database.Cursor cursor = getContentResolver().query(uri,
                        new String[]{android.provider.OpenableColumns.SIZE}, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) expected = cursor.getLong(0);
                } catch (Exception ignored) { }
                while ((read = input.read(buffer)) != -1) {
                    output.write(buffer, 0, read);
                    copied += read;
                    if (expected > 0) {
                        final String status = String.format(java.util.Locale.US,
                                "Copying disc image… %.0f%%", 100.0 * copied / expected);
                        runOnUiThread(() -> { if (importDialog != null) importDialog.setMessage(status); });
                    }
                }
                result = image.getAbsolutePath();
            } catch (Exception ignored) {
                image.delete();
                Log.e("Chronicle", "failed to copy selected disc image", ignored);
            }
            if (result == null) hideImportStatus();
            Log.i("Chronicle", "document copy complete: " + (result != null));
            nativeDiscImageResult(result);
        });
    }

    private native void nativeDiscImageResult(String path);
}
