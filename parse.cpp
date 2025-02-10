#include "parse.h"
#include <string>
#include <sstream>
#include <stdint.h>

using namespace std;

// TODO: change entry types from string to integers.

// Common function for parsing the logs
logentry parselogline(const string& line) {
    logentry entry;
    stringstream ss(line);
    string tmp;
    
    // Intentionally kept it simple, focus being on speed...
    if (line[0] == 'T') {
        if (line[1] == 'I') {
            // TID: 0, IP: 0x74538c83aab0, ADDR: 0x74538c82222f, Size (B): 8, isRead: 0
            entry.type = "MA";
            ss >> tmp >> entry.tid >> tmp >> tmp >> 
            hex >> entry.ip >> tmp >> tmp >> entry.address 
            >> dec >> tmp >> tmp >> tmp >> entry.size >> tmp >> 
            tmp >> entry.isRead;
        }
        else if (line[1] == 'h') {
            if (line[7] == 'e') {
                // Thread ended: 2
                entry.type = "TE";
                ss >> tmp >> tmp >> entry.tid;
            }
            else if (line[7] == 'b') {
                // Thread begin: 0
                entry.type = "TB";
                ss >> tmp >> tmp >> entry.tid;
            }
        }
    } 
    else if (line[0] == 'A') {
        // After lock release: TID: 2, Lock address: 0x74538c83aa08
        // After lock acquire: TID: 1, Lock address: 0x74538c83aa08
        entry.type = (line[11] == 'r') ? "ALR" : "ALA";
        ss >> tmp >> tmp >> tmp >> tmp >> entry.tid >> tmp >> tmp >> tmp >> hex >> entry.lockAddress >> dec;
    } 
    else if (line[0] == 'B') {
        // Before pthread_create(): Parent: 0
        // Before lock release: TID: 2, Lock address: 0x74538c83aa08
        // Before lock acquire: TID: 1, Lock address: 0x74538c83aa08
        if (line[7] == 'p') {
            entry.type = "BPC";
            ss >> tmp >> tmp >> tmp >> entry.tid;
        } 
        else {
            entry.type = (line[12] == 'r') ? "BLR" : "BLA";
            ss >> tmp >> tmp >> tmp >> tmp >> entry.tid >> tmp >> tmp >> tmp >> hex >> entry.lockAddress >> dec;
        }
    } 
    else entry.type = "Invalid";
    
    return entry;
}