# Historical source PORT lines

Every `PORT:` marker left in `src/` is listed here. The edits preserve the original DOS
addresses, C calling convention, integer widths, or virtual-machine scheduling only.

## `src/t06.c`

- The `vhw_cpu_poll_yield` declaration and call connect the original memory-poll wait to a
  host scheduler yield. VHW yields once per 4096 calls and leaves IRQ delivery asynchronous;
  the call does not change emulated CPU state. It also lets the host honor an existing window
  quit request while the historical pure-memory poll is running.

## `src/t16_keyboard.c`

- `BIOS_KEYBOARD_FLAGS_ADDRESS`, `BIOS_KEYBOARD_BUFFER_HEAD_ADDRESS`, and
  `BIOS_KEYBOARD_BUFFER_TAIL_ADDRESS` map the original BDA offsets `0x417`, `0x41a`, and
  `0x41c` through `KE_LOWMEM`, which addresses the virtual machine's low-memory shadow.

## `src/t17_mouse.c`

- `mouse_vector_entry` maps the original mouse IVT offset through `KE_LOWMEM` so the DOS
  vector-table access reaches the virtual machine's low-memory shadow.
- The `unsigned short` cast preserves Watcom's 16-bit comparison with `-1` under GCC's integer
  promotions; the tested `AX == 0xffff` success case remains true.

## `src/u_0acdd.c`

- The `subdivide_heightfield` prototype spells out its four historical `short` parameters to
  match the definition later in this translation unit and avoid GCC's old-style declaration
  conflict. The 32-bit cdecl argument slots are unchanged.

## `src/u_0d4ba.c`

- `EMS_INTERRUPT_VECTOR_ADDRESS` maps the original IVT offset `0x19c` through `KE_LOWMEM` to
  the virtual machine's low-memory shadow.

## `src/u_0dfc3.c`

- The `set_gc_read_map`, `set_seq_plane_mask`, and `set_gc_mode` declarations use one
  `unsigned int` parameter type per translation unit, matching their port interfaces and
  avoiding GCC's conflicting block-scope declarations. The x86 cdecl arguments retain their
  32-bit stack slots.

## `src/u_0e914.c`

- The `set_vga_horizontal_pan_register` prototype gives its historical byte argument an
  explicit `unsigned char` type to match the definition and avoid GCC's old-style declaration
  conflict. The 32-bit cdecl argument slot is unchanged.
