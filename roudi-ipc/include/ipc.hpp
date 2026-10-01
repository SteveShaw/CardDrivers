#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <sys/mman.h>
#include <unistd.h>

namespace ipc {
inline constexpr char registry_name[] = "/roudi";
inline constexpr uint32_t magic = 0x524f5544, version = 1;
inline constexpr unsigned slot_count = 64, capacity = 256;
struct Quote { uint32_t price, quantity; };
// All layouts are native-endian, same-build Linux ABI. Only fixed-width POD data.
struct Slot {
    alignas(8) uint64_t committed; // 0 = free/incomplete; 1 = published
    uint64_t identity, start_ticks, heartbeat_ns;
    int32_t pid;
    uint32_t reserved;
    char socket_path[108];
    uint32_t padding;
};
struct Registry {
    uint32_t magic, version, bytes, slots;
    uint64_t epoch;
    Slot entries[slot_count];
};
struct Cell { alignas(8) uint64_t stamp, packed_quote; };
struct Region {
    uint32_t magic, version, bytes, capacity;
    uint64_t identity;
    alignas(8) uint64_t published;
    Cell cells[ipc::capacity];
};
// One SOCK_SEQPACKET packet; explicit fields, no implicit padding on supported ABI.
struct Wire {
    uint32_t magic, version, bytes, kind; // 1=request, 2=response
    uint64_t identity, nonce;
    uint32_t status, region_bytes; // 0=OK, 1=bad request
};
static_assert(sizeof(Quote)==8 && sizeof(Slot)==152 && sizeof(Wire)==40);
static_assert(sizeof(Registry)==9752 && sizeof(Region)==4128);
static_assert(std::atomic_ref<uint64_t>::is_always_lock_free);
inline uint64_t load(const uint64_t& x) {
    return std::atomic_ref<uint64_t>(const_cast<uint64_t&>(x)).load();
}
inline void store(uint64_t& x, uint64_t value) { std::atomic_ref<uint64_t>(x).store(value); }
struct Fd {
    int n=-1;
    explicit Fd(int value=-1): n(value) {}
    ~Fd() { if(n>=0) close(n); }
    Fd(const Fd&)=delete;
    Fd& operator=(const Fd&)=delete;
    Fd(Fd&& other) noexcept: n(std::exchange(other.n,-1)) {}
    Fd& operator=(Fd&& other) noexcept { std::swap(n,other.n); return *this; }
};
template<class T> struct Map {
    T* p=nullptr;
    explicit Map(int fd) {
        void* a=mmap(nullptr,sizeof(T),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
        if(a==MAP_FAILED) fail("mmap");
        p=static_cast<T*>(a);
    }
    ~Map() { if(p) munmap(p,sizeof(T)); }
    Map(const Map&)=delete;
    Map& operator=(const Map&)=delete;
    static void fail(const char*);
};
[[noreturn]] void fail(const char* what);
template<class T> void Map<T>::fail(const char* what) { ipc::fail(what); }
int checked(int result,const char* what);
struct Lock {
    int fd; bool held;
    explicit Lock(int fd);
    ~Lock();
};
uint64_t now_ns();
uint64_t random_id();
uint64_t start_ticks(int pid);
Fd process_fd(const Slot& slot);
bool dead(int pidfd);
std::string runtime_dir();
void install_signals();
bool running();
bool valid(const Registry& r);
void register_producer(const Slot& slot);
Fd listener(const std::string& path);
void serve(int listener,int memfd,uint64_t identity);
Fd acquire(const Slot& slot);
void publish(Region& r, uint64_t sequence,Quote quote);
bool read(const Region& r,uint64_t sequence,Quote& quote);
}
