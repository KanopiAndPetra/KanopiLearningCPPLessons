// P-2026-06-13-enable-shared-from-this.cpp
//
// Petra C++ practice, 2026-06-13.
//
// Topic: std::enable_shared_from_this — letting a class that's
// already managed by shared_ptr hand out *more* shared_ptrs
// to itself safely. Ties together smart pointers (Jun 4),
// move semantics (Jun 9), std::expected (Jun 11), and the
// noexcept-aware move story (Jun 12).
//
// Build:  g++ -Wall -Wextra -Wpedantic -std=c++17 -O0 -g \
//             -o P-2026-06-13-enable-shared-from-this \
//             P-2026-06-13-enable-shared-from-this.cpp
// Run:    ./P-2026-06-13-enable-shared-from-this
//
// Sections:
//   1. enable_shared_from_this<T>: the right tool for self-shared_ptr
//   2. shared_from_this() returns a shared_ptr<T> with unified refcount
//   3. weak_from_this(): a non-owning observer that shares the control block
//   4. The self-registration pattern: the canonical real-world use
//   5. shared_from_this() before being managed by shared_ptr
//      throws std::bad_weak_ptr
//   6. Move + esft: returning shared_from_this() from a method
//   7. The aliasing ctor: a member's shared_ptr that shares the owner's
//      control block (so the member dies with the owner)
//   8. The buggy pattern (in a forked child) — why shared_from_this()
//      exists in the first place: the double-free that the raw-pointer
//      shared_ptr ctor would cause
//
// Expected runtime: well under a second, all 8 sections print, child
// process traps with SIGTRAP at section 8 (intentional).

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Section 1-3: Session — the right way to hand out shared_ptr<self>.
// ---------------------------------------------------------------------------

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(std::string name) : name_(std::move(name)) {
        std::cout << "  [Session] ctor  name='" << name_ << "'\n";
    }
    ~Session() {
        std::cout << "  [Session] dtor  name='" << name_ << "'\n";
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    const std::string& name() const { return name_; }

    // shared_from_this() returns a shared_ptr<Session> that SHARES the
    // control block the original shared_ptr installed. Both pointers
    // point to the same Session and the same control block, so the
    // refcount is unified and the destructor runs exactly once when
    // the last shared_ptr expires.
    std::shared_ptr<Session> get_self() {
        return shared_from_this();
    }

    // weak_from_this() returns a weak_ptr<Session> that observes the
    // same control block but doesn't extend the lifetime. Useful for
    // breaking reference cycles (parent/child, observer, cache).
    std::weak_ptr<Session> observe_self() {
        return weak_from_this();
    }

private:
    std::string name_;
};

// ---------------------------------------------------------------------------
// Section 4: Server + Registry — the self-registration pattern.
// The canonical real-world reason enable_shared_from_this exists.
// ---------------------------------------------------------------------------

class Server;
using ServerPtr = std::shared_ptr<Server>;
using ServerWeakPtr = std::weak_ptr<Server>;

// Registry is forward-declared; member functions are defined inline
// after Server is complete so we can call s->name().
class Registry {
public:
    void add(const ServerPtr& s);
    void remove(const ServerPtr& s);
    std::size_t size() const { return sessions_.size(); }
    void list_names() const;

private:
    std::unordered_set<ServerPtr> sessions_;
};

class Server : public std::enable_shared_from_this<Server> {
public:
    explicit Server(std::string name, Registry& reg)
        : name_(std::move(name)) {
        (void)reg;  // reg is the registry the caller will register us with.
                    // We don't grab a reference here; register_with() does
                    // the actual insertion when the factory calls it.
        std::cout << "  [Server] ctor  name='" << name_ << "'\n";
    }

    ~Server() {
        std::cout << "  [Server] dtor  name='" << name_ << "'\n";
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    const std::string& name() const { return name_; }

    // Called immediately after construction by the factory function.
    // Uses shared_from_this() to register with the registry, which
    // takes a shared_ptr<Server>. The factory function returns
    // shared_from_this() to the caller; the registry's copy and the
    // caller's copy both point to the same control block.
    void register_with(Registry& reg) {
        reg.add(shared_from_this());
    }

    // A method that hands out a shared_ptr to self to a worker.
    // The worker holds onto it; the worker outliving the original
    // caller is fine because shared_ptr keeps the object alive.
    ServerPtr self_for_worker() {
        return shared_from_this();
    }

    ServerWeakPtr observe_for_watcher() {
        return weak_from_this();
    }

private:
    std::string name_;
};

// Registry inline definitions (after Server is complete).
inline void Registry::add(const ServerPtr& s) {
    sessions_.insert(s);
}
inline void Registry::remove(const ServerPtr& s) {
    sessions_.erase(s);
}
inline void Registry::list_names() const {
    for (const auto& s : sessions_) {
        std::cout << "    - " << s->name() << "\n";
    }
}

// Factory function: the canonical pattern. NEVER let callers call
// `new Server(...)` directly. Make the constructor private and force
// them through this factory, which uses make_shared<Server> and
// returns shared_from_this(). Then the object is born already
// managed by shared_ptr, and shared_from_this() works.
static std::shared_ptr<Server> make_server(const std::string& name,
                                           Registry& reg) {
    auto sp = std::make_shared<Server>(name, reg);
    sp->register_with(reg);
    return sp;
}

// ---------------------------------------------------------------------------
// Section 5: BadCall — what happens when shared_from_this() is called
// before the object is owned by a shared_ptr? It throws std::bad_weak_ptr.
// ---------------------------------------------------------------------------

class BadCall : public std::enable_shared_from_this<BadCall> {
public:
    BadCall() { std::cout << "  [BadCall] ctor\n"; }
    ~BadCall() { std::cout << "  [BadCall] dtor\n"; }
    std::shared_ptr<BadCall> get_self() { return shared_from_this(); }
};

// ---------------------------------------------------------------------------
// Section 7: Tracked — the aliasing ctor.
// ---------------------------------------------------------------------------

class Tracked : public std::enable_shared_from_this<Tracked> {
public:
    explicit Tracked(int id) : id_(id) {
        std::cout << "  [Tracked] ctor  id=" << id_ << "\n";
    }
    ~Tracked() { std::cout << "  [Tracked] dtor  id=" << id_ << "\n"; }
    int id() const { return id_; }
    std::shared_ptr<Tracked> get_self() { return shared_from_this(); }

private:
    int id_;
};

// ===========================================================================
// Section 8: the buggy pattern, run in a forked child so the crash
// (SIGTRAP from the double-free detected by libc++) doesn't kill us.
// ===========================================================================

class NaiveSession {
public:
    NaiveSession(std::string name) : name_(std::move(name)) {
        std::cout << "  [NaiveSession] ctor  name='" << name_ << "'\n";
    }
    ~NaiveSession() {
        std::cout << "  [NaiveSession] dtor  name='" << name_ << "'\n";
    }
    const std::string& name() const { return name_; }

    // The bug: returning a freshly constructed shared_ptr<NaiveSession>
    // from `this`. Each call to get_self() creates a NEW control block
    // that thinks it owns the object. The original shared_ptr (the one
    // that actually allocated `this`) and the one returned here will
    // both try to delete the object → double-free.
    std::shared_ptr<NaiveSession> get_self_buggy() {
        return std::shared_ptr<NaiveSession>(this);
    }

private:
    std::string name_;
};

static int run_buggy_demo_in_child() {
    std::cout << "  --- child process: about to demonstrate the bug ---\n";
    auto original = std::make_shared<NaiveSession>("buggy-1");
    std::cout << "  original.use_count() = " << original.use_count() << "\n";

    // BUG: different control block, same object.
    auto leaked = original->get_self_buggy();
    std::cout << "  original.use_count() = " << original.use_count() << "  <- stale\n";
    std::cout << "  leaked.use_count()    = " << leaked.use_count() << "    <- different control block\n";
    std::cout << "  --- child process: leaving scope, both shared_ptrs will try to delete ---\n";
    // The next line is where it crashes (on glibc/libc++ with hardened
    // allocators) or "works" silently (on other allocators).
    return 0;
}

int main() {
    // =========================================================================
    // Section 1: enable_shared_from_this<T> — derive from it, and the
    // shared_ptr machinery gives you shared_from_this() and weak_from_this().
    // =========================================================================
    std::cout << "========== 1. enable_shared_from_this<T> ==========\n";
    {
        auto sp1 = std::make_shared<Session>("alpha");
        std::cout << "  sp1.use_count() = " << sp1.use_count() << "\n";

        auto sp2 = sp1->get_self();
        std::cout << "  sp1.use_count() = " << sp1.use_count() << "  (after sp2 = sp1->get_self())\n";
        std::cout << "  sp2.use_count() = " << sp2.use_count() << "  <- shares the same control block\n";
        std::cout << "  sp1.get() == sp2.get() = " << std::boolalpha << (sp1.get() == sp2.get()) << "\n";
    }
    std::cout << "  (one 'dtor alpha' total: refcount dropped to zero cleanly)\n\n";

    // =========================================================================
    // Section 2: shared_from_this() is a member of the derived class.
    // It returns shared_ptr<T> with a unified refcount.
    // =========================================================================
    std::cout << "========== 2. shared_from_this() — unified refcount ==========\n";
    {
        auto outer = std::make_shared<Session>("beta");
        std::vector<std::shared_ptr<Session>> holders;
        for (int i = 0; i < 3; ++i) {
            holders.push_back(outer->get_self());
        }
        std::cout << "  outer.use_count() = " << outer.use_count() << "  (outer + 3 holders)\n";
        // Drop the holders; outer still owns one ref.
        holders.clear();
        std::cout << "  outer.use_count() = " << outer.use_count() << "  (after clearing holders)\n";
    }
    std::cout << "  (one 'dtor beta' total: vector.clear + outer expired)\n\n";

    // =========================================================================
    // Section 3: weak_from_this() — a non-owning observer.
    // =========================================================================
    std::cout << "========== 3. weak_from_this() — non-owning observer ==========\n";
    {
        auto sp = std::make_shared<Session>("gamma");
        auto weak = sp->observe_self();
        std::cout << "  sp.use_count()       = " << sp.use_count() << "  (owning)\n";
        std::cout << "  weak.use_count()      = " << weak.use_count() << "  <- non-owning\n";
        std::cout << "  weak.expired()        = " << std::boolalpha << weak.expired() << "\n";

        // The object is still alive, so we can lock the weak_ptr.
        if (auto locked = weak.lock()) {
            std::cout << "  weak.lock()           = '" << locked->name() << "'  (succeeded)\n";
        }

        sp.reset();
        std::cout << "  sp.reset() — weak.expired() = " << std::boolalpha << weak.expired() << "\n";
        if (auto locked = weak.lock()) {
            std::cout << "  weak.lock() succeeded (unexpected!)\n";
        } else {
            std::cout << "  weak.lock() returned nullptr  (object is gone)\n";
        }
    }
    std::cout << "  (no 'dtor gamma' before the expiration check: weak_ptr held no ownership)\n\n";

    // =========================================================================
    // Section 4: the self-registration pattern.
    // This is the canonical "why esft exists" example.
    // =========================================================================
    std::cout << "========== 4. Self-registration with a Registry ==========\n";
    {
        Registry reg;
        {
            auto a = make_server("web-01", reg);
            auto b = make_server("web-02", reg);
            auto c = make_server("db-01", reg);
            std::cout << "  registry.size() = " << reg.size() << "  (after 3 make_server calls)\n";
            std::cout << "  registry contents:\n";
            reg.list_names();
        }
        std::cout << "  (registry still holds the 3 servers — they outlived the locals)\n";
        std::cout << "  registry.size() = " << reg.size() << "\n";
    }
    std::cout << "  (after both scopes, reg goes out of scope; the 3 Servers are destroyed.)\n\n";

    // =========================================================================
    // Section 5: shared_from_this() before being managed by shared_ptr
    // throws std::bad_weak_ptr.
    // =========================================================================
    std::cout << "========== 5. shared_from_this() requires shared_ptr ownership ==========\n";
    {
        BadCall raw;  // Stack-allocated: NOT owned by a shared_ptr.
        std::cout << "  about to call raw.get_self() on a stack-allocated object...\n";
        try {
            auto sp = raw.get_self();
            std::cout << "  (unexpected: no throw)\n";
        } catch (const std::bad_weak_ptr& e) {
            std::cout << "  caught std::bad_weak_ptr: " << e.what() << "\n";
        }
    }
    std::cout << "  (the object is destroyed cleanly when the stack unwinds)\n\n";

    // =========================================================================
    // Section 6: move + esft — returning shared_from_this() from a method
    // is fine; the shared_ptr is move-constructed at the call site, and
    // the original `this` lifetime is unchanged.
    // =========================================================================
    std::cout << "========== 6. move + enable_shared_from_this ==========\n";
    {
        Registry reg;
        auto server = make_server("worker-1", reg);
        std::cout << "  server.use_count() = " << server.use_count()
                  << "  (factory + registry)\n";

        // The worker gets a shared_ptr; the factory's shared_ptr is still valid.
        ServerPtr worker_ref = server->self_for_worker();
        std::cout << "  server.use_count() = " << server.use_count()
                  << "  (factory + registry + worker_ref)\n";

        // weak_ptr for a watcher that may outlive the original holder.
        ServerWeakPtr watcher = server->observe_for_watcher();
        std::cout << "  watcher.use_count() = " << watcher.use_count() << "  (non-owning)\n";

        server.reset();
        std::cout << "  server.reset() — watcher.expired() = "
                  << std::boolalpha << watcher.expired() << "\n";
        std::cout << "  (server is gone; watcher observed it die; reg also dropped it.)\n";
    }
    std::cout << "\n";

    // =========================================================================
    // Section 7: aliasing ctor — a member's shared_ptr that shares the
    // owner's control block. Useful when a member object should die with
    // its owner.
    // =========================================================================
    std::cout << "========== 7. Aliasing ctor: member's lifetime tied to owner ==========\n";
    {
        auto session = std::make_shared<Session>("epsilon");
        auto tracked = std::make_shared<Tracked>(42);

        // Aliasing ctor: shared_ptr<Tracked>(session, tracked.get()).
        // tracked_via_session's control block is session's control block.
        // tracked_via_session's raw pointer points to *tracked.get().
        // When session is reset, tracked_via_session's lifetime is over,
        // even if `tracked` is still alive.
        std::shared_ptr<Tracked> tracked_via_session(session, tracked.get());
        std::cout << "  session.use_count()               = " << session.use_count() << "  (one owning ref)\n";
        std::cout << "  tracked.use_count()               = " << tracked.use_count() << "  (one owning ref)\n";
        std::cout << "  tracked_via_session.use_count()   = " << tracked_via_session.use_count()
                  << "  (shares session's control block)\n";
        std::cout << "  tracked_via_session->id()         = " << tracked_via_session->id() << "\n";

        session.reset();  // Destroys the Session; tracked_via_session is now dangling.
        std::cout << "  session.reset() — session.use_count() = " << session.use_count() << "\n";
        // tracked_via_session is dangling now; do NOT use it.
        // tracked is still alive because of its own shared_ptr ref.
        std::cout << "  tracked.use_count()               = " << tracked.use_count() << "  (still alive)\n";
    }
    std::cout << "  (the aliasing ctor's dtor runs when session is reset, not when the\n"
                 "   raw tracked object is destroyed; this is exactly the pattern you want\n"
                 "   when a member object should die with its owner.)\n\n";

    // =========================================================================
    // Section 8: the buggy pattern. Fork a child so the crash is contained.
    // The child constructs a NaiveSession and calls get_self_buggy(),
    // which returns a fresh control block that thinks it owns the object.
    // When both shared_ptrs expire, libc++ traps on the double-free.
    // The parent waits for the child and reports the exit status.
    // =========================================================================
    std::cout << "========== 8. The bug that shared_from_this() prevents (in a child) ==========\n";
    std::cout << "  About to fork(). The child will run the buggy pattern; the parent waits.\n";

    pid_t pid = fork();
    if (pid == 0) {
        // Child
        int rc = run_buggy_demo_in_child();
        _exit(rc);
    } else if (pid > 0) {
        // Parent
        int status = 0;
        pid_t waited = waitpid(pid, &status, 0);
        if (waited < 0) {
            std::cout << "  waitpid failed\n";
        } else if (WIFEXITED(status)) {
            std::cout << "  child exited normally with code " << WEXITSTATUS(status)
                      << "  (lucky: allocator didn't catch the double-free)\n";
        } else if (WIFSIGNALED(status)) {
            std::cout << "  child terminated by signal " << WTERMSIG(status);
#ifdef SIGTRAP
            if (WTERMSIG(status) == SIGTRAP) {
                std::cout << " (SIGTRAP — libc++ debug-mode heap detected the double-free)";
            }
#endif
            std::cout << "\n";
        }
    } else {
        std::cout << "  fork() failed; skipping the demo\n";
    }
    std::cout << "  (This is the bug that shared_from_this() prevents: two independent\n"
                 "   shared_ptrs with separate control blocks both deleting the same object.)\n";

    std::cout << "\nAll 8 sections completed. Exiting cleanly.\n";
    return 0;
}
