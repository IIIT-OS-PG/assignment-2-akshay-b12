#include<iostream>
#include<string>
#include<vector>
#include<map>
#include<list>
#include<algorithm>
#include<fstream>
#include<sstream>
#include<cstring>


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include<thread>
#include <arpa/inet.h>
#include<openssl/sha.h>

using namespace std;

#define BACKLOG 10
#define BUFFERSIZE 512

struct peerData
{
    string username;
    string password;
    int groupID;
    bool isActive;
    string IPaddr;
    string portnum;
    list<pair<string, pair<string, long long>>> files_available; // Filename, filepath, filesize
    peerData()
    {
        username='\0';
        password='\0';
        groupID=-1;
        isActive=true;
        IPaddr='\0';
        portnum='\0';
    }
};
struct filesUploadedData
{
    string filename;
    string owner;
    vector<int> totalSHA1;
    vector<vector<int>> piecewiseSHA1;
    list<string> availableWith;
};

struct pending_peers_data
{
    int id;
    bool isActive;
    string IPaddr;
    string portnum;
};

struct groupData
{
    int groupID;
    string owner;
    list<string> members;
    
};

struct activeTracker
{
    int id;
    string IPaddr;
    string portnum;
};
class tracker
{
    private:
            int trackerNumber;
            map<string, peerData> peers;    // username, peerData
            map<int,pending_peers_data> pendng_peers;   //id, struct pending_peers_data
            map<string, filesUploadedData> fileInfo;    // filename, filesUploadedData
            map<int, groupData> groupInfo;  // id, groupData
            int activePeers;
            //int pendingPeers;
            pthread_mutex_t mutexOnactivePeersCount;// = PTHREAD_MUTEX_INITIALIZER;
            ////// handle file tracker_info.txt
    public:
            tracker();
            ~tracker();
            //int initial_connect();
            int createUser(string username, string password, string IPaddr, string portnum, peerData* currPeer, int sock_fd);
            bool loginUser(string username, string password, string IPaddr, string portnum, peerData* currPeer, int sock_fd);
            int createGroup(int groupID, string owner, int sock_fd);
            int joinGroup(int groupID, peerData* currentPeer, int sock_fd);
            int leaveGroup(int groupID, string username, int sock_fd);
            int listPendingJoinReq(); ////////////////////////// how to do
            int acceptRequest(int groupID, peerData* currentPeer, int sock_fd);
            int listGroups(int sock_fd);
            int listFiles(int groupID, int sock_fd);
            int uploadFile(string username, int groupID, string srcFilePath, long long fsize, int sock_fd);
            int downloadFile(string username, int groupID, string filename,  int sock_fd);
            int logout(string username, int sock_fd);
            int showDownloads(); /////// how to get data from all threads that are downloading???
            int stopShare(int groupID, string filename);
            int AlwaysListen(string arg);
            void peer_thread_fn(int sock_fd,string peerIP);
};