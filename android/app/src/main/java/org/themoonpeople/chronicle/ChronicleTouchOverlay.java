package org.themoonpeople.chronicle;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;

/** Android-composited controls. No draw calls enter the game's Vulkan frame. */
final class ChronicleTouchOverlay extends View {
    private static final String[] LABELS = {"↑", "↓", "←", "→", "×", "○", "□", "△",
            "L1", "R1", "L2", "R2", "SELECT", "START"};
    private static final int[] COLORS = {0xffedf2ff, 0xffedf2ff, 0xffedf2ff, 0xffedf2ff,
            0xff73b8ff, 0xffff878b, 0xffefa1e9, 0xff8be7a5,
            0xffedf2ff, 0xffedf2ff, 0xffedf2ff, 0xffedf2ff, 0xffedf2ff, 0xffedf2ff};
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final float[] cx = new float[16], cy = new float[16], radius = new float[16];
    private final SparseArray<Capture> pointers = new SparseArray<>();
    private int lastMask;
    private float lastLx, lastLy, lastRx, lastRy;
    private boolean controlsVisible = true;

    private static final class Capture {
        final int control;
        float x, y;
        Capture(int control, float x, float y) { this.control = control; this.x = x; this.y = y; }
    }

    private native void nativeSetTouchState(int mask, float lx, float ly, float rx, float ry);

    ChronicleTouchOverlay(Context context) {
        super(context);
        setFocusable(false);
        setLayerType(View.LAYER_TYPE_HARDWARE, null);
    }

    private static float clamp(float value, float lo, float hi) {
        return Math.max(lo, Math.min(hi, value));
    }

    private void layoutControls() {
        float w = getWidth(), h = getHeight(), s = Math.min(w, h);
        if (w <= 0 || h <= 0) return;
        float r = clamp(s * .039f, 46, 77), shoulder = clamp(s * .035f, 46, 70);
        float small = clamp(s * .030f, 44, 60), step = 2 * r + 12;
        float side = clamp(.08f + .08f * s / w, .10f, .16f);
        float left = side * w, right = (1 - side) * w, row = h * .58f;
        for (int i = 0; i < 16; ++i) radius[i] = i < 8 ? r : i < 12 ? shoulder : i < 14 ? small : clamp(s * .075f, 80, 140);
        cx[0] = cx[1] = left; cy[0] = row - step; cy[1] = row + step;
        cx[2] = left - step; cx[3] = left + step; cy[2] = cy[3] = row;
        cx[4] = cx[7] = right; cy[4] = row + step; cy[7] = row - step;
        cx[5] = right + step; cx[6] = right - step; cy[5] = cy[6] = row;
        cx[8] = cx[10] = left; cx[9] = cx[11] = right;
        cy[8] = cy[9] = h * .215f; cy[10] = cy[11] = h * .10f;
        cx[12] = w * .43f; cx[13] = w * .57f; cy[12] = cy[13] = h * .92f;
        cx[14] = (side + .025f) * w; cx[15] = (1 - side - .025f) * w;
        cy[14] = cy[15] = h * .86f;
    }

    private float distance(int control, float x, float y) {
        return (float) Math.hypot(x - cx[control], y - cy[control]);
    }

    private int hit(float x, float y) {
        for (int i = 0; i < 16; ++i) if (distance(i, x, y) <= radius[i]) return i;
        return -1;
    }

    void setControlsVisible(boolean visible) {
        controlsVisible = visible;
        if (!visible) { pointers.clear(); publish(); }
    }

    private void publish() {
        int mask = 0;
        float lx = 0, ly = 0, rx = 0, ry = 0;
        for (int i = 0; i < pointers.size(); ++i) {
            Capture p = pointers.valueAt(i);
            if (p.control < 0) continue;
            if (p.control < 14) {
                if (distance(p.control, p.x, p.y) <= radius[p.control]) mask |= 1 << p.control;
            } else {
                float x = clamp((p.x - cx[p.control]) / radius[p.control], -1, 1);
                float y = clamp((p.y - cy[p.control]) / radius[p.control], -1, 1);
                float magnitude = (float) Math.hypot(x, y);
                if (magnitude > 1) {
                    x /= magnitude;
                    y /= magnitude;
                }
                if (p.control == 14) { lx = x; ly = y; } else { rx = x; ry = y; }
            }
        }
        if (!controlsVisible) { mask = 0; lx = ly = rx = ry = 0; }
        if (mask != lastMask || lx != lastLx || ly != lastLy || rx != lastRx || ry != lastRy) {
            lastMask = mask; lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry;
            nativeSetTouchState(mask, lx, ly, rx, ry);
            invalidate();
        }
    }

    @Override protected void onSizeChanged(int w, int h, int oldW, int oldH) {
        super.onSizeChanged(w, h, oldW, oldH);
        layoutControls();
        pointers.clear();
        publish();
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked(), index = event.getActionIndex();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            float x = event.getX(index), y = event.getY(index);
            pointers.put(event.getPointerId(index), new Capture(hit(x, y), x, y));
        } else if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < event.getPointerCount(); ++i) {
                Capture p = pointers.get(event.getPointerId(i));
                if (p != null) { p.x = event.getX(i); p.y = event.getY(i); }
            }
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            pointers.remove(event.getPointerId(index));
        } else if (action == MotionEvent.ACTION_CANCEL) {
            pointers.clear();
        }
        publish();
        return true; // Empty parts of the screen are inert and never reach SDL.
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!controlsVisible) return;
        for (int i = 14; i < 16; ++i) {
            drawDisc(canvas, cx[i], cy[i], radius[i], 0x29202a3a, 0x779fabc0);
            float dx = i == 14 ? lastLx : lastRx, dy = i == 14 ? lastLy : lastRy;
            drawDisc(canvas, cx[i] + dx * radius[i] * .45f, cy[i] + dy * radius[i] * .45f,
                    radius[i] * .37f, 0x552d374d, 0xaac9d7f0);
        }
        for (int i = 0; i < 14; ++i) {
            boolean pressed = (lastMask & (1 << i)) != 0;
            drawDisc(canvas, cx[i], cy[i], radius[i], pressed ? 0x99283450 : 0x3c202a3a,
                    pressed ? 0xe6d5e3ff : 0x99c9d7f0);
            paint.setColor(COLORS[i]); paint.setStyle(Paint.Style.FILL);
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTypeface(android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.NORMAL));
            paint.setTextSize(i >= 12 ? radius[i] * .40f : i >= 8 ? radius[i] * .50f : radius[i] * .70f);
            canvas.drawText(LABELS[i], cx[i], cy[i] - (paint.ascent() + paint.descent()) / 2, paint);
        }
    }

    private void drawDisc(Canvas canvas, float x, float y, float r, int fill, int edge) {
        paint.setStyle(Paint.Style.FILL); paint.setColor(fill);
        canvas.drawCircle(x, y, r, paint);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(Math.max(2, r * .027f));
        paint.setColor(edge); canvas.drawCircle(x, y, r, paint);
    }
}
