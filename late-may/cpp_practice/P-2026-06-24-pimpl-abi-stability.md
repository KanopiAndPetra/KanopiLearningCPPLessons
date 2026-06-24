# C++ practice 2026-06-24 — PIMPL ABI stability (the capstone)

## What I set out to learn

The Jun 22 list closed the variant/visitor arc; Jun 23 closed the
same arc and surfaced a small set of PIMPL follow-ons. The first of
those was:

> **PIMPL ABI capstone** — compile two TUs, save one `.o`, edit the
> shape's `.cpp`, recompile only that, link, run. The unchanged
> `.o` is the proof that the PIMPL preserved ABI.

That's today's session. The arc so far has been:

| Jun   | PIMPL theme                                         |
|-------|-----------------------------------------------------|
| 17    | First PIMPL on a single `Box`                       |
| 18    | PIMPL on `Inventory` (vector inside the Impl)       |
| 19/20 | PIMPL + virtual dispatch (Shape hierarchy)          |
| **24** (today) | **PIMPL ABI proof: same driver.o, two impl.o's** |

The earlier sessions established *what* PIMPL gives you (smaller
rebuild graph, encapsulation of private state). Today is the
observable proof of the ABI claim: a single driver object file
links and runs against two completely different Implementations of
the same facade.

## What I confirmed

### (1) The facade's size is fixed regardless of Impl

```cpp
// pimpl_widget.h
class Widget {
    std::unique_ptr<Impl> impl_;   // the ONLY member
};
```

```text
sizeof(Widget)  = 8 bytes   (in driver_v1 AND driver_v2)
alignof(Widget) = 8 bytes
```

This is the heart of the ABI stability claim. No matter what I
throw into `Widget::Impl` — `std::vector`, 256-byte buffers,
helper structs, anything — the public layout of `Widget` stays at
one pointer. The compiler, the linker, and the rest of the build
graph all see a stable 8-byte type.

### (2) The driver .o's Widget surface is exactly the public API

```text
$ nm driver.o | grep Widget | c++filt | sort -u
Widget::Widget(Widget&&)
Widget::Widget(std::string)
Widget::name() const
Widget::set_name(std::string)
Widget::set_value(int)
Widget::value() const
Widget::version() const
Widget::~Widget()
```

Eight symbols. The driver's view of `Widget` is *exactly* what
`pimpl_widget.h` declares — and not a single byte more. The driver
does not know that `Impl` exists; it does not know what fields
`Impl` has; it does not know whether `Impl` contains a vector, a
buffer, or a `std::map`. Those things live entirely behind the
facade.

### (3) The two Impl variants are dramatically different

```text
v1: struct Widget::Impl {                  // ~36 bytes total
    std::string name_;
    int value_;
    std::uint64_t version_ = 0xA1A1A1A1A1A1A1A1ULL;
};

v2: struct Widget::Impl {                  // ~360 bytes total
    std::string name_;
    int value_;
    std::vector<int> history_;             // <-- new
    std::uint8_t big_buffer_[256];         // <-- new
    std::uint64_t checksum_;               // <-- new
    std::uint64_t version_ = 0xB2B2B2B2B2B2B2B2ULL;
    struct Aux { int a; double b; char pad[16]; };   // <-- new
    Aux aux_;
};
```

On-disk sizes of the object files:

```text
pimpl_widget_v1.o   =  87,512 bytes
pimpl_widget_v2.o   = 108,312 bytes     (+20,800 bytes for the new Impl members)
driver.o            = 123,888 bytes     (independent of the Impl size)
driver_v1           =  54,216 bytes
driver_v2           =  58,200 bytes     (+3,984 bytes — the bigger Impl
                                        in the executable's data segment)
```

The driver's object file is the same size whether it's linked
against v1 or v2 — because driver.o contains no code or data that
depends on Impl's layout. The difference shows up only in the
*executable* (the linked driver_v2), and only because the bigger
Impl ends up in the executable's data segment.

### (4) The build is reproducible

```text
$ g++ ... -c P-2026-06-24-pimpl-abi-stability.cpp -o driver.o
$ g++ ... -c P-2026-06-24-pimpl-abi-stability.cpp -o driver_check.o
$ shasum -a 256 driver.o driver_check.o
92bdda135bef81f03f1f39050ba5b1fad406dbcbcca5dc28e177c9180274273d  driver.o
92bdda135bef81f03f1f39050ba5b1fad406dbcbcca5dc28e177c9180274273d  driver_check.o
```

Same SHA256 both times. The "compile driver.o once" step in the
recipe is not a lie — the artifact is byte-stable.

### (5) Both binaries run; the only difference is the version() magic

```text
$ ./driver_v1
...
version = 0xA1A1A1A1A1A1A1A1
FINAL version = 0xA1A1A1A1A1A1A1A1

$ ./driver_v2
...
version = 0xB2B2B2B2B2B2B2B2
FINAL version = 0xB2B2B2B2B2B2B2B2
```

Same source, same driver.o, two different widget.o files. Both
binaries print `sizeof(Widget) = 8`, both correctly exercise
`set_name`/`set_value`/`name`/`value`/`version`, both correctly
move-construct. The only observable difference at the public API
level is the `version()` magic number — which is **exactly the
point**: Impl's internals can grow without recompiling clients,
and a deliberately-designed public API hook (`version()`) can
expose a fingerprint when wanted.

## The recipe (this is the "ABI proof" command sequence)

```bash
cd late-may/cpp_practice/

# 1. Compile the driver ONCE. This is the client.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -c P-2026-06-24-pimpl-abi-stability.cpp -o driver.o

# 2. Compile Widget v1 (small Impl).
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -DPIMPL_V1 \
    -c pimpl_widget.cpp -o pimpl_widget_v1.o

# 3. Compile Widget v2 (BIG Impl, different version magic).
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -DPIMPL_V2 \
    -c pimpl_widget.cpp -o pimpl_widget_v2.o

# 4. Link v1 — uses driver.o + pimpl_widget_v1.o.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o driver_v1 driver.o pimpl_widget_v1.o

# 5. Link v2 — uses the SAME driver.o + pimpl_widget_v2.o.
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o driver_v2 driver.o pimpl_widget_v2.o

# 6. Run both. Same driver source, two completely different Impls.
./driver_v1
./driver_v2
```

If you replace `driver.o` with "the precompiled object file in
your package's `/usr/lib`", this is exactly the ABI contract that
a PIMPL-using library promises its clients: clients compiled
against version N keep working when the library is rebuilt with
version N+1, even if the N+1 implementation grew dramatically.

## A subtle thing: the dtor must be out-of-line

```cpp
// pimpl_widget.h
class Widget {
public:
    ~Widget();   // <-- declaration only, NOT = default
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// pimpl_widget.cpp
Widget::~Widget() = default;
```

This is not optional. If the dtor were `= default` in the header,
then `~Widget()` would be implicitly generated *in every TU that
includes the header*. The implicit dtor needs to destroy
`impl_`, which is a `unique_ptr<Impl>`. To destroy
`unique_ptr<Impl>`, the compiler needs the *complete* definition
of `Impl` — which lives in `pimpl_widget.cpp`. So an inline
defaulted dtor would force every including TU to see `Impl`'s
definition, which defeats the whole PIMPL.

The out-of-line `~Widget() = default` in `pimpl_widget.cpp` is
where the dtor body is actually instantiated, where the
`unique_ptr<Impl>` destructor is actually called, and where the
full `Impl` definition is needed. The header stays clean.

This is the single most common PIMPL bug: someone writes
`~Widget() = default` in the header (or even just lets the
compiler implicitly generate it), and suddenly every TU that
includes the header needs to see `Widget::Impl`. The PIMPL is
silently defeated.

## What I took away

- **PIMPL is a real ABI promise, not just an encapsulation trick.**
  The driver.o's surface (`nm driver.o | grep Widget`) is exactly
  the public API declared in the header. Adding fields to Impl
  changes *nothing* in the driver's compiled code.
- **The facade's size and layout are determined entirely by the
  header.** `sizeof(Widget) = 8` bytes, full stop. The Impl can
  be 36 bytes or 360 bytes; the facade doesn't care.
- **The out-of-line `~Widget() = default` is non-negotiable.** It's
  the linchpin: without it, the dtor is generated in every
  including TU and the PIMPL is silently broken.
- **A public API hook can deliberately expose Impl-version
  differences** (`version()` returning `0xA1...` vs `0xB2...`).
  This is useful for cache-invalidation checks, plugin
  compatibility, etc.
- **The build is reproducible.** driver.o has the same SHA256 on
  rebuilds — so the "compile once, reuse forever" promise isn't
  dependent on some weird incremental-build accident.

This closes the PIMPL arc opened on Jun 17. The remaining items
from the Jun 22 list:

- **`shared_ptr<Impl>` PIMPL** — replace `unique_ptr<Impl>` with
  `shared_ptr<Impl>`. Copy ctor / assign become defaultable; deep
  copy disappears; cost becomes refcount.
- **Acyclic visitor** — `dynamic_cast` *inside* the visitor's
  `visit(Base&)` to break the dependency cycle of the classic
  visitor pattern. Loses double-dispatch but gains extensibility.

## Build and run

```bash
cd late-may/cpp_practice/

# 1. driver.o
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -c P-2026-06-24-pimpl-abi-stability.cpp -o driver.o

# 2. widget v1 + v2 .o's (different -D flag, same source)
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -DPIMPL_V1 \
    -c pimpl_widget.cpp -o pimpl_widget_v1.o
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g -DPIMPL_V2 \
    -c pimpl_widget.cpp -o pimpl_widget_v2.o

# 3. Link both
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o driver_v1 driver.o pimpl_widget_v1.o
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -o driver_v2 driver.o pimpl_widget_v2.o

# 4. Run both. Same source, two impls, only version() differs.
./driver_v1
./driver_v2

# ASan + UBSan (compile against v2 — biggest Impl, most surface
# area for memory bugs to hide):
g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
    -fsanitize=address,undefined -DPIMPL_V2 \
    P-2026-06-24-pimpl-abi-stability.cpp pimpl_widget.cpp \
    -o P-2026-06-24-pimpl-abi-stability-asan
./P-2026-06-24-pimpl-abi-stability-asan
```

All builds clean (no warnings under `-Wall -Wextra -Wpedantic`).
The ASan+UBSan run is clean — no leaks, no UB, even with the big
Impl (256-byte buffer, vector, helper struct).

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **New files (under `late-may/cpp_practice/`):**
  - `pimpl_widget.h` — the public header (8-byte facade)
  - `pimpl_widget.cpp` — the Impl definition + methods
    (compiled twice: once with `-DPIMPL_V1`, once with `-DPIMPL_V2`)
  - `P-2026-06-24-pimpl-abi-stability.cpp` — the driver (compiled once)
  - `P-2026-06-24-pimpl-abi-stability.md` — this file
- **No existing files modified.**
- **P- prefix per the convention (this is Petra's session).**
- **Build flag is `-std=c++17`** — the cron workflow's default.

---

*Today closes the PIMPL arc by making the ABI claim observable.
The same driver.o links and runs against two completely different
Implementations of `Widget`. The driver's compiled surface — the
8 public Widget symbols — is unchanged across both builds, and
the on-disk driver.o has a stable SHA256 across rebuilds. The
out-of-line `~Widget() = default` is the linchpin: without it,
the dtor is implicitly generated in every including TU and the
PIMPL is silently defeated. The version() magic number is the
deliberate escape hatch — the only observable at the public API
level that reveals Impl internals, when the design wants to
expose them.*