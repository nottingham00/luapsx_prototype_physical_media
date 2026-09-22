package com.mjfpng.luapsx;

import java.io.File;

final class DiscImageLocator {
    private DiscImageLocator() {}

    static File getDiscRoot() {
        String root = System.getProperty("bluray.vfs.root");
        if (root == null || root.length() == 0) return null;
        return new File(root);
    }

    static File findFirstCue(File psxDir) {
        if (psxDir == null || !psxDir.isDirectory()) return null;
        File[] list = psxDir.listFiles();
        if (list == null) return null;
        for (int i = 0; i < list.length; ++i) {
            String n = list[i].getName().toLowerCase();
            if (list[i].isFile() && n.endsWith(".cue")) return list[i];
        }
        return null;
    }
}
