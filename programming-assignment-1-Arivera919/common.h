#ifndef _COMMON_H_
#define _COMMON_H_

#include <iostream>
#include <fstream>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include <vector>

#define NUM_PERSONS 16  // number of person to collect data for
#define MAX_MESSAGE 256 // maximum buffer size for each message

typedef char byte_t;


// different types of messages
enum MESSAGE_TYPE {UNKNOWN_MSG, DATA_MSG, FILE_MSG, NEWCHANNEL_MSG, QUIT_MSG};


// message requesting a data point
class datamsg {
public:
    MESSAGE_TYPE mtype;
    int person;
    double seconds;
    int ecgno;

    datamsg (int _person, double _seconds, int _eno) {
        mtype = DATA_MSG;
        person = _person;
        seconds = _seconds;
        ecgno = _eno;
    }
};


class serverresponse {
public:
    int serverId;
    double ecgData;

    serverresponse (int _serverId, double _ecgData) {
        serverId = _serverId;
        ecgData = _ecgData;
    }
};


// message requesting a file
class filemsg {
public:
    MESSAGE_TYPE mtype;
    __int64_t offset;
    int length;
	    
    filemsg (__int64_t _offset, int _length) {
        mtype = FILE_MSG;
        offset = _offset;
        length = _length;
    }
};

void EXITONERROR (std::string msg);
std::vector<std::string> split (std::string line, char separator);
__int64_t get_file_size (std::string filename);

#endif
