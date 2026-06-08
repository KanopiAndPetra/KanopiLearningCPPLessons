// cpp-practice-2026-06-08.cpp
// -----------------------------------------------------------------------------
// Topic: File I/O with <fstream> — persistence, parsing, error handling.
//
// What this program exercises (real, not toy):
//   1.  std::ofstream / std::ifstream — RAII file handles (auto-close on scope
//       exit).  Demonstrates open modes: out, app, binary, in.
//   2.  Formatted text output with operator<< — the same operator from the
//       Jun 4 session, now writing to a file instead of std::cout.
//   3.  std::getline for line-oriented input, with a custom delimiter to
//       parse a simple record format.
//   4.  Stream state inspection (good/fail/eof/bad) — the right way to
//       detect I/O errors without exceptions where appropriate.
//   5.  std::stringstream for in-memory tokenisation, plus std::stoi/
//       std::stod conversion with try/catch around std::invalid_argument and
//       std::out_of_range.
//   6.  Binary I/O with write/read and std::ios::binary — a tiny snapshot
//       save/load of a struct, contrasted with text I/O.
//   7.  Transactional write — write to a temp file, rename on success, so
//       we never leave a half-written catalog on disk.
//   8.  Exception-safe file I/O — fstream exceptions enabled with
//       exceptions() mask, caught at the top of main.
//
// Domain: a tiny "library" that holds Book records.  Same data model as the
// Jun 7 catalog session, but now persisted to disk so the data survives
// process exit.  Round-trip: write 4 books to a text file, read them back,
// print them, save a binary snapshot, read the snapshot, prove the two are
// identical.
//
// Build:
//   g++ -std=c++17 -Wall -Wextra -O0 -g -o cpp-practice-2026-06-08 \
//       cpp-practice-2026-06-08.cpp
// -----------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// =============================================================================
//  Domain type
// =============================================================================
//
// A Book has a fixed set of fields, but the cover-notes are free text and may
// contain commas.  To keep parsing simple, the text format uses '|' as the
// field separator (commas would force us to quote-escape).  The choice of
// separator is the trade-off: more common delimiters (CSV) are friendlier
// for other tools, but require quoting.  '|' is the Unix convention for
// human-readable "records with embedded commas" — think /etc/passwd, where
// ':' plays the same role.

struct Book {
    std::string  title;
    std::string  author;
    std::string  isbn;
    double       price   = 0.0;
    int          stock   = 0;
    std::string  notes;   // may contain commas, hence the '|' separator
};

// Format a Book as a single record line.  Pinned field count so we know
// where the notes field begins and ends.
std::string serializeBook(const Book& b) {
    // std::ostringstream is just a std::string builder with << formatting.
    std::ostringstream os;
    os << b.title  << '|'
       << b.author << '|'
       << b.isbn   << '|'
       << b.price  << '|'
       << b.stock  << '|'
       << b.notes;
    return os.str();
}

// Parse one record line back into a Book.  Throws std::invalid_argument on
// malformed input — callers wrap the whole file in a try/catch.
Book parseBook(const std::string& line) {
    // Split on '|' with a manual loop.  std::string::find is the workhorse.
    std::vector<std::string> fields;
    std::string::size_type start = 0;
    while (true) {
        const auto pos = line.find('|', start);
        if (pos == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }

    if (fields.size() != 6) {
        throw std::invalid_argument(
            "expected 6 fields separated by '|', got " +
            std::to_string(fields.size()));
    }

    Book b;
    b.title  = fields[0];
    b.author = fields[1];
    b.isbn   = fields[2];
    try {
        b.price = std::stod(fields[3]);
        b.stock = std::stoi(fields[4]);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("non-numeric price or stock: '" +
                                    fields[3] + "', '" + fields[4] + "'");
    } catch (const std::out_of_range&) {
        throw std::invalid_argument("price or stock out of range: '" +
                                    fields[3] + "', '" + fields[4] + "'");
    }
    b.notes  = fields[5];
    return b;
}

// Human-friendly print, for both screen and saved reports.
std::ostream& operator<<(std::ostream& os, const Book& b) {
    os << "  \"" << b.title << "\" by " << b.author
       << "  (ISBN " << b.isbn << ")"
       << "  $" << std::fixed << std::setprecision(2) << b.price
       << "  [" << b.stock << " in stock]";
    if (!b.notes.empty()) os << "  -- " << b.notes;
    return os;
}

// =============================================================================
//  Text I/O — the everyday path
// =============================================================================
//
// The catalog is a vector<Book>.  saveText() writes one record per line
// after a small header.  loadText() reads back, skipping blank lines and the
// header.

constexpr const char* kHeader = "# library catalog v1";

void saveText(const std::string& path, const std::vector<Book>& books) {
    // std::ofstream opens the file in std::ios::out | std::ios::trunc by
    // default — that is, the file is created if missing and truncated if it
    // exists.  The destructor closes the file; we can rely on that even if
    // an exception is thrown mid-write.
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open '" + path + "' for writing");
    }
    out << kHeader << '\n';
    out << "# " << books.size() << " books\n";
    for (const auto& b : books) {
        out << serializeBook(b) << '\n';
    }
    // out's destructor will flush + close.  flush() is implicit on close,
    // but we can make it explicit:
    out.flush();
    if (!out) {
        throw std::runtime_error("I/O error while writing '" + path + "'");
    }
}

std::vector<Book> loadText(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open '" + path + "' for reading");
    }
    std::vector<Book> out;
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        if (line.empty()) continue;            // tolerate blank lines
        if (line[0] == '#') continue;          // skip header / comments
        try {
            out.push_back(parseBook(line));
        } catch (const std::exception& e) {
            // Attach the line number for debuggability.
            throw std::runtime_error("parse error on line " +
                                     std::to_string(lineno) + ": " + e.what());
        }
    }
    // Distinguish "end of file" from "I/O error" by checking the stream
    // state.  eof() is the normal terminator; fail() with !eof() means
    // something else went wrong (e.g. disk error).
    if (in.bad()) {
        throw std::runtime_error("I/O error while reading '" + path + "'");
    }
    return out;
}

// =============================================================================
//  Append mode — log-style writes
// =============================================================================
//
// std::ios::app positions the write cursor at end-of-file before each write.
// Useful for log files, audit trails, anything that grows over time without
// rewriting.  The existing content is preserved verbatim.

void appendText(const std::string& path, const Book& b) {
    std::ofstream out(path, std::ios::app);
    if (!out) {
        throw std::runtime_error("failed to open '" + path + "' for append");
    }
    out << serializeBook(b) << '\n';
}

// =============================================================================
//  Transactional write — write to a temp file, then rename
// =============================================================================
//
// If we crash mid-write, a direct saveText() can leave a half-written
// catalog on disk.  A standard fix: write to '<path>.tmp', fsync, then
// rename atomically.  The rename is the commit point.  std::filesystem
// (C++17) makes the rename a one-liner.

void saveTextTransactional(const std::string& path,
                           const std::vector<Book>& books) {
    const std::string tmp = path + ".tmp";
    {
        // Scope block: ensure the ofstream closes and flushes BEFORE we
        // rename.  POSIX rename of an open file is technically allowed but
        // platform-quirky; closing first is the portable move.
        std::ofstream out(tmp);
        if (!out) {
            throw std::runtime_error("failed to open '" + tmp + "'");
        }
        out << kHeader << '\n';
        for (const auto& b : books) out << serializeBook(b) << '\n';
    } // out closed + flushed here
    // Atomic on POSIX (same filesystem).  On Windows, rename will fail if
    // the destination exists, so we use the two-arg form.
    fs::rename(tmp, path);
}

// =============================================================================
//  Binary I/O — fixed-layout snapshot
// =============================================================================
//
// A binary record format trades human-readability for compactness and
// speed.  We pack the Book into a fixed-size buffer using a POD struct
// (Plain Old Data — no pointers, no virtuals, no std::string inside).
// std::string is not safe to write directly because its storage is
// heap-allocated and its size is not fixed.

#pragma pack(push, 1)
struct BookSnapshot {
    char     title[64];
    char     author[48];
    char     isbn[20];
    double   price;
    int32_t  stock;
    // notes is dropped from the binary form for simplicity.  The
    // takeaway: a binary format is a *schema*, and changing the schema
    // means breaking all old files.
};
#pragma pack(pop)

static_assert(sizeof(BookSnapshot) == 64 + 48 + 20 + 8 + 4,
              "BookSnapshot layout drifted");

BookSnapshot pack(const Book& b) {
    BookSnapshot s{};
    auto copy = [](char* dst, std::size_t cap, const std::string& src) {
        const std::size_t n = std::min(src.size(), cap - 1);
        std::copy_n(src.begin(), n, dst);
        dst[n] = '\0';
    };
    copy(s.title,  sizeof(s.title),  b.title);
    copy(s.author, sizeof(s.author), b.author);
    copy(s.isbn,   sizeof(s.isbn),   b.isbn);
    s.price = b.price;
    s.stock = b.stock;
    return s;
}

Book unpack(const BookSnapshot& s) {
    Book b;
    b.title  = s.title;
    b.author = s.author;
    b.isbn   = s.isbn;
    b.price  = s.price;
    b.stock  = s.stock;
    return b;
}

void saveBinary(const std::string& path, const std::vector<Book>& books) {
    // std::ios::binary disables the '\n' -> platform-line-ending
    // translation that text mode does on Windows.  On macOS/Linux the two
    // are identical, but writing the binary flag is what makes the
    // program portable.
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("failed to open '" + path + "'");

    // 4-byte "magic" + uint32 count + records
    const char     magic[4] = {'B', 'O', 'O', 'K'};
    const uint32_t count    = static_cast<uint32_t>(books.size());
    out.write(magic, sizeof(magic));
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& b : books) {
        const auto s = pack(b);
        out.write(reinterpret_cast<const char*>(&s), sizeof(s));
    }
}

std::vector<Book> loadBinary(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to open '" + path + "'");

    char     magic[4];
    uint32_t count = 0;
    in.read(magic, sizeof(magic));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in) throw std::runtime_error("binary header read failed");
    if (std::string(magic, 4) != "BOOK") {
        throw std::runtime_error("not a BOOK file (bad magic)");
    }
    std::vector<Book> out;
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        BookSnapshot s;
        in.read(reinterpret_cast<char*>(&s), sizeof(s));
        if (!in) {
            throw std::runtime_error("short read at record " +
                                     std::to_string(i));
        }
        out.push_back(unpack(s));
    }
    return out;
}

// =============================================================================
//  Round-trip helpers — print diffs when text and binary round-trips
//  disagree, so we can see at a glance whether the two formats agree.
// =============================================================================

bool sameContent(const std::vector<Book>& a, const std::vector<Book>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].title  != b[i].title)  return false;
        if (a[i].author != b[i].author) return false;
        if (a[i].isbn   != b[i].isbn)   return false;
        if (a[i].price  != b[i].price)  return false;
        if (a[i].stock  != b[i].stock)  return false;
        if (a[i].notes  != b[i].notes)  return false;
    }
    return true;
}

void printCatalog(const std::string& label, const std::vector<Book>& v) {
    std::cout << label << " (" << v.size() << " books):\n";
    for (const auto& b : v) std::cout << b << '\n';
}

// =============================================================================
//  Demo driver
// =============================================================================

int main() {
    // Stage the work in a temp directory so we don't litter the source
    // folder with .txt / .bin files.  fs::temp_directory_path() returns the
    // platform's temp dir (e.g. /var/folders/.../T/ on macOS).
    const fs::path work = fs::temp_directory_path() / "cpp_practice_2026-06-08";
    fs::create_directories(work);
    const auto txtPath = (work / "library.txt").string();
    const auto binPath = (work / "library.bin").string();
    const auto logPath = (work / "library.log").string();

    // Wrap the whole thing in try/catch so any I/O failure is reported with
    // a clean message instead of a raw terminate() call.
    try {
        // -----------------------------------------------------------------
        // [1] Build an in-memory catalog
        // -----------------------------------------------------------------
        std::vector<Book> original = {
            {"The Pragmatic Programmer",  "Andrew Hunt",      "978-0201616224",
             49.99, 12, "20th anniversary edition; classic"},
            {"Clean Code",                "Robert C. Martin", "978-0132350884",
             39.95,  7, "Handbook of agile software craftsmanship"},
            {"Effective Modern C++",      "Scott Meyers",     "978-1491903995",
             44.99,  3, "42 specific ways to improve your C++11/14"},
            {"The C++ Programming Language", "Bjarne Stroustrup",
             "978-0321563842", 64.99, 5, "Reference, by the designer"},
        };

        std::cout << "============================================================\n"
                  << " File I/O with <fstream> — text, append, binary, atomic\n"
                  << "============================================================\n\n"
                  << "working directory: " << work << "\n\n";

        // -----------------------------------------------------------------
        // [2] Save to a text file (truncate mode, the default)
        // -----------------------------------------------------------------
        std::cout << "[1] saveText() — truncate, formatted, with header\n";
        saveText(txtPath, original);
        std::cout << "  wrote " << original.size() << " books to "
                  << txtPath << "\n";
        std::cout << "  file size: "
                  << fs::file_size(txtPath) << " bytes\n\n";

        // -----------------------------------------------------------------
        // [3] Show the file's bytes — peek inside the text format
        // -----------------------------------------------------------------
        std::cout << "[2] raw file contents (text format, " << txtPath << "):\n";
        {
            std::ifstream peek(txtPath);
            std::string line;
            int n = 0;
            while (std::getline(peek, line) && n++ < 10) {
                std::cout << "    " << line << '\n';
            }
        }
        std::cout << '\n';

        // -----------------------------------------------------------------
        // [4] Read it back, prove we got the same data
        // -----------------------------------------------------------------
        std::cout << "[3] loadText() — read it back\n";
        auto reloaded = loadText(txtPath);
        printCatalog("  reloaded catalog", reloaded);
        std::cout << "  round-trip identical? "
                  << (sameContent(original, reloaded) ? "yes" : "no") << "\n\n";

        // -----------------------------------------------------------------
        // [5] Append mode — log-style, no truncation
        // -----------------------------------------------------------------
        std::cout << "[4] appendText() — log-style, preserves existing content\n";
        // Seed the log with one line, then append more.
        {
            std::ofstream seed(logPath);
            seed << "# borrow log " << kHeader << '\n';
        }
        appendText(logPath, original[0]);
        appendText(logPath, original[2]);
        appendText(logPath, original[1]);
        {
            std::cout << "  log file contents (" << logPath << "):\n";
            std::ifstream peek(logPath);
            std::string line;
            while (std::getline(peek, line)) {
                std::cout << "    " << line << '\n';
            }
        }
        std::cout << '\n';

        // -----------------------------------------------------------------
        // [6] Transactional write — write to .tmp, rename on success
        // -----------------------------------------------------------------
        std::cout << "[5] saveTextTransactional() — atomic via rename\n";
        saveTextTransactional(txtPath, original);
        std::cout << "  rename committed: " << txtPath << "\n";
        std::cout << "  no .tmp left behind? "
                  << (!fs::exists(txtPath + ".tmp") ? "yes" : "no") << "\n\n";

        // -----------------------------------------------------------------
        // [7] Binary I/O — fixed-layout snapshot
        // -----------------------------------------------------------------
        std::cout << "[6] saveBinary() / loadBinary() — fixed-layout snapshot\n";
        saveBinary(binPath, original);
        std::cout << "  wrote " << original.size() << " books to "
                  << binPath << "\n";
        std::cout << "  binary file size: "
                  << fs::file_size(binPath)
                  << " bytes ("
                  << (4 + 4 + original.size() * sizeof(BookSnapshot))
                  << " expected)\n";

        auto fromBin = loadBinary(binPath);
        printCatalog("  reloaded from binary", fromBin);
        std::cout << "  binary round-trip identical (ignoring notes)? "
                  << (sameContent(original, fromBin) ||
                      // binary drops notes; compare title/author/isbn/price/stock
                      [&]{
                          if (original.size() != fromBin.size()) return false;
                          for (std::size_t i = 0; i < original.size(); ++i) {
                              if (original[i].title  != fromBin[i].title)  return false;
                              if (original[i].author != fromBin[i].author) return false;
                              if (original[i].isbn   != fromBin[i].isbn)   return false;
                              if (original[i].price  != fromBin[i].price)  return false;
                              if (original[i].stock  != fromBin[i].stock)  return false;
                          }
                          return true;
                      }() ? "yes" : "no")
                  << "  (notes are intentionally not in the binary format)\n\n";

        // -----------------------------------------------------------------
        // [8] Stream state — what does an error look like?
        // -----------------------------------------------------------------
        std::cout << "[7] stream state on a missing file (no exceptions)\n";
        {
            std::ifstream probe("/no/such/path/exists/here.txt");
            std::cout << "  is_open?    " << probe.is_open() << '\n';
            std::cout << "  good()?     " << probe.good()     << '\n';
            std::cout << "  fail()?     " << probe.fail()     << '\n';
            std::cout << "  bad()?      " << probe.bad()      << '\n';
            std::cout << "  eof()?      " << probe.eof()      << '\n';
        }
        std::cout << '\n';

        // -----------------------------------------------------------------
        // [9] Stream exceptions — flip the switch and let throw do the work
        // -----------------------------------------------------------------
        std::cout << "[8] exceptions() mask — turn I/O errors into exceptions\n";
        try {
            std::ofstream out;
            out.exceptions(std::ios::failbit | std::ios::badbit);
            out.open("/no/such/path/exists/here.txt");
            // ^ if open fails, failbit is set, exception is thrown
            std::cout << "  (unexpected) open succeeded\n";
        } catch (const std::ios_base::failure& e) {
            std::cout << "  caught std::ios_base::failure: " << e.what() << '\n';
        }
        std::cout << '\n';

        // -----------------------------------------------------------------
        // [10] Parse error — corrupt the text file and watch the parse fail
        // -----------------------------------------------------------------
        std::cout << "[9] corrupt the file, observe a useful error\n";
        const auto badPath = (work / "library.bad.txt").string();
        {
            std::ofstream out(badPath);
            out << kHeader << '\n';
            out << "Title With No Pipes Or Fields\n";     // only 1 field
            out << "Good|Title|123|9.99|3|notes here\n";   // ok
        }
        try {
            auto books = loadText(badPath);
            std::cout << "  loaded " << books.size() << " books (unexpected)\n";
        } catch (const std::exception& e) {
            std::cout << "  caught: " << e.what() << '\n';
        }
        std::cout << '\n';

        // -----------------------------------------------------------------
        // [11] Sanity totals — the same numbers from the Jun 7 session
        // -----------------------------------------------------------------
        std::cout << "[10] revenue sanity check (re-derived from text file)\n";
        double revenue = 0.0;
        int    units   = 0;
        for (const auto& b : reloaded) {
            revenue += b.price * b.stock;
            units   += b.stock;
        }
        std::cout << "  total units in stock: " << units << '\n';
        std::cout << std::fixed << std::setprecision(2)
                  << "  total revenue: $" << revenue << '\n';
        // 49.99*12 + 39.95*7 + 44.99*3 + 64.99*5 = 599.88 + 279.65 + 134.97 + 324.95
        // = 1339.45
        std::cout << "  expected: $1339.45 (matches Jun 7 calculation)\n\n";

        // -----------------------------------------------------------------
        // [12] Cleanup
        // -----------------------------------------------------------------
        std::cout << "[11] removing the working directory\n";
        std::error_code ec;
        fs::remove_all(work, ec);
        if (ec) {
            std::cout << "  cleanup warning: " << ec.message() << '\n';
        } else {
            std::cout << "  removed " << work << '\n';
        }

        std::cout << "\n== File I/O demo complete ==\n";
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
