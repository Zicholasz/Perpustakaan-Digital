# Terminal Animations for Perpustakaan Digital

Files added (local only):

- `include/animation.h` — header for the animation API
- `source/animation.c` — implementation with several terminal animations
- `source/animation_demo.c` — small standalone demo program that runs animations

How to build (Windows, PowerShell):

Using MinGW / GCC (recommended):

```powershell
# From project root
gcc -Iinclude -O2 -o bin/animation_demo.exe source/animation.c source/animation_demo.c
.
# Run the demo
.\bin\animation_demo.exe
```

If you use a different compiler, compile both `source/animation.c` and
`source/animation_demo.c` and link them together, and ensure the include
path includes `include/`.

How to integrate into your existing program (optional):

1. Add `#include "animation.h"` where you want to call animations.
2. Call `animation_init()` early (optional) and then call animations such as
   `animation_typewriter("Halo", 30);` or `animation_run_demo();`.

Notes and compatibility:

- The module uses ANSI escape sequences; on modern Windows terminals VT processing
  is enabled in `animation_init()`. If an older terminal doesn't support ANSI,
  animations will still run but color/movement may be limited.
- This code is added locally and does not change existing modular files.
