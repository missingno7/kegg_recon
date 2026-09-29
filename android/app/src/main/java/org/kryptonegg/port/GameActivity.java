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
        return new String[] { GameData.dir(this).getAbsolutePath() };
    }
}
