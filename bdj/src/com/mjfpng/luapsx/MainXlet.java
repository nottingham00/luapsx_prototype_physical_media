package com.mjfpng.luapsx;

import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics;
import java.io.File;

import javax.tv.xlet.Xlet;
import javax.tv.xlet.XletContext;
import javax.tv.xlet.XletStateChangeException;

import org.havi.ui.HScene;
import org.havi.ui.HSceneFactory;

/**
 * BD-J side of the prototype.
 *
 * This validates that the Blu-ray exposes a PSX/CUE/BIN set through the
 * virtual filesystem. It does not perform a sandbox escape or exploit.
 */
public final class MainXlet implements Xlet, Runnable {
    private XletContext context;
    private HScene scene;
    private volatile boolean running;
    private String[] lines = new String[] { "LuaPSX", "Starting..." };

    public void initXlet(XletContext context) throws XletStateChangeException {
        this.context = context;
        scene = HSceneFactory.getInstance().getDefaultHScene();
        scene.setBackgroundMode(HScene.BACKGROUND_FILL);
        scene.setBackground(Color.black);
        scene.setFont(new Font("Dialog", Font.PLAIN, 28));
        scene.setVisible(true);
    }

    public void startXlet() throws XletStateChangeException {
        running = true;
        Thread t = new Thread(this, "LuaPSX-disc-scan");
        t.start();
    }

    public void pauseXlet() {
        running = false;
    }

    public void destroyXlet(boolean unconditional) throws XletStateChangeException {
        running = false;
        if (scene != null) scene.setVisible(false);
        context = null;
    }

    public void run() {
        try {
            File root = DiscImageLocator.getDiscRoot();
            if (root == null) {
                setLines(new String[] { "LuaPSX", "bluray.vfs.root is unavailable" });
                return;
            }

            File psx = new File(root, "PSX");
            File cue = DiscImageLocator.findFirstCue(psx);
            if (cue == null) {
                setLines(new String[] {
                    "LuaPSX",
                    "Disc root: " + root,
                    "No .CUE found in " + psx
                });
                return;
            }

            CueInfo info = CueParser.parse(cue);
            long totalBytes = 0;
            for (int i = 0; i < info.binFiles.size(); ++i) {
                totalBytes += ((File)info.binFiles.elementAt(i)).length();
            }

            File core = new File(root, "LUAPSX/cores/pcsx_rearmed_libretro.so");
            String coreState = core.isFile() ? "present" : "not included";

            setLines(new String[] {
                "LuaPSX physical-disc prototype",
                "CUE: " + cue.getName(),
                "BIN files: " + info.binFiles.size(),
                "Tracks: " + info.tracks.size(),
                "Image bytes: " + totalBytes,
                "Emulator core: " + coreState,
                "BD-J validation: OK",
                "Native content: /mnt/disc/PSX/" + cue.getName(),
                "Native core: /mnt/disc/LUAPSX/cores/pcsx_rearmed_libretro.so"
            });
        } catch (Throwable t) {
            setLines(new String[] { "LuaPSX", "Validation failed", t.toString() });
        }
    }

    private void setLines(String[] newLines) {
        lines = newLines;
        repaint();
    }

    private void repaint() {
        if (!running || scene == null) return;
        Graphics g = scene.getGraphics();
        if (g == null) return;
        try {
            g.setColor(Color.black);
            g.fillRect(0, 0, 1920, 1080);
            g.setColor(Color.white);
            int y = 110;
            for (int i = 0; i < lines.length; ++i) {
                g.drawString(lines[i], 90, y);
                y += 48;
            }
        } finally {
            g.dispose();
        }
        scene.repaint();
    }
}
