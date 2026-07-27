package com.soulfu.game;

import org.libsdl.app.SDLActivity;

/**
 * SoulFu main activity — extends SDLActivity which handles
 * the native library loading, GL surface, input, and lifecycle.
 */
public class SoulFuActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "soulfu"       // gl4es is linked statically into libsoulfu.so
        };
    }
}
