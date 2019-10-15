#ifndef HEADER_H
#define HEADER_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include<cmath>
#include <unistd.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include<thread>
#include <arpa/inet.h>
using namespace std;

#define BUFFERSIZE 512
#define BACKLOG 10
#define CHUNKSIZE 524288
#define SUBCHUNKSIZE 4096
#define SUBCHUNKCOUNT 128

void* receive(void*);
void* sendfile(void*);
#endif
