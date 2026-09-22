package com.mjfpng.luapsx;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

final class CueParser {
    private CueParser() {}

    static CueInfo parse(File cue) throws IOException {
        CueInfo info = new CueInfo(cue);
        BufferedReader br = new BufferedReader(new FileReader(cue));
        CueInfo.Track currentTrack = null;
        String line;
        try {
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.length() == 0 || line.startsWith("REM ")) continue;

                if (startsIgnoreCase(line, "FILE ")) {
                    String name = parseFileName(line);
                    File bin = new File(cue.getParentFile(), name);
                    if (!bin.exists() || !bin.isFile()) {
                        throw new IOException("Missing BIN referenced by CUE: " + bin);
                    }
                    if ((bin.length() % 2352L) != 0) {
                        throw new IOException("BIN is not a multiple of 2352 bytes: " + bin);
                    }
                    info.binFiles.addElement(bin);
                    currentTrack = null;
                } else if (startsIgnoreCase(line, "TRACK ")) {
                    String[] p = splitWs(line);
                    if (p.length < 3) throw new IOException("Bad TRACK line: " + line);
                    currentTrack = new CueInfo.Track();
                    currentTrack.number = Integer.parseInt(p[1]);
                    currentTrack.mode = p[2];
                    info.tracks.addElement(currentTrack);
                } else if (startsIgnoreCase(line, "INDEX 01 ") && currentTrack != null) {
                    String[] p = splitWs(line);
                    if (p.length < 3) throw new IOException("Bad INDEX line: " + line);
                    currentTrack.index01Frames = parseMsf(p[2]);
                }
            }
        } finally {
            br.close();
        }

        if (info.binFiles.size() == 0) throw new IOException("CUE has no FILE entries");
        if (info.tracks.size() == 0) throw new IOException("CUE has no TRACK entries");
        return info;
    }

    private static boolean startsIgnoreCase(String s, String prefix) {
        return s.regionMatches(true, 0, prefix, 0, prefix.length());
    }

    private static String parseFileName(String line) throws IOException {
        int firstQuote = line.indexOf('"');
        if (firstQuote >= 0) {
            int secondQuote = line.indexOf('"', firstQuote + 1);
            if (secondQuote < 0) throw new IOException("Bad FILE line: " + line);
            return line.substring(firstQuote + 1, secondQuote);
        }
        String[] p = splitWs(line);
        if (p.length < 2) throw new IOException("Bad FILE line: " + line);
        return p[1];
    }

    private static String[] splitWs(String s) {
        java.util.StringTokenizer t = new java.util.StringTokenizer(s);
        String[] out = new String[t.countTokens()];
        for (int i = 0; i < out.length; ++i) out[i] = t.nextToken();
        return out;
    }

    private static int parseMsf(String s) throws IOException {
        String[] p = s.split(":");
        if (p.length != 3) throw new IOException("Bad MSF: " + s);
        int m = Integer.parseInt(p[0]);
        int sec = Integer.parseInt(p[1]);
        int frame = Integer.parseInt(p[2]);
        if (sec < 0 || sec >= 60 || frame < 0 || frame >= 75) {
            throw new IOException("Bad MSF range: " + s);
        }
        return ((m * 60) + sec) * 75 + frame;
    }
}
