#include "ipc.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <array>
#include <vector>
#include <set>
#include <sstream>
#include <fcntl.h>
#include <poll.h>

int main(int argc,char** argv) try {
    using namespace ipc;
    install_signals();
    std::string specification="AAPL";
    if(argc==3 && std::string(argv[1])=="--symbols") specification=argv[2];
    else if(argc!=1) throw std::runtime_error("usage: producer [--symbols AAPL[:CONTRACT],MSFT[:CONTRACT],...]");
    if(specification.empty() || specification.back()==',') throw std::runtime_error("empty symbol entry");
    std::vector<std::pair<std::string,std::string>> streams;
    std::set<std::string> instruments;
    std::istringstream input(specification);
    for(std::string entry;std::getline(input,entry,',');) {
        const auto colon=entry.find(':');
        auto symbol=entry.substr(0,colon);
        auto instrument=colon==std::string::npos?symbol:entry.substr(colon+1);
        if(symbol.size()>=32 || instrument.size()>=64 ||
           !valid_name(symbol.c_str(),symbol.size()+1) || !valid_name(instrument.c_str(),instrument.size()+1))
            throw std::runtime_error("invalid symbol/contract: use uppercase A-Z, 0-9, dot, underscore or hyphen");
        if(!instruments.insert(instrument).second) throw std::runtime_error("duplicate instrument");
        streams.emplace_back(symbol,instrument);
    }
    if(streams.empty() || streams.size()>max_streams) throw std::runtime_error("expected 1..16 instrument streams");
    auto id=random_id(); auto path=runtime_dir()+"/p-"+std::to_string(getpid())+"-"+std::to_string(id)+".sock";
    auto sock=listener(path);
    struct Cleanup { std::string path; ~Cleanup(){unlink(path.c_str());} } cleanup{path};
    Fd fd(checked(memfd_create("quotes",MFD_CLOEXEC|MFD_ALLOW_SEALING),"memfd_create"));
    checked(ftruncate(fd.n,sizeof(Region)),"ftruncate"); Map<Region> map(fd.n);
    auto& r=*map.p; r={}; r.magic=magic; r.version=version; r.bytes=sizeof(r); r.capacity=capacity; r.identity=id;
    r.stream_count=static_cast<uint32_t>(streams.size());
    for(size_t i=0;i<streams.size();++i) {
        std::strcpy(r.streams[i].symbol,streams[i].first.c_str());
        std::strcpy(r.streams[i].instrument,streams[i].second.c_str());
    }
    checked(fcntl(fd.n,F_ADD_SEALS,F_SEAL_SHRINK|F_SEAL_GROW|F_SEAL_SEAL),"seal");
    Slot slot{}; slot.pid=getpid(); slot.identity=id; slot.start_ticks=start_ticks(getpid()); slot.heartbeat_ns=now_ns();
    if(!slot.start_ticks) throw std::runtime_error("cannot read /proc/self/stat");
    std::strcpy(slot.socket_path,path.c_str());
    std::cout<<"PRODUCER pid="<<getpid()<<" identity="<<id<<std::endl;
    std::array<uint64_t,max_streams> sequences{};
    uint64_t next_registration=0;
    while(running()) {
        if(now_ns()>=next_registration) { register_producer(slot); next_registration=now_ns()+200'000'000; }
        for(uint32_t i=0;i<r.stream_count;++i) {
            auto& seq=sequences[i];
            if(seq==std::numeric_limits<uint64_t>::max()/2) throw std::runtime_error("sequence exhausted");
            ++seq; publish(r.streams[i],seq,{uint32_t(seq),uint32_t(seq*3)});
        }
        serve(sock.n,fd.n,id);
        poll(nullptr,0,10);
    }
    // Consumer reclaims the slot using pidfd even if we cannot unregister.
    return 0;
} catch(const std::exception& e) { std::cerr<<"producer: "<<e.what()<<'\n'; return 1; }
