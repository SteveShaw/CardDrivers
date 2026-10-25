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
#include <unordered_map>
#include <set>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>

struct Connection {
    ipc::Slot slot; ipc::Fd process, memory; ipc::Map<ipc::Region> map;
    std::atomic<unsigned> remaining{0};
    Connection(ipc::Slot s,ipc::Fd p,ipc::Fd m):slot(s),process(std::move(p)),memory(std::move(m)),map(memory.n) {}
};

constexpr size_t reader_count=3;
struct Subscription {
    std::shared_ptr<Connection> connection;
    uint32_t stream_index;
    uint64_t next=1; // Accessed only by this underlying's assigned reader.
};
struct Route { size_t worker; unsigned streams; };
// One mapping per producer, shared by subscriptions across readers. Routes and
// membership are protected by the mutex; cursors are private to one reader.
struct Connections {
    std::mutex mutex;
    std::unordered_map<uint64_t,std::shared_ptr<Connection>> producers;
    std::unordered_map<std::string,Route> routes;
    std::array<std::vector<std::shared_ptr<Subscription>>,reader_count> groups;
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
        std::vector<std::shared_ptr<Subscription>> snapshot;
        {
            std::lock_guard lock(connections.mutex);
            snapshot=connections.groups[worker];
        }
        for(auto& subscription:snapshot) {
            if(stop.stop_requested()) break;
            auto& c=subscription->connection;
            auto& stream=c->map.p->streams[subscription->stream_index];
            if(dead(c->process.n)) {
                {
                    std::lock_guard lock(connections.mutex);
                    std::erase(connections.groups[worker],subscription);
                    auto route=connections.routes.find(stream.symbol);
                    if(--route->second.streams==0) connections.routes.erase(route);
                }
                std::osyncstream(std::cout)<<"UNSUBSCRIBE pid="<<c->slot.pid<<" symbol="<<stream.symbol
                    <<" instrument="<<stream.instrument<<" worker="<<worker<<std::endl;
                // Global DETACH is emitted only after ALL readers have stopped
                // reading this producer. Snapshot references defer final unmap.
                if(c->remaining.fetch_sub(1)==1) {
                    { std::lock_guard lock(connections.mutex); connections.producers.erase(c->slot.identity); }
                    std::osyncstream(std::cout)<<"DETACH pid="<<c->slot.pid<<" identity="<<c->slot.identity<<std::endl;
                }
                continue;
            }
            auto& next=subscription->next;
            auto head=load(stream.published);
            if(head>=next && head-next>=capacity) {
                auto first=head-capacity+1;
                std::osyncstream(std::cout)<<"DROPPED pid="<<c->slot.pid<<" count="<<first-next
                    <<" symbol="<<stream.symbol<<" instrument="<<stream.instrument<<" worker="<<worker<<std::endl;
                next=first;
            }
            for(unsigned n=0;n<capacity && next<=head;++n) {
                if(stop.stop_requested() || dead(c->process.n)) break;
                Quote q{}; if(!read(stream,next,q)) break;
                std::osyncstream(std::cout)<<"QUOTE pid="<<c->slot.pid<<" seq="<<next<<" price="<<q.price<<" quantity="<<q.quantity
                    <<" symbol="<<stream.symbol<<" instrument="<<stream.instrument<<" worker="<<worker<<std::endl;
                ++next;
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
                if(connections.producers.contains(s.identity)) continue;
            }
            auto p=process_fd(s); if(p.n<0) continue;
            auto m=acquire(s); if(m.n<0||dead(p.n)) continue;
            auto c=std::make_shared<Connection>(s,std::move(p),std::move(m)); const auto& r=*c->map.p;
            if(r.magic!=magic||r.version!=version||r.bytes!=sizeof(Region)||r.capacity!=capacity||r.identity!=s.identity||
               r.reserved!=0||r.stream_count==0||r.stream_count>max_streams) continue;
            std::set<std::string> instruments;
            bool valid_streams=true;
            for(uint32_t i=0;i<r.stream_count;++i) {
                const auto& stream=r.streams[i];
                if(!valid_name(stream.symbol,sizeof(stream.symbol))||!valid_name(stream.instrument,sizeof(stream.instrument))||
                   !instruments.insert(stream.instrument).second) { valid_streams=false; break; }
            }
            if(!valid_streams) continue;
            c->remaining.store(r.stream_count);
            std::vector<std::pair<size_t,std::shared_ptr<Subscription>>> pending;
            {
                std::lock_guard lock(connections.mutex);
                connections.producers.emplace(s.identity,c);
                for(uint32_t i=0;i<r.stream_count;++i) {
                    const auto& stream=r.streams[i];
                    auto route=connections.routes.find(stream.symbol);
                    if(route==connections.routes.end()) {
                        // Balance distinct active underlyings, not producer or
                        // contract counts. Existing underlyings retain affinity.
                        std::array<size_t,reader_count> counts{};
                        for(const auto& [symbol,entry]:connections.routes) { (void)symbol; ++counts[entry.worker]; }
                        const size_t worker=static_cast<size_t>(std::ranges::min_element(counts)-counts.begin());
                        route=connections.routes.emplace(stream.symbol,Route{worker,0}).first;
                    }
                    ++route->second.streams; // Reserve route until subscription is published.
                    auto subscription=std::make_shared<Subscription>(Subscription{c,i,1});
                    const auto head=load(stream.published);
                    if(head>=capacity) subscription->next=head-capacity+1;
                    pending.emplace_back(route->second.worker,std::move(subscription));
                }
            }
            std::osyncstream(std::cout)<<"ATTACH pid="<<s.pid<<" identity="<<s.identity<<std::endl;
            for(const auto& [worker,subscription]:pending) {
                const auto& stream=r.streams[subscription->stream_index];
                std::osyncstream(std::cout)<<"SUBSCRIBE pid="<<s.pid<<" symbol="<<stream.symbol
                    <<" instrument="<<stream.instrument<<" worker="<<worker<<std::endl;
            }
            {
                std::lock_guard lock(connections.mutex);
                for(auto& [worker,subscription]:pending) connections.groups[worker].push_back(std::move(subscription));
            }
        }
        poll(nullptr,0,20);
    }
    readers.stop_all();
    readers.join_all();
    for(const auto& error:reader_errors) if(error) std::rethrow_exception(error);
    return 0;
} catch(const std::exception& e) { std::cerr<<"consumer: "<<e.what()<<'\n'; return 1; }
