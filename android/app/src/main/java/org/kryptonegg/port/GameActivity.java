package org.kryptonegg.port;

import org.libsdl.app.SDLActivity;

/**
 * The game: SDL3's activity running libmain.so's SDL_main (port/android/loader.c), which maps
 * libkegame.so below 2 GB and starts it. libkegame.so is deliberately not in getLibraries():
 * Java must not load it at an arbitrary address.
 */
public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected String[] getArguments() {
        // Development hook (docs/android/building.md): extra "ke.args" replaces the arguments,
        // e.g. the in-app lockstep runner of debug builds made with -Pke.lockstep=1.
        String[] extra = getIntent() != null ? getIntent().getStringArrayExtra("ke.args") : null;
        if (extra != null && BuildConfig.DEBUG) {
            return extra;
        }
        return new String[] { GameData.dir(this).getAbsolutePath() };
    }
}
