/* Animation frame list at _DATA 0x25ec: the first game object in link order (no code), before U00010.
   { image offset, ticks } frames; the closing entry { 0, -21 } jumps back 21 frames (loop). */
struct Frame { unsigned img; int time; };

struct Frame g_25ec[22] = {
    { 0x4a96, 2 },
    { 0x4af4, 2 },
    { 0x4ba8, 2 },
    { 0x4c72, 2 },
    { 0x4d9a, 2 },
    { 0x4eda, 2 },
    { 0x5062, 2 },
    { 0x5238, 2 },
    { 0x546a, 2 },
    { 0x571a, 2 },
    { 0x59f2, 2 },
    { 0x5cf2, 2 },
    { 0x5ff2, 2 },
    { 0x62f2, 2 },
    { 0x6622, 2 },
    { 0x6956, 2 },
    { 0x6c90, 2 },
    { 0x6fb2, 2 },
    { 0x72cc, 2 },
    { 0x75c4, 2 },
    { 0x78b2, 2 },
    { 0, -21 }
};
