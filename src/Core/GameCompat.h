#pragma once

#include <cstdint>

// Game-compatibility probe. The SDK resolves GObjects / GNames / ProcessEvent /
// AppendString as image-base-relative RVAs hardcoded in Basic.hpp. When the game
// updates, those RVAs point at garbage and the first SDK access crashes. This
// module lets us detect that mismatch WITHOUT crashing (every game-memory read is
// wrapped in SEH) so the caller can trigger an automatic self-update instead of
// dying.
namespace GameCompat
{
    // TimeDateStamp from the main executable's PE header (IMAGE_NT_HEADERS.
    // FileHeader.TimeDateStamp). Changes on every recompile of the game, read
    // straight from the already-mapped headers — no game-memory access. Used as
    // the "game build id" key for server-side binary resolution. 0 on failure.
    uint32_t GameTimeDateStamp();

    // True if the hardcoded SDK offsets still land on coherent data in the current
    // game process (GObjects array sane, ProcessEvent/AppendString point at code).
    // All probing is SEH-guarded: on any access violation it returns false rather
    // than propagating the fault. False => game likely patched => do NOT init the
    // SDK hooks; trigger an auto-update instead.
    bool IsGameCompatible();
}
