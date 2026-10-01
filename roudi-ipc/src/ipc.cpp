#include "ipc.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>

namespace ipc {
namespace {
// Lock-free atomic operations are signal-safe, including delivery to a worker.
static_assert(std::atomic<bool>::is_always_lock_free);
std::atomic<bool> stopped=false;
void stop(int) { stopped.store(true,std::memory_order_relaxed); }
}
void install_signals() { struct sigaction a{}; a.sa_handler=stop; sigemptyset(&a.sa_mask); sigaction(SIGINT,&a,nullptr); sigaction(SIGTERM,&a,nullptr); }
bool running() { return !stopped.load(std::memory_order_relaxed); }
[[noreturn]] void fail(const char* what) { throw std::runtime_error(std::string(what)+": "+strerror(errno)); }
int checked(int value,const char* what) { if(value<0) fail(what); return value; }
Lock::Lock(int value):fd(value),held(flock(fd,LOCK_EX|LOCK_NB)==0) {
    if(!held && errno!=EWOULDBLOCK && errno!=EINTR) fail("flock");
}
Lock::~Lock() { if(held) flock(fd,LOCK_UN); }
uint64_t now_ns() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
uint64_t random_id() { uint64_t id=0; do { if(getrandom(&id,sizeof(id),0)!=sizeof(id)) fail("getrandom"); } while(!id); return id; }
uint64_t start_ticks(int pid) {
    std::ifstream f("/proc/"+std::to_string(pid)+"/stat"); std::string line; std::getline(f,line);
    auto end=line.rfind(')'); if(end==std::string::npos) return 0;
    std::istringstream in(line.substr(end+2)); std::string token;
    for(int field=3;field<=22;++field) { if(!(in>>token)) return 0; }
    try { return std::stoull(token); } catch(...) { return 0; }
}
bool dead(int fd) { pollfd p{fd,POLLIN,0}; return poll(&p,1,0)>0; }
Fd process_fd(const Slot& s) {
    if(s.pid<=0 || !s.start_ticks) return Fd{};
    Fd fd(static_cast<int>(syscall(SYS_pidfd_open,s.pid,0)));
    if(fd.n<0) {
        if(errno==ESRCH) return Fd{};
        fail("pidfd_open (Linux >=5.3 required)");
    }
    // Check AFTER opening pidfd: PID reuse before open is rejected; reuse after
    // open cannot change the task referenced by this fd. Socket identity adds a nonce.
    if(start_ticks(s.pid)!=s.start_ticks || dead(fd.n)) return Fd{};
    return fd;
}
std::string runtime_dir() {
    std::string path="/tmp/roudi-ipc-"+std::to_string(getuid());
    if(mkdir(path.c_str(),0700)<0 && errno!=EEXIST) fail("mkdir");
    struct stat st{}; checked(lstat(path.c_str(),&st),"lstat");
    if(!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&077)!=0) throw std::runtime_error("unsafe runtime directory");
    return path;
}
bool valid(const Registry& r) { return r.magic==magic && r.version==version && r.bytes==sizeof(r) && r.slots==slot_count; }
void register_producer(const Slot& s) {
    Fd fd(shm_open(registry_name,O_RDWR|O_CLOEXEC,0600));
    if(fd.n<0) { if(errno==ENOENT) return; fail("shm_open"); }
    Lock lock(fd.n); if(!lock.held) return;
    struct stat st{}; checked(fstat(fd.n,&st),"fstat"); if(st.st_size!=sizeof(Registry)) return;
    Map<Registry> map(fd.n); auto& r=*map.p; if(!valid(r)) return;
    Slot* empty=nullptr;
    for(auto& e:r.entries) {
        if(load(e.committed)==1 && e.identity==s.identity && e.pid==s.pid && e.start_ticks==s.start_ticks) { e.heartbeat_ns=now_ns(); return; }
        if(load(e.committed)!=1 && !empty) empty=&e;
    }
    if(!empty) return; // Full registry: retry; consumer reclaims dead slots.
    store(empty->committed,0);
    // Atomic commit prevents a killed writer leaving a partially visible record.
    std::memcpy(reinterpret_cast<char*>(empty)+8,reinterpret_cast<const char*>(&s)+8,sizeof(Slot)-8);
    store(empty->committed,1);
}
static sockaddr_un address(const char* path) {
    sockaddr_un a{}; a.sun_family=AF_UNIX;
    if(strnlen(path,sizeof(a.sun_path))>=sizeof(a.sun_path)) throw std::runtime_error("socket path too long");
    std::strcpy(a.sun_path,path); return a;
}
Fd listener(const std::string& path) {
    Fd fd(checked(socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0),"socket"));
    auto a=address(path.c_str()); checked(bind(fd.n,reinterpret_cast<sockaddr*>(&a),sizeof(a)),"bind");
    checked(listen(fd.n,16),"listen"); return fd;
}
static bool ready(int fd,short events) { pollfd p{fd,events,0}; return poll(&p,1,100)>0 && (p.revents&events); }
static bool peer(int fd,int expected=0) {
    ucred cred{}; socklen_t size=sizeof(cred);
    return getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&cred,&size)==0 && cred.uid==getuid() && (!expected || cred.pid==expected);
}
void serve(int listenfd,int memfd,uint64_t identity) {
    // Bound work per iteration so an idle client cannot starve publishing forever.
    Fd client(accept4(listenfd,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK));
    if(client.n<0) { if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR) return; fail("accept"); }
    if(!peer(client.n)||!ready(client.n,POLLIN)) return;
    Wire q{}; auto bytes=recv(client.n,&q,sizeof(q),MSG_TRUNC);
    bool ok=bytes==sizeof(q)&&q.magic==magic&&q.version==version&&q.bytes==sizeof(q)&&q.kind==1&&q.identity==identity&&q.status==0&&q.region_bytes==0;
    Wire response{magic,version,sizeof(Wire),2,identity,q.nonce,ok?0U:1U,ok?uint32_t(sizeof(Region)):0U};
    iovec io{&response,sizeof(response)}; msghdr msg{}; msg.msg_iov=&io; msg.msg_iovlen=1;
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    if(ok) {
        msg.msg_control=control; msg.msg_controllen=sizeof(control);
        auto* c=CMSG_FIRSTHDR(&msg); c->cmsg_level=SOL_SOCKET; c->cmsg_type=SCM_RIGHTS; c->cmsg_len=CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(c),&memfd,sizeof(memfd));
    }
    (void)sendmsg(client.n,&msg,MSG_NOSIGNAL);
}
Fd acquire(const Slot& s) {
    Fd socketfd(checked(socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0),"socket"));
    auto a=address(s.socket_path);
    if(connect(socketfd.n,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0) return Fd{};
    if(!peer(socketfd.n,s.pid)) return Fd{};
    Wire q{magic,version,sizeof(Wire),1,s.identity,random_id(),0,0};
    if(send(socketfd.n,&q,sizeof(q),MSG_NOSIGNAL)!=sizeof(q)||!ready(socketfd.n,POLLIN)) return Fd{};
    Wire response{}; iovec io{&response,sizeof(response)};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int)*8)]{};
    msghdr msg{}; msg.msg_iov=&io; msg.msg_iovlen=1; msg.msg_control=control; msg.msg_controllen=sizeof(control);
    auto bytes=recvmsg(socketfd.n,&msg,MSG_CMSG_CLOEXEC);
    Fd received; unsigned count=0; bool ancillary_ok=true;
    for(auto* c=CMSG_FIRSTHDR(&msg);c;c=CMSG_NXTHDR(&msg,c)) {
        if(c->cmsg_level!=SOL_SOCKET || c->cmsg_type!=SCM_RIGHTS || c->cmsg_len<CMSG_LEN(0)) { ancillary_ok=false; continue; }
        auto n=(c->cmsg_len-CMSG_LEN(0))/sizeof(int);
        for(size_t i=0;i<n;++i) { int fd; std::memcpy(&fd,CMSG_DATA(c)+i*sizeof(int),sizeof(fd)); Fd item(fd); ++count; if(count==1) received=std::move(item); }
    }
    if(bytes!=sizeof(response)||(msg.msg_flags&(MSG_TRUNC|MSG_CTRUNC))||!ancillary_ok||count!=1||response.magic!=magic||response.version!=version||response.bytes!=sizeof(Wire)||response.kind!=2||response.status||response.identity!=s.identity||response.nonce!=q.nonce||response.region_bytes!=sizeof(Region)) return Fd{};
    struct stat st{}; if(fstat(received.n,&st)<0||st.st_size!=sizeof(Region)) return Fd{};
    int seals=fcntl(received.n,F_GET_SEALS); constexpr int required=F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_SEAL;
    if(seals<0||(seals&required)!=required) return Fd{};
    return received;
}
void publish(Region& r,uint64_t seq,Quote q) {
    auto& c=r.cells[(seq-1)%capacity];
    store(c.stamp,seq*2-1); store(c.packed_quote,(uint64_t(q.price)<<32)|q.quantity);
    store(c.stamp,seq*2); store(r.published,seq);
}
bool read(const Region& r,uint64_t seq,Quote& q) {
    const auto& c=r.cells[(seq-1)%capacity];
    if(load(c.stamp)!=seq*2) return false;
    auto packed=load(c.packed_quote); if(load(c.stamp)!=seq*2) return false;
    q={uint32_t(packed>>32),uint32_t(packed)}; return true;
}
}
