package com.mjfpng.luapsx;

import java.io.File;
import java.util.Vector;

final class CueInfo {
    final File cueFile;
    final Vector binFiles = new Vector();
    final Vector tracks = new Vector();

    CueInfo(File cueFile) {
        this.cueFile = cueFile;
    }

    static final class Track {
        int number;
        String mode;
        int index01Frames = -1;

        public String toString() {
            return "Track " + number + " " + mode + " INDEX01=" + index01Frames;
        }
    }
}
