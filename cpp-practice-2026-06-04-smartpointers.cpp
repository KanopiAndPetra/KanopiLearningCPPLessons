/**
 * C++ Practice: Smart Pointers
 * Date: June 4, 2026
 *
 * Demonstrates the three main smart pointer types in modern C++:
 *   - std::unique_ptr : sole ownership, non-copyable
 *   - std::shared_ptr : shared ownership via reference counting
 *   - std::weak_ptr   : non-owning observer of a shared_ptr
 *
 * Also covers:
 *   - make_unique / make_shared factories
 *   - custom deleters
 *   - moving unique_ptr between scopes/functions
 *   - cyclic reference problem solved by weak_ptr
 *   - aliasing constructors
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <utility>

// -----------------------------------------------------------------------------
// A small domain type: a Project owns Resources. Used to demonstrate ownership
// transfer and shared ownership in a realistic (if simplified) shape.
// -----------------------------------------------------------------------------
struct Resource {
    std::string name;
    explicit Resource(const std::string& n) : name(n) {
        std::cout << "  [ctor] Resource acquired: " << name << "\n";
    }
    ~Resource() { std::cout << "  [dtor] Resource released: " << name << "\n"; }
};

struct Project {
    std::string title;
    std::unique_ptr<Resource> primary;     // sole ownership
    std::shared_ptr<Resource> sharedTool;  // shared with other systems
    std::weak_ptr<Resource>   cachedPeer;  // non-owning; may dangle

    Project(const std::string& t) : title(t) {
        std::cout << "  [ctor] Project created: " << title << "\n";
    }
    ~Project() { std::cout << "  [dtor] Project destroyed: " << title << "\n"; }
};

// -----------------------------------------------------------------------------
// Helper: function that "consumes" a unique_ptr (takes ownership).
// Returning it transfers ownership back to the caller.
// -----------------------------------------------------------------------------
std::unique_ptr<Resource> buildResource(const std::string& name) {
    auto r = std::make_unique<Resource>(name);
    return r; // move out
}

// Custom deleter used to show non-trivial cleanup
struct Releaser {
    void operator()(Resource* r) const {
        std::cout << "  [custom deleter] Releasing via functor: " << r->name << "\n";
        delete r;
    }
};

int main() {
    std::cout << "============================================================\n";
    std::cout << " Smart Pointers in Modern C++\n";
    std::cout << "============================================================\n\n";

    // -----------------------------------------------------------------
    // 1) std::unique_ptr — sole, exclusive ownership
    // -----------------------------------------------------------------
    std::cout << "[1] unique_ptr: exclusive ownership\n";
    {
        std::unique_ptr<Resource> u1 = std::make_unique<Resource>("DB Connection");
        std::cout << "  u1 owns: " << u1->name << "\n";
        // std::unique_ptr<Resource> u2 = u1; // COMPILE ERROR — not copyable
        std::unique_ptr<Resource> u2 = std::move(u1); // ownership transfer
        std::cout << "  After move, u1 is " << (u1 ? "alive" : "empty") << "\n";
        std::cout << "  u2 now owns: " << u2->name << "\n";
    }
    std::cout << "  (block ended — Resource auto-destroyed)\n\n";

    // -----------------------------------------------------------------
    // 2) unique_ptr returned from a function
    // -----------------------------------------------------------------
    std::cout << "[2] unique_ptr moving in/out of functions\n";
    auto r = buildResource("Temp File Handle");
    std::cout << "  main now owns: " << r->name << "\n\n";

    // -----------------------------------------------------------------
    // 3) unique_ptr with a custom deleter
    // -----------------------------------------------------------------
    std::cout << "[3] unique_ptr with custom deleter\n";
    {
        std::unique_ptr<Resource, Releaser> custom(
            new Resource("Logger Sink"), Releaser{});
        std::cout << "  using custom deleter\n";
    }
    std::cout << "\n";

    // -----------------------------------------------------------------
    // 4) std::shared_ptr — shared ownership with reference counting
    // -----------------------------------------------------------------
    std::cout << "[4] shared_ptr: shared ownership\n";
    std::shared_ptr<Resource> s1;
    {
        auto s2 = std::make_shared<Resource>("Cache Entry");
        std::cout << "  use_count inside block: " << s2.use_count() << "\n";
        s1 = s2; // copy — both own the same Resource
        std::cout << "  use_count after copy to s1: " << s2.use_count() << "\n";
    }
    std::cout << "  block ended, but s1 still alive. use_count: "
              << s1.use_count() << "\n";
    std::cout << "  s1 owns: " << s1->name << "\n";
    s1.reset(); // explicit release
    std::cout << "  after reset, use_count: " << s1.use_count() << "\n\n";

    // -----------------------------------------------------------------
    // 5) shared_ptr in a container — last reference wins
    // -----------------------------------------------------------------
    std::cout << "[5] shared_ptr inside a vector\n";
    std::vector<std::shared_ptr<Resource>> pool;
    pool.push_back(std::make_shared<Resource>("Worker-1"));
    pool.push_back(std::make_shared<Resource>("Worker-2"));
    pool.push_back(std::make_shared<Resource>("Worker-3"));
    std::cout << "  pool size: " << pool.size() << "\n";
    pool.erase(pool.begin()); // drops one reference
    std::cout << "  after erase(begin), pool size: " << pool.size() << "\n\n";

    // -----------------------------------------------------------------
    // 6) std::weak_ptr — non-owning observer
    // -----------------------------------------------------------------
    std::cout << "[6] weak_ptr: non-owning observer\n";
    std::weak_ptr<Resource> observer;
    {
        auto shared = std::make_shared<Resource>("Ephemeral Cache");
        observer = shared; // weak_ptr does NOT increase refcount
        std::cout << "  use_count (shared owns): " << shared.use_count() << "\n";
        if (auto locked = observer.lock()) {
            std::cout << "  observer.lock() succeeded: " << locked->name << "\n";
        } else {
            std::cout << "  observer.lock() failed: object gone\n";
        }
    } // shared goes out of scope here
    std::cout << "  expired? " << (observer.expired() ? "yes" : "no") << "\n";
    if (auto locked = observer.lock()) {
        std::cout << "  unexpected: still alive\n";
    } else {
        std::cout << "  observer.lock() returned null — object gone\n";
    }
    std::cout << "\n";

    // -----------------------------------------------------------------
    // 7) Cyclic reference solved with weak_ptr
    // -----------------------------------------------------------------
    std::cout << "[7] weak_ptr breaks cycles\n";
    struct Node {
        std::string name;
        std::shared_ptr<Node> next;   // strong — would cause a cycle
        std::weak_ptr<Node>   prev;   // weak  — breaks the cycle
        explicit Node(const std::string& n) : name(n) {
            std::cout << "  [ctor] Node " << name << "\n";
        }
        ~Node() { std::cout << "  [dtor] Node " << name << "\n"; }
    };

    {
        auto a = std::make_shared<Node>("A");
        auto b = std::make_shared<Node>("B");
        a->next  = b; // shared
        b->prev  = a; // weak (not shared!)
        std::cout << "  a.use_count=" << a.use_count()
                  << "  b.use_count=" << b.use_count() << "\n";
    }
    std::cout << "  (both Nodes destroyed cleanly — no leak)\n\n";

    // -----------------------------------------------------------------
    // 8) aliasing constructor: shared_ptr to a member shares owner
    // -----------------------------------------------------------------
    std::cout << "[8] shared_ptr aliasing constructor\n";
    struct Pair {
        Resource first;
        Resource second;
        Pair(const std::string& a, const std::string& b)
            : first(a), second(b) {}
    };
    auto pairOwner = std::make_shared<Pair>("Alpha", "Beta");
    // shared_ptr aliases the embedded Resource, but owns the whole Pair
    std::shared_ptr<Resource> alias(pairOwner, &pairOwner->second);
    std::cout << "  alias points to: " << alias->name << "\n";
    std::cout << "  pairOwner.use_count (shared): " << pairOwner.use_count() << "\n";
    pairOwner.reset();
    if (alias) std::cout << "  alias survived pairOwner.reset() because pairOwner still has 1 ref via alias\n";

    // -----------------------------------------------------------------
    // 9) Practical demo: Project with mixed ownership
    // -----------------------------------------------------------------
    std::cout << "\n[9] Realistic Project composition\n";
    {
        Project p("Search Indexer");
        p.primary    = std::make_unique<Resource>("File Handle");
        p.sharedTool = std::make_shared<Resource>("Tokenizer");
        std::cout << "  primary owns: " << p.primary->name
                  << ", sharedTool use_count: " << p.sharedTool.use_count() << "\n";

        // Other module wants to use the same tokenizer
        std::shared_ptr<Resource> otherModule = p.sharedTool;
        std::cout << "  after sharing with other module, use_count: "
                  << p.sharedTool.use_count() << "\n";

        // Cache a weak reference for later lookup
        p.cachedPeer = p.sharedTool;
        if (auto live = p.cachedPeer.lock()) {
            std::cout << "  cached peer is live: " << live->name << "\n";
        }
    }
    std::cout << "  (Project destroyed; Resources released)\n";

    std::cout << "\n============================================================\n";
    std::cout << " End of smart pointers demo\n";
    std::cout << "============================================================\n";
    return 0;
}
