#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFORequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;


void patient_thread_function (/* add necessary arguments */int p_no, int n, BoundedBuffer* request_buffer, int m) {
    // functionality of the patient threads

    //take a patient num; 
    //for n requests, produce a datamsg(num, time, ECGNO)
    //      time dependent on current requests
    //      at 0 -> time = 0.000; at 1 -> time = 0.004; at 2 -> time = 0.008;
    datamsg x(p_no, 0, EGCNO);
    char* buf = new char[m];
    memcpy(buf, &x, sizeof(datamsg));
    request_buffer->push(buf, sizeof(datamsg));
    for(int i = 1; i < n; i++){
        x.seconds = i * 0.004;
        memcpy(buf, &x, sizeof(datamsg));
        request_buffer->push(buf, sizeof(datamsg));
    }

    delete[] buf;

}

void file_thread_function (/* add necessary arguments */string f, FIFORequestChannel* chan, int m, BoundedBuffer* request_buffer) {
    // functionality of the file thread

    // get file size
    //open output file; allocate the memory with fseek; close file;
    // while offset < file size, produce a filemsg(offset, m)+filename and push to request buffer
    //      incrementing offset; watch out for final message
    filemsg fm(0,0);
    int len = sizeof(filemsg) + (f.size() + 1);
    char* buf2 = new char[len];
    memcpy(buf2, &fm, sizeof(filemsg));
    strcpy(buf2 + sizeof(filemsg), f.c_str());
    chan->cwrite(buf2, len);

    int64_t filesize = 0;
    chan->cread(&filesize, sizeof(int64_t));
    //cout << filesize << endl;

    string filename = "./received/" + f;
    FILE* fp = fopen(filename.c_str(), "w");
    fseek(fp, filesize, SEEK_SET);
    fclose(fp);

    
    char* buf3 = new char[len];

    int offset = 0;
    int transfer_length = m;
    while(filesize > m){
        filemsg transfer(offset, transfer_length);
        memcpy(buf3, &transfer, sizeof(filemsg));
        strcpy(buf3 + sizeof(filemsg), f.c_str());
        //cout << fm.offset << " " << filesize << endl;
        request_buffer->push(buf3, len);
        offset += m;
        filesize -= m;
    }
    transfer_length = filesize;
    //cout << fm.offset << " " << filesize << endl;
    filemsg transfer(offset, transfer_length);
    //cout << fm.length << endl;
    memcpy(buf3, &transfer, sizeof(filemsg));
    strcpy(buf3 + sizeof(filemsg), f.c_str());
    request_buffer->push(buf3, len);
    delete[] buf2;
    delete[] buf3;
    
}

void worker_thread_function (/* add necessary arguments */BoundedBuffer* request_buffer, FIFORequestChannel* chan, BoundedBuffer* response_buffer, int m) {
    // functionality of the worker threads

    // forever loop
    // pop message from request_buffer
    // look at process_request:server.cpp to make decision
    // send message across fifo channel and collect response
    char* buf4 = new char[m];
    char* buf5 = new char[m];
    char* buf6 = new char[m];
    struct data_pair hpair;

    while(true){
        int size = request_buffer->pop(buf4, m);
        MESSAGE_TYPE mes = *((MESSAGE_TYPE*) buf4);
        if(mes == QUIT_MSG){
            //cout << "got a quit message" << endl;
            chan->cwrite ((char *) &mes, sizeof (MESSAGE_TYPE));
            break;
        }

        chan->cwrite(buf4, size);
        chan->cread(buf5, m);

        if(mes == DATA_MSG){
            //cout << "got a data message" << endl;
            datamsg d = *((datamsg*) buf4);
            /*if(d.ecgno != EGCNO){
                hpair.done = true;
            }*/
            hpair.p_no = d.person;
            hpair.data = *((double*) buf5);
            memcpy(buf6, &hpair, sizeof(data_pair));
            response_buffer->push(buf6, sizeof(data_pair));
            //cout << "worker done pushing" << endl;
        } else if(mes == FILE_MSG){
            filemsg f = *((filemsg*) buf4);
	        string filename = buf4 + sizeof(filemsg);
	        filename = "./received/" + filename;
            FILE* fp = fopen(filename.c_str(), "rb+");
            fseek(fp, f.offset, SEEK_SET);
            fwrite(buf5, 1, f.length, fp);
            fclose(fp);
        }
    }

    delete[] buf4;
    delete[] buf5;
    delete[] buf6;
    
    //if DATA:
    //      create pair of patient num from message and response from server
    //      push pair to the response buffer
    //if FILE:
    //      collect filename from message
    //      open the file in update mode 
    //      fseek(SEEK_SET) to offset of filemsg
    //      write the buffer from the server
    
    
}

void histogram_thread_function (/* add necessary arguments */BoundedBuffer* response_buffer, HistogramCollection* hc, int m) {
    // functionality of the histogram threads

    //forever loop
    //pop response from response buffer
    //call HC::update(resp->patient num, resp->double)
    char* buf7 = new char[m];

    while(true){
        response_buffer->pop(buf7, sizeof(data_pair));
        struct data_pair t = *(data_pair*) buf7;
        if(t.done){
            break;
        } else{
            hc->update(t.p_no, t.data);
            //cout << "hist updated" << endl;
        }
    }

    delete[] buf7;    
    
}

void create_new_channels(FIFORequestChannel** channels, FIFORequestChannel *chan, int w){
    char buf_w[MAX_MESSAGE];
    MESSAGE_TYPE nc = NEWCHANNEL_MSG;

    for(int j = 0; j < w; j++){
        chan->cwrite((char*) &nc, sizeof(MESSAGE_TYPE));
        chan->cread(buf_w, sizeof(buf_w));
        
        channels[j] = new FIFORequestChannel(buf_w, FIFORequestChannel::CLIENT_SIDE);
        //cout << "Made a channel " << channels[j]->name() << endl;
    }

    //cout << "Created all channels" << endl;

    //delete[] buf_w;
}





int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
		}
	}
    
	// fork and exec the server
    int pid = fork();
    if (pid == 0) {
        execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    }
    
	// initialize overhead (including the control channel)
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    //for p patients in n req
        //iterate over req
        //put in bounded buffer

    //if bb not empty
        //pop req
        //implement workers
        //put data sent into resp buffer

    //array of producer threads (if data, p elements; if file, 1 element)
    thread* producers = new thread[p];
    /*if(f != ""){
        producers = new thread[1];
    }*/
    //array of fifos (w elements)
    FIFORequestChannel** worker_chans = new FIFORequestChannel*[w];
    //array of w worker threads
    thread* workers = new thread[w];
    //array of histogram threads (if data, h elements; if file, zero elements)
    thread* hist = new thread[h];
    
    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }    
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    //sequential ver
    //for patient num[1,p]
    //      for n requests, produce a datamasg(patient num, time, ECGNO) and push to request buffer
    //          time dependent on current requests
    /*char buf[MAX_MESSAGE];
    datamsg x(0, 0, EGCNO);*/
    
    /*for(int i = 1; i <= p; i++){
        if(f != ""){
            file_thread_function(f, chan, m, &request_buffer);
            break;
        }
        patient_thread_function(i, n, &request_buffer);
        for(int j = 0; j < n; j++){
            x.person = i;
            x.seconds = j * 0.004;
            memcpy(buf, &x, sizeof(datamsg));
            request_buffer.push(buf, sizeof(datamsg));
        }
    }*/

    
    
    //for nxp requests
    //      pop message from request_buffer
    //      look at process_request:server.cpp to make decision
    //      send message across fifo channel and collect response
    //      if DATA:
    //              create pair of patient num from message and response from server
    //              push pair to response buffer
    /*char buf2[MAX_MESSAGE];
    char buf3[MAX_MESSAGE];
    char buf4[MAX_MESSAGE];
    struct data_pair p_pair;*/
    

    //for(int i = 0; i < n*p; i++){
        /*int size = request_buffer.pop(buf2, sizeof(datamsg));
        chan->cwrite(buf2, size);
        chan->cread(buf3, sizeof(double));

        if(f == ""){
            datamsg* d = (datamsg*) buf2;
            p_pair.p_no = d->person;
            p_pair.data = *((double*) buf3);
            memcpy(buf4, &p_pair, sizeof(data_pair));
            response_buffer.push(buf4, sizeof(data_pair));
            

        } else if(m == FILE_MSG){
            filemsg* f = (filemsg*) buf2;
            string filename = buf2 + sizeof(filemsg);
	        
        }*/
        
        
    //}
    /*if(f != ""){
        for(int i = 0; i < fs; i++){
            worker_thread_function(&request_buffer, chan, &response_buffer);
        }
        
    } else{
        for(int i = 0; i < n*p; i++){
            worker_thread_function(&request_buffer, chan, &response_buffer);
        }
    }
    //for nxp response
    //      pop response from the response buffer
    //      call HC::update(resp->patient num, resp->double)
    //char buf5[MAX_MESSAGE];
    if(f == ""){
        for(int i = 0; i < n * p; i++){
            response_buffer.pop(buf5, sizeof(data_pair));
            struct data_pair* t = (data_pair*) buf5;
            hc.update(t->p_no, t->data);
            histogram_thread_function(&response_buffer, &hc);
        }
        
    }*/

    /* create all threads here */
    //if data:
    //      create p patient threads (store in producer array)
    //      create w worker threads (store in worker array)
    //          create w channel for worker (store in fifo array)
    //      create h histogram threads (store in histogram array)
    //if file:
    //      create 1 file thread (store in produce array)
    //      create w worker threads (store in worker array)
    //          create w channel for worker (store in fifo array)

	//if data:
    //      create p patient threads
    //if file:
    //      create 1 file thread
    //
    MESSAGE_TYPE nc = NEWCHANNEL_MSG;
    
    
    FIFORequestChannel* file_chan = nullptr;
    
    if(f == ""){
        
        for(int y = 1; y <= p; y++){
            producers[y - 1] = thread(patient_thread_function, y, n, &request_buffer, m);
        }

        
        
    } else{
        char buf_f[MAX_MESSAGE];
        chan->cwrite((char*) &nc, sizeof(MESSAGE_TYPE));
        chan->cread(buf_f, sizeof(buf_f));
        file_chan = new FIFORequestChannel(buf_f, FIFORequestChannel::CLIENT_SIDE);
        producers[0] = thread(file_thread_function, f, file_chan, m, &request_buffer);
        
        
    }

    //cout << "Point 1" << endl;
    
    //create worker threads
    //      create w channels
    create_new_channels(worker_chans, chan, w);
    
    for(int i = 0; i < w; i++){
        workers[i] = thread(worker_thread_function, &request_buffer, worker_chans[i], &response_buffer, m);
    }
    //cout << "Point 2" << endl;
    
    //if data
    //      create h histogram threads
    if(f == ""){
        for(int k = 0; k < h; k++){
            hist[k] = thread(histogram_thread_function, &response_buffer, &hc, m);
        }
    }

    //cout << "Point 3" << endl;
    

    /* join all threads here */
    //iterate over all thread arrays, calling join
    //      order: producers before consumers
    //      patient -> worker -> histogram
    for(int l = 0; l < p; l++){
        producers[l].join();
        if(f != ""){
            break;
        }
    }

    /*if(f == ""){
        char* buf_q = new char[m];
        datamsg x(1, 0, 2);
        memcpy(buf_q, &x, sizeof(datamsg));
        for(int a = 0; a < h; a++){
            request_buffer.push(buf_q, sizeof(datamsg));
        }

        delete[] buf_q;
    }*/

    MESSAGE_TYPE wq = QUIT_MSG;
    for(int d = 0; d < w; d++){
        request_buffer.push((char*) &wq, sizeof(MESSAGE_TYPE));
    }
    

    for(int g = 0; g < w; g++){
        workers[g].join();
    }

    if(f == ""){
        char* buf_q = new char[m];
        struct data_pair qpair;
        qpair.data = -1;
        qpair.p_no = -1;
        qpair.done = true;
        memcpy(buf_q, &qpair, sizeof(data_pair));
        for(int i = 0; i < h; i++){
            response_buffer.push(buf_q, sizeof(data_pair));
        }

        delete[] buf_q;

        for(int t = 0; t < h; t++){
            hist[t].join();
        }
    }

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	//quit and close all channels in fifo array

    // quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    
    cout << "All Done!" << endl;
    delete chan;
    if(f != ""){
        file_chan->cwrite((char *) &q, sizeof (MESSAGE_TYPE));
        delete file_chan;
    }
    for(int s = 0; s < w; s++){
        delete worker_chans[s];
    }
    delete[] worker_chans;
    delete[] producers;
    delete[] workers;
    delete[] hist;
    

	// wait for server to exit
	wait(nullptr);
}
