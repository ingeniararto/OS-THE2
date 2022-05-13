#include <iostream>
#include "hw2_output.h"
#include <string.h>
#include <cstring>
#include <vector>

using namespace std;

int row, col;

enum command_type {
    BREAK,
    CONTINUE,
    STOP,
};

typedef struct p_private{
    int gid;
    int s_i; //row position
    int s_j; //column position
    int t_g; 
    int n_g;
    int where;
    vector<pair<int,int>> position;
} p_private;

typedef struct s_smoker{
    int sid;
    int t_s;
    int n_s;
    int where;
    vector<pair<int,int>> position;
    vector<int> cignum;
} s_smoker;

typedef struct command
{
    int time;
    command_type type;
} command;


p_private* create_p_private(char* line){
    p_private* new_private = new p_private();
    new_private->gid = stoi(strtok(line, " "));
    new_private->s_i = stoi(strtok(NULL, " "));
    new_private->s_j = stoi(strtok(NULL, " "));
    new_private->t_g = stoi(strtok(NULL, " "));
    new_private->n_g = stoi(strtok(NULL, " "));
    new_private->where = 0;
    return new_private;
}

s_smoker* create_s_smoker(char* line){
    s_smoker* new_smoker = new s_smoker();
    new_smoker->sid = stoi(strtok(line, " "));
    new_smoker->t_s = stoi(strtok(NULL, " "));
    new_smoker->n_s = stoi(strtok(NULL, " "));
    new_smoker->where = 0;
    return new_smoker;
}

void* proper_private_thread(void* pprivate){
    p_private* proper_private = (struct p_private*)(pprivate);
    hw2_notify(PROPER_PRIVATE_CREATED, proper_private->gid, 0, 0);

    hw2_notify(PROPER_PRIVATE_EXITED, proper_private->gid, 0, 0);
    pthread_exit(NULL);
}

void* sneaky_smoker_thread(void* ssmoker){
    s_smoker* sneaky_smoker = (struct s_smoker*)ssmoker;
    hw2_notify(SNEAKY_SMOKER_CREATED, sneaky_smoker->sid, 0, 0);

    hw2_notify(SNEAKY_SMOKER_EXITED, sneaky_smoker->sid, 0, 0);
    pthread_exit(NULL);
}


int main(){

    hw2_init_notifier();
    string get_line;
    char *line;
    
    vector<p_private*> p_privates;
    p_private* new_private;
    
    vector<s_smoker*> s_smokers;
    s_smoker* new_ssmoker;

    vector<command*> commands;
    command* new_command;

    getline(cin, get_line);
    line = new char[get_line.length() + 1];
    copy(get_line.begin(), get_line.end(), line);
    line[get_line.length()] = '\n';

    int i = 0;
    while(1){
        i++;
        if(line[i]==' '){
            break;
        }
    }
    char* first_part = strtok(line, " "); //taking row and column number
    row = stoi(first_part);
    col = stoi(strtok(NULL, " "));
    //cout << "row:" << row << "col:" << col << endl;


    int** grid = new int* [row]; //creating grid
    for(int i = 0; i<row; i++){
        grid[i]= new int[col];
    }
    

    for(int i=0; i<row; i++){ //initializing grid
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';

        char* cigbutt = strtok(line, " ");
        for(int j=0; j<col; j++){
            grid[i][j] = stoi(cigbutt);
            //cout << grid[i][j] << endl;
            cigbutt = strtok(NULL, " ");
        }
    }

    getline(cin, get_line); //starting parsing proper privates
    int pp_num = stoi(get_line);
    //cout << "pp_num" << pp_num << endl;

    for(int i =0; i<pp_num; i++){
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';
        new_private = create_p_private(line);
        for(int i=0; i<new_private->n_g; i++){
            getline(cin, get_line);
            free(line);
            line = new char[get_line.length() + 1];
            copy(get_line.begin(), get_line.end(), line);
            line[get_line.length()] = '\n';
            int p_row = stoi(strtok(line, " "));
            int p_col = stoi(strtok(NULL, " "));
            new_private->position.push_back(make_pair(p_row,p_col));
        }
        p_privates.push_back(new_private);
    }

    getline(cin, get_line); //starting parsing commands
    int order_num = stoi(get_line);
    //cout << "order_num" << order_num << endl;

    for(int i=0; i<order_num; i++){
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';
        new_command = new command();
        new_command->time = stoi(strtok(line, " "));
        char* type = strtok(NULL, " ");
        if(type[0]=='c'){
            new_command->type = CONTINUE;
        }
        else if(type[0]=='b'){
            new_command->type = BREAK;
        }
        else{
            new_command->type=STOP;
        }
        commands.push_back(new_command);
        
    }

    getline(cin, get_line); //starting parsing sneaky smokers
    int ss_num = stoi(get_line);
    //cout << "ss_num" << ss_num << endl;

    for(int i =0; i<ss_num; i++){
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';
        new_ssmoker = create_s_smoker(line);
        for(int i=0; i<new_ssmoker->n_s; i++){
            getline(cin, get_line);
            free(line);
            line = new char[get_line.length() + 1];
            copy(get_line.begin(), get_line.end(), line);
            line[get_line.length()] = '\n';
            int p_row = stoi(strtok(line, " "));
            int p_col = stoi(strtok(NULL, " "));
            new_ssmoker->position.push_back(make_pair(p_row,p_col));
            new_ssmoker->cignum.push_back(stoi(strtok(NULL, " ")));
        }
        s_smokers.push_back(new_ssmoker);
    }


    //start processes

    int j=0;
    int k=0;
    pthread_t threads[pp_num+ss_num];
    while(j< pp_num){
        pthread_create(&threads[i], NULL, proper_private_thread, (void *)p_privates[j]);
        j++;
    }
    while(k<ss_num){
        pthread_create(&threads[i], NULL, sneaky_smoker_thread, (void *)s_smokers[k]);
        k++;
    }
    for(int a=0; a<j+k; a++){
        pthread_join(a, NULL);
    }


    return 0;
}