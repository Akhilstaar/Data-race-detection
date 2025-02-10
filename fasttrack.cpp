#include "fasttrack.h"
#include "parse.h"
#include <bits/stdc++.h>
using namespace std;

#define so_size_steps 1000
#define sl_size_steps 10000

// #define GE

void fasttrack(const vector<string> &tracedata) {

    // We assume that the maximum number of threads is known and stored in a constant called maxThreads.
    int maxThreads=0;
    int tot_count=0;

    for (const string& line : tracedata) {
        logentry cl = parselogline(line);
        if(cl.type == "TB"){
            // maxThreads++;
            maxThreads = max(maxThreads, 1+cl.tid);
        }
    }

    #ifdef DEBUG
    cout << "maxThreads : " << maxThreads << '\n';
    #endif

    // St t is the current vector clock of the thread t, 
    vector<vector<int>> st_t(maxThreads, vector<int>(maxThreads,1));

    // Same as in DJIT, with the difference being in read & write states
    // Read state contains epoch, is_shared, vc
    // Write state contains only epoch

    struct rd_d {
        bool is_s = false;
        pair<int,int> epoch = {-1, -1};  // time @ thread
        vector<int> vc;
    };

    map<uintptr_t, rd_d> sx_r;
    map<uintptr_t, pair<int,int>> sx_w;
    map<uintptr_t, vector<int>> st_s_map;

    // Fork-join queue
    queue<int> fj;
    
    // Data Race
    map<string,int> dr;
    vector<bool> running_threads(maxThreads, 0);        
    set<int> rt;

    for (const string& line : tracedata) {
        logentry cl = parselogline(line);

        if(cl.type == "MA"){
            // ma++;

            int current_clock = st_t[cl.tid][cl.tid];
            pair<int,int> current_epoch = {current_clock, cl.tid};

            if(cl.isRead){

                // First access
                if(sx_r.find(cl.address) == sx_r.end()){
                    rd_d new_rd;
                    new_rd.vc.resize(maxThreads,0);
                    new_rd.vc[cl.tid] = current_clock;
                    new_rd.epoch = {current_clock, cl.tid};
                    sx_r[cl.address] = new_rd;
                    continue; // N0 Race 
                }
                
                // FT_READ_SAME_EPOCH
                // I doubt it's correctness, coz there may be a write in between 2 epochs.
                // if(sx_r[cl.address].epoch == current_epoch){
                //     // skipping without checking for data race ???
                //     // Currently implemented as the paper states.
                //     // ie. counting a particular DR only once
                //     continue;
                // }

                auto& sr = sx_r[cl.address];
                sr.vc[cl.tid] = current_clock;
                // if read is shared
                if(!sr.is_s && sr.epoch.second != cl.tid 
                    // && (sr.epoch.first >= st_t[cl.tid][sr.epoch.second]) // maybe this is wrong assumption.
                ) sr.is_s=1;
                sr.epoch = {current_clock, cl.tid};

                if(sx_w.find(cl.address) == sx_w.end()){
                    continue; // No writers => no race
                }
                
                pair<int,int> wx=sx_w[cl.address];
                #ifdef GE
                if ( (st_t[cl.tid][wx.second] <= wx.first) && (running_threads[wx.second])) {
                #else        
                if ( (st_t[cl.tid][wx.second] < wx.first) && (running_threads[wx.second])) {
                #endif
                    stringstream race_key;
                    tot_count++;
                    race_key << "0x" << hex << cl.address << dec << " W-R TID:" << min(wx.second,cl.tid) << " TID:" << max(wx.second,cl.tid);
                    dr[race_key.str()]++;
                }
            } 
            else {
                // First write
                if(sx_w.find(cl.address) == sx_w.end()){
                    sx_w[cl.address] = current_epoch;
                    continue; // No RACE
                }
                
                bool racy = false;
                pair<int,int>& wx = sx_w[cl.address];
                // pair<int,int> old_writer = wx;

                #ifdef GE
                if ( (wx.second != cl.tid) && (st_t[cl.tid][wx.second] <= wx.first) && (running_threads[wx.second])) {
                #else
                if ( (wx.second != cl.tid) && (st_t[cl.tid][wx.second] < wx.first) && (running_threads[wx.second])) {
                #endif
                    stringstream race_key;
                    tot_count++;
                    race_key << "0x" << hex << cl.address << dec << " W-W TID:" << min(wx.second , cl.tid) << " TID:" << max(wx.second , cl.tid);
                    dr[race_key.str()]++;
                    racy = true;
                }

                if(sx_r.find(cl.address) == sx_r.end()){
                    // NO Readers => No more races possible.
                    if(!racy) wx = current_epoch;
                    continue;
                }

                // wx = current_epoch;
                rd_d& rx = sx_r[cl.address];

                if (!rx.is_s) {
                    // Only 1 reader
                    #ifdef GE
                    if ((st_t[cl.tid][rx.epoch.second] <= rx.epoch.first ) && (running_threads[rx.epoch.second])) {
                    #else
                    if ((st_t[cl.tid][rx.epoch.second] < rx.epoch.first ) && (running_threads[rx.epoch.second])) {
                    #endif
                        tot_count++;
                        stringstream race_key;
                        race_key << "0x" << hex << cl.address << dec << " R-W TID:" << min(rx.epoch.second, cl.tid) << " TID:" << max(rx.epoch.second, cl.tid);
                        dr[race_key.str()]++;
                        racy = true;
                    }
                } 
                else {
                    for (int u = 0; u < maxThreads; ++u) {
                        if(u==cl.tid) continue;
                        if(!running_threads[u]) continue;
                        
                        #ifdef GE
                        if (st_t[cl.tid][u] <= rx.vc[u]) {
                        #else        
                        if (st_t[cl.tid][u] < rx.vc[u]) {
                        #endif
                            tot_count++;
                            stringstream race_key;
                            race_key << "0x" << hex << cl.address << dec << " R-W TID:" << min(u,cl.tid) << " TID:" << max(u,cl.tid);
                            dr[race_key.str()]++;
                            racy = true;
                        }
                    }
                }

                if (!racy) {
                    sx_w[cl.address] = current_epoch;
                    // if (rx.is_s) {
                    //     rx.is_s = false;
                    //     rx.epoch = current_epoch;
                    //     fill(rx.vc.begin(), rx.vc.end(), 0);
                    // }
                }
            }

        }
        else if(cl.type == "ALR"){
            // alr++;
            // after a thread performs a lock release 
            // releasing_lock[cl.tid]=0;

            auto& lv = st_s_map[cl.lockAddress];
            if(lv.empty()){
                // Release without acquire ???
                // cout << "what !!\n";
                // cout << line << endl;
                lv.resize(maxThreads, 0);
            }
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
            // releasing_lock[cl.tid]=1;
            // ignoring for now
        }
        else if(cl.type == "BLA"){
            // bla++;
            // aquiring_lock[cl.tid]=1;
            
            // ignoring for now
        }
        else if(cl.type == "BPC"){
            // bpc++;
            
            fj.push(cl.tid); // parent_tid
        }
        else if(cl.type == "TB"){
            // tb++;
            // for(auto &ele :st_t[cl.tid]) ele=1; // just in case a thread is created after deletion.
            // need to reset other vector clocks as well but

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
            cout << line << endl;
            cout << "Invalid log file format !!\n";
            exit(0);
        }
        
        // debugging ;)
        
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
    }

    ofstream fasttrack_out_file("fasttrack_output.log");
    if (!fasttrack_out_file) {
        cerr << "Error opening fasttrack_output.log" << endl;
        return;
    }

    for (const auto& race : dr) {
        fasttrack_out_file << race.first << " Count: " << race.second << endl;
    }

    fasttrack_out_file.close();

    cout << "FastTrack Output saved to 'fasttrack_output.log'\n";
    cout << "Total " << tot_count << " data races, " << dr.size() << " lines\n";

    return;
}