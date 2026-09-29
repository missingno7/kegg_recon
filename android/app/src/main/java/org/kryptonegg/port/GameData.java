package org.kryptonegg.port;

import android.content.Context;

import java.io.File;
import java.util.Locale;

/** The user's Krypton Egg data files in the app's internal storage (files/game). */
final class GameData {
    /** The files the game opens (port/host/config.c required_assets). */
    static final String[] REQUIRED = {
        "KE_ALL.PAL", "KE_BRICK.BOB", "KE_DIGIT.BOB", "KE_END.DIG", "KE_FILL.BOB",
        "KE_FONT.BOB", "KE_GO.DIG", "KE_INFOS.DIG", "KE_LDCWC.TAB", "KE_LVL.DIG",
        "KE_MAIN.DIG", "KE_MENU.BOB", "KE_MENU.DIG", "KE_MENU.GIF", "KE_MONST.BOB",
        "KE_MONST.GIF", "KE_NMY.BOB", "KE_ORDER.GIF", "KE_PAUSE.DIG", "KE_RACK.BOB",
        "KE_SCORE.DIG", "KE_SCORE.GIF", "KE_SPELL.BOB", "KE_TIT.DIG", "KE_TIT.GIF",
    };
    /** Copied when present: publisher pages and the initial high-score table. */
    static final String[] OPTIONAL = {
        "KE_PUB.DIG", "KE_PUB1.GIF", "KE_PUB2.GIF", "KE_PUB3.GIF", "KE_SCORE.LST",
    };

    private GameData() {}

    static File dir(Context context) {
        return new File(context.getFilesDir(), "game");
    }

    /** Canonical upper-case name if `name` (any case, any path) is a game file, else null. */
    static String canonical(String name) {
        int slash = Math.max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
        String leaf = name.substring(slash + 1).toUpperCase(Locale.ROOT);
        for (String f : REQUIRED) {
            if (f.equals(leaf)) return f;
        }
        for (String f : OPTIONAL) {
            if (f.equals(leaf)) return f;
        }
        return null;
    }

    /** Names of required files missing (or empty) in `dir`; empty when complete. */
    static String missing(File dir) {
        StringBuilder missing = new StringBuilder();
        for (String f : REQUIRED) {
            File file = new File(dir, f);
            if (!file.isFile() || file.length() == 0) {
                if (missing.length() > 0) missing.append(", ");
                missing.append(f);
            }
        }
        return missing.toString();
    }

    static boolean isComplete(Context context) {
        return missing(dir(context)).isEmpty();
    }
}
