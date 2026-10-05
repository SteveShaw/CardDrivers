#include "registry.hpp"
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/stat.h>

namespace ipc
{
RegistryOwner::RegistryOwner()
    :lifetime_fd_(checked(open((runtime_dir()+"/consumer.lock").c_str(),
                               O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600),"owner lock")),
     lifetime_lock_(lifetime_fd_.n)
{
    // Keep the same stable lock inode as the original project, so old owners
    // cannot replace /roudi while this owner is alive. Never unlink this file.
    if(!lifetime_lock_.held) throw std::runtime_error("another consumer is running as registry owner");
    if(shm_unlink(registry_name)<0 && errno!=ENOENT) fail("shm_unlink");
    registry_fd_=Fd(checked(shm_open(registry_name,O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600),"shm_open"));
    unlink_.enabled=true; // Also handles failure during construction.
    checked(ftruncate(registry_fd_.n,sizeof(Registry)),"ftruncate");
    Map<Registry> registry(registry_fd_.n);
    while(running())
    {
        Lock lock(registry_fd_.n);
        if(!lock.held)
        {
            poll(nullptr,0,10);
            continue;
        }
        *registry.p= {};
        registry.p->version=version;
        registry.p->bytes=sizeof(Registry);
        registry.p->slots=slot_count;
        registry.p->epoch=random_id();
        registry.p->magic=magic;
        return;
    }
    // The caller observes running()==false and exits without starting work.
}

RegistrySnapshot snapshot_registry(int owner_fd)
{
    Fd opened;
    int fd=owner_fd;
    if(fd<0)
    {
        opened=Fd(shm_open(registry_name,O_RDONLY|O_CLOEXEC,0));
        if(opened.n<0)
        {
            if(errno==ENOENT) return {}; // Readers may start before the owner.
            fail("reader shm_open");
        }
        fd=opened.n;
    }
    Lock lock(fd,owner_fd<0?LockMode::shared:LockMode::exclusive);
    if(!lock.held) return {};
    struct stat st {};
    checked(fstat(fd,&st),"registry fstat");
    if(st.st_size!=sizeof(Registry)) return {}; // Not initialized yet.
    const auto collect=[](auto& registry)
    {
        RegistrySnapshot result;
        if(!valid(registry)) return result;
        result.available=true;
        result.epoch=registry.epoch;
        for(auto& slot:registry.entries)
        {
            if(load(slot.committed)!=1) continue;
            if constexpr(!std::is_const_v<std::remove_reference_t<decltype(registry)>>)
            {
                // Only the owner has a mutable mapping and can clear slots.
                auto process=process_ref(slot);
                if(!process.valid())
                {
                    store(slot.committed,0);
                    continue;
                }
            }
            result.slots.push_back(slot);
        }
        return result;
    };
    if(owner_fd>=0)
    {
        Map<Registry> registry(fd);
        return collect(*registry.p);
    }
    Map<const Registry> registry(fd);
    return collect(*registry.p);
}
}
