#include "djit.h"
#include "fasttrack.h"
#include <iostream>
#include <fstream>
#include <chrono>

using namespace std;
using namespace chrono;

int main(int argc, char* argv[]){
    cin.tie(0); ios::sync_with_stdio(0);

    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <algo> <trace>" << endl;
        cerr << "algo: Either `djit` or `fasttrack`" << endl;
        cerr << "trace: Absolute path to the input trace file" << endl;
        return 1;
    }

    string algo = argv[1];
    string tracefile = argv[2];

    // processing the file

    vector<string> tracedata;
    ifstream file(tracefile);

    if(!file.is_open()){
        cerr << "Error: Unable to open file " << tracefile << endl;
        return 1;
    }

    string line;
    while (getline(file, line)) {
        tracedata.push_back(line);
    }
    tracedata.pop_back();  // last empty line

    file.close();

    // Passing the file contents to the algo's
    // probably, I should've checked validity earlier to avoid overhead

    if (algo == "djit") djit(tracedata);
    else if (algo == "fasttrack") fasttrack(tracedata);
    else if (algo == "compare") {
        auto start1 = high_resolution_clock::now();
        fasttrack(tracedata);
        auto end1 = high_resolution_clock::now();
        auto start2 = high_resolution_clock::now();
        djit(tracedata);
        auto end2 = high_resolution_clock::now();
        auto duration1 = duration_cast<microseconds>(end1 - start1);
        auto duration2 = duration_cast<microseconds>(end2 - start2);

        cout << duration2.count() << " ms: Time taken by DJIT+\n";
        cout << duration1.count() << " ms: Time taken by FASTRACK" << endl;
    }
    else {
        cerr << "Error: Unknown algorithm '" << algo << "'. Use 'djit' or 'fasttrack'." << endl;
        return 1;
    }

    return 0;
}