#include "header.h"

/////////// Shared variables ///////////////
int activeFileTransferCount=0;
pthread_mutex_t mutexOnFileTransferCount = PTHREAD_MUTEX_INITIALIZER;

struct sendArgStruct
{
    string clientFolderPath="./";
    string filepath;
    vector<pair<int,int>> chunkNumbers; // Chunk number, Chunk size
    int sock_fd;
    //filename, path
    // list of chunk numbers of file

};

struct receiveArgstruct
{
    string server_ip;
    string server_port;
    string filename;
    vector<pair<int,int>> chunkNumbers; // Chunk number, Chunk size
};

struct activeTracker
{
    int id;
    string IPaddr;
    string portnum;
};

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

void* AlwaysListen(void* arg)
{
    char* server_port = (char*) arg;
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
        return (void*)1;
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

        pthread_mutex_lock(&mutexOnFileTransferCount);
        activeFileTransferCount++;
        pthread_mutex_unlock(&mutexOnFileTransferCount);
        pthread_t *newFileTransferThread = new pthread_t;
        int retVal;
        sendArgStruct *newArgs = new sendArgStruct;
        newArgs->sock_fd = *new_fd;
        ////// TO DO: Fill newArgs //////////////
        if( (retVal=pthread_create( newFileTransferThread, NULL, &sendfile, (void*)newArgs)) )
        {
          cout<<"Thread creation failed: "<<retVal<<endl;
        }
        //close(*new_fd);  // parent doesn't need this
    }

    return 0;
}

void* sendfile(void* arg)
{
    sendArgStruct *structArg = (sendArgStruct*) arg;
    // Receive filename
    char* buf = new char[BUFFERSIZE];
    int numbytes;
    if ((numbytes = recv(structArg->sock_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    //receiveArgstruct* sendArgs = (receiveArgstruct*) getSendArgs;
    string filepath(buf, numbytes);
    structArg->filepath=filepath;

///////// Read "filepath" data: chunknum and chunksize
    ifstream fin("metadata.txt");
    while(!fin.eof())
    {
        string temppath;
        getline(fin,temppath);
        if(temppath==filepath)
            break;
    }
    string data;
    getline(fin,data);
    stringstream chunkData(data);
    char bigbuf[SUBCHUNKSIZE];
    char* pos=bigbuf;
    pos+=sizeof(uint32_t);  //to store count
    int chunkcount=0;
    while(!chunkData.eof())
    {
        string chunknum, chunksize;
        int temp;
        getline(chunkData,chunknum);
        temp=stoi(chunknum);
        *(uint32_t*)pos=htonl(temp);
        pos+=sizeof(uint32_t);
        getline(chunkData,chunksize);
        temp=stoi(chunksize);
        *(uint32_t*)pos=htonl(temp);
        pos+=sizeof(uint32_t);
        chunkcount++;
    }
    *(uint32_t*)bigbuf=htonl(chunkcount);
    numbytes = pos-bigbuf;

    if(send(structArg->sock_fd, bigbuf, numbytes, 0) == -1)
            perror("send");
    memset(bigbuf, 0, SUBCHUNKSIZE);
    fin.close();
    if ((numbytes = recv(structArg->sock_fd, bigbuf, SUBCHUNKSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    pos=bigbuf;
    uint32_t vecsize = ntohl(*(uint32_t*)pos);
    pos+=sizeof(uint32_t);
    for(int i=0; i<vecsize; ++i)
    {
        pair<int, int> x;
        x.first = ntohl(*(uint32_t*)pos);
        pos+=sizeof(uint32_t);
        x.second = ntohl(*(uint32_t*)pos);
        pos+=sizeof(uint32_t);
        structArg->chunkNumbers.push_back(x);
    }
    memset(bigbuf, 0, SUBCHUNKSIZE);
    fin.open(structArg->filepath);
    for(int i=0; i<structArg->chunkNumbers.size(); ++i)
    {
        int numOfSubChunks=ceil(structArg->chunkNumbers[i].second/SUBCHUNKSIZE);
        fin.seekg(CHUNKSIZE*structArg->chunkNumbers[i].first);
        for(int j=0; j<numOfSubChunks; ++j)
        {
            memset(bigbuf, 0, SUBCHUNKSIZE);
            if(j<numOfSubChunks-1)
            {
                fin.read(bigbuf, SUBCHUNKSIZE);
                numbytes=SUBCHUNKSIZE;
            }
            else
            {
                int lastsize=structArg->chunkNumbers[i].second%SUBCHUNKSIZE;
                fin.read(bigbuf, lastsize);
                numbytes=lastsize;
            }
            if(send(structArg->sock_fd, bigbuf, numbytes, 0) == -1)
                perror("send");
            
            if ((numbytes = recv(structArg->sock_fd, buf, BUFFERSIZE, 0)) == -1) 
            {
                perror("recv");
                exit(1);
            }
        }
    }
    /*
    string filename = structArg->clientFolderPath + structArg->filename;
    ifstream fin (filename);
    for(int i=0; i<structArg->chunkNumbers.size(); ++i)
    {
        char buf[BUFFERSIZE];
        fin.seekg(BUFFERSIZE*structArg->chunkNumbers[i].first);
        fin.read(buf, structArg->chunkNumbers[i].second);
        if(send(structArg->sock_fd, buf, structArg->chunkNumbers[i].second, 0) == -1)
            perror("send");
    }
    close(structArg->sock_fd);
    pthread_mutex_lock(&mutexOnFileTransferCount);
    activeFileTransferCount--;
    pthread_mutex_unlock(&mutexOnFileTransferCount);
    */
}

void connect_to_peers(string servIP, string servPort, string filename, vector<pair<int, int>> &fileInfo, int *serv_socket_fd)
{
    //receiveArgstruct* newArgs = (receiveArgstruct*) arg;
    ///////  Create file with filename and dummy data
    //ofstream fout("output.txt");
    //char dummydata[BUFFERSIZE], ch='A';
    //memset(dummydata,ch,BUFFERSIZE);
    //fout.write(dummydata,BUFFERSIZE);
    //cout<<addr<<endl;
    int sockfd;  
    char buf[BUFFERSIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    //if (argc != 2) 
    //{
    //    fprintf(stderr,"usage: client hostname\n");
    //    exit(1);
    //}

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char* server_addr = new char[servIP.size()+1];
    strcpy(server_addr,servIP.c_str());
    char* server_port = new char[servPort.size()+1];
    strcpy(server_port, servPort.c_str());
    if ((rv = getaddrinfo(server_addr, server_port, &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }
    
    if (p == NULL) 
    {
        fprintf(stderr, "client: failed to connect\n");
    }
    *serv_socket_fd = sockfd;   // save socket fd created for connection

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
    // Send filename info to client 
    //int structSize=newArgs->server_ip.length()+newArgs->server_port.length()+newArgs->filename.length()+sizeof(newArgs->chunkNumbers[0])*newArgs->chunkNumbers.size();
    
    char* pos = buf;

    //strcpy(pos, newArgs->server_ip.c_str());
    //pos += newArgs->server_ip.length();
    //*pos = ' ';
    //pos++;
    //strcpy(pos, newArgs->server_port.c_str());
    //pos += newArgs->server_port.length();
    //*pos = ' ';
    //pos++;
    strcpy(pos, filename.c_str());
    pos += filename.length();
    *pos = ' ';
    pos++;
    //for(int i=0; i<newArgs->chunkNumbers.size(); ++i)
    //{
    //    *(uint32_t*)pos = htonl(newArgs->chunkNumbers[i].first);
    //    pos+=sizeof(uint32_t);
    //    *(uint32_t*)pos = htonl(newArgs->chunkNumbers[i].second);
    //    pos+=sizeof(uint32_t);
    //    //memcpy(pos, &newArgs->chunkNumbers, sizeof(newArgs->chunkNumbers[0])*newArgs->chunkNumbers.size());
    //}
    //pos += sizeof(newArgs->chunkNumbers[0])*newArgs->chunkNumbers.size()
    int structSize = pos-buf;
    if(send(sockfd, buf, structSize, 0) == -1)
            perror("send");

    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(sockfd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }

    pos=buf;
    uint32_t count=ntohl(*(uint32_t*)pos);  //number of vector<pair<int,int>>
    pos+=sizeof(uint32_t);
    //cout<<"GroupID\tGroupOwner\n";
    for(int i=0; i<count; ++i)
    {
        pair<int, int> chunkdata;
        chunkdata.first = ntohl(*(uint32_t*)pos);
        pos+=sizeof(uint32_t);
        chunkdata.second = ntohl(*(uint32_t*)pos);
        pos+=sizeof(uint32_t);
        fileInfo.push_back(chunkdata);
    }
}

void recvFile(vector<pair<int,int>> chunkstoget, ofstream &fout, pthread_mutex_t &mutexOnFout, int server_fd)
{
    char bigbuf[SUBCHUNKSIZE];
    char* pos=bigbuf;
    *(uint32_t*)pos=htonl(chunkstoget.size());
    pos+=sizeof(uint32_t);
    for(int i=0; i<chunkstoget.size(); ++i)
    {
        *(uint32_t*)pos=htonl(chunkstoget[i].first);
        pos+=sizeof(uint32_t);
        *(uint32_t*)pos=htonl(chunkstoget[i].second);
        pos+=sizeof(uint32_t);
    }
    int datasize=pos-bigbuf;
    if(send(server_fd, bigbuf, datasize, 0) == -1)
                perror("send");

    for(int i=0; i<chunkstoget.size(); ++i)
    {
        char writebuf[CHUNKSIZE];
        char* pos=writebuf;
        int numOfSubChunks=ceil(chunkstoget[i].second/SUBCHUNKSIZE);
        fout.seekp((chunkstoget[i].first)*CHUNKSIZE,ios_base::beg);
        for(int j=0; j<numOfSubChunks; ++j)
        {
            int numbytes;
            char buf[SUBCHUNKSIZE];
            if ((numbytes = recv(server_fd, buf, SUBCHUNKCOUNT, 0)) == -1) 
            {
                perror("recv");
                exit(1);
            }
            
            memcpy(pos, buf, numbytes);
            pos+=numbytes;
            if(send(server_fd, "Success", strlen("Success"), 0) == -1)
                perror("send");
        }
        int numbytes = pos-writebuf;
        pthread_mutex_lock(&mutexOnFout);
        fout.write(writebuf, numbytes);
        pthread_mutex_unlock(&mutexOnFout);
    }
}
int init_tracker_connection(char* myportnum, char* infoFile, activeTracker* trackerptr)
{
    ifstream fin(infoFile);
    string tracker_info;
    getline(fin,tracker_info);
    stringstream ss(tracker_info);
    string temp;
    ss>>temp;
    trackerptr->id = stoi(temp);
    ss>>temp;
    trackerptr->IPaddr = temp;
    ss>>temp;
    trackerptr->portnum = temp;

    int sockfd, numbytes;  
    char buf[BUFFERSIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    //if (argc != 2) 
    //{
    //    fprintf(stderr,"usage: client hostname\n");
    //    exit(1);
    //}

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char* server_addr = new char[trackerptr->IPaddr.size()+1];
    strcpy(server_addr,trackerptr->IPaddr.c_str());
    char* server_port = new char[trackerptr->portnum.size()+1];
    strcpy(server_port, trackerptr->portnum.c_str());
    if ((rv = getaddrinfo(server_addr, server_port, &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) 
    {
        fprintf(stderr, "client: failed to connect\n");
        return -2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
    // Send filename info to client 
    //int structSize=newArgs->server_ip.length()+newArgs->server_port.length()+newArgs->filename.length()+sizeof(newArgs->chunkNumbers[0])*newArgs->chunkNumbers.size();
    char* sendbuf = new char[BUFFERSIZE];
    

    if(send(sockfd, myportnum, strlen(myportnum), 0) == -1)
            perror("send");
    if ((numbytes = recv(sockfd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    //puts(buf);
    string ackRecv(buf,numbytes);
    if(ackRecv=="Success")
        return sockfd;
    else
        return -3;

}

int createUser(string username, string password, int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"create_user");
    pos+=strlen("create_user");
    *pos=' ';
    pos++;
    strcpy(pos,username.c_str());
    pos+=username.length();
    *pos=' ';
    pos++;
    strcpy(pos,password.c_str());
    pos+=password.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    //puts(buf);
    string ackRecv(buf,numbytes);
    if(ackRecv == "Already Exists")
        cout<<"User already exists with tracker\n";
    else if(ackRecv=="Success")
        cout<<"User created successfully with tracker\n";
    return 1;
}

int loginUser(string username, string password, int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"login");
    pos+=strlen("login");
    *pos=' ';
    pos++;
    strcpy(pos,username.c_str());
    pos+=username.length();
    *pos=' ';
    pos++;
    strcpy(pos,password.c_str());
    pos+=password.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");

    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    string ackRecv(buf,numbytes);
    if(ackRecv == "Failure")
        cout<<"User does't exist...Register yourself with tracker\n";
    else if(ackRecv=="Invalid")
        cout<<"Invalid Password..Try again\n";
    else if(ackRecv=="Success")
        cout<<"Logged-in successfully\n";
    return 1;  
}

int createGroup(string grpID, int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"create_group");
    pos+=strlen("create_group");
    *pos=' ';
    pos++;
    strcpy(pos,grpID.c_str());
    pos+=grpID.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    string ackRecv(buf,numbytes);
    if(ackRecv == "Not registered")
        cout<<"User not registered with tracker\n";
    else if(ackRecv=="Already Exists")
        cout<<"Group with id "<<grpID<<" already exists\n";
    else if(ackRecv=="Not logged")
        cout<<"User not logged-in...Login to create group\n";
    else if(ackRecv=="Success")
        cout<<"Group created successfully\n";
    return 1;
}

int joinGroup(string grpID, int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"join_group");
    pos+=strlen("join_group");
    *pos=' ';
    pos++;
    strcpy(pos,grpID.c_str());
    pos+=grpID.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }

    string ackRecv(buf,numbytes);
    if(ackRecv == "Invalid")
        cout<<"Invalid group ID\n";
    else if(ackRecv=="Not logged")
        cout<<"User not logged-in...Login to join group\n";
    else if(ackRecv=="Already Exists")
        cout<<"User already member of group "<<grpID<<endl;
    else if(ackRecv=="Success")
        cout<<"User added to group "<<grpID<<" successfully\n";
    return 1;
}

int leaveGroup(string grpID, int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"leave_group");
    pos+=strlen("leave_group");
    *pos=' ';
    pos++;
    strcpy(pos,grpID.c_str());
    pos+=grpID.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    string ackRecv(buf,numbytes);
    if(ackRecv == "Not registered")
        cout<<"User not registered with tracker\n";
    else if(ackRecv=="Invalid")
        cout<<"Invalid group ID\n";
    else if(ackRecv=="Not member")
        cout<<"User not a member of group ID\n";
    else if(ackRecv=="Success")
        cout<<"User successfully removed from group "<<grpID<<endl;
    return 1;
}

int listGroups(int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"list_groups");
    pos+=strlen("list_groups");
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    pos=buf;
    uint32_t count=ntohl(*(uint32_t*)pos);
    pos+=sizeof(uint32_t);
    cout<<"GroupID\tGroupOwner\n";
    for(int i=0; i<count; ++i)
    {
        uint32_t grpID=ntohl(*(uint32_t*)pos);
        pos+=sizeof(uint32_t);
        cout<<grpID<<"  ";
        string owner(pos);
        cout<<owner<<endl;
        pos+=owner.length()+1;
    }
    return 1;
}

int listFiles(string grpID, int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"list_files");
    pos+=strlen("list_files");
    *pos=' ';
    pos++;
    strcpy(pos,grpID.c_str());
    pos+=grpID.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }

    stringstream ss(buf);
    cout<<"Owner\tFilename\n";
    while(!ss.eof())
    {
        string tmpstr;
        ss>>tmpstr;
        cout<<tmpstr<<"  ";
        ss>>tmpstr;
        cout<<tmpstr<<endl;
    }
    return 1;
}

int downloadFile(string grpID, string filename, string destpath, int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"download_file");
    pos+=strlen("download_file");
    *pos=' ';
    pos++;
    strcpy(pos,grpID.c_str());
    pos+=grpID.length();
    *pos=' ';
    pos++;
    strcpy(pos,filename.c_str());
    pos+=filename.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }

    stringstream ss(buf);
    vector<pair<string, string>> peerServerInfo;
    vector<string> serverFilepath;
    int charcount=numbytes;
    int servercount=0;
    string filesize;
    ss>>filesize;
    charcount = charcount-filesize.length()-1;
    long long fsize = stoll(filesize);
    while(!ss.eof() && charcount>0)
    {
        string servIP, servPort, servFpath;
        ss>>servIP;
        ss>>servPort;
        ss>>servFpath;
        serverFilepath.push_back(servFpath);
        peerServerInfo.push_back(make_pair(servIP,servPort));
        charcount = charcount-servIP.length()-1-servPort.length()-1-servFpath.length()-1;
        servercount++;
    }
    ofstream fout(destpath+'/'+filename);
    char* dummydata = new char[fsize], ch='0';
    memset(dummydata,ch,fsize);
    fout.write(dummydata,fsize);
    int numOfChunks = ceil(fsize/CHUNKSIZE);
    vector<int> server_fd;
    vector<vector<pair<int, int>>> peerFileInfo;
    for(int i=0; i<servercount; ++i)
    {
        vector<pair<int, int>> temp;
        int tempServfd;
        connect_to_peers(peerServerInfo[i].first, peerServerInfo[i].second, filename, temp, &tempServfd);
        server_fd.push_back(tempServfd);
        peerFileInfo.push_back(temp);
    }
    vector<pair<int, int>>* chunkstoget = new vector<pair<int, int>> [servercount];
    vector<bool> found(numOfChunks,false);
    vector<pair<int, int>>::iterator* itr = new vector<pair<int, int>>::iterator[servercount];
    for(int i=0; i<servercount; ++i)
    {
        itr[i]=peerFileInfo[i].begin();
    }
    for(int i=0; i<numOfChunks;)
    {
        for(int j=0; j<servercount;++j)
        {
            if(found[itr[j]->first]== false)
            {
                chunkstoget[j].push_back(make_pair(itr[j]->first, itr[j]->second));
                found[itr[j]->first]=true;
                itr[j]++;
                i++;
            }
        }
    }
    pthread_mutex_t mutexOnFout = PTHREAD_MUTEX_INITIALIZER;;
    for(int i=0; i<servercount; ++i)
    {
        thread recvfilethread(recvFile, chunkstoget[i], std::ref(fout), std::ref(mutexOnFout), server_fd[i]);
        recvfilethread.detach();
    }
    return 1;
}
int uploadFile(string filepath, string grpID, int tracker_socket_fd)
{
    string filename;
    size_t found = filepath.find_last_of('/');
    filename=filepath.substr(found+1);
    std::ifstream fin(filepath, std::ifstream::ate | std::ifstream::binary);
    long long filesize = (long long) fin.tellg(); 
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"upload_file");
    pos+=strlen("upload_file");
    *pos=' ';
    pos++;
    strcpy(pos,filepath.c_str());
    pos+=filepath.length();
    *pos=' ';
    pos++;
    strcpy(pos,grpID.c_str());
    pos+=grpID.length();
    *pos=' ';
    pos++;
    string fsize(to_string(filesize));
    strcpy(pos,fsize.c_str());
    pos+=fsize.length();
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    fin.close();
    fin.open("metadata.txt");
    bool flag=false;
    while(!fin.eof())
    {
        string temppath;
        getline(fin,temppath);
        if(temppath==filepath)
        {
            flag=true;
            break;
        }
    }
    fin.close();
    ofstream fout("metadata.txt", ios::app);
    if(!flag)
    {
        fout<<endl;
        int numOfChunks = ceil(filesize/CHUNKSIZE);
        fout<<filepath<<endl;
        for(int i=0; i<numOfChunks-1; ++i)
        {
            fout<<i<<","<<CHUNKSIZE<<",";
        }
        int lastsize=filesize%CHUNKSIZE;
        fout<<(numOfChunks-1)<<","<<lastsize;
        flag=true;
    }
    fout.close();
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    string ackRecv(buf,numbytes);
    if(ackRecv == "Success")
        cout<<"File successfully uploaded\n";
    return 1;
}

int logout(int tracker_socket_fd)
{
    char buf[BUFFERSIZE];
    char *pos=buf;
    strcpy(pos,"logout");
    pos+=strlen("logout");
    *pos=' ';
    pos++;
    int datasize=pos-buf;
    if(send(tracker_socket_fd, buf, datasize, 0) == -1)
         perror("send");
    
    int numbytes;
    memset(buf, 0, BUFFERSIZE);
    if ((numbytes = recv(tracker_socket_fd, buf, BUFFERSIZE, 0)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    string ackRecv(buf,numbytes);
    if(ackRecv=="Invalid")
        cout<<"Peer with this username not logged-in with tracker\n";
    if(ackRecv == "Success")
        cout<<"Logged out successfully\n";
    return 1;
}
int main(int argc, char* argv[])      // args: myIP, portnum to listen, tracker_info.txt
{
    ///////// TO DO : Establish connection with tracker
    activeTracker trackobj;
    int tracker_socket_fd = init_tracker_connection(argv[2], argv[3], &trackobj);
    if(tracker_socket_fd > 0)
        cout<<"Successfully connected to tracker\n";
    else
    {
        cout<<"Cannot connect to tracker\n";
        exit(-1);
    }
    string ip_comm;
    pthread_t listenThread;
     
    int listenRet;
    char* server_port=new char[strlen(argv[2])+1];
    strcpy(server_port,argv[2]);
    //cout<<"Enter server port num :";
    //cin>>server_port;
    if( (listenRet=pthread_create( &listenThread, NULL, &AlwaysListen, (void*)server_port)) )
    {
      cout<<"Thread creation for listen failed: "<<listenRet<<endl;
    }
    
    while(getline(cin, ip_comm))
	{		
		vector<string> ip_args;
		
		istringstream ss(ip_comm);
		
		while(!ss.eof())
        {
            string tmpstr;
            ss>>tmpstr;
            ip_args.push_back(tmpstr);
        }
        if(ip_args[0] == "create_user")
        {
            /*** 
            1.Create char buffer containing space separated username, paswd
            2. Create send-thread, send this buffer to thread
            3. Join thread, wait for tracker to accept or reject
            */
           createUser(ip_args[1], ip_args[2],tracker_socket_fd);
        }
        else if(ip_args[0] == "login")
        {
            loginUser(ip_args[1], ip_args[2], tracker_socket_fd);
        }
        else if(ip_args[0] == "create_group")
        {
            createGroup(ip_args[1], tracker_socket_fd);
        }
        else if(ip_args[0] == "join_group")
        {
            joinGroup(ip_args[1], tracker_socket_fd);
        }
        else if(ip_args[0] == "leave_group")
        {
            leaveGroup(ip_args[1], tracker_socket_fd);
        }
        else if(ip_args[0] == "list_requests")
        {
            
        }
        else if(ip_args[0] == "accept_request")
        {
            joinGroup(ip_args[1], tracker_socket_fd);
        }
        else if(ip_args[0] == "list_groups")
        {
            listGroups(tracker_socket_fd);
        }
        else if(ip_args[0] == "list_files")
        {
            listFiles(ip_args[1], tracker_socket_fd);
        }
        else if(ip_args[0] == "upload_file")
        {
            uploadFile(ip_args[1], ip_args[2], tracker_socket_fd);
        }
        else if(ip_args[0] == "download_file")
        {
            downloadFile(ip_args[1], ip_args[2], ip_args[3], tracker_socket_fd);
        }
        else if(ip_args[0]=="logout")
        {
            logout(tracker_socket_fd);
            break;
        }
    }
    //pthread_join(listenThread, NULL);
}