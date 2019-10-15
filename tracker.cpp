#ifndef _TRACKER_H
#define _TRACKER_H
#include "tracker.h"

tracker::tracker()
{
    mutexOnactivePeersCount = PTHREAD_MUTEX_INITIALIZER;
    activePeers=0;
}
tracker::~tracker()
{
    //write complete destructor
}


void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int tracker::createUser(string username, string password, string IPaddr, string portnum, peerData* currPeer, int sock_fd)
{
    peerData *newPeer = new peerData;
    newPeer->username = username;
    newPeer->password = password;
    newPeer->IPaddr = IPaddr;
    newPeer->portnum = portnum;
    newPeer->groupID = -1;
    newPeer->isActive = false;
    currPeer->username = username;
    currPeer->password = password;
    currPeer->IPaddr = IPaddr;
    currPeer->portnum = portnum;
    currPeer->groupID = -1;
    currPeer->isActive = false;
    std::pair<std::map<string,peerData>::iterator,bool> ret;
    ret =  peers.insert(make_pair(username, *newPeer));
    if (ret.second==false) 
    {
        // If element already exists
        if(send(sock_fd, "Already Exists", strlen("Already Exists"), 0) == -1)
            perror("send");
        return 0;
    }
    else
    {
        if(send(sock_fd, "Success", strlen("Success"), 0) == -1)
            perror("send");
        return 1;
    }
}

bool tracker::loginUser(string username, string password, string IPaddr, string portnum,  peerData* currPeer, int sock_fd)
{
    map<string, peerData>:: iterator itr;
    if((itr = peers.find(username)) != peers.end())
    {
        if(itr->second.password==password)
        {
            itr->second.IPaddr=IPaddr;
            //itr->second.password=password;
            itr->second.isActive=true;
            //itr->second.files_available=files_available;
            currPeer->username = username;
            currPeer->password = password;
            currPeer->IPaddr = IPaddr;
            currPeer->portnum = portnum;
            //currPeer->groupID = -1;
            currPeer->isActive = true;
            if(send(sock_fd, "Success", strlen("Success"), 0) == -1)
                perror("send");
            return true;    // User present, might not be active;
        }
        else
        {
            currPeer->username = username;
            currPeer->IPaddr = IPaddr;
            currPeer->portnum = portnum;
            currPeer->isActive = false;
            if(send(sock_fd, "Invalid", strlen("Invalid"), 0) == -1)
                perror("send");
            return false;
        }
    }
        
    else
    {
        if(send(sock_fd, "Failure", strlen("Failure"), 0) == -1)
            perror("send");
        return false;   // user not registered with tracker
    }
}

int tracker::createGroup(int groupID, string owner, int sock_fd)
{
    groupData *newGroup = new groupData;
    newGroup->groupID = groupID;
    newGroup->owner = owner;
    newGroup->members.push_back(owner);
    map<string, peerData>::iterator peerItr;
    if((peerItr = peers.find(owner)) == peers.end())
    {
        if(send(sock_fd, "Not registered", strlen("Not registered"), 0) == -1)
            perror("send");
        return -1;  // User not registered
    }
    if(peerItr->second.isActive== true)
    {
        std::pair<std::map<int,groupData>::iterator,bool> ret;
        ret =  groupInfo.insert(make_pair(groupID, *newGroup));
        if (ret.second==false)
        {
            if(send(sock_fd, "Already Exists", strlen("Already Exists"), 0) == -1)
                perror("send");
            return 0;   // If group already exists
        }
        else
        {
            if(send(sock_fd, "Success", strlen("Success"), 0) == -1)
                perror("send");
            return 1;   // New group creaed successfully
        }
    }
    else
    {
        if(send(sock_fd, "Not logged", strlen("Not logged"), 0) == -1)
                perror("send");
    }
        
}

int tracker::joinGroup(int groupID, peerData* currentPeer, int sock_fd)
{
    map<int, groupData>:: iterator itr;
    if((itr = groupInfo.find(groupID)) == groupInfo.end())
    {
        if(send(sock_fd, "Invalid", strlen("Invalid"), 0) == -1)
            perror("send");
        return -2;  // Invalid group ID
    }
    auto listItr = find(itr->second.members.begin(), itr->second.members.end(), currentPeer->username);
    if(listItr == itr->second.members.end())
    {
        if(currentPeer->isActive==true)
        {
            itr->second.members.push_back(currentPeer->username);    // Add in group
            if(send(sock_fd, "Success", strlen("Success"), 0) == -1)
                perror("send");
            return 1;   // Successfully added to group
        }
        else
        {
            if(send(sock_fd, "Not logged", strlen("Not logged"), 0) == -1)
                perror("send");
        }
    }
    else
    {
        if(send(sock_fd, "Already Exists", strlen("Already Exists"), 0) == -1)
            perror("send");
        return 0;   // Member already exists
    }
}

int tracker::leaveGroup(int groupID, string username, int sock_fd)
{
    map<string, peerData>:: iterator peerItr;
    if((peerItr = peers.find(username)) == peers.end())
    {
        if(send(sock_fd, "Not registered", strlen("Not registered"), 0) == -1)
            perror("send");
        return -3;  // User not registered
    }

    map<int, groupData>:: iterator groupItr;
    if((groupItr = groupInfo.find(groupID)) == groupInfo.end())
    {
        if(send(sock_fd, "Invalid", strlen("Invalid"), 0) == -1)
            perror("send");
        return -2;  // Invalid group ID
    }

    auto listItr = find(groupItr->second.members.begin(), groupItr->second.members.end(), username);
    if(listItr == groupItr->second.members.end())
    {
        if(send(sock_fd, "Not member", strlen("Not member"), 0) == -1)
            perror("send");
        return -1;  //  User not member of group
    }
    else
    {
        groupItr->second.members.erase(listItr);
        if(send(sock_fd, "Success", strlen("Success"), 0) == -1)
            perror("send");
        return 1;   // User successfully removed from group
    }
    
}

int tracker::listPendingJoinReq()
{

}

int tracker::acceptRequest(int groupID, peerData* currentPeer, int sock_fd)
{
    joinGroup(groupID, currentPeer, sock_fd);
}

int tracker::listGroups(int sock_fd)
{
    //cout<<"GroupID/t/tOwner\n";
    char buf[BUFFERSIZE];
    char* pos=buf;
    uint32_t count=0;
    *(uint32_t*)pos = htonl(count);
    pos+=sizeof(uint32_t);
    for(auto mapItr = groupInfo.begin(); mapItr!=groupInfo.end(); ++mapItr)
    {
        *(uint32_t*)pos = htonl(mapItr->first);
        pos+=sizeof(uint32_t);
        strcpy(pos,mapItr->second.owner.c_str());
        pos += mapItr->second.owner.length();
        *pos = '\0';
        pos++;
        count++;
    }
    *(uint32_t*)buf = htonl(count);
    int structSize = pos-buf;
    if(send(sock_fd, buf, structSize, 0) == -1)
        perror("send");
    return 1;
}

int tracker::listFiles(int groupID, int sock_fd)
{
    map<int, groupData>:: iterator groupItr;
    if((groupItr = groupInfo.find(groupID)) == groupInfo.end())
    {
        if(send(sock_fd, "Invalid", strlen("Invalid"), 0) == -1)
            perror("send");
        return -1;  // Invalid group ID
    }
    char buf[BUFFERSIZE];
    char* pos=buf;
    for (auto memItr = groupItr->second.members.begin(); memItr!=groupItr->second.members.end(); ++memItr)
    {
        map<string, peerData>::iterator peerItr;
        peerItr=peers.find(*memItr);
        for(auto fileItr=peerItr->second.files_available.begin(); fileItr!=peerItr->second.files_available.end(); ++fileItr)
        {
            strcpy(pos,memItr->c_str());
            pos+=memItr->length();
            *pos=' ';
            pos++;
            strcpy(pos,fileItr->second.first.c_str());
            pos+=fileItr->second.first.length();
            *pos=' ';
            pos++;
        }
    }
    int structSize = pos-buf;
    if(send(sock_fd, buf, structSize, 0) == -1)
        perror("send");
    return 1;
    ////// list will be obtained from other class object which stores usrs and their files shared 
    
}

int tracker::uploadFile(string username, int groupID, string srcFilePath, long long fsize, int sock_fd)
{
    map<int, groupData>:: iterator groupItr;
    if((groupItr = groupInfo.find(groupID)) == groupInfo.end())
    {
        if(send(sock_fd, "Invalid", strlen("Invalid"), 0) == -1)
            perror("send");
        return -1;  // Invalid group ID
    }
    for (auto memItr = groupItr->second.members.begin(); memItr!=groupItr->second.members.end(); ++memItr)
    {
        if(*memItr == username)
        {
            map<string, peerData>::iterator peerItr;
            peerItr=peers.find(*memItr);
            string filename;
            size_t found = srcFilePath.find_last_of('/');
            filename=srcFilePath.substr(found+1);
            peerItr->second.files_available.push_back(make_pair(filename,make_pair(srcFilePath, fsize)));
            break;
        }
    }
    if(send(sock_fd, "Success", strlen("Success"), 0) == -1)
        perror("send");
    return 1;
}

int tracker::downloadFile(string username, int groupID, string filename, int sock_fd)
{
    map<int, groupData>:: iterator groupItr;
    if((groupItr = groupInfo.find(groupID)) == groupInfo.end())
    {
        if(send(sock_fd, "Invalid", strlen("Invalid"), 0) == -1)
            perror("send");
        return -1;  // Invalid group ID
    }
    char buf[BUFFERSIZE];
    char* pos=buf;
    long long filesize=0;
    bool flag = false;
    for (auto memItr = groupItr->second.members.begin(); memItr!=groupItr->second.members.end(); ++memItr)
    {
        map<string, peerData>::iterator peerItr;
        peerItr=peers.find(*memItr);
        list<pair<string,string>>::iterator fileItr;
        for(auto fileItr = peerItr->second.files_available.begin(); fileItr!=peerItr->second.files_available.end(); ++fileItr)
        {
            if(fileItr->first==filename)
            {
                if(!flag)
                {    
                    filesize=fileItr->second.second;
                    string fsize(to_string(filesize));
                    strcpy(pos,fsize.c_str());
                    pos+=fsize.length();
                    *pos=' ';
                    pos++;
                    flag=true;
                }
                strcpy(pos,peerItr->second.IPaddr.c_str());
                pos+=peerItr->second.IPaddr.length();
                *pos=' ';
                pos++;
                strcpy(pos,peerItr->second.portnum.c_str());
                pos+=peerItr->second.portnum.length();
                *pos=' ';
                pos++;
                strcpy(pos,fileItr->second.first.c_str());  // file path in server-peer
                pos+=fileItr->second.first.length();
                *pos=' ';
                pos++;
                
            }
        }
    }

    int structSize = pos-buf;
    if(send(sock_fd, buf, structSize, 0) == -1)
        perror("send");
    return 1;
}
int tracker::logout(string username, int sock_fd)
{
    map<string, peerData>::iterator peerItr;
    if((peerItr=peers.find(username))==peers.end())
    {
        if(send(sock_fd, "Invalid", strlen("Invalid"), 0) == -1)
            perror("send");
        return -1;  // Peer with username not present
    }
    peerItr->second.isActive=false;
    peerItr->second.files_available.clear();
    peerItr->second.IPaddr.clear();
    peerItr->second.portnum.clear();
    if(send(sock_fd, "Success", strlen("Success"), 0) == -1)
        perror("send");
    return 1;
}
int tracker::AlwaysListen(string arg)
{
    char* server_port = new char[arg.length()+1];
    strcpy(server_port, arg.c_str());
    int sockfd;  // listen on sock_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    
    hints.ai_socktype = SOCK_STREAM;    // stream protocol, TCP
    hints.ai_flags = AI_PASSIVE; // use machine's IP

    // get all the addrinfo structures, each of which contains an Internet address that can be specified in a call to bind() or connect()
    if ((rv = getaddrinfo(NULL, server_port, &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) 
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) 
    {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) 
    {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) 
    {  // main accept() loop
        sin_size = sizeof their_addr;
        int *new_fd = new int;      // new connection on new_fd
        *new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (*new_fd == -1) 
        {
            perror("accept");
            //continue;
        }

        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        printf("server: got connection from %s\n", s);
        string peerIP (s);
        //int port = ntohs(((struct sockaddr_in *)&their_addr)->sin_port);
        //string peerPort(to_string(port));
        pthread_mutex_lock(&mutexOnactivePeersCount);
        activePeers++;
        pthread_mutex_unlock(&mutexOnactivePeersCount);
        thread newFileTransferThread(&tracker::peer_thread_fn, this, *new_fd , peerIP);
        newFileTransferThread.detach();
        //int retVal;        
        //if( (retVal=pthread_create( newFileTransferThread, NULL, &peer_thread_fn, (void*)(new_fd))) )
        //{
        //  cout<<"Thread creation failed: "<<retVal<<endl;
        //}
        //close(*new_fd);  // parent doesn't need this
    }

    return 0;
}

bool new_peer_connect(string peerIP, string peerPort,peerData* currentPeer,int sock_fd)
{
    currentPeer->IPaddr=peerIP;
    currentPeer->portnum=peerPort;
    string ack("Success");
    if(send(sock_fd, ack.c_str(), ack.length(), 0) == -1)
            perror("send");
}
void tracker::peer_thread_fn(int sock_fd, string peerIP)
{
    // Receive filename and chunk nums from client
    char* peer_command = new char[BUFFERSIZE];
    char* peerListenPort = new char[BUFFERSIZE];
    int numbytes;
    peerData currentPeer;
    if ((numbytes = recv(sock_fd, peerListenPort, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    string peerPort(peerListenPort,numbytes);
    new_peer_connect(peerIP, peerPort, &currentPeer, sock_fd);
    
    while(1)
    {
        memset(peer_command, 0, BUFFERSIZE);
        if ((numbytes = recv(sock_fd, peer_command, BUFFERSIZE, 0)) == -1) 
        {
            perror("recv");
            exit(1);
        }
        string recvstr(peer_command);
        stringstream ss(recvstr);
        vector<string> comm;
        int charcount=numbytes;
        while(!ss.eof() && charcount>=0)
        {
            string tmpstr;
            ss>>tmpstr;
            comm.push_back(tmpstr);
            charcount = charcount-tmpstr.length()-1;
        }
        if(comm[0]=="create_user")
        {
            createUser(comm[1],comm[2],currentPeer.IPaddr,currentPeer.portnum, &currentPeer, sock_fd);
        }
        else if(comm[0]=="login")
        {
            loginUser(comm[1],comm[2],currentPeer.IPaddr, currentPeer.portnum, &currentPeer, sock_fd);
        }
        else if(comm[0]=="create_group")
        {
            int grpID=stoi(comm[1]);
            createGroup(grpID,currentPeer.username, sock_fd);
        }
        else if(comm[0]=="join_group")
        {
            int grpID=stoi(comm[1]);
            joinGroup(grpID, &currentPeer, sock_fd);
        }
        else if(comm[0]=="leave_group")
        {
            int grpID=stoi(comm[1]);
            leaveGroup(grpID,currentPeer.username, sock_fd);
        }
        else if(comm[0]=="list_requests")
        {

        }
        else if(comm[0]=="accept_request")
        {
            int grpID=stoi(comm[1]);
            acceptRequest(grpID,&currentPeer,sock_fd);
        }
        else if(comm[0]=="list_groups")
        {
            listGroups(sock_fd);
        }
        else if(comm[0]=="list_files")
        {
            int grpID=stoi(comm[1]);
            listFiles(grpID, sock_fd);
        }
        else if(comm[0]=="upload_file")
        {
            int grpID=stoi(comm[2]);
            long long fsize = stoll(comm[3]);
            uploadFile(currentPeer.username,grpID, comm[1], fsize, sock_fd);
        }
        else if(comm[0]=="download_file")
        {
            int grpID=stoi(comm[1]);
            downloadFile(currentPeer.username, grpID, comm[2], sock_fd);
        }
        else if(comm[0]=="logout")
        {
            logout(currentPeer.username, sock_fd);
            break;
        }
        else if(comm[0]=="Hello")
        {
            
        }
        else
        {
            if(send(sock_fd, "Invalid Command", strlen("Invalid Command"), 0) == -1)
                perror("send");
        }
    }
    
}
int main(int argc, char* argv[])
{
    //pthread_t listenThread;
    tracker trackerObj;
    ifstream fin(argv[1]);
    string tracker_info;
    activeTracker myinfo;
    while(getline(fin,tracker_info))
    {
        stringstream ss(tracker_info);
        string temp;
        ss>>temp;
        if(strcmp(argv[2],temp.c_str())==0)
        {
            myinfo.id=stoi(temp);
            ss>>temp;
            myinfo.IPaddr=temp;
            ss>>temp;
            myinfo.portnum=temp;
        }
    }
    
    //int listenRet;
    //char* server_port= new char[myinfo.portnum.length()+1];
    //strcpy(server_port,myinfo.portnum.c_str());
    //if( (listenRet=pthread_create( &listenThread, NULL, &AlwaysListen, (void*)server_port)) )
    //{
    //  cout<<"Thread creation for listen failed: "<<listenRet<<endl;
    //}

    thread listenThread(&tracker::AlwaysListen, &trackerObj, myinfo.portnum);
    listenThread.join();

}
#endif