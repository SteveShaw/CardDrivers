#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>

// RHEL 7 x86_64 system call number for memfd_create
#ifndef __NR_memfd_create
#define __NR_memfd_create 319
#endif

// Flags for memfd_create (taken from linux/mman.h)
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

int main() {
    const char *test_string = "Hello from RHEL 7 memfd memory!";
    char read_buffer[100];
    int fd;

    printf("[*] Attempting to create an in-memory file via memfd_create...\n");

    // Invoke the backported RHEL 7 kernel syscall directly
    fd = syscall(__NR_memfd_create, "rhel7_memfd_test", MFD_CLOEXEC);

    if (fd == -1) {
        perror("[!] memfd_create failed");
        printf("[!] Ensure you are running a standard RHEL 7 kernel (3.10.0-xxx or newer).\n");
        return EXIT_FAILURE;
    }

    printf("[+] Success! Created memfd file descriptor: %d\n", fd);

    // Write data to the in-memory file
    printf("[*] Writing data to memory...\n");
    if (write(fd, test_string, strlen(test_string) + 1) == -1) {
        perror("[!] write failed");
        close(fd);
        return EXIT_FAILURE;
    }

    // Seek back to the beginning of the file to read it
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("[!] lseek failed");
        close(fd);
        return EXIT_FAILURE;
    }

    // Read data back from the in-memory file
    printf("[*] Reading data back from memory...\n");
    if (read(fd, read_buffer, sizeof(read_buffer)) == -1) {
        perror("[!] read failed");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[+] Data read successfully: \"%s\"\n", read_buffer);

    // Clean up
    close(fd);
    printf("[*] File descriptor closed. Test complete.\n");

    return EXIT_SUCCESS;
}
