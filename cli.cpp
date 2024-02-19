#include <iostream>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <string>
#include <vector>
#include <thread>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
using namespace std;

pthread_mutex_t lockk = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waitt = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_listpush = PTHREAD_MUTEX_INITIALIZER;
vector<pthread_t> thread_ids;
vector<int> background_job_list;

bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

vector<vector<string> > parse()
{
    vector< vector<string> > commands;
    vector< vector<string> > nullVec;
    // Open the file
    ifstream inputFile("commands.txt");

    // Check if the file is open
    if (!inputFile.is_open()) {
        cerr << "Error opening file." << endl;
        return nullVec; // Return NULL
    }

    // clean parse.txt:
    ofstream cleanFile("parse.txt");
    cleanFile.close();

    string line;
    while (getline(inputFile, line)) {
        // Parse the line into strings using stringstream
        istringstream iss(line);
        vector<string> tokens;

        // Read each space-separated string
        string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        int current_token = 0;

        string command, input, option, redirection, filename, background_job;
        command = tokens[current_token++];
        if(isAlpha(tokens[current_token][0]))
        {
            input = tokens[current_token++];
        }
        if (tokens[current_token][0] == '-')
        {
            option = tokens[current_token++];
        }
        if (tokens[current_token] == "<" || tokens[current_token] == ">")
        {
            redirection = tokens[current_token++];
            filename = tokens[current_token++];
        }
        else redirection = "-";
        if (current_token != tokens.size() && tokens[current_token] == "&") 
            background_job = "y";
        else 
            background_job = "n";
        
        ofstream outputFile("parse.txt", ios::app);
        if (!outputFile.is_open()) {
            cerr << "Error opening file." << endl;
            return nullVec; // Return NULL
        }
        outputFile << "----------" << endl;
        outputFile << "Command: " << command << endl;
        outputFile << "Inputs: " << input << endl;
        outputFile << "Options: " << option << endl;
        outputFile << "Redirection: " << redirection << endl;
        outputFile << "Background Job: " << background_job << endl;
        outputFile << "----------" << endl;
        outputFile.close();

        vector<string> newLine;

        newLine.push_back(command);
        newLine.push_back(input);
        newLine.push_back(option);
        newLine.push_back(redirection);
        newLine.push_back(filename);
        newLine.push_back(background_job);

        commands.push_back(newLine);
    }

    
    // Close the file
    inputFile.close();

    return commands;
}

vector<string> commandCleaned (vector<string> command)
{
    vector<string> result;
    result.push_back(command[0]);
    if (command[1] != "") result.push_back(command[1]);
    if (command[2] != "") result.push_back(command[2]);

    return result;
}

void* outThread(void * file)
{
    pthread_mutex_lock(&lockk);


    char buff[256];
    cout << "---- " << pthread_self() << endl;
    while (fgets(buff, sizeof(buff), reinterpret_cast<FILE *>(file))) {
        cout << buff;
    }
    cout << "---- " << pthread_self() << endl;
    fflush(reinterpret_cast<FILE *>(file));
    //fclose(reinterpret_cast<FILE *>(file));


    pthread_mutex_unlock(&lockk);
	return NULL;
}

void* wait_for_all(int self = 0) {
    pthread_mutex_lock(&waitt);
    //wait background processes
    for(int i = 0; i < background_job_list.size(); i++) {
        waitpid(background_job_list[i], NULL, 0);
    }
    background_job_list.clear();

    //wait threads
    for(int i = 0; i < thread_ids.size(); i++)
    {
        pthread_join(thread_ids[i], NULL);
    }
    thread_ids.clear();
    pthread_mutex_unlock(&waitt);
    return NULL;
}


void runCommand(vector<string> command)
{
    if(command[0] == "wait")
    {
        wait_for_all();
        return;
    }
    vector<string> cleanCommand = commandCleaned(command);
    int output_to_std = (command[3] != ">");
    int isBackground = (command[5] == "y");
    int fd[2];
    FILE* temp;
    
    if(output_to_std) 
    {
        pipe(fd);
    }
    int rc = fork();
    if(rc == 0)
    {
        char *myargs[cleanCommand.size() + 1];
        for(int i = 0; i < cleanCommand.size(); i++) myargs[i] = strdup(cleanCommand[i].c_str());
        myargs[cleanCommand.size()] = NULL;


        
        if (output_to_std)
        {
            close(fd[0]); 
            
            if(command[3] == "<") {
                freopen(command[4].c_str(), "r", stdin);
            }

            dup2(fd[1], STDOUT_FILENO); 
            close(fd[1]);
        }
        else {
            close(STDOUT_FILENO); 
            open(command[4].c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0666);
        }
        

        execvp(myargs[0], (myargs));
    }
    else if (rc > 0)
    {
        

        if(output_to_std)
        {
                close(fd[1]);

                pthread_t thread;
                FILE *fp = fdopen(fd[0], "r");
                
                pthread_create(&thread, NULL, outThread, (void *) fp);

                pthread_mutex_lock(&lock_listpush);
                thread_ids.push_back(thread);
                pthread_mutex_unlock(&lock_listpush);
        }

        if(!isBackground) {waitpid(rc, NULL, 0);}
        else background_job_list.push_back(rc);
    }

}



int main() {
    vector<vector<string> > commands = parse();
    for(int a = 0; a < commands.size(); a++)
        runCommand(commands[a]);
    wait_for_all();
    return 0;
}