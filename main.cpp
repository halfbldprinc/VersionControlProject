#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cstring>

namespace fs = std::filesystem;

using namespace std;

string cpath, vcpPath;

class VCP {
private:
//helper function for hashing metadate of file
    string hashFile(const string &fpath) {
        ifstream file(fpath, ios::binary);
        if (!file) { cerr << "Error: " << fpath << " inaccessible!"; return ""; }
        string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        size_t fhash = hash<string>{}(content);
        stringstream hs; hs << hex << fhash;//hash to hexadecimal value 
        return hs.str();
    }
public:
//init command
    void init() {
        string vcpPath = cpath + "/.vcp";
        if (fs::exists(vcpPath)) { 
            cerr << "Project already initialized in this directory."; 
            return; }
        fs::create_directory(vcpPath);
        ofstream trackerFile(vcpPath + "/tracker.txt");
        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        string projName;
        cout << "Enter a name for the project: ";
        cin.ignore();
        getline(cin, projName);
        stringstream timestampStream;
        timestampStream << put_time(localtime(&now), "%Y-%m-%d_%H-%M-%S");
        projName += "_" + timestampStream.str(); //concat timestamp to name   ensuring name is unique 
        trackerFile << projName << endl;
        cout << "Project " << projName << " initialized successfully";
    }
//end of init func

//state command 
    void state() {
        string tracker = vcpPath + "/tracker.txt";
        if (!fs::exists(vcpPath) || !fs::exists(tracker)) { 
            cerr << "Project not initialized yet.\nInitialize the project first."; 
            return; }
        unordered_map<string, string> fmap;
        ifstream trackerReader(tracker);
        string projName;
        getline(trackerReader, projName);
        string fname, fhash;
        while (trackerReader >> fname >> fhash) 
            fmap[fname] = fhash;

        vector<string> newFiles, modFiles;
        for (const auto &f : fs::recursive_directory_iterator(cpath)) {
            if (fs::is_regular_file(f)) {
                string relPath = fs::relative(f.path(), cpath).string(), currHash = hashFile(f.path().string());
                if (fmap.find(relPath) == fmap.end()) newFiles.push_back(relPath);
                else if (fmap[relPath] != currHash) modFiles.push_back(relPath);
            }
        }

        if (!newFiles.empty()) { 
            cout << "New files:\n"; 
            for (const auto &f : newFiles) 
                cout << "\t" << f << endl; }
        if (!modFiles.empty()) {
             cout << "Modified files:\n"; 
             for (const auto &f : modFiles) 
             cout << "\t" << f << endl; }
    }
//end of state func

//add command 
    void add(const string &fpath) {
        string tracker = vcpPath + "/tracker.txt";
        if (!fs::exists(vcpPath) || !fs::exists(tracker)) { 
            cerr << "Project not initialized yet.\nInitialize the project first."; 
            return; }
        if (!fs::exists(fpath)) { 
            cerr << "Error: " << fpath << " does not exist!"; 
            return; }
        string relFpath = fs::relative(fpath, cpath).string();
        unordered_map<string, string> fmap;
        ifstream trackerFile(tracker);
        string projName;
        getline(trackerFile, projName);
        string fname, fhash;
        while (trackerFile >> fname >> fhash) 
            fmap[fname] = fhash;
        trackerFile.close();

        string chash = hashFile(fpath);
        fmap[relFpath] =chash;
        ofstream trackerOut(tracker, ios::trunc);
        trackerOut << projName << endl;
        for (const auto &e : fmap) 
            trackerOut << e.first << " " << e.second << endl;
        trackerOut.close();
        cout << "Done.";
    }

    void help() {
        cout << "Usage\n"; 
        cout << "./vcp init      Initialize a new project in the current directory\n " ;
        cout << "./vcp state     Check the state of the project (new/modified files)\n";
        cout<<"./vcp add <file> Add or update a file in the project"<<endl;
    }
};

int main(int argc, char *argv[]) {
    VCP vcp;
    cpath = fs::current_path().string();
    vcpPath = cpath + "/.vcp";

    if (argc < 2) { cerr << "Error: Missing command."; vcp.help(); return 1; }
    string cmd = argv[1];
    if (cmd == "init") { vcp.init(); }
    else if (cmd == "state") { vcp.state(); }
    else if (cmd == "add" && argc > 2) { vcp.add(argv[2]); }
    else { cerr << "Error: Invalid command or missing argument."; vcp.help(); return 1; }
    return 0;
}
