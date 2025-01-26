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
#define BUFFER_SIZE 8192 //attention  buffer size must match with server side code 

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
    bool send_file(int serverSocket,const string& fileName) {
        ifstream f(fileName, ios::binary);
        if (!f) {
            cerr << "Failed to locate : " << fileName << endl;
            return false;
        }
        int ack;// for two way handshake 
    
        char buffer[BUFFER_SIZE];
        while (f.read(buffer,sizeof(buffer))) {
            send(serverSocket,buffer,f.gcount(),0);
            recv(serverSocket,&ack,sizeof(ack),0);
        }
        if (f.gcount() > 0) {//remaining last bit
            send(serverSocket,buffer, f.gcount(),0);
            recv(serverSocket,&ack,sizeof(ack), 0);
        }
        f.close();
        return true;
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
    string t=vcpPath+"/tracker.txt";
    if(!fs::exists(vcpPath)||!fs::exists(t)){cerr<<"Proj not init.\nInit first.";return;}
    unordered_map<string,string> nI;
    unordered_map<string,string> mF;
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
        cout<<"New files & folders:\n";
        for(const auto&i:nI){
            if(i.first[0]=='.')continue;
            cout<<"\t"<<i.first<<endl;
        }
    }else{cout<<"No new files or folders.\n";}
    if(!mF.empty()){
        cout<<"\nModified files:\n";
        for(const auto&f:mF){
            if(f.first[0]=='.')continue;
            cout<<"\t"<<f.first<<endl;
        }
    }else{cout<<"No modified files.\n";}
}
//end of state func

//add command 
   void add(const string &fpath) {
    string tracker = vcpPath + "/tracker.txt";
    if (!fs::exists(vcpPath) || !fs::exists(tracker)) { 
        cerr << "Project not initialized yet.\nInitialize the project first."; 
        return; 
    }
    if (!fs::exists(fpath)) { 
        cerr << "Error: " << fpath << " does not exist!"; 
        return; 
    }

    string relFpath = fs::relative(fpath, cpath).string();
    unordered_map<string, string> fmap;

    // Read existing tracker file
    ifstream trackerFile(tracker);
    string projName;
    getline(trackerFile, projName);
    string fname, fhash;
    while (trackerFile >> fname >> fhash) 
        fmap[fname] = fhash;
    trackerFile.close();

    // Check if it's a directory
    if (fs::is_directory(fpath)) {
        // Recursively traverse the directory and add files
        for (const auto &entry : fs::recursive_directory_iterator(fpath)) {
            if (fs::is_regular_file(entry.path())) {  // Skip subdirectories
                string relFilePath = fs::relative(entry.path(), cpath).string();
                string fileHash = hashFile(entry.path().string());
                fmap[relFilePath] = fileHash;
            }
        }
    } else if (fs::is_regular_file(fpath)) {
        // Single file case
        string chash = hashFile(fpath);
        fmap[relFpath] = chash;
    } else {
        cerr << "Error: Unsupported file type for " << fpath << endl;
        return;
    }

    // Write back to the tracker file
    ofstream trackerOut(tracker, ios::trunc);
    trackerOut << projName << endl;
    for (const auto &e : fmap) 
        trackerOut << e.first << " " << e.second << endl;
    trackerOut.close();

     
}
//end of add function 

//submit command
    int submit() {
        int cSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (cSocket ==-1) {
            cerr << "Something went wrong Please try again. " <<endl;
            return -1;
        }
        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERVER_PORT);

        if (connect(cSocket,(struct sockaddr*)&serverAddr ,sizeof(serverAddr))== -1) {
            cerr << "Connection failed, Please make sure you are connected to internet!" << endl;
            close(cSocket);
            return -1;
        }

        ifstream trackerFile(".vcp/tracker.txt");
        if (!trackerFile) {
            cerr << "Project not initialized yet.\nInitialize the project first." << endl;
            close(cSocket);
            return -1;
        }

        string repo;
        getline(trackerFile, repo);
        send(cSocket, repo.c_str(), repo.size(),0);
        
        int response;
        recv(cSocket, &response, sizeof(response), 0);
        
        // if (response == 0) {
        //     cerr << "Couldn't locate your reposiroty on server!" << endl;
        //     close(cSocket);
        //     return -1;
        // }

        std::string line;
    
    while (getline(trackerFile, line)) {
        std::istringstream sss(line);
        std::string fileName;
        
        // Read the first word before the space
        sss >> fileName;
        
            send(cSocket, fileName.c_str(), fileName.size(),0);
            recv(cSocket, &response, sizeof(response), 0);
            if (response == 0) {
                cerr << fileName << "\t ✗"<< endl;
                continue;
            }
            if (!send_file(cSocket, fileName)) {
                cerr << "Something went wrong. Please try again"<< endl;
                break;
            }
            cout<< fileName << "\t ✔︎"<< endl;
        }
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
    vcp.add(".vcp");
    return 0;
}
