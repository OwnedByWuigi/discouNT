# discouNT ntos layout

- `arch/x86/` — x86 and AMD64 interrupt, HAL, boot protocol, and linker support
- `core/` — kernel entry, executive services, setup, exports, commands, and bug checks
- `io/` — I/O manager and driver loading/dispatch support
- `loader/` — PE and ELF image loading
- `mm/` — general, physical, and virtual memory management
- `ob/` — object manager
- `rtl/` — runtime support routines

Headers live beside the implementation they describe. Kernel includes are qualified
from the `kernel/` root, for example `#include "mm/mm.h"`.
