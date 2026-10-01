#include "ipc.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>
#include <array>
#include <exception>
#include <mutex>
#include <syncstream>
#include <thread>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>

struct Connection {
    ipc::Slot slot; ipc::Fd process, memory; ipc::Map<ipc::Region> map; uint64_t next=1;
    Connection(ipc::Slot s,ipc::Fd p,ipc::Fd m):slot(s),process(std::move(p)),memory(std::move(m)),map(memory.n) {}
};

constexpr size_t reader_count=3;
// Only the assigned reader accesses Connection::next. The short-held mutex
// protects membership; shared ownership keeps snapshots mapped until reads end.
struct Connections {
    std::mutex mutex;
    std::array<std::vector<std::shared_ptr<Connection>>,reader_count> groups;
};

struct Readers {
    std::array<std::jthread,reader_count> threads;
    void stop_all() { for(auto& thread:threads) thread.request_stop(); }
    void join_all() { for(auto& thread:threads) if(thread.joinable()) thread.join(); }
    // Request all stops before jthread member destructors join, including when
    // discovery or creation of a later worker throws.
    ~Readers() { stop_all(); }
};

void read_quotes(std::stop_token stop, Connections& connections,size_t worker) {
    using namespace ipc;
    while(!stop.stop_requested()) {
        std::vector<std::shared_ptr<Connection>> snapshot;
        {
            std::lock_guard lock(connections.mutex);
            snapshot=connections.groups[worker];
        }
        for(auto& c:snapshot) {
            if(stop.stop_requested()) break;
            if(dead(c->process.n)) {
                // The reader alone detaches, so no QUOTE can follow DETACH.
                { std::lock_guard lock(connections.mutex); std::erase(connections.groups[worker],c); }
                std::osyncstream(std::cout)<<"DETACH pid="<<c->slot.pid<<" identity="<<c->slot.identity<<" worker="<<worker<<std::endl;
                continue;
            }
            auto head=load(c->map.p->published);
            if(head>=c->next && head-c->next>=capacity) {
                auto first=head-capacity+1;
                std::osyncstream(std::cout)<<"DROPPED pid="<<c->slot.pid<<" count="<<first-c->next<<" worker="<<worker<<std::endl;
                c->next=first;
            }
            for(unsigned n=0;n<capacity && c->next<=head;++n) {
                if(stop.stop_requested() || dead(c->process.n)) break;
                Quote q{}; if(!read(*c->map.p,c->next,q)) break;
                std::osyncstream(std::cout)<<"QUOTE pid="<<c->slot.pid<<" seq="<<c->next<<" price="<<q.price<<" quantity="<<q.quantity<<" worker="<<worker<<std::endl;
                ++c->next;
            }
        }
        poll(nullptr,0,20);
    }
}

int main() try {
    using namespace ipc; install_signals();
    // Never unlink the lock file: a stable inode prevents split-brain consumers.
    auto path=runtime_dir()+"/consumer.lock";
    Fd owner(checked(open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600),"owner lock"));
    Lock lifetime(owner.n); if(!lifetime.held) throw std::runtime_error("another consumer is running");
    if(shm_unlink(registry_name)<0 && errno!=ENOENT) fail("shm_unlink");
    Fd fd(checked(shm_open(registry_name,O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600),"shm_open"));
    struct Cleanup { ~Cleanup(){shm_unlink(ipc::registry_name);} } cleanup;
    // A producer can observe a zero-sized object before this lock: it retries.
    checked(ftruncate(fd.n,sizeof(Registry)),"ftruncate"); Map<Registry> registry(fd.n);
    for(;;) {
        Lock lock(fd.n); if(!lock.held) { if(!running()) return 0; poll(nullptr,0,10); continue; }
        *registry.p={}; registry.p->version=version; registry.p->bytes=sizeof(Registry); registry.p->slots=slot_count; registry.p->epoch=random_id(); registry.p->magic=magic; break;
    }
    std::cout<<"READY epoch="<<registry.p->epoch<<std::endl;
    Connections connections;
    std::array<std::exception_ptr,reader_count> reader_errors;
    std::atomic<bool> reader_failed=false;
    // Declared last: jthread stops/joins before connections or error state die,
    // including exception unwinding in discovery/FD acquisition.
    Readers readers;
    for(size_t worker=0;worker<reader_count;++worker) {
        readers.threads[worker]=std::jthread([&,worker](std::stop_token stop) {
            try { read_quotes(stop,connections,worker); }
            catch(...) { reader_errors[worker]=std::current_exception(); reader_failed.store(true); }
        });
    }
    while(running() && !reader_failed.load()) {
        std::vector<Slot> slots;
        { Lock lock(fd.n); if(lock.held) {
            for(auto& s:registry.p->entries) if(load(s.committed)==1) {
                auto p=process_fd(s);
                if(p.n<0) { store(s.committed,0); continue; }
                slots.push_back(s);
            }
        } }
        for(const auto& s:slots) {
            if(!running() || reader_failed.load()) break;
            {
                std::lock_guard lock(connections.mutex);
                if(std::ranges::any_of(connections.groups,[&](const auto& group) {
                    return std::ranges::any_of(group,[&](const auto& c){return c->slot.identity==s.identity;});
                })) continue;
            }
            auto p=process_fd(s); if(p.n<0) continue;
            auto m=acquire(s); if(m.n<0||dead(p.n)) continue;
            auto c=std::make_shared<Connection>(s,std::move(p),std::move(m)); const auto& r=*c->map.p;
            if(r.magic!=magic||r.version!=version||r.bytes!=sizeof(Region)||r.capacity!=capacity||r.identity!=s.identity) continue;
            auto head=load(r.published); if(head>=capacity) c->next=head-capacity+1;
            size_t worker;
            {
                std::lock_guard lock(connections.mutex);
                auto smallest=std::ranges::min_element(connections.groups,{},[](const auto& group){return group.size();});
                worker=static_cast<size_t>(smallest-connections.groups.begin());
            }
            // Only this thread inserts. Choose by the current count snapshot;
            // concurrent deaths can lower other groups before publication.
            // Log outside the mutex, before the reader can see this connection.
            std::osyncstream(std::cout)<<"ATTACH pid="<<s.pid<<" identity="<<s.identity<<" worker="<<worker<<std::endl;
            { std::lock_guard lock(connections.mutex); connections.groups[worker].push_back(std::move(c)); }
        }
        poll(nullptr,0,20);
    }
    readers.stop_all();
    readers.join_all();
    for(const auto& error:reader_errors) if(error) std::rethrow_exception(error);
    return 0;
} catch(const std::exception& e) { std::cerr<<"consumer: "<<e.what()<<'\n'; return 1; }
