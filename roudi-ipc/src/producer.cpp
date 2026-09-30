#include "ipc.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <fcntl.h>
#include <poll.h>

int main() try
{
    using namespace ipc;
    install_signals();
    auto id = random_id();
    auto path = runtime_dir() + "/p-" + std::to_string( getpid() ) + "-" + std::to_string( id ) + ".sock";
    auto sock = listener( path );
    struct Cleanup
    {
        std::string path;
        ~Cleanup()
        {
            unlink( path.c_str() );
        }
    } cleanup{path};
    Fd fd( checked( memfd_create( "quotes", MFD_CLOEXEC | MFD_ALLOW_SEALING ), "memfd_create" ) );
    checked( ftruncate( fd.n, sizeof( Region ) ), "ftruncate" );
    Map<Region> map( fd.n );
    auto& r = *map.p;
    r = {};
    r.magic = magic;
    r.version = version;
    r.bytes = sizeof( r );
    r.capacity = capacity;
    r.identity = id;
    checked( fcntl( fd.n, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL ), "seal" );
    Slot slot{};
    slot.pid = getpid();
    slot.identity = id;
    slot.start_ticks = start_ticks( getpid() );
    slot.heartbeat_ns = now_ns();
    if( !slot.start_ticks ) throw std::runtime_error( "cannot read /proc/self/stat" );
    std::strcpy( slot.socket_path, path.c_str() );
    std::cout << "PRODUCER pid=" << getpid() << " identity=" << id << std::endl;
    uint64_t seq = 0, next_registration = 0;
    while( running() )
    {
        if( now_ns() >= next_registration )
        {
            register_producer( slot );
            next_registration = now_ns() + 200'000'000;
        }
        if( seq == std::numeric_limits<uint64_t>::max() / 2 ) throw std::runtime_error( "sequence exhausted" );
        ++seq;
        publish( r, seq, {uint32_t( seq ), uint32_t( seq * 3 )} );
        serve( sock.n, fd.n, id );
        poll( nullptr, 0, 10 );
    }
    // Consumer reclaims the slot using pidfd even if we cannot unregister.
    return 0;
}
catch( const std::exception& e )
{
    std::cerr << "producer: " << e.what() << '\n';
    return 1;
}
