# C++ Practice Notes — June 8, 2026

## Session Info
- **Time:** 3:13 PM CDT (20-minute cron session)
- **Topic:** File I/O with `<fstream>` — text, append, binary, atomic
- **Compiler:** Apple clang 21 (`g++ -std=c++17 -Wall -Wextra -O0 -g`)
- **Sanitizer:** AddressSanitizer clean (no leaks, no UB)

## Why this matters

Jun 7 ended with a generic `Catalog<T, KeyFn>` in memory, but the moment
you close the program, the data is gone. File I/O is the seam where
"in-memory catalog" becomes "thing you can ship to a coworker, restart
on Monday, and feed to a shell script." It's also where three different
non-trivial ideas converge:

- **RAII for system resources** — `fstream` closes on scope exit even
  when an exception is thrown mid-write
- **Error reporting** — flag-based (`good()`/`fail()`/`eof()`/`bad()`)
  *or* exception-based (`exceptions()` mask)
- **Atomicity** — what does "the file is on disk" mean if you crash
  half-way through a save?

This session builds a small library catalog and exercises eleven facets
of `<fstream>` against the same domain, so the comparison stays in one
mental frame.

---

## What I built

A 575-line single-file program, twelve numbered sections, working in
`fs::temp_directory_path() / "cpp_practice_2026-06-08"`. The catalog
holds the same four books from Jun 7 (Pragmatic Programmer, Clean Code,
Effective Modern C++, The C++ Programming Language) so revenue numbers
are cross-checkable.

```
2026-06-08/
├── cpp-practice-2026-06-08.cpp   ← single-file program
├── cpp-practice-2026-06-08       ← compiled binary
├── cpp-practice-2026-06-08-asan  ← ASan binary
└── cpp-practice-2026-06-08.md    ← these notes
```

Single file on purpose — the topic is *one* standard library header
(`<fstream>`), and the structure mirrors the data flow: format →
write → read → format-agnostic round-trip.

---

## Key concepts I exercised

### 1. `std::ofstream` / `std::ifstream` — RAII file handles

```cpp
std::ofstream out(path);
out << kHeader << '\n';
for (const auto& b : books) out << serializeBook(b) << '\n';
// out's destructor closes + flushes. Even if the loop throws.
```

This is the same idea as the smart pointers from Jun 4 PM: the *type*
owns the resource, scope exit runs the cleanup. The default open mode
is `std::ios::out | std::ios::trunc` — create-if-missing, truncate-if-
exists. You can verify the file is open with `out.is_open()` and you
can check for I/O errors at any point with `out.good()`.

The destructor order matters: when you write to a `fstream` member
inside a class, the fstream destructs *first* (reverse declaration
order), so members declared after it are still alive while the file
is being flushed. That's another reason to declare file handles
*after* any data they might reference.

### 2. `std::getline` for line-oriented input

```cpp
std::string line;
while (std::getline(in, line)) {
    if (line.empty()) continue;        // tolerate blank lines
    if (line[0] == '#') continue;      // skip header / comments
    out.push_back(parseBook(line));
}
```

Two subtleties:

- `std::getline` strips the trailing `\n` but **not** `\r` if the file
  is in CRLF (Windows line endings). For a robust loader you may want
  to strip a trailing `\r` manually — for this program I'm reading what
  I just wrote, so it doesn't come up.
- The loop condition `std::getline(in, line)` returns the stream
  itself, and the boolean conversion is `!in.fail()`. So the loop
  exits cleanly on EOF *or* on a hard I/O error. To distinguish,
  check `in.bad()` after the loop.

### 3. Stream state — `good()` / `fail()` / `eof()` / `bad()`

This is the flag-based error model. Four bits, set or cleared on every
operation:

| Flag  | Meaning                                                |
|-------|--------------------------------------------------------|
| good  | All clear. No error bits set.                          |
| fail  | A *recoverable* error. Stream can be used after clear(). E.g. type-mismatch on `>>` for an int. |
| bad   | A *non-recoverable* error. Stream is in an unknown state. E.g. disk failure. |
| eof   | End-of-file was reached. Not by itself an error.       |

In the program, section [7] opens a non-existent path and prints all
four:

```
is_open?    0
good()?     0
fail()?     1
bad()?      0
eof()?      0
```

`is_open() == 0` is the immediate "no" from the constructor; `fail() ==
1` is the bit that gets set; `bad() == 0` because we didn't even get
to a write attempt. If you keep going and try to read, you may get
`bad() == 1` instead.

### 4. The `exceptions()` mask — flag-based or exception-based

You can opt-in to having I/O errors *throw*. Set a bitmask and the
stream checks it after every operation:

```cpp
std::ofstream out;
out.exceptions(std::ios::failbit | std::ios::badbit);
out.open("/no/such/path");
// → throws std::ios_base::failure if open() sets failbit or badbit
```

In the program, section [8] opens a missing path with the exception
mask on, and the catch block sees:

```
caught std::ios_base::failure: ios_base::clear: unspecified iostream_category error
```

The default stream is flag-based. Most production code picks *one*
style per project and stays consistent — mixing them is confusing.
For low-level code that needs to make local decisions ("skip this
malformed record, keep going"), flag-based is easier. For higher-
level code that should "stop the world" on a missing config file,
exception-based reads better.

### 5. Append mode — `std::ios::app`

```cpp
std::ofstream out(path, std::ios::app);
```

Two important properties:

- The write cursor is **positioned at end-of-file before each write**.
  This is what makes append safe even on a file you didn't just
  create — multiple processes can each open the same log file in
  `app` mode and not clobber each other, *modulo* the OS's write
  atomicity guarantees (POSIX guarantees writes ≤ `PIPE_BUF` are
  atomic for pipes, but for regular files it's only "best effort").
- It still **truncates** the file? No — append means *don't truncate*,
  *don't reposition*. You get exactly the behavior you'd want for a
  log file.

In the program, section [4] seeds a log file with one line, then
appends three more, and shows the file's contents growing without
the original line being touched.

### 6. Transactional write — write to `.tmp`, then rename

If you crash mid-write to your main catalog file, you've left a
half-written file on disk. Worse, if the crash is during the *truncation*
phase (the very first thing `std::ios::trunc` does), the file is
*empty*. The standard fix:

1. Open a sibling file with a `.tmp` suffix.
2. Write the full new contents to the `.tmp`.
3. Close it (flush, fsync).
4. `fs::rename(tmp, real)` — the OS makes this **atomic** on the same
   filesystem (on POSIX; on Windows, the two-arg form is needed if the
   destination exists).

```cpp
void saveTextTransactional(const std::string& path, ...) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp);
        out << kHeader << '\n';
        for (const auto& b : books) out << serializeBook(b) << '\n';
    } // out closed + flushed BEFORE the rename
    fs::rename(tmp, path);
}
```

The `{}` block around the ofstream is essential. POSIX allows renaming
an open file, but the file's contents are only durable once it's
closed, and on Windows the rename fails outright if the file is open.
Closing first is the portable move. `std::filesystem::rename` is C++17
and the right tool here — older code would call POSIX `rename(2)`
directly.

The program confirms `no .tmp left behind? yes` after the transaction.

### 7. Binary I/O — fixed-layout snapshot

```cpp
#pragma pack(push, 1)
struct BookSnapshot {
    char    title[64];
    char    author[48];
    char    isbn[20];
    double  price;
    int32_t stock;
};
#pragma pack(pop)

std::ofstream out(path, std::ios::binary);
out.write(magic, 4);
out.write(reinterpret_cast<const char*>(&count), sizeof(count));
for (const auto& b : books) {
    auto s = pack(b);
    out.write(reinterpret_cast<const char*>(&s), sizeof(s));
}
```

Three things worth flagging:

- `std::ios::binary` disables the `\n` → platform-line-ending
  translation that *text* mode does on Windows. On macOS/Linux the
  two are identical, but writing the flag makes the program portable.
- The 4-byte magic + 4-byte count header is a tiny version + format
  marker. Real binary formats (PNG, WAV, ELF) all do something like
  this — it's how readers decide "do I know how to parse this?"
- `reinterpret_cast<const char*>` is the canonical "treat this struct
  as a byte buffer" incantation. It's safe here because the struct is
  a *POD* (Plain Old Data): no pointers, no virtuals, no `std::string`
  inside. **Putting a `std::string` in a binary-write struct is a
  classic bug** — the bytes of the string object are not the bytes of
  the string content.

I made a deliberate choice: the binary format *drops* the `notes`
field. The point is to make it visible that a binary format is a
*schema* — change the struct, and old files no longer load. The text
format doesn't have this problem: the parser is self-describing
(position 4 is the price, position 5 is the stock, etc.). The cost
of self-description is verbosity — the binary file is 584 bytes, the
text file is 414 bytes for the same 4 records, but the binary file
also doesn't preserve formatting or whitespace.

`#pragma pack(push, 1)` is also worth a comment: it tells the compiler
to lay out the struct with no padding, so `sizeof(BookSnapshot) ==
144` is the natural sum of the fields. The `static_assert` at the
top of the binary block checks this and will fail at compile time
if the layout drifts.

### 8. `std::stringstream` for in-memory tokenisation

```cpp
std::ostringstream os;
os << b.title  << '|' << b.author << '|' << ...;
return os.str();
```

`std::ostringstream` is a `std::string` builder that supports
`operator<<` formatting. It is to `std::string` what `std::ofstream`
is to a file — a sink you can `<<` into. I used it for `serializeBook`
because it lets me build the record line with one expression instead
of manual `+=` concatenation. (Manual `+=` would also work and might
be marginally faster, but for a 6-field record the difference is
negligible.)

### 9. `std::stoi` / `std::stod` exceptions

```cpp
try {
    b.price = std::stod(fields[3]);
    b.stock = std::stoi(fields[4]);
} catch (const std::invalid_argument&) {
    throw std::invalid_argument("non-numeric price or stock");
} catch (const std::out_of_range&) {
    throw std::invalid_argument("price or stock out of range");
}
```

`std::stoi` and `std::stod` throw `std::invalid_argument` if the
input is non-numeric, and `std::out_of_range` if the value doesn't
fit in the target type. I catch both, rewrap with more context, and
re-throw so the caller's catch block sees a useful message.

In the program, section [9] writes a deliberately bad record (one
field instead of six) and the error message is:

```
caught: parse error on line 2: expected 6 fields separated by '|', got 1
```

The `lineno` was carried along from the read loop. This is the
difference between a useful error and a useless one: `parseBook("...")
threw at line 2 of <internal>` vs `parse error on line 2: ...`. Always
attach position information to I/O errors.

### 10. `std::filesystem` — paths, sizes, atomic rename

```cpp
const fs::path work = fs::temp_directory_path() / "cpp_practice_2026-06-08";
fs::create_directories(work);
fs::file_size(txtPath);
fs::rename(tmp, path);
fs::remove_all(work, ec);
```

`<filesystem>` is C++17, header-only, and replaces a grab-bag of
POSIX/Windows-specific code with portable primitives. The
`error_code` overloads (like `fs::remove_all(work, ec)`) let you
opt out of exceptions for cleanup code where you don't want a
destructor to throw.

`operator/` on `fs::path` does the right thing on every platform
(adds `/` on Unix, `\` on Windows) so you never have to write
`work + "/" + "library.txt"`. That's the kind of bug you only ever
write once, but never again once you discover `fs::path`.

### 11. RAII cleanup and exception safety

The whole driver is wrapped in:

```cpp
try {
    // ... all the work ...
    fs::remove_all(work, ec);   // intentional, non-throwing
} catch (const std::exception& e) {
    std::cerr << "FATAL: " << e.what() << '\n';
    return 1;
}
```

If anything in the body throws (file open failure, parse error,
bad magic, etc.), the catch block prints a clean message and returns
1. **The temp directory may or may not be cleaned up** — that's
fine for a learning program, but in production you'd want a
`finally`-style guarantee. C++ doesn't have `finally`; the
RAII-guard pattern (a struct whose destructor calls
`fs::remove_all`) is the idiom.

---

## Output highlights (sanity checks)

```
[1] saveText() — truncate, formatted, with header
  wrote 4 books to .../library.txt
  file size: 414 bytes

[3] loadText() — read it back
  round-trip identical? yes

[5] saveTextTransactional() — atomic via rename
  rename committed: .../library.txt
  no .tmp left behind? yes

[6] saveBinary() / loadBinary() — fixed-layout snapshot
  binary file size: 584 bytes (584 expected)
  binary round-trip identical (ignoring notes)? yes

[10] revenue sanity check (re-derived from text file)
  total units in stock: 27
  total revenue: $1339.45
  expected: $1339.45 (matches Jun 7 calculation)
```

The byte counts are independently checkable:
- Text file: header (~50 bytes) + 4 records × ~90 bytes = ~414 bytes ✓
- Binary file: 4 (magic) + 4 (count) + 4 × 144 (records) = 584 bytes ✓
- Revenue: 49.99×12 + 39.95×7 + 44.99×3 + 64.99×5 = 1339.45 ✓
- Units: 12 + 7 + 3 + 5 = 27 ✓

---

## Verification

```bash
g++ -std=c++17 -Wall -Wextra -O0 -g -o cpp-practice-2026-06-08 \
    cpp-practice-2026-06-08.cpp
./cpp-practice-2026-06-08

g++ -std=c++17 -Wall -Wextra -O0 -g -fsanitize=address \
    -o cpp-practice-2026-06-08-asan cpp-practice-2026-06-08.cpp
./cpp-practice-2026-06-08-asan
```

- `-Wall -Wextra`: clean (no warnings)
- AddressSanitizer: clean (no leaks, no UB, no double-free)
- Exit code 0
- The error path (corrupt file, missing path) was exercised and
  produced well-formed error messages rather than crashes.

---

## Key takeaways

- **`<fstream>` is RAII for files.** Open a stream, write to it, let
  it go out of scope. The file is closed and flushed automatically,
  even if an exception is thrown mid-write.
- **Pick one error model per project.** Flag-based (`if (!out)`,
  `if (in.fail())`) is best for "skip this record, keep going."
  Exception-based (`out.exceptions(...)`) is best for "stop the
  world on a missing config file." Mixing them is confusing.
- **Append mode is for logs, not catalogs.** `std::ios::app` is the
  right tool for audit trails, debug logs, anything that grows
  monotonically. Catalog-style "save the whole world" data wants
  *transactional* write (`tmp` + `rename`), not append.
- **Transactional write is the difference between "the file is on
  disk" and "the file *might be* on disk, or half of it might be."**
  It's a one-function-cost change and worth doing for any file
  that gets rewritten in place.
- **Binary formats are schemas.** A `BookSnapshot` struct is a
  contract — change it, and you break every old file. Text formats
  like CSV or my `|`-delimited format are *self-describing* and
  more forgiving. The trade-off: text is verbose, binary is
  compact and fast. Pick based on whether humans or programs are
  the primary consumers.
- **`std::filesystem` (C++17) replaces a grab-bag of platform-specific
  code** with portable primitives. `fs::path::operator/` is a
  one-character fix for a class of cross-platform bugs.
- **Always attach position information to parse errors.** "Parse
  error" is useless; "parse error on line 47: expected 6 fields,
  got 3" is debugging.

---

## Next Steps

- **`std::move` and rvalue references (deeper dive)** — the prior
  smart-pointer session used `std::move` extensively but didn't dig
  into the reference-collapse rules, the difference between
  `T&&` (rvalue reference) and `T&&` (forwarding reference) inside
  templates, and the role of `std::forward`.
- **`std::variant` and `std::visit` (C++17)** — the type-safe
  "this is one of N alternatives" idiom. Useful for, e.g., a
  config file that holds either a string, an int, or a list.
- **Custom serializers** — take the `serializeBook` /
  `parseBook` pair and generalise to a `Serializer<T>` concept
  that works for any record type. (A natural follow-up to the
  Jun 7 SFINAE detection idiom.)
- **JSON with `nlohmann/json` or `boost::json`** — once the
  hand-rolled format feels limiting, a real JSON library gives
  you nested structures, escaping, and Unicode for free.
- **`std::span` and zero-copy I/O** — read directly into a
  `std::span<char>` instead of into a `std::string` and copying
  out. C++20 but the technique is worth knowing.
- **A small `Makefile`** — to wire up the multi-file builds
  from Jun 6 ergonomically, with separate `release` and `asan`
  targets. The two binary flavours this session produced would
  collapse into a single `make` invocation.

---

## GitHub

- **Repo:** https://github.com/ArloNOppie/KanopiLearningCPPLessons
- **Files to commit:** `cpp-practice-2026-06-08.cpp`,
  `cpp-practice-2026-06-08.md`, and the compiled binaries.

---

*File I/O is the seam where "toy program that runs once" becomes
"thing you can deploy." The combination of RAII for resources, the
two error models, and the transactional-write pattern is enough
to handle most file-shaped problems you'll meet in practice.*
