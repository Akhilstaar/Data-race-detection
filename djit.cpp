// #define VAR_SZ
#ifndef VAR_SZ

#include "djit.h"
#include "parse.h"
#include <bits/stdc++.h>
using namespace std;

#define so_size_steps 1000
#define sl_size_steps 10000

// #define GE

void djit(const vector<string> &tracedata) {
    // thread fork/join, memory read and write, and lock acquire and release logs
    
    /*
        Type of inputs thta can be there in the file :

        // Assuming, only these are possible (for now)

        // After lock release: TID: 2, Lock address: 0x74538c83aa08
        // After lock acquire: TID: 1, Lock address: 0x74538c83aa08

        // Before pthread_create(): Parent: 0
        // Before lock release: TID: 2, Lock address: 0x74538c83aa08
        // Before lock acquire: TID: 1, Lock address: 0x74538c83aa08

        // Thread begin: 0
        // TID: 0, IP: 0x74538c822211, ADDR: 0x74538c83ae06, Size (B): 1, isRead: 1
        // TID: 0, IP: 0x74538c83aab0, ADDR: 0x74538c82222f, Size (B): 8, isRead: 0

    */
   
   // Mentioned in the paper
   // We assume that the maximum number of threads is known and stored in a constant called maxThreads.
   
   int maxThreads=0;
   int tot_count=0;
    
    // will change this later
    for (const string& line : tracedata) {
        logentry cl = parselogline(line);
        if(cl.type == "TB"){
            // maxThreads++;
            maxThreads = max(maxThreads, cl.tid);
        }
    }
    maxThreads++;
    
    #ifdef DEBUG
    cout << "maxThreads : " << maxThreads << '\n';
    #endif

    // TODO: use a struct for a thread instead of different data structures like this

    // Each thread maintains a vector of time frames.
    // ie. a 2d vector of size maxThreads x maxThreads
    
    // Each initializing thread t fills its vector of time frames with ones—∀i : stt[i]←1.
    vector<vector<int>> st_t(maxThreads, vector<int>(maxThreads,1));
    
    // int sl_size=1000;
    // // shared location map
    // unordered_map<uintptr_t,int> sl_map;
    // int sl_map_sz=0;
    // // synchronization object map 
    // int so_size=1000;
    // unordered_map<uintptr_t,int> so_map;
    // int so_map_sz=0;

    // // The access history of each shared location v is filled with zeros 
    // // (since no thread has accessed it yet) — ∀i : arv[i]←0, awv[i]←0.
    // vector<vector<int>> ar(sl_size, vector<int>(maxThreads,0));
    // vector<vector<int>> aw(sl_size, vector<int>(maxThreads,0));

    // // The vector of each synchronization object S is filled with zeros—∀i : stS[i]←0.
    // vector<vector<int>> st_s(so_size, vector<int>(maxThreads,0));

    // The access history of each shared location v is filled with zeros 
    // (since no thread has accessed it yet) — ∀i : arv[i]←0, awv[i]←0.
    map<uintptr_t, vector<int>> ar, aw;
    
    // The vector of each synchronization object S is filled with zeros—∀i : stS[i]←0.
    map<uintptr_t, vector<int>> st_s_map;

    // Fork-join queue
    queue<int> fj;

    
    // vector<bool> aquiring_lock(maxThreads,0);
    // vector<bool> releasing_lock(maxThreads,0);

    // Data Race
    map<string,int> dr;
    vector<bool> running_threads(maxThreads, 0);        
    set<int> rt;

    for (const string& line : tracedata) {
        logentry cl = parselogline(line);
        
        if(cl.type == "MA"){
            if(cl.isRead){
                // ma++;
                // if(aquiring_lock[cl.tid]) continue;
                // if(releasing_lock[cl.tid]) continue;

                // First ACCESS
                if(ar.find(cl.address) == ar.end()){
                    ar[cl.address] = vector<int>(maxThreads,0);
                    ar[cl.address][cl.tid] = st_t[cl.tid][cl.tid];
                    continue; // No RACE
                }

                ar[cl.address][cl.tid] = st_t[cl.tid][cl.tid];
                
                if(aw.find(cl.address) == aw.end()){
                    continue; // No writers => no race
                }

                for(int i=0; maxThreads>i; i++){
                    if(i == cl.tid) continue;
                    if(!running_threads[i]) continue;

                    #ifdef GE
                    if(aw[cl.address][i] >= st_t[cl.tid][i]){
                    #else
                    if(aw[cl.address][i] > st_t[cl.tid][i]){
                    #endif
                        // DATA RACE
                        tot_count++;
                        stringstream race_key;
                        race_key << "0x" << hex << cl.address << dec << " W-R TID:" << min(i, cl.tid) << " TID:" << max(i, cl.tid);
                        dr[race_key.str()]++;
                        #ifdef DEBUG
                        // cout << race_key.str() << endl;
                        #endif
                    }
                }
            }
            else {
                if(aw.find(cl.address) == aw.end()){
                    aw[cl.address] = vector<int>(maxThreads,0);
                    aw[cl.address][cl.tid] = st_t[cl.tid][cl.tid];
                    continue;
                    // No_RACE
                }

                aw[cl.address][cl.tid] = st_t[cl.tid][cl.tid];
                bool racy=0;

                if(ar.find(cl.address) == ar.end()){
                    // No reader/s
                    for(int i=0; maxThreads>i; i++){
                        if(i == cl.tid) continue;
                        if(!running_threads[i]) continue;

                        #ifdef GE
                        if(aw[cl.address][i] >= st_t[cl.tid][i]){
                        #else
                        if(aw[cl.address][i] > st_t[cl.tid][i]){
                        #endif
                            // DATA RACE
                            tot_count++;
                            racy=1;
                            stringstream race_key;
                            race_key << "0x" << hex << cl.address << dec << " W-W TID:" << min(i, cl.tid) << " TID:" << max(i, cl.tid);
                            dr[race_key.str()]++;
                            #ifdef DEBUG
                            // cout << race_key.str() << endl;
                            #endif
                        }
                    }
                }
                else {
                    for(int i=0; maxThreads>i; i++){
                        if(i == cl.tid) continue;
                        if(!running_threads[i]) continue;
                        
                        #ifdef GE
                        if(aw[cl.address][i] >= st_t[cl.tid][i]){
                        #else
                        if(aw[cl.address][i] > st_t[cl.tid][i]){
                        #endif
                            // DATA RACE
                            tot_count++;
                            racy=1;
                            stringstream race_key;
                            race_key << "0x" << hex << cl.address << dec << " W-W TID:" << min(i, cl.tid) << " TID:" << max(i, cl.tid);
                            dr[race_key.str()]++;
                            #ifdef DEBUG
                            // cout << race_key.str() << endl;
                            #endif
                        }

                        #ifdef GE
                        if(ar[cl.address][i] >= st_t[cl.tid][i]){
                        #else
                        if(ar[cl.address][i] > st_t[cl.tid][i]){
                        #endif
                            // DATA RACE
                            tot_count++;
                            racy=1;
                            stringstream race_key;
                            race_key << "0x" << hex << cl.address << dec << " R-W TID:" << min(i, cl.tid) << " TID:" << max(i, cl.tid);
                            dr[race_key.str()]++;
                            #ifdef DEBUG
                            // cout << race_key.str() << endl;
                            #endif
                        }
                    }
                    // if(!racy){
                    //     // remove all readers
                    //     ar.erase(cl.address);
                    // }
                }
            }
        }
        else if(cl.type == "ALR"){
            // alr++;
            // after a thread performs a lock release 
            // releasing_lock[cl.tid]=0;

            // The issuing thread t starts a new time frame. Therefore, it increments the entry corresponding to t in t’s vector—stt[t]←stt[t]+1.
            auto& lv = st_s_map[cl.lockAddress];
            if (lv.empty()) {
                lv.resize(maxThreads, 0);
            }
            
            // 2. Each entry in S’s vector is updated to hold the maximum between the current value and that of t’s vector—∀i : stS[i]←max(stt[i], stS[i]).
            for(int i=0; maxThreads>i; i++){
                lv[i] = max(lv[i], st_t[cl.tid][i]);
            }
            st_t[cl.tid][cl.tid]++;
        }
        else if(cl.type == "ALA"){
            // ala++;
            // after a thread aquires lock
            // aquiring_lock[cl.tid]=0;

            // first acquire of synchronization object
            if (st_s_map.find(cl.lockAddress) == st_s_map.end()) {
                st_s_map[cl.lockAddress] = vector<int>(maxThreads, 0);
            }

            // 1. The issuing thread t updates each entry in its vector to hold the maximum between its current value and that of S’s vector—∀i : stt[i]←max(stt[i], stS[i]).
            vector<int>& lv = st_s_map[cl.lockAddress];
            for(int i=0; maxThreads>i; i++){
                st_t[cl.tid][i] = max(st_t[cl.tid][i], lv[i]);
            }            
        }
        else if(cl.type == "BLR"){
            // blr++;
            // ignoring for now
        }
        else if(cl.type == "BLA"){
            // bla++;            
            // ignoring for now
        }
        else if(cl.type == "BPC"){
            // bpc++;
            fj.push(cl.tid); // parent_tid
        }
        else if(cl.type == "TB"){
            // tb++;
            if(!fj.empty()){
                int par = fj.back();
                fj.pop();
                
                // Copy parent's vector clock to child
                for(int i=0; maxThreads>i; i++){
                    st_t[cl.tid][i] = st_t[par][i];
                }
            }

            st_t[cl.tid][cl.tid]++;
            rt.insert(cl.tid);
            running_threads[cl.tid]=1;
        }
        else if(cl.type == "TE"){
            rt.erase(cl.tid);
            running_threads[cl.tid]=0;
            // te++;
        }

        // For debugging purposes only ;)
        
        #ifdef DEBUG
        // if (cl.type == "Invalid") {
        //     cout << "Invalid line in the log !!\n";
        //     continue;
        // }
        // cout << "Type: " << cl.type << ", TID: " << cl.tid;
        // if (!cl.address.empty()) cout << ", ADDR: " << cl.address;
        // if (!cl.ip.empty()) cout << ", IP: " << cl.ip;
        // if (cl.size != 0) cout << ", Size: " << cl.size;
        // if (!cl.lockAddress.empty()) cout << ", Lock Address: " << cl.lockAddress;
        // if (cl.parentTid != 0) cout << ", Parent TID: " << cl.parentTid;
        // cout << endl;

        // if (cl.type == "MA") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.ip << " " << cl.address << " " << cl.size << " " << cl.isRead << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "TE") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "TB") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "ALR") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "ALA") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "BPC") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.parentTid << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "BLR") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "BLA") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }
        #endif
    }

    ofstream djit_out_file("djit_output.log");

    if (!djit_out_file) {
        cerr << "error while opening djit_output.log" << endl;
        return;
    }

    for(const auto &ele: dr){
        djit_out_file << ele.first << " Count: " << ele.second << endl;
    }

    djit_out_file.close();
    cout << "DJIT+ output saved to 'djit_output.log'\n";
    cout << "Total " << tot_count << " data races, " << dr.size() << " lines\n";
    
    // For debugging purposes only ;)
    #ifdef DEBUG    
    // cout << ma << "\n";
    // cout << alr << "\n";
    // cout << ala << "\n";
    // cout << bpc << "\n";
    // cout << blr << "\n";
    // cout << bla << "\n";
    // cout << tb << "\n";
    #endif

    return;
}

#else

#include "djit.h"
#include "parse.h"
#include <bits/stdc++.h>
using namespace std;

// #define GE

void djit(const vector<string> &tracedata) {
    // thread fork/join, memory read and write, and lock acquire and release logs
    
    /*
        Type of inputs thta can be there in the file :

        // Assuming, only these are possible (for now)

        // After lock release: TID: 2, Lock address: 0x74538c83aa08
        // After lock acquire: TID: 1, Lock address: 0x74538c83aa08

        // Before pthread_create(): Parent: 0
        // Before lock release: TID: 2, Lock address: 0x74538c83aa08
        // Before lock acquire: TID: 1, Lock address: 0x74538c83aa08

        // Thread begin: 0
        // TID: 0, IP: 0x74538c822211, ADDR: 0x74538c83ae06, Size (B): 1, isRead: 1
        // TID: 0, IP: 0x74538c83aab0, ADDR: 0x74538c82222f, Size (B): 8, isRead: 0

    */
   
   // Mentioned in the paper
   // We assume that the maximum number of threads is known and stored in a constant called maxThreads.
   
    int maxThreads = 0;
    int tot_count = 0;

    // will change this later
    for (const string& line : tracedata) {
        logentry cl = parselogline(line);
        if (cl.type == "TB") {
            maxThreads = max(maxThreads, cl.tid);
        }
    }
    maxThreads++;

    #ifdef DEBUG
    cout << "maxThreads : " << maxThreads << '\n';
    #endif

    // TODO: use a struct for a thread instead of different data structures like this

    // Each thread maintains a vector of time frames.
    // ie. a 2d vector of size maxThreads x maxThreads

    // Each initializing thread t fills its vector of time frames with ones—∀i : stt[i]←1.
    vector<vector<int>> st_t(maxThreads, vector<int>(maxThreads, 1));
    
    // The access history of each shared location v is filled with zeros 
    // (since no thread has accessed it yet) — ∀i : arv[i]←0, awv[i]←0.
    // second var for size here
    map<pair<uintptr_t, int>, vector<int>> ar, aw;

    // The vector of each synchronization object S is filled with zeros—∀i : stS[i]←0.
    map<uintptr_t, vector<int>> st_s_map;

    // Fork-join queue
    queue<int> fj;

    // Data Race
    map<string, int> dr;
    vector<bool> running_threads(maxThreads, 0);
    set<int> rt;

    vector<int> possible_sizes = {1, 2, 4, 8, 16, 32};

    for (const string& line : tracedata) {
        logentry cl = parselogline(line);

        if (cl.type == "MA") {
            pair<uintptr_t, int> key = make_pair(cl.address, cl.size);
            uintptr_t current_start = cl.address;
            uintptr_t current_end = current_start + cl.size - 1;

            if (cl.isRead) {
                // ma++;

                // First ACCESS
                if (ar.find(key) == ar.end()) {
                    ar[key] = vector<int>(maxThreads, 0);
                    ar[key][cl.tid] = st_t[cl.tid][cl.tid];
                    continue; // NO Race
                }

                ar[key][cl.tid] = st_t[cl.tid][cl.tid];

                bool any_aw_entries = false;
                // Iterating over the possible addresses
                // Everything else remains same as in fixed size addresses
                for (uintptr_t ad_start = current_start - 31; ad_start <= current_end; ad_start++) {
                    for (int s : possible_sizes) {
                        auto aw_key = make_pair(ad_start, s);
                        if (aw.find(aw_key) == aw.end()) continue; // No writers => no race

                        uintptr_t ad_end = ad_start+s-1;
                        if ((ad_end < current_start) || (ad_start > current_end)) continue; // going out of overlapping region ;)

                        any_aw_entries = true;

                        for (int i=0; maxThreads>i; i++) {
                            if (i == cl.tid) continue;
                            if (!running_threads[i]) continue;

                            #ifdef GE
                            if (aw[aw_key][i] >= st_t[cl.tid][i]) {
                            #else
                            if (aw[aw_key][i] > st_t[cl.tid][i]) {
                            #endif
                                // DATA RACE
                                tot_count++;
                                stringstream race_key;
                                race_key << "0x" << hex << cl.address << dec << " W-R TID:" << min(i, cl.tid) << " TID:" << max(i, cl.tid);
                                dr[race_key.str()]++;
                                #ifdef DEBUG
                                // cout << race_key.str() << endl;
                                #endif
                            }
                        }
                    }
                }
            }
            else {
                if (aw.find(key) == aw.end()) {
                    aw[key] = vector<int>(maxThreads, 0);
                    aw[key][cl.tid] = st_t[cl.tid][cl.tid];
                    continue;
                    // No_RACE
                }

                aw[key][cl.tid] = st_t[cl.tid][cl.tid];
                bool racy = false;

                for (uintptr_t ad_start=current_start-31; current_end>=ad_start; ad_start++) {
                    for (int s : possible_sizes) {
                        auto aw_key = make_pair(ad_start, s);
                        if (aw.find(aw_key) == aw.end()) continue;

                        uintptr_t ad_end = ad_start+s-1;
                        if ((ad_end < current_start) || (ad_start > current_end)) continue; // going out of overlapping region ;)

                        for (int i=0; i < maxThreads; i++) {
                            if (i == cl.tid) continue;
                            if (!running_threads[i]) continue;

                            #ifdef GE
                            if (aw[aw_key][i] >= st_t[cl.tid][i]) {
                            #else
                            if (aw[aw_key][i] > st_t[cl.tid][i]) {
                            #endif
                                // DATA RACE
                                racy = true;
                                tot_count++;
                                stringstream race_key;
                                race_key << "0x" << hex << cl.address << dec << " W-W TID:" << min(i, cl.tid) << " TID:" << max(i, cl.tid);
                                dr[race_key.str()]++;
                            }
                        }
                    }
                }

                for (uintptr_t ad_start = (current_start >= 31) ? current_start - 31 : 0; ad_start <= current_end; ++ad_start) {
                    for (int s : possible_sizes) {
                        auto ar_key = make_pair(ad_start, s);
                        if (ar.find(ar_key) == ar.end()) continue;

                        uintptr_t ad_end = ad_start + s - 1;
                        if (ad_end < current_start || ad_start > current_end) continue;

                        for (int i=0; i < maxThreads; i++) {
                            if (i == cl.tid) continue;
                            if (!running_threads[i]) continue;

                            #ifdef GE
                            if (ar[ar_key][i] >= st_t[cl.tid][i]) {
                            #else
                            if (ar[ar_key][i] > st_t[cl.tid][i]) {
                            #endif
                                racy = true;
                                tot_count++;
                                stringstream race_key;
                                race_key << "0x" << hex << cl.address << dec << " R-W TID:" << min(i, cl.tid) << " TID:" << max(i, cl.tid);
                                dr[race_key.str()]++;
                            }
                        }
                    }
                }
            
                // if(!racy){
                //     // remove all readers
                //     for(auto s: possible_sizes){
                //         ar.erase({cl.address,s});
                //     }
                // } 
            }
        } 
        else if (cl.type == "ALR") {
            // alr++;
            // after a thread performs a lock release 
            // releasing_lock[cl.tid]=0;

            // The issuing thread t starts a new time frame. Therefore, it increments the entry corresponding to t in t’s vector—stt[t]←stt[t]+1.
            auto& lv = st_s_map[cl.lockAddress];
            if (lv.empty()) {
                lv.resize(maxThreads, 0);
            }
            
            // 2. Each entry in S’s vector is updated to hold the maximum between the current value and that of t’s vector—∀i : stS[i]←max(stt[i], stS[i]).
            for(int i=0; maxThreads>i; i++){
                lv[i] = max(lv[i], st_t[cl.tid][i]);
            }
            st_t[cl.tid][cl.tid]++;
        } 
        else if (cl.type == "ALA") {
            // ala++;
            // after a thread aquires lock
            // aquiring_lock[cl.tid]=0;

            // first acquire of synchronization object
            if (st_s_map.find(cl.lockAddress) == st_s_map.end()) {
                st_s_map[cl.lockAddress] = vector<int>(maxThreads, 0);
            }

            // 1. The issuing thread t updates each entry in its vector to hold the maximum between its current value and that of S’s vector—∀i : stt[i]←max(stt[i], stS[i]).
            vector<int>& lv = st_s_map[cl.lockAddress];
            for(int i=0; maxThreads>i; i++){
                st_t[cl.tid][i] = max(st_t[cl.tid][i], lv[i]);
            }
        } 
        else if(cl.type == "BLR"){
            // blr++;
            // ignoring for now
        }
        else if(cl.type == "BLA"){
            // bla++;            
            // ignoring for now
        }
        else if(cl.type == "BPC"){
            // bpc++;
            fj.push(cl.tid); // parent_tid
        }
        else if(cl.type == "TB"){
            // tb++;
            if(!fj.empty()){
                int par = fj.back();
                fj.pop();
                
                // Copy parent's vector clock to child
                for(int i=0; maxThreads>i; i++){
                    st_t[cl.tid][i] = st_t[par][i];
                }
            }

            st_t[cl.tid][cl.tid]++;
            rt.insert(cl.tid);
            running_threads[cl.tid]=1;
        }
        else if(cl.type == "TE"){
            rt.erase(cl.tid);
            running_threads[cl.tid]=0;
            // te++;
        }
        else{
            // something else in the log file..
        }

        // For debugging purposes only ;)

        #ifdef DEBUG
        // if (cl.type == "Invalid") {
        //     cout << "Invalid line in the log !!\n";
        //     continue;
        // }
        // cout << "Type: " << cl.type << ", TID: " << cl.tid;
        // if (!cl.address.empty()) cout << ", ADDR: " << cl.address;
        // if (!cl.ip.empty()) cout << ", IP: " << cl.ip;
        // if (cl.size != 0) cout << ", Size: " << cl.size;
        // if (!cl.lockAddress.empty()) cout << ", Lock Address: " << cl.lockAddress;
        // if (cl.parentTid != 0) cout << ", Parent TID: " << cl.parentTid;
        // cout << endl;

        // if (cl.type == "MA") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.ip << " " << cl.address << " " << cl.size << " " << cl.isRead << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "TE") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "TB") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "ALR") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "ALA") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "BPC") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.parentTid << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "BLR") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }

        // if (cl.type == "BLA") {
        //     ii++;
        //     cout << line << endl;
        //     cout << cl.tid << " " << cl.lockAddress << endl;
        //     if(ii>mx_nm) break;
        // }
        #endif
    }

    ofstream djit_out_file("djit_output.log");

    if (!djit_out_file) {
        cerr << "error while opening djit_output.log" << endl;
        return;
    }

    for(const auto &ele: dr){
        djit_out_file << ele.first << " Count: " << ele.second << endl;
    }

    djit_out_file.close();
    cout << "DJIT+ output saved to 'djit_output.log'\n";
    cout << "Total " << tot_count << " data races, " << dr.size() << " lines\n";
    
    // For debugging purposes only ;)
    #ifdef DEBUG    
    // cout << ma << "\n";
    // cout << alr << "\n";
    // cout << ala << "\n";
    // cout << bpc << "\n";
    // cout << blr << "\n";
    // cout << bla << "\n";
    // cout << tb << "\n";
    #endif
    return;
}

#endif