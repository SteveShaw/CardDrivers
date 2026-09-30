#include "ipc.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>

struct Connection
{
    ipc::Slot slot;
    ipc::Fd process, memory;
    ipc::Map<ipc::Region> map;
    uint64_t next = 1;
    Connection( ipc::Slot s, ipc::Fd p, ipc::Fd m ): slot( s ), process( std::move( p ) ), memory( std::move( m ) ), map( memory.n ) {}
};
int main() try
{
    using namespace ipc;
    install_signals();
    // Never unlink the lock file: a stable inode prevents split-brain consumers.
    auto path = runtime_dir() + "/consumer.lock";
    Fd owner( checked( open( path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600 ), "owner lock" ) );
    Lock lifetime( owner.n );
    if( !lifetime.held ) throw std::runtime_error( "another consumer is running" );
    if( shm_unlink( registry_name ) < 0 && errno != ENOENT ) fail( "shm_unlink" );
    Fd fd( checked( shm_open( registry_name, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600 ), "shm_open" ) );
    struct Cleanup
    {
        ~Cleanup()
        {
            shm_unlink( ipc::registry_name );
        }
    } cleanup;
    // A producer can observe a zero-sized object before this lock: it retries.
    checked( ftruncate( fd.n, sizeof( Registry ) ), "ftruncate" );
    Map<Registry> registry( fd.n );
    for( ;; )
    {
        Lock lock( fd.n );
        if( !lock.held )
        {
            if( !running() ) return 0;
            poll( nullptr, 0, 10 );
            continue;
        }
        *registry.p = {};
        registry.p->version = version;
        registry.p->bytes = sizeof( Registry );
        registry.p->slots = slot_count;
        registry.p->epoch = random_id();
        registry.p->magic = magic;
        break;
    }
    std::cout << "READY epoch=" << registry.p->epoch << std::endl;
    std::vector<std::unique_ptr<Connection>> connections;
    while( running() )
    {
        std::vector<Slot> slots;
        {
            Lock lock( fd.n );
            if( lock.held )
            {
                for( auto& s : registry.p->entries ) if( load( s.committed ) == 1 )
                    {
                        auto p = process_fd( s );
                        if( p.n < 0 )
                        {
                            store( s.committed, 0 );
                            continue;
                        }
                        slots.push_back( s );
                    }
            }
        }
        for( const auto& s : slots )
        {
            if( std::ranges::any_of( connections, [&]( const auto & c )
        {
            return c->slot.identity == s.identity;
        } ) ) continue;
            auto p = process_fd( s );
            if( p.n < 0 ) continue;
            auto m = acquire( s );
            if( m.n < 0 || dead( p.n ) ) continue;
            auto c = std::make_unique<Connection>( s, std::move( p ), std::move( m ) );
            const auto& r = *c->map.p;
            if( r.magic != magic || r.version != version || r.bytes != sizeof( Region ) || r.capacity != capacity || r.identity != s.identity ) continue;
            auto head = load( r.published );
            if( head >= capacity ) c->next = head - capacity + 1;
            std::cout << "ATTACH pid=" << s.pid << " identity=" << s.identity << std::endl;
            connections.push_back( std::move( c ) );
        }
        std::erase_if( connections, []( const auto & c )
        {
            if( !dead( c->process.n ) ) return false;
            std::cout << "DETACH pid=" << c->slot.pid << " identity=" << c->slot.identity << std::endl;
            return true;
        } );
        for( auto& c : connections )
        {
            auto head = load( c->map.p->published );
            if( head >= c->next && head - c->next >= capacity )
            {
                auto first = head - capacity + 1;
                std::cout << "DROPPED pid=" << c->slot.pid << " count=" << first - c->next << std::endl;
                c->next = first;
            }
            for( unsigned n = 0; n < capacity && c->next <= head; ++n )
            {
                if( dead( c->process.n ) ) break;
                Quote q{};
                if( !read( *c->map.p, c->next, q ) ) break;
                std::cout << "QUOTE pid=" << c->slot.pid << " seq=" << c->next << " price=" << q.price << " quantity=" << q.quantity << '\n';
                ++c->next;
            }
        }
        std::cout.flush();
        poll( nullptr, 0, 20 );
    }
    return 0;
}
catch( const std::exception& e )
{
    std::cerr << "consumer: " << e.what() << '\n';
    return 1;
}
