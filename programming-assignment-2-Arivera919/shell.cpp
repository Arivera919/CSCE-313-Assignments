#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    int stdin_copy = dup(0);
    int stdout_copy = dup(1);
    vector<pid_t>bg_proc;
    char* prevDir = nullptr;
    for (;;) {
        // need date/time, username, and absolute path to current dir
        char* user = getenv("USER");
        time_t curTime = time(NULL);
        char* displaytime = ctime(&curTime);
        string tm = displaytime;
        tm.replace(tm.length() - 1, 1, "\0");
        char* curDir = getcwd(NULL, 0);
        cout << YELLOW << tm << " " << user << ":" << curDir << "$" << NC << " ";

        // get user inputted command
        string input;
        getline(cin, input);

        int status = 0;

        for(long unsigned int i = 0; i < bg_proc.size(); i++){
            status = 0;
            
            if(waitpid(bg_proc.at(i), &status, WNOHANG) == bg_proc.at(i)){
                bg_proc.erase(bg_proc.begin() + i);
            }
            
        }

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            free(curDir);
            free(prevDir);
            break;
        }



        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }
        //char** args = new char*[tknr.commands.at(0)->args.size() + 1];

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        /*for (auto cmd : tknr.commands) {
            for (auto str : cmd->args) {
                cerr << "|" << str << "| ";
            }
            if (cmd->hasInput()) {
                cerr << "in< " << cmd->in_file << " ";
            }
            if (cmd->hasOutput()) {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }*/

        if(tknr.commands.at(0)->args.at(0) == "cd"){
            if(tknr.commands.at(0)->args.at(1) == "-"){
                free(curDir);
                chdir(prevDir);
                continue;
            } else {
                free(curDir);
                prevDir = getcwd(NULL, 0);
                chdir(tknr.commands.at(0)->args.at(1).c_str());
                continue;
            }
        }


        //TODO: create functions for I/O redirection and handling args
        if(tknr.commands.size() > 1){
            int pipefds[2];
            

            for(long unsigned int i = 0; i < tknr.commands.size(); i++){
                char** args = new char*[tknr.commands.at(i)->args.size() + 1];
                pipe(pipefds);
                pid_t pid = fork();
                
                if(pid == 0){

                    for(long unsigned int j = 0; j < tknr.commands.at(i)->args.size(); j++){
                        args[j] = (char*) tknr.commands.at(i)->args.at(j).c_str();
                    }

                    args[tknr.commands.at(i)->args.size()] = NULL;

                    if ((i == 0) || (i == (tknr.commands.size() - 1))) {
                        if(tknr.commands.at(i)->hasInput()){// has "<"
                            int fd = open(tknr.commands.at(i)->in_file.c_str(), O_RDONLY);
                            dup2(fd, 0);

                        } 
                
                        if(tknr.commands.at(i)->hasOutput()){// has ">"
                            int fd = open(tknr.commands.at(i)->out_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                            dup2(fd, 1);


                        }
                    }

                    
                    

                    if(i < tknr.commands.size() - 1){
                        dup2(pipefds[1], 1);
                    }

                    execvp(args[0], args);
                } else{
                    dup2(pipefds[0], 0);
                    close(pipefds[1]);
                    if(i == (tknr.commands.size() - 1)){
                        int status = 0;
                        waitpid(pid, &status, 0);
                        if (status > 1) {  // exit if child didn't exec properly
                            exit(status);
                        }
                    }

                    delete[] args;
                }
                
                
            }



        } else{
            // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            char** args = new char*[tknr.commands.at(0)->args.size() + 1];

            if (pid == 0) {  // if child, exec to run command
                // run single commands with arguments


                for(long unsigned int j = 0; j < tknr.commands.at(0)->args.size(); j++){
                    args[j] = (char*) tknr.commands.at(0)->args.at(j).c_str();
                }

                args[tknr.commands.at(0)->args.size()] = NULL;

                
                if(tknr.commands.at(0)->hasInput()){// has "<"
                    int fd = open(tknr.commands.at(0)->in_file.c_str(), O_RDONLY);
                    dup2(fd, 0);

                } 
                
                if(tknr.commands.at(0)->hasOutput()){// has ">"
                    int fd = open(tknr.commands.at(0)->out_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    dup2(fd, 1);


                }            

                if (execvp(args[0], args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
            
            }
            else {  // if parent, wait for child to finish
                
                if(tknr.commands.at(0)->isBackground()){
                    bg_proc.push_back(pid);
                    //continue;
                } else{
                    int status = 0;
                    waitpid(pid, &status, 0);
                    if (status > 1) {  // exit if child didn't exec properly
                        exit(status);
                    }
                }

                
                
                delete[] args;
            
            }
        }
        free(curDir);
        if (prevDir != nullptr){
            free(prevDir);
        }

        dup2(stdin_copy, 0);
        dup2(stdout_copy, 1);
        
        
        
        /*
        // fork to create child
        pid_t pid = fork();
        if (pid < 0) {  // error check
            perror("fork");
            exit(2);
        }

        if (pid == 0) {  // if child, exec to run command
            // run single commands with no arguments

            
            for(long unsigned int j = 0; j < tknr.commands.at(0)->args.size(); j++){
                args[j] = (char*) tknr.commands.at(0)->args.at(j).c_str();
            }

            args[tknr.commands.at(0)->args.size()] = NULL;

            //char* args[] = {(char*) tknr.commands.at(0)->args.at(0).c_str(), nullptr};
            

            if (execvp(args[0], args) < 0) {  // error check
                perror("execvp");
                exit(2);
            }
            
        }
        else {  // if parent, wait for child to finish
            int status = 0;
            waitpid(pid, &status, 0);
            if (status > 1) {  // exit if child didn't exec properly
                exit(status);
            }
            
        }
        delete[] args;*/
    }
    //free(prevDir);

}
