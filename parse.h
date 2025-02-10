#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include <vector>
#include <string>

// not using separate name for parent_tid, used tid.
struct logentry {
    std::string type;
    int tid;
    uintptr_t address;
    int size;
    bool isRead;
    uintptr_t ip;
    uintptr_t lockAddress;
};

logentry parselogline(const std::string& line);

#endif