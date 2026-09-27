/* A negative terminal delta steps from the sentinel back into the frame sequence. */
struct SpriteFrame { unsigned image_offset; int duration_or_delta; };

#define COLLISION_ANIMATION_LOOP_BACK (-21)

struct SpriteFrame collision_animation_frames[22] = {
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
    { 0, COLLISION_ANIMATION_LOOP_BACK }
};
