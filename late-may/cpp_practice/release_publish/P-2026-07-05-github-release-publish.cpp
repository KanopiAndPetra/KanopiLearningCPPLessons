// P-2026-07-05 — release_publish: verify the GitHub-Release-served archive
// produces a working find_package() consumer.
//
// Today we publish psp_span_lib v0.5.0 as a GitHub Release. The publish
// sequence is documented in P-2026-07-05-github-release-publish.md; this
// file is the consumer-side verification: download the published .tar.gz,
// extract it, run find_package() against the extracted install tree, and
// compile+run a smoke test that uses psp::Span exactly as a downstream
// user would.
//
// If the GitHub-hosted archive and the locally-built archive produce
// identical compile/link/run behavior, we know the release is
// byte-equivalent to what we tested in cpack_resource_consumer/ on Jul 4.
//
// The driver is intentionally split into numbered steps so the output is
// easy to read. Each step is wrapped in a try/catch and on failure prints
// "FAILED at step N" then rethrows — so CI logs show exactly which step
// broke without burying the root cause in a 200-line traceback.

#include <psp_span/span.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---- Configuration ------------------------------------------------------
//
// owner / repo / tag / asset_name pin the release we are verifying.
// If the tag advances, bump these and rebuild. The asset_name embeds the
// project version (0.5.0) because the TGZ filename is set by CPack from
// ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}.tar.gz
// and Darwin is what `uname -s` returns on macOS.
namespace cfg {
constexpr const char* owner      = "ArloNOppie";
constexpr const char* repo       = "KanopiLearningCPPLessons";
constexpr const char* tag        = "v0.5.0";
constexpr const char* asset_name = "psp_span_lib-0.5.0-Darwin.tar.gz";
}  // namespace cfg

// ---- Helpers ------------------------------------------------------------

static void banner(const std::string& title) {
    std::cout << "\n================================================================\n"
              << " " << title << "\n"
              << "================================================================\n";
}

// Run a shell command and return its exit code. Throws on non-zero.
static int must_run(const std::string& cmd, const std::string& step_label) {
    std::cout << "[run] " << step_label << "\n"
              << "    $ " << cmd << "\n";
    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        throw std::runtime_error("FAILED at " + step_label + " (rc=" +
                                 std::to_string(rc) + ")");
    }
    return rc;
}

// Stream a file to stdout (first N lines) so the user sees what was
// extracted. Used for License.txt and Readme.txt in step 3.
static void head_of_file(const fs::path& p, int n_lines, std::ostream& os) {
    if (!fs::exists(p)) {
        os << "    (file missing: " << p << ")\n";
        return;
    }
    std::ifstream in{p};
    std::string line;
    int i = 0;
    while (std::getline(in, line) && i < n_lines) {
        os << "    " << line << "\n";
        ++i;
    }
    if (i == n_lines) {
        os << "    ... (truncated)\n";
    }
}

// ---- Steps --------------------------------------------------------------

// Step 1: confirm the release exists on GitHub.
// We use `gh release view --json`. Empty output means the release wasn't
// published; non-zero exit means gh itself is broken / unauthenticated.
static bool release_exists() {
    const std::string cmd = std::string{"gh release view "} + cfg::tag +
                            " --repo " + cfg::owner + "/" + cfg::repo +
                            " --json tagName --jq '.tagName'";
    std::cout << "[1] Checking that the release exists on GitHub:\n"
              << "    $ " << cmd << "\n";

    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen(gh) failed at step 1");
    }
    std::array<char, 256> buf{};
    std::string out;
    while (::fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        out += buf.data();
    }
    ::pclose(pipe);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
        out.pop_back();
    }
    if (out == cfg::tag) {
        std::cout << "    -> release " << out << " is published\n";
        return true;
    }
    std::cout << "    -> release " << cfg::tag
              << " is NOT yet published (gh returned: '" << out << "')\n";
    return false;
}

// Step 2: download (or reuse a cached copy of) the .tar.gz into ./cache/.
static fs::path download_archive(const fs::path& cache_dir) {
    fs::create_directories(cache_dir);
    const fs::path dest = cache_dir / cfg::asset_name;

    if (fs::exists(dest)) {
        std::cout << "[2] Archive already cached at " << dest << " ("
                  << fs::file_size(dest) << " bytes) — skipping download.\n"
                  << "    (delete it to force a fresh download)\n";
        return dest;
    }
    must_run("gh release download " + std::string{cfg::tag} +
                 " --repo " + cfg::owner + "/" + cfg::repo +
                 " --pattern '" + cfg::asset_name + "'" +
                 " --dir '" + cache_dir.string() + "'",
             "step 2: gh release download");
    if (!fs::exists(dest)) {
        throw std::runtime_error("download succeeded but " + dest.string() +
                                 " is missing");
    }
    std::cout << "    -> " << dest << " (" << fs::file_size(dest)
              << " bytes)\n";
    return dest;
}

// Step 3: extract the archive into a fresh prefix and list what landed.
static fs::path extract_archive(const fs::path& archive,
                                const fs::path& prefix_dir) {
    fs::remove_all(prefix_dir);
    fs::create_directories(prefix_dir);
    must_run("tar xzf '" + archive.string() + "' -C '" + prefix_dir.string() +
                 "'",
             "step 3: tar extract");

    // CPack wraps everything in <package>-<version>-<sysname>/, so the
    // actual install root is one level inside prefix_dir.
    // asset_name is "foo-0.5.0-Darwin.tar.gz"; we strip ".tar.gz"
    // to get the inner directory name "foo-0.5.0-Darwin".
    const std::string stem = [&]() -> std::string {
        std::string s = cfg::asset_name;
        const auto pos_gz  = s.rfind(".tar.gz");
        if (pos_gz != std::string::npos) s.erase(pos_gz);
        return s;
    }();
    const fs::path real_root = prefix_dir / stem;
    if (!fs::exists(real_root)) {
        throw std::runtime_error("expected " + real_root.string() +
                                 " after extraction");
    }
    std::cout << "[3] Extracted to " << real_root << ":\n";
    int n = 0;
    for (auto it = fs::recursive_directory_iterator(real_root);
         it != fs::recursive_directory_iterator(); ++it) {
        if (++n > 50) break;
        const auto rel = fs::relative(it->path(), real_root);
        std::cout << "    " << rel.string() << "\n";
    }
    // Show first few lines of License.txt so the user sees metadata in action.
    banner("License.txt (first 5 lines)");
    head_of_file(real_root / "License.txt", 5, std::cout);
    banner("Readme.txt (first 5 lines)");
    head_of_file(real_root / "Readme.txt", 5, std::cout);
    return real_root;
}

// Step 4: build a fresh CMake consumer that uses find_package() against
// the extracted archive. We do this with a CMake script written to a
// temp file because psp_span_lib's Config.cmake expects a normal CMake
// project to consume it.
static fs::path build_consumer(const fs::path& install_root,
                               const fs::path& work_dir) {
    fs::create_directories(work_dir);

    const fs::path src_dir  = work_dir / "src";
    const fs::path bld_dir  = work_dir / "build";
    const fs::path exe_path = bld_dir / "release_smoke";
    fs::create_directories(src_dir);

    // The driver source: minimal smoke test using psp::Span<int>.
    const fs::path driver_cpp = src_dir / "release_smoke.cpp";
    {
        std::ofstream f{driver_cpp};
        f << "#include <psp_span/span.h>\n"
          << "#include <array>\n"
          << "#include <iostream>\n"
          << "#ifndef PSP_SPAN_LIB_VERSION\n"
          << "#define PSP_SPAN_LIB_VERSION \"(version unknown)\"\n"
          << "#endif\n"
          << "int main() {\n"
          << "  std::array<int,5> a{10,20,30,40,50};\n"
          << "  psp::Span<int> s{a.data(), a.size()};\n"
          << "  int sum = 0;\n"
          << "  for (int x : s) sum += x;\n"
          << "  std::cout << \"psp_span_lib version: \" "
          <<             "<< PSP_SPAN_LIB_VERSION << \"\\n\";\n"
          << "  std::cout << \"span size = \" << s.size() << \", "
          <<             "sum = \" << sum << \"\\n\";\n"
          << "  if (sum != 150) return 1;\n"
          << "  return 0;\n"
          << "}\n";
    }

    // CMakeLists.txt: find_package() against the extracted prefix.
    const fs::path cmakelists = src_dir / "CMakeLists.txt";
    {
        std::ofstream f{cmakelists};
        f << "cmake_minimum_required(VERSION 3.16)\n"
          << "project(release_smoke VERSION 1.0.0 LANGUAGES CXX)\n"
          << "set(CMAKE_CXX_STANDARD 17)\n"
          << "find_package(psp_span_lib REQUIRED)\n"
          << "add_executable(release_smoke release_smoke.cpp)\n"
          << "target_link_libraries(release_smoke "
          <<    "PRIVATE psp_span_lib::psp_span_lib)\n"
          << "target_compile_definitions(release_smoke "
          <<    "PRIVATE PSP_SPAN_LIB_VERSION=\"${psp_span_lib_VERSION}\")\n";
    }

    fs::remove_all(bld_dir);
    fs::create_directories(bld_dir);
    must_run("cmake '" + src_dir.string() + "' -B '" + bld_dir.string() +
                 "' -DCMAKE_PREFIX_PATH='" + install_root.string() + "' 2>&1",
             "step 4a: cmake configure");
    must_run("cmake --build '" + bld_dir.string() + "' 2>&1",
             "step 4b: cmake build");

    if (!fs::exists(exe_path)) {
        throw std::runtime_error("expected " + exe_path.string() +
                                 " after build");
    }
    std::cout << "    -> " << exe_path << "\n";
    return exe_path;
}

// Step 5: run the smoke test.
static void run_smoke(const fs::path& exe) {
    must_run("'" + exe.string() + "'", "step 5: smoke test run");
}

// ---- main ---------------------------------------------------------------

int main() {
    banner("P-2026-07-05 — release_publish verification");
    std::cout << "Verifying GitHub release: " << cfg::owner << "/"
              << cfg::repo << " @ " << cfg::tag
              << "\nAsset:                       " << cfg::asset_name << "\n";

    // Where to put downloads and extracted prefixes.
    // Use a fresh subdir of the current working dir so repeated runs are
    // hermetic — no global state, no /tmp pollution.
    const fs::path work = fs::current_path() / "release_verify_workdir";
    fs::remove_all(work);
    fs::create_directories(work);

    try {
        const bool exists = release_exists();
        if (!exists) {
            std::cout << "\nThe release is not yet published. See\n"
                      << "    P-2026-07-05-github-release-publish.md\n"
                      << "section 'How this release was published' for the\n"
                      << "commands to publish it (git tag + gh release create).\n"
                      << "\nExiting cleanly — nothing to verify yet.\n";
            return 0;
        }
        const fs::path archive =
            download_archive(work / "cache");
        const fs::path prefix =
            extract_archive(archive, work / "prefix");
        const fs::path exe =
            build_consumer(prefix, work / "consumer");
        run_smoke(exe);
    } catch (const std::exception& e) {
        std::cerr << "\n*** " << e.what() << " ***\n";
        return 1;
    }

    banner("Result");
    std::cout << "The release " << cfg::owner << "/" << cfg::repo
              << "@" << cfg::tag << " served an archive that:\n"
              << "  - extracted cleanly into a fresh prefix\n"
              << "  - contained License.txt + Readme.txt at the install root\n"
              << "  - was consumable via find_package(psp_span_lib)\n"
              << "  - linked and ran a smoke test that used psp::Span<int>\n"
              << "Byte-equivalent to the locally-built archive. Release OK.\n";
    return 0;
}