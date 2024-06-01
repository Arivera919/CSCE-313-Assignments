/*
	Original author of the starter code
	Tanzir Ahmed
	Department of Computer Science & Engineering
	Texas A&M University
	Date: 2/8/20

	Please include your Name, UIN, and the date below
	Name: Abdiel Rivera
	UIN: 530001766
	Date: 2/1/2024
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;

/* Verifies if the commandline argument passed to client.cpp "num_of_servers"
   is one of the following numbers (2,4,8,16). */
void verify_server_count(int num_of_servers)
{
	/*
		TODO: Allow only 2, 4, 8 or 16 servers
		If the number of servers is any other number
		Print "There can only be 2, 4, 8 or 16 servers\n"
		In the next line, print "Exiting Now!\n"
	*/
	if(num_of_servers != 2 && num_of_servers != 4 && num_of_servers != 8 && num_of_servers != 16){
		EXITONERROR("There can only be 2, 4, 8, or 16 servers\nExiting Now!\n");
	}

}

/* Establishes a new channel with the server by sending appropriate NEWCHANNEL_MSG message.
   Sets this new channel as main channel (point chan to new channel) for communication with the server. */
void set_new_channel(FIFORequestChannel *chan, FIFORequestChannel **channels, int buffersize)
{
	channels[0] = nullptr;

	/*  TODO:
		1)request new channel
		  Hint: use cwrite and cread*/
		MESSAGE_TYPE m = NEWCHANNEL_MSG;
		chan->cwrite(&m, sizeof(MESSAGE_TYPE));

		char* buf = new char[buffersize];
		chan->cread(buf, sizeof(buf));
		/*2)open channel on client side*/
		channels[0] = new FIFORequestChannel(buf, FIFORequestChannel::CLIENT_SIDE);
		/*3)set the above channel as the main channel*/
		chan = channels[0];
		/*Remember to clean up any object you allocated memory to
	*/
		delete[] buf;
}

/* Send datamsg request to server using "chan" FIFO Channel and
   cout the response with serverID */
void req_single_data_point(FIFORequestChannel *chan,
						   int person,
						   double time,
						   int ecg)
{

	/*
		TODO: Reading a single data point
		Hint: Go through datamsg and serverresponse classes
		1) sending a data msg
			Hint: use cwrite*/
		char buf[MAX_MESSAGE];
		datamsg x(person, time, ecg);

		memcpy(buf, &x, sizeof(datamsg));
		chan->cwrite(buf, sizeof(datamsg));
	
		/*2) receiving response
			Hint: use cread
	*/
		serverresponse resp(-1, -1);
		chan->cread(&resp, sizeof(serverresponse));

	// Don't Change the cout format present below
	//	cout << resp.ecgData <<" data on server:"<< resp.serverId << " on channel " << chan->name() << endl << endl;
	cout << resp.ecgData <<" data on server:"<< resp.serverId << " on channel " << chan->name() << endl << endl;

}

/* Sends 1000 datamsg requests to one server through FIFO Channel and
   dumps the data to x1.csv regardless of the patient ID */
void req_multiple_data_points(FIFORequestChannel *chan,
							  int person,
							  double time,
							  int ecg)
{
	// TODO: open file "x1.csv" in received directory using ofstream
	ofstream myfile;
	myfile.open("received/x1.csv", ios::out);
	// set-up timing for collection
	timeval t;
	gettimeofday(&t, nullptr);
	double t1 = (1.0e6 * t.tv_sec) + t.tv_usec;

	// requesting data points
	time = 0.0;

	// TODO: Receive 1000 data points
	for (int i = 0; i < 1000; i++)
	{
		myfile << time << flush;

		char buf[MAX_MESSAGE];
		ecg = 1;
		datamsg x(person, time, ecg);

		memcpy(buf, &x, sizeof(datamsg));
		chan->cwrite(buf, sizeof(datamsg));

		serverresponse resp(-1, -1);
		chan->cread(&resp, sizeof(serverresponse));

		char buf2[MAX_MESSAGE];
		ecg = 2;
		datamsg y(person, time, ecg);

		memcpy(buf2, &y, sizeof(datamsg));
		chan->cwrite(buf2, sizeof(datamsg));

		serverresponse resp2(-1, -1);
		chan->cread(&resp2, sizeof(serverresponse));

		myfile << ", " << resp.ecgData << ", " << resp2.ecgData << flush;

		time += 0.004;
		myfile << endl;
	}

	// compute time taken to collect
	gettimeofday(&t, nullptr);
	double t2 = (1.0e6 * t.tv_sec) + t.tv_usec;

	// display time taken to collect
	cout << "Time taken to collect 1000 datapoints :: " << ((t2 - t1) / 1.0e6) << " seconds on channel " << chan->name() << endl;

	// closing file
	myfile.close();

}

/* Request Server to send contents of file (could be .csv or any other format) over FIFOChannel and
   dump it to a file in recieved folder */
void transfer_file(FIFORequestChannel *chan,
				   string filename,
				   int buffersize)
{
	// sending a file msg to get length of file
	// create a filemsg, buf, and use cwrite to write the message
	filemsg fm(0,0);
	int len = sizeof(filemsg) + (filename.size() + 1);
	char* buf_req = new char[len];
	memcpy(buf_req, &fm, sizeof(filemsg));
	strcpy(buf_req + sizeof(filemsg), filename.c_str());
	chan->cwrite(buf_req, len);
	
	// receiving response, i.e., the length of the file
	// Hint: use cread
	int64_t filesize = 0;
	chan->cread(&filesize, sizeof(int64_t));

	char* buf_resp = new char[buffersize];
	// opening file to receive into
	ofstream myfile;
	myfile.open("./received/" + filename);
	
	// set-up timing for transfer
	timeval t;
	gettimeofday(&t, nullptr);
	double t1 = (1.0e6 * t.tv_sec) + t.tv_usec;

	// requesting file
	// TODO: Create a filemsg with the current offset you want
	// to read from. Receive data from the server.
	
	
	char* buf3 = new char[len];
	//memcpy(buf3, &transfer, sizeof(filemsg));
	//strcpy(buf3 + sizeof(filemsg), filename.c_str());
	//chan->cwrite(buf_req, len);
	int offset = 0;
	int transfer_length = buffersize;
	while(filesize > buffersize){
		filemsg transfer(offset, transfer_length);
		memcpy(buf3, &transfer, sizeof(filemsg));
		strcpy(buf3 + sizeof(filemsg), filename.c_str());
		chan->cwrite(buf3, len);
		chan->cread(buf_resp, buffersize);
		//fwrite(buf_resp, sizeof(buf_resp), buffersize, fp);
		myfile.write(buf_resp, transfer_length);
		offset += buffersize;
		filesize -= buffersize;
	}
	//transfer->offset = filesize;
	transfer_length = filesize;
	filemsg transfer(offset, transfer_length);
	memcpy(buf3, &transfer, sizeof(filemsg));
	strcpy(buf3 + sizeof(filemsg), filename.c_str());
	chan->cwrite(buf3, len);
	chan->cread(buf_resp, transfer_length);
	//fwrite(buf_resp, sizeof(buf_resp), filesize, fp);
	myfile.write(buf_resp, transfer_length);
	

	// compute time taken to transfer
	gettimeofday(&t, nullptr);
	double t2 = (1.0e6 * t.tv_sec) + t.tv_usec;

	// display time taken to transfer
	cout << "Time taken to transfer file :: " << ((t2 - t1) / 1.0e6) << " seconds" << endl
		 << endl;

	// closing file
	delete[] buf_req;
	delete[] buf_resp;
	delete[] buf3;
	myfile.close();
	
}

int main(int argc, char *argv[])
{
	int person = 0;
	double time = 0.0;
	int ecg = 0;

	int buffersize = MAX_MESSAGE;

	string filename = "";

	bool datachan = false;

	int num_of_servers = 2;

	int opt;
	while ((opt = getopt(argc, argv, "p:t:e:m:f:s:c")) != -1)
	{
		switch (opt)
		{
		case 'p':
			person = atoi(optarg);
			break;
		case 't':
			time = atof(optarg);
			break;
		case 'e':
			ecg = atoi(optarg);
			break;
		case 'm':
			buffersize = atoi(optarg);
			break;
		case 'f':
			filename = optarg;
			break;
		case 's':
			num_of_servers = atoi(optarg);
			break;
		case 'c':
			datachan = true;
			break;
		}
	}
	
	/*
	for(iterate n times for n servers){
		fork and exec
		if(fork successful){
			FIFORequestChannel* ch = new chan("control_"+to_string(serverId)+"_", FIFORequestChannel::CLIENT_SIDE);
			channels.push_back(chan);
		}
	}
	FIFORequestChannel* serverchannel; //calculate based on patient number and number of servers*/
	
	/*
		Example Code Snippet for reference, you can delete after you understood the basic functionality.

		FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);

		// example data point request
		char buf[MAX_MESSAGE]; // 256
		datamsg x(1, 0.0, 1);

		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question

		serverresponse resp(-1,-1);
		chan->cread(&resp, sizeof(serverresponse)); //answer

		cout << resp.ecgData <<" data on server:"<< resp.serverId << " on channel " << chan->name() << endl << endl;

		// sending a non-sense message, you need to change this
		filemsg fm(0, 0);
		string fname = "teslkansdlkjflasjdf.dat";

		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);  // I want the file length;

		delete[] buf2;

		// closing the channel
		MESSAGE_TYPE m = QUIT_MSG;
		chan.cwrite(&m, sizeof(MESSAGE_TYPE));

	*/

	verify_server_count(num_of_servers);

	bool data = ((person != 0) && (time != 0.0) && (ecg != 0));
	bool file = (filename != "");
	// Initialize FIFORequestChannel channels.
	// What should be the size of the object?
	// HINT: We need n channels(one for each of the servers) plus one more for new datachannel request.
	FIFORequestChannel** channels = new FIFORequestChannel*[num_of_servers + 1];
	//int step1;
	//int target_server;
	//pid_t* pid = new pid_t[num_of_servers];
	// fork to create servers
	for(int i = 0; i < num_of_servers; i++){
		//cout << "made it here";
		int pid = fork();
		if(pid == 0){
			string m = to_string(buffersize);
			string s = to_string(i + 1);
			char *args[] = {(char*)"./server", (char*)"-m", (char*)m.c_str(), (char*)"-s", (char*)s.c_str(), NULL};
			execvp(args[0], args);
		} else {
			FIFORequestChannel* ch = new FIFORequestChannel("control_"+to_string(i + 1)+"_", FIFORequestChannel::CLIENT_SIDE);
			channels[i + 1] = ch;
		}
	}
	// open all requested channels

	// Initialize step and serverId
	int step = 16 / num_of_servers;
	int serverId = ceil((double) person / step);

	if (!filename.empty())
	{
		string patient = filename.substr(0, filename.find('.'));
		std::istringstream ss(patient);
		int patient_id;
		// If ss has a patient id, set serverId using ceil again
		// else, use the first server for file transfer
		if(ss >> patient_id) {
			serverId = ceil( (double) patient_id / step);
		} else {
			serverId = 1;
		}
		

	}

	// By this line, you should have picked a server channel to which you can send requests to.
	FIFORequestChannel* serverchannel = channels[serverId];
	// TODO: Create pointer for all channels
	//cout << serverchannel->name() << endl;
	//cout << buffersize << endl;
	if (datachan)
	{
		// call set_new_channel
		set_new_channel(serverchannel, channels, buffersize);
		//cout << serverchannel->name();
	}
	//cout << buffersize << endl;
	// request a single data point
	if (data)
	{
		// call req_single_data_point
		req_single_data_point(serverchannel, person, time, ecg);
	}

	// request a 1000 data points for a person
	if (!data && person != 0)
	{
		// call req_multiple_data_points
		req_multiple_data_points(serverchannel, person, time, ecg);
	}

	// transfer a file
	if (file)
	{
		// call transfer_file
		transfer_file(serverchannel, filename, buffersize);
	}

	// close the data channel
	// if datachan has any value, cwrite with channels[0]
	MESSAGE_TYPE m = QUIT_MSG;
	if(datachan) {
		channels[0]->cwrite(&m, sizeof(MESSAGE_TYPE));
		delete channels[0];
	}
	// close and delete control channels for all servers
	for(int i = num_of_servers; i > 0; i--){
		channels[i]->cwrite(&m, sizeof(MESSAGE_TYPE));
		delete channels[i];
	}
	delete[] channels;
	// wait for child to close
	//int status;
	
	wait(nullptr);
	
	//delete[] pid;

}
