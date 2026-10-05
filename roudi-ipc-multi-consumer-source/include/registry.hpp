#pragma once
#include "ipc.hpp"
#include <vector>

namespace ipc
{
// Only this object owns the name /roudi. Reader consumers never construct it.
class RegistryOwner
{
    Fd lifetime_fd_;
    Lock lifetime_lock_;
    struct UnlinkOnExit
    {
        bool enabled=false;
        ~UnlinkOnExit()
        {
            if(enabled) shm_unlink(registry_name);
        }
    } unlink_;
    Fd registry_fd_;
public:
    RegistryOwner();
    int fd() const
    {
        return registry_fd_.n;
    }
};

struct RegistrySnapshot
{
    bool available=false;
    uint64_t epoch=0;
    std::vector<Slot> slots;
};

// owner_fd >= 0: exclusively scan/reclaim the owner's registry.
// owner_fd == -1: reopen by name O_RDONLY every time; take LOCK_SH, PROT_READ.
// No mapping, registry lock or registry pointer escapes this function.
RegistrySnapshot snapshot_registry(int owner_fd=-1);
}
