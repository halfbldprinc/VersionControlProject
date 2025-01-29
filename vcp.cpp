#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <unordered_set>

#define SERVER_IP "127.0.0.1"//usng loopback addrees for testing phase   change with acutal server ip
#define SERVER_PORT 8080 //change port  
#define BUFFER_SIZE 16384 //attention  buffer size must match with server side code 

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
//helper function for submit 
//helper function for add, ignore hidden and executable files for security reasons
    bool isExe(const fs::path &fpath) {
    fs::file_status status = fs::status(fpath);
    return (status.permissions() & fs::perms::owner_exec) != fs::perms::none || 
           (status.permissions() & fs::perms::group_exec) != fs::perms::none || 
           (status.permissions() & fs::perms::others_exec) != fs::perms::none || 
           fpath.extension().empty(); 
}

    bool send_all(int socket, const char* buffer, size_t length) {
        size_t total_sent = 0;
        while (total_sent < length) {
            ssize_t sent = send(socket, buffer + total_sent, length - total_sent, 0);
            if (sent == -1) return false;
            total_sent += sent;
        }
        return true;
    }

    bool recv_ack(int socket) {
        char ack;
        return recv(socket, &ack, sizeof(ack), 0) > 0 && ack == 1;
    }

    bool send_file(int socket, const string& fileName) {
        ifstream f(fileName, ios::binary | ios::ate);
        if (!f) {
            cerr << "Failed to locate: " << fileName << endl;
            return false;
        }

        streamsize fileSize = f.tellg();
        f.seekg(0, ios::beg);
        uint64_t size = static_cast<uint64_t>(fileSize);
        if (!send_all(socket, reinterpret_cast<const char*>(&size), sizeof(size)) || !recv_ack(socket)) {
            cerr << "Failed to send file size for: " << fileName << endl;
            return false;
        }

        char buffer[BUFFER_SIZE];
        while (fileSize > 0) {
            size_t chunkSize = min(static_cast<streamsize>(BUFFER_SIZE), fileSize);
            f.read(buffer, chunkSize);
            if (!send_all(socket, buffer, chunkSize)) {
                cerr << "Failed to send file: " << fileName << endl;
                return false;
            }
            fileSize -= chunkSize;
        }

        return recv_ack(socket);
    }

    int connectToServer() {
        int cSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (cSocket == -1) {
            cerr << "Socket creation failed.\n";
            return -1;
        }

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

        if (connect(cSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
            cerr << "Connection failed.\n";
            close(cSocket);
            return -1;
        }

        return cSocket;
    }



public:




//init command
    void init() {
        
        string t=vcpPath+"/tracker.txt";
        if(fs::exists(t)){
            cerr << "Project initialized already.\n"; 
            return;}

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
        string t=vcpPath+"/tracker.txt";
        if( !fs::exists(t)){
            cerr << "Project not initialized yet.\nInitialize the project first."<<endl; 
            return;}
        unordered_map<string,string> nI; //new 
        unordered_map<string,string> mF; //modified 
        ifstream tf(t);
        string pN;
        getline(tf,pN);
        string fN,fH;
        unordered_set<string> fS;
        unordered_map<string,string> fM;
        while(tf>>fN>>fH){
            if(fN.back()=='/'){fS.insert(fN);}
            else{fM[fN]=fH;}
        }
        tf.close();
        for(const auto&e:fs::recursive_directory_iterator(cpath,fs::directory_options::skip_permission_denied)){
            string rF=fs::relative(e.path(),cpath).string();
            string fN=e.path().filename().string();
            if(fN[0]=='.')continue;
            if(fs::is_regular_file(e.status())){
                string cH=hashFile(e.path().string());
                if(fM.find(rF)==fM.end()){nI[rF]=cH;}
                else if(fM[rF]!=cH){mF[rF]=cH;}
            }
            if(fs::is_directory(e.status())){
                if(fS.find(rF)==fS.end()){nI[rF]="";}
            }
        }
        if(!nI.empty()){
            cout<<"New :\n";
            for(const auto&i:nI){
                if(i.first[0]=='.')continue;
                cout<<"\t"<<i.first<<endl;
            }
        }
        if(!mF.empty()){
            cout<<"\nModified :\n";
            for(const auto&f:mF){
                if(f.first[0]=='.')continue;
                cout<<"\t"<<f.first<<endl;
            }
        }
    }
//end of state func

//add command 
    bool add(const string &fpath) {
        string t=vcpPath+"/tracker.txt";
        if( !fs::exists(t)){
            cerr << "Project not initialized yet.\nInitialize the project first."<<endl; 
            return false;}

        if (!fs::exists(fpath)) { 
            cerr << "Error: " << fpath << " does not exist!"; 
            return false; 
        }

        string relFpath = fs::relative(fpath, cpath).string();
        unordered_map<string, string> fmap;

        ifstream trackerFile(t);
        string projName;
        getline(trackerFile, projName);
        string fname, fhash;
        while (trackerFile >> fname >> fhash) 
            fmap[fname] = fhash;
        trackerFile.close();

        if (fs::is_directory(fpath)) {
            for (const auto &entry : fs::recursive_directory_iterator(fpath)) {
                if (fs::is_regular_file(entry.path())) { 
                    if (isExe(entry.path())) { 
                        continue; 
                    }
                    string relFilePath = fs::relative(entry.path(), cpath).string();
                    string fileHash = hashFile(entry.path().string());
                    fmap[relFilePath] = fileHash;
                }
            }
        } else if (fs::is_regular_file(fpath)) {
            
            if (isExe(fpath)) { 
                return false; 
            }
            string chash = hashFile(fpath);
            fmap[relFpath] = chash;
        } else {
            cerr << "Error: Unsupported file type for " << fpath << endl;
            return false;
        }

        ofstream trackerOut(t, ios::trunc);
        trackerOut << projName << endl;
        for (const auto &e : fmap) 
            trackerOut << e.first << " " << e.second << endl;
        trackerOut.close();
        return true;
    }
//end of add function 

//submit command
   int submit() {
        int cSocket = connectToServer();
        if (cSocket == -1) return -1;

        ifstream trackerFile(vcpPath + "/tracker.txt");
        if (!trackerFile) {
            cerr << "Tracker file missing.\n";
            close(cSocket);
            return -1;
        }

        string projectName;
        getline(trackerFile, projectName);
        uint32_t repoLen = projectName.size();
        if (!send_all(cSocket, reinterpret_cast<const char*>(&repoLen), sizeof(repoLen)) || 
            !send_all(cSocket, projectName.c_str(), repoLen) || 
            !recv_ack(cSocket)) {
            cerr << "Failed to send project name.\n";
            close(cSocket);
            return -1;
        }

        string line;
        while (getline(trackerFile, line)) {
            size_t tabPos = line.find('\t');
            if (tabPos == string::npos) continue;
            string relativePath = line.substr(0, tabPos);
            string absolutePath = (fs::path(cpath) / relativePath).string();

            if (!fs::exists(absolutePath)) {
                cerr << "File not found: " << absolutePath << ", skipping.\n";
                continue;
            }

            uint32_t nameLen = relativePath.size();
            if (!send_all(cSocket, reinterpret_cast<const char*>(&nameLen), sizeof(nameLen)) || 
                !send_all(cSocket, relativePath.c_str(), nameLen) || 
                !recv_ack(cSocket)) {
                cerr << "Failed to send filename: " << relativePath << endl;
                continue;
            }

            cout << "Uploading: " << absolutePath << endl;
            if (!send_file(cSocket, absolutePath)) {
                cerr << "Upload failed for: " << absolutePath << endl;
                continue;
            }
            cout << relativePath << " uploaded.\n";
        }

        uint32_t endMarker = 0;
        send_all(cSocket, reinterpret_cast<const char*>(&endMarker), sizeof(endMarker));

        trackerFile.close();
        close(cSocket);
        return 0;
    }
  //end of submit function 


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
    else if (cmd == "submit" ) { vcp.submit(); }
    else if (cmd=="clone" && argc > 2) {
        //clone from server using link add given or saved 
    }
    else { cerr << "Error: Invalid command or missing argument."; vcp.help(); return 1; }
    
    return 0;
}