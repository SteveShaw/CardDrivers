#include "ipc.hpp"
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>

static void require(bool condition,const char* message)
{
    if(!condition) { throw std::runtime_error(message); }
}

int main() try
{
    using namespace ipc;
    Slot self{};
    self.pid=getpid();
    self.start_ticks=start_ticks(getpid());
    auto normal=process_ref(self);
    require(normal.valid() && !dead(normal),"native process reference");

    checked(setenv("ROUDI_FORCE_LEGACY","1",1),"setenv");
    auto legacy=process_ref(self);
    require(legacy.valid() && !legacy.is_pidfd && !dead(legacy),"legacy process reference");
    ++self.start_ticks;
    require(!process_ref(self).valid(),"start-time mismatch must reject PID generation");

    pid_t child=checked(fork(),"fork");
    if(child==0) {
        for(;;) { pause(); }
    }
    struct Child {
        pid_t pid;
        ~Child()
        {
            if(pid>0) {
                kill(pid,SIGKILL);
                waitpid(pid,nullptr,0);
            }
        }
    } cleanup{child};
    Slot slot{};
    slot.pid=child;
    slot.start_ticks=start_ticks(child);
    auto reference=process_ref(slot);
    require(reference.valid() && !dead(reference),"live child reference");
    checked(kill(child,SIGSTOP),"stop child");
    int status=0;
    checked(waitpid(child,&status,WUNTRACED),"wait stopped child");
    require(WIFSTOPPED(status) && !dead(reference),"stopped child is not dead");
    checked(kill(child,SIGKILL),"kill child");
    siginfo_t info{};
    checked(waitid(P_PID,child,&info,WEXITED|WNOWAIT),"wait zombie");
    require(dead(reference),"unreaped zombie must be detected");
    checked(waitpid(child,&status,0),"reap child");
    cleanup.pid=-1;
    require(dead(reference),"pinned proc fd must remain dead after reap");

    checked(unsetenv("ROUDI_ALLOW_UNSEALED_SHM"),"unsetenv");
    bool refused=false;
    try {
        auto fd=create_quote_memory();
    } catch(const std::runtime_error&) {
        refused=true;
    }
    require(refused,"unsealed storage requires explicit opt-in");
    checked(setenv("ROUDI_ALLOW_UNSEALED_SHM","1",1),"setenv");
    auto fd=create_quote_memory();
    struct stat st {};
    checked(fstat(fd.n,&st),"fstat");
    require(st.st_nlink==0 && st.st_size==sizeof(Region),"anonymous fallback region size/lifetime");
    require(valid_quote_memory(fd.n),"opted-in consumer accepts fallback");
    Map<Region> writer(fd.n);
    Map<const Region> reader(fd.n);
    publish(writer.p->streams[0],1, {123,456});
    Quote quote{};
    require(read(reader.p->streams[0],1,quote) && quote.price==123 && quote.quantity==456,"fallback shared quote data");
    checked(unsetenv("ROUDI_ALLOW_UNSEALED_SHM"),"unsetenv");
    require(!valid_quote_memory(fd.n),"strict consumer rejects unsealed fallback");
    std::cout<<"PASS process generation, stopped/zombie/reaped child, pinned proc FD, explicit unsealed opt-in and shared data\n";
} catch(const std::exception& error)
{
    std::cerr<<error.what()<<'\n';
    return 1;
}
