#include "ipc.hpp"
#include "linux_compat.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/vfs.h>

namespace ipc
{
namespace
{
// Lock-free atomic operations are signal-safe, including delivery to a worker.
static_assert(std::atomic<bool>::is_always_lock_free);
std::atomic<bool> stopped=false;
void stop(int)
{
    stopped.store(true,std::memory_order_relaxed);
}
}
void install_signals()
{
    struct sigaction a {};
    a.sa_handler=stop;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT,&a,nullptr);
    sigaction(SIGTERM,&a,nullptr);
}
bool running()
{
    return !stopped.load(std::memory_order_relaxed);
}
[[noreturn]] void fail(const char* what)
{
    throw std::runtime_error(std::string(what)+": "+strerror(errno));
}
int checked(int value,const char* what)
{
    if(value<0) fail(what);
    return value;
}
Lock::Lock(int value,LockMode mode):
    fd(value),
    held(flock(fd,(mode==LockMode::shared?LOCK_SH:LOCK_EX)|LOCK_NB)==0)
{
    if(!held && errno!=EWOULDBLOCK && errno!=EINTR) fail("flock");
}
Lock::~Lock()
{
    if(held) flock(fd,LOCK_UN);
}
uint64_t now_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
uint64_t random_id()
{
    // No sys/random.h or glibc getrandom wrapper dependency. IDs are generation
    // tokens, not secrets. /dev/urandom must be accessible to these processes.
    Fd fd(checked(open("/dev/urandom",O_RDONLY|O_CLOEXEC),"open /dev/urandom"));
    uint64_t id=0;
    do
    {
        size_t done=0;
        while(done<sizeof(id))
        {
            auto n=::read(fd.n,reinterpret_cast<char*>(&id)+done,sizeof(id)-done);
            if(n<0 && errno==EINTR) continue;
            if(n<0) fail("read /dev/urandom");
            if(n==0) throw std::runtime_error("unexpected EOF from /dev/urandom");
            done+=static_cast<size_t>(n);
        }
    }
    while(!id);
    return id;
}
uint64_t start_ticks(int pid)
{
    std::ifstream f("/proc/"+std::to_string(pid)+"/stat");
    std::string line;
    std::getline(f,line);
    auto end=line.rfind(')');
    if(end==std::string::npos) return 0;
    std::istringstream in(line.substr(end+2));
    std::string token;
    for(int field=3; field<=22; ++field)
    {
        if(!(in>>token)) return 0;
    }
    try
    {
        return std::stoull(token);
    }
    catch(...)
    {
        return 0;
    }
}
static bool enabled(const char* name)
{
    const char* value=std::getenv(name);
    return value && std::strcmp(value,"1")==0;
}
static bool force_legacy()
{
#ifdef ROUDI_DISABLE_MODERN_SYSCALLS
    return true;
#else
    return enabled("ROUDI_FORCE_LEGACY");
#endif
}
bool dead(const ProcessRef& process)
{
    if(!process.valid()) return true;
    if(process.is_pidfd)
    {
        pollfd p{process.fd.n,POLLIN,0};
        int result=poll(&p,1,0);
        if(result<0 && errno!=EINTR) fail("poll pidfd");
        return result>0;
    }
    // pread has no shared file position: safe when several symbol readers use
    // one process handle. The opened proc inode never switches to a reused PID.
    char buffer[4096];
    ssize_t count;
    do
    {
        count=pread(process.fd.n,buffer,sizeof(buffer)-1,0);
    }
    while(count<0 && errno==EINTR);
    if(count<0)
    {
        if(errno==ESRCH || errno==ENOENT) return true;
        fail("pread process stat");
    }
    if(count==0) return true;
    buffer[count]='\0';
    std::string line(buffer,static_cast<size_t>(count));
    const auto end=line.rfind(')');
    if(end==std::string::npos) throw std::runtime_error("invalid /proc stat");
    std::istringstream input(line.substr(end+2));
    char state=0;
    input>>state;
    if(state=='Z' || state=='X' || state=='x') return true;
    std::string token;
    for(int field=4; field<=22; ++field) if(!(input>>token)) throw std::runtime_error("short /proc stat");
    return std::stoull(token)!=process.start;
}
ProcessRef process_ref(const Slot& s)
{
    if(s.pid<=0 || !s.start_ticks) return {};
#if defined(SYS_pidfd_open) && !defined(ROUDI_DISABLE_MODERN_SYSCALLS)
    if(!force_legacy())
    {
        Fd fd(static_cast<int>(syscall(SYS_pidfd_open,s.pid,0)));
        if(fd.n>=0)
        {
            ProcessRef result{std::move(fd),s.start_ticks,true};
            if(start_ticks(s.pid)!=s.start_ticks || dead(result)) return {};
            return result;
        }
        if(errno==ESRCH) return {};
        if(errno!=ENOSYS && errno!=EINVAL && errno!=EPERM) fail("pidfd_open");
    }
#endif
    Fd fd(open(("/proc/"+std::to_string(s.pid)+"/stat").c_str(),O_RDONLY|O_CLOEXEC));
    if(fd.n<0)
    {
        if(errno==ENOENT || errno==ESRCH) return {};
        fail("open process stat");
    }
    ProcessRef result{std::move(fd),s.start_ticks,false};
    if(dead(result)) return {};
    return result;
}
Fd create_quote_memory()
{
#if defined(SYS_memfd_create) && !defined(ROUDI_DISABLE_MODERN_SYSCALLS)
    if(!force_legacy())
    {
        // Direct syscall avoids depending on the newer glibc memfd_create wrapper.
        Fd fd(static_cast<int>(syscall(SYS_memfd_create,"quotes",MFD_CLOEXEC|MFD_ALLOW_SEALING)));
        if(fd.n>=0)
        {
            checked(ftruncate(fd.n,sizeof(Region)),"ftruncate memfd");
            if(fcntl(fd.n,F_ADD_SEALS,F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_SEAL)>=0) return fd;
            if(errno!=EINVAL && errno!=ENOSYS && errno!=EOPNOTSUPP) fail("seal memfd");
        }
        else if(errno!=ENOSYS && errno!=EINVAL) fail("memfd_create syscall");
    }
#else
    (void)force_legacy();
#endif
    if(!enabled("ROUDI_ALLOW_UNSEALED_SHM"))
        throw std::runtime_error("sealed memfd unavailable; upgrade kernel/headers or explicitly set ROUDI_ALLOW_UNSEALED_SHM=1 in all processes");
    // An immediately unlinked POSIX shm object retains FD/mmap lifetime semantics,
    // but the old kernel cannot enforce a fixed size. This is opt-in, not silent.
    for(unsigned attempt=0; attempt<32; ++attempt)
    {
        const auto name="/roudi-quotes-"+std::to_string(getuid())+"-"+std::to_string(random_id());
        Fd fd(shm_open(name.c_str(),O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600));
        if(fd.n<0)
        {
            if(errno==EEXIST) continue;
            fail("quote shm_open");
        }
        checked(shm_unlink(name.c_str()),"quote shm_unlink");
        checked(ftruncate(fd.n,sizeof(Region)),"ftruncate quote shm");
        return fd;
    }
    throw std::runtime_error("unable to create unique quote shm");
}
bool valid_quote_memory(int fd)
{
    struct stat st {};
    if(fstat(fd,&st)<0 || st.st_size!=sizeof(Region)) return false;
    const int seals=fcntl(fd,F_GET_SEALS);
    const int seal_error=errno;
    constexpr int required=F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_SEAL;
    if(seals>=0 && (seals&required)==required) return true;
    if(!enabled("ROUDI_ALLOW_UNSEALED_SHM")) return false;
    // Only accept our unlinked POSIX shm fallback on tmpfs, not an arbitrary
    // regular file or an unsealed memfd. Same-UID peers are still trusted.
    if(!((seals<0 && seal_error==EINVAL) || seals==F_SEAL_SEAL)) return false;
    struct statfs fs {};
    if(fstatfs(fd,&fs)<0 || fs.f_type!=0x01021994 || st.st_nlink!=0 || st.st_uid!=getuid() || !S_ISREG(st.st_mode)) return false;
    char target[256];
    const auto count=readlink(("/proc/self/fd/"+std::to_string(fd)).c_str(),target,sizeof(target));
    if(count<=0 || count==sizeof(target)) return false;
    return std::string(target,static_cast<size_t>(count)).starts_with("/dev/shm/roudi-quotes-"+std::to_string(getuid())+"-");
}
std::string runtime_dir()
{
    std::string path="/tmp/roudi-ipc-"+std::to_string(getuid());
    if(mkdir(path.c_str(),0700)<0 && errno!=EEXIST) fail("mkdir");
    struct stat st {};
    checked(lstat(path.c_str(),&st),"lstat");
    if(!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&077)!=0) throw std::runtime_error("unsafe runtime directory");
    return path;
}
bool valid(const Registry& r)
{
    return r.magic==magic && r.version==version && r.bytes==sizeof(r) && r.slots==slot_count;
}
void register_producer(const Slot& s)
{
    Fd fd(shm_open(registry_name,O_RDWR|O_CLOEXEC,0600));
    if(fd.n<0)
    {
        if(errno==ENOENT) return;
        fail("shm_open");
    }
    Lock lock(fd.n);
    if(!lock.held) return;
    struct stat st {};
    checked(fstat(fd.n,&st),"fstat");
    if(st.st_size!=sizeof(Registry)) return;
    Map<Registry> map(fd.n);
    auto& r=*map.p;
    if(!valid(r)) return;
    Slot* empty=nullptr;
    for(auto& e:r.entries)
    {
        if(load(e.committed)==1 && e.identity==s.identity && e.pid==s.pid && e.start_ticks==s.start_ticks)
        {
            e.heartbeat_ns=now_ns();
            return;
        }
        if(load(e.committed)!=1 && !empty) empty=&e;
    }
    if(!empty) return; // Full registry: retry; consumer reclaims dead slots.
    store(empty->committed,0);
    // Atomic commit prevents a killed writer leaving a partially visible record.
    std::memcpy(reinterpret_cast<char*>(empty)+8,reinterpret_cast<const char*>(&s)+8,sizeof(Slot)-8);
    store(empty->committed,1);
}
static sockaddr_un address(const char* path)
{
    sockaddr_un a{};
    a.sun_family=AF_UNIX;
    if(strnlen(path,sizeof(a.sun_path))>=sizeof(a.sun_path)) throw std::runtime_error("socket path too long");
    std::strcpy(a.sun_path,path);
    return a;
}
Fd listener(const std::string& path)
{
    Fd fd(checked(socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0),"socket"));
    auto a=address(path.c_str());
    checked(bind(fd.n,reinterpret_cast<sockaddr*>(&a),sizeof(a)),"bind");
    checked(listen(fd.n,16),"listen");
    return fd;
}
static bool ready(int fd,short events)
{
    pollfd p{fd,events,0};
    return poll(&p,1,100)>0 && (p.revents&events);
}
static bool peer(int fd,int expected=0)
{
    ucred cred{};
    socklen_t size=sizeof(cred);
    return getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&cred,&size)==0 && cred.uid==getuid() && (!expected || cred.pid==expected);
}
void serve(int listenfd,int memfd,uint64_t identity)
{
    // Bound work per iteration so an idle client cannot starve publishing forever.
    Fd client(accept4(listenfd,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK));
    if(client.n<0)
    {
        if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR) return;
        fail("accept");
    }
    if(!peer(client.n)||!ready(client.n,POLLIN)) return;
    Wire q{};
    auto bytes=recv(client.n,&q,sizeof(q),MSG_TRUNC);
    bool ok=bytes==sizeof(q)&&q.magic==magic&&q.version==version&&q.bytes==sizeof(q)&&q.kind==1&&q.identity==identity&&q.status==0&&q.region_bytes==0;
    Wire response{magic,version,sizeof(Wire),2,identity,q.nonce,ok?0U:1U,ok?uint32_t(sizeof(Region)):0U};
    iovec io{&response,sizeof(response)};
    msghdr msg{};
    msg.msg_iov=&io;
    msg.msg_iovlen=1;
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))] {};
    if(ok)
    {
        msg.msg_control=control;
        msg.msg_controllen=sizeof(control);
        auto* c=CMSG_FIRSTHDR(&msg);
        c->cmsg_level=SOL_SOCKET;
        c->cmsg_type=SCM_RIGHTS;
        c->cmsg_len=CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(c),&memfd,sizeof(memfd));
    }
    (void)sendmsg(client.n,&msg,MSG_NOSIGNAL);
}
Fd acquire(const Slot& s)
{
    Fd socketfd(checked(socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0),"socket"));
    auto a=address(s.socket_path);
    if(connect(socketfd.n,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0) return Fd{};
    if(!peer(socketfd.n,s.pid)) return Fd{};
    Wire q{magic,version,sizeof(Wire),1,s.identity,random_id(),0,0};
    if(send(socketfd.n,&q,sizeof(q),MSG_NOSIGNAL)!=sizeof(q)||!ready(socketfd.n,POLLIN)) return Fd{};
    Wire response{};
    iovec io{&response,sizeof(response)};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int)*8)] {};
    msghdr msg{};
    msg.msg_iov=&io;
    msg.msg_iovlen=1;
    msg.msg_control=control;
    msg.msg_controllen=sizeof(control);
    auto bytes=recvmsg(socketfd.n,&msg,MSG_CMSG_CLOEXEC);
    Fd received;
    unsigned count=0;
    bool ancillary_ok=true;
    for(auto* c=CMSG_FIRSTHDR(&msg); c; c=CMSG_NXTHDR(&msg,c))
    {
        if(c->cmsg_level!=SOL_SOCKET || c->cmsg_type!=SCM_RIGHTS || c->cmsg_len<CMSG_LEN(0))
        {
            ancillary_ok=false;
            continue;
        }
        auto n=(c->cmsg_len-CMSG_LEN(0))/sizeof(int);
        for(size_t i=0; i<n; ++i)
        {
            int fd;
            std::memcpy(&fd,CMSG_DATA(c)+i*sizeof(int),sizeof(fd));
            Fd item(fd);
            ++count;
            if(count==1) received=std::move(item);
        }
    }
    if(bytes!=sizeof(response)||(msg.msg_flags&(MSG_TRUNC|MSG_CTRUNC))||!ancillary_ok||count!=1||response.magic!=magic||response.version!=version||response.bytes!=sizeof(Wire)||response.kind!=2||response.status||response.identity!=s.identity||response.nonce!=q.nonce||response.region_bytes!=sizeof(Region)) return Fd{};
    if(!valid_quote_memory(received.n)) return Fd{};
    return received;
}
bool valid_name(const char* name,size_t size)
{
    const size_t length=strnlen(name,size);
    if(length==0 || length==size) return false;
    for(size_t i=0; i<length; ++i)
    {
        const char c=name[i];
        if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-')) return false;
    }
    return true;
}
void publish(Stream& r,uint64_t seq,Quote q)
{
    auto& c=r.cells[(seq-1)%capacity];
    store(c.stamp,seq*2-1);
    store(c.packed_quote,(uint64_t(q.price)<<32)|q.quantity);
    store(c.stamp,seq*2);
    store(r.published,seq);
}
bool read(const Stream& r,uint64_t seq,Quote& q)
{
    const auto& c=r.cells[(seq-1)%capacity];
    if(load(c.stamp)!=seq*2) return false;
    auto packed=load(c.packed_quote);
    if(load(c.stamp)!=seq*2) return false;
    q= {uint32_t(packed>>32),uint32_t(packed)};
    return true;
}
}
