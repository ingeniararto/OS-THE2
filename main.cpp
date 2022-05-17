#include <iostream>
#define cerr errc
#include "hw2_output.h"
#include <string.h>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <sys/time.h>
#undef cerr
using namespace std;

unsigned long row, col;
int a;
template<typename T>
struct _2DimensonalArray{
    inline void init_arr(){
        arr = new T[row*col];
    }
    T* arr;
    T* operator[](size_t p_row){
        return arr+(col*p_row);
    }
    inline void zero_init_arr(){
        arr = new T[row*col]{};
    }
};
_2DimensonalArray<int> grid;
_2DimensonalArray<pthread_mutex_t> mutex;
_2DimensonalArray<pair<bool,bool>> cell_occupied;
_2DimensonalArray<bool> s_cell_occupied;

pthread_mutex_t lockers_queue = PTHREAD_MUTEX_INITIALIZER;

enum command_type {
    BREAK,
    CONTINUE,
    STOP,
};

typedef struct p_private{
    int gid;
    int s_i; //row position
    int s_j; //column position
    long t_g; 
    int n_g;
    int where;
    vector<pair<int,int>> position;
} p_private;

typedef struct s_smoker{
    int sid;
    long t_s;
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

static long get_timestamp(struct timeval start_time)
{
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return (cur_time.tv_sec - start_time.tv_sec) * 1000000 
           + (cur_time.tv_usec - start_time.tv_usec);
}

void gather(int i, int j, int a, int b, long time, int gid){
    hw2_notify(PROPER_PRIVATE_ARRIVED, gid, i, j);
    for(int row=i; row<a; row++){
        for(int col=j; col<b; col++){
            while(grid[row][col]){
                struct timeval cur_time;
                gettimeofday(&cur_time, NULL);
                while(get_timestamp(cur_time) < time);
                grid[row][col]--;
                hw2_notify(PROPER_PRIVATE_GATHERED, gid, row, col);
            }
        }
    }
    hw2_notify(PROPER_PRIVATE_CLEARED, gid, 0, 0);
    for(int row=i; row<a; row++){
        for(int col=j; col<b; col++){
            cell_occupied[row][col].first = false;
        }
    }


}


void smoke(int i, int j, int a, int b, long time, int sid, int cignum){
    hw2_notify(SNEAKY_SMOKER_ARRIVED, sid, i+1, j+1);
    while(cignum){
        for(int row=i; row<a && cignum; row++){
            for(int col=j; col<b && cignum; col++){
                if(row==i+1 && col == j+1){
                    continue;
                }
                    struct timeval cur_time;
                    gettimeofday(&cur_time, NULL);
                    while(get_timestamp(cur_time) < time);
                    grid[row][col]++;
                    cignum--;
                    hw2_notify(SNEAKY_SMOKER_FLICKED, sid, row, col);
            }
        }
    }
    hw2_notify(SNEAKY_SMOKER_LEFT, sid, 0, 0);
    for(int row=i; row<a; row++){
        for(int col=j; col<b; col++){
            cell_occupied[row][col].first = false;
        }
    }
    s_cell_occupied[i+1][j+1]=false;


}

void* proper_private_thread(void* pprivate){
    p_private* proper_private = (struct p_private*)(pprivate);
    hw2_notify(PROPER_PRIVATE_CREATED, proper_private->gid, 0, 0);
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    //while((time = get_timestamp(start_time))<proper_private->t_g);
    int a = proper_private->s_i;
    int b = proper_private->s_j;
    long time = proper_private->t_g;
    int gid = proper_private->gid;
    for(int i=0; i<proper_private->n_g; i++){
        int p_row = proper_private->position[i].first;
        int p_col = proper_private->position[i].second;
        bool can_go_in;
        go_back_and_check:
        can_go_in = true;
        for(int j=p_row; j<p_row+a; j++){
            for(int k=p_col; k<p_col+b; k++){
                if(cell_occupied[j][k].first || cell_occupied[j][k].second){
                    can_go_in = false;
                    j = p_row+a;
                    break;
                }
            }
        }
        if(can_go_in){
            pthread_mutex_lock(&lockers_queue);
            for(int j=p_row; j<p_row+a; j++){
                for(int k=p_col; k<p_col+b; k++){
                    if(cell_occupied[j][k].first || cell_occupied[j][k].second){
                        can_go_in = false;
                        j = p_row+a;
                        break;
                    }
                }
            }
            if(!can_go_in){
                pthread_mutex_unlock(&lockers_queue);
                goto go_back_and_check;
            }
            for(int j=p_row; j<p_row+a; j++){
                for(int k=p_col; k<p_col+b; k++){
                    cell_occupied[j][k].first = true;
                }
            }
            pthread_mutex_unlock(&lockers_queue);
            // here we have area
            gather(p_row, p_col, p_row+a, p_col+b, time, gid);
        }
        else{
            goto go_back_and_check;
        }
    }
    //cerr << proper_private->gid << ":" << time << "\n";
    hw2_notify(PROPER_PRIVATE_EXITED, proper_private->gid, 0, 0);
    pthread_exit(NULL);
}

void* sneaky_smoker_thread(void* ssmoker){
    s_smoker* sneaky_smoker = (struct s_smoker*)ssmoker;
    hw2_notify(SNEAKY_SMOKER_CREATED, sneaky_smoker->sid, 0, 0);
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    long time = sneaky_smoker->t_s;
    int sid = sneaky_smoker->sid;
    int i, j, k;
    for(int i=0; i<sneaky_smoker->n_s; i++){
        int p_row = sneaky_smoker->position[i].first;
        int p_col = sneaky_smoker->position[i].second;
        int cignum = sneaky_smoker->cignum[i];
        bool can_go_in;
        go_back_and_check:
        can_go_in = true;
        for(j=p_row-1; j<p_row+2; j++){
            for(k=p_col-1; k<p_col+2; k++){
                if(cell_occupied[j][k].first){
                    can_go_in = false;
                    j = p_row+a;
                    break;
                }
            }
        }
        if(s_cell_occupied[p_row][p_col]){
            can_go_in=false;
        }
        if(can_go_in){
            pthread_mutex_lock(&lockers_queue);
            for(j=p_row-1; j<p_row+2; j++){
                for(k=p_col-1; k<p_col+2; k++){
                    if(cell_occupied[j][k].first){
                        can_go_in = false;
                        j = p_row+a;
                        break;
                    }
                }
            }
            if(s_cell_occupied[p_row][p_col]){
                can_go_in=false;
            }
            if(!can_go_in){
                pthread_mutex_unlock(&lockers_queue);
                goto go_back_and_check;
            }
            s_cell_occupied[p_row][p_col]=true;
            for(j=p_row-1; j<p_row+2; j++){
                for(k=p_col-1; k<p_col+2; k++){
                    cell_occupied[j][k].second = true;
                }
            }
            pthread_mutex_unlock(&lockers_queue);
            // here we have area
            smoke(p_row-1, p_col-1, p_row+2, p_col+2, time, sid, cignum);
        }
        else{
            goto go_back_and_check;
        }
    }
    //cerr << sneaky_smoker->sid << ":" << time << "\n";
    hw2_notify(SNEAKY_SMOKER_EXITED, sneaky_smoker->sid, 0, 0);
    pthread_exit(NULL);
}

void* command_thread(void* command_var){
    command* command = (struct command*)command_var;
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    long time;
    while((time = get_timestamp(start_time))<command->time);
    switch (command->type)
    {
    case 0:
        cerr << command->type << ":" << time << "\n";
        hw2_notify(ORDER_BREAK, 0, 0, 0);
        break;
    case 1:
        cerr << command->type << ":" << time << "\n";
        hw2_notify(ORDER_CONTINUE, 0, 0, 0);
        break;
    case 2:
        cerr << command->type << ":" << time << "\n";
        hw2_notify(ORDER_STOP, 0, 0, 0);
        break;
    default:
        break;
    }
    pthread_exit(NULL);
}


int main(){

    hw2_init_notifier();
    string get_line;
    char *line;
    int pp_num = 0;
    int order_num =0;
    int ss_num = 0;
    
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

    char* first_part = strtok(line, " "); //taking row and column number
    row = stoi(first_part);
    col = stoi(strtok(NULL, " "));
    //cout << "row:" << row << "col:" << col << endl;

    mutex.init_arr();
    grid.init_arr();
    cell_occupied.zero_init_arr();
    s_cell_occupied.zero_init_arr();
    

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
    pp_num = stoi(get_line);
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

    /*getline(cin, get_line); //starting parsing commands
    order_num = stoi(get_line);
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
        
    }*/

    getline(cin, get_line); //starting parsing sneaky smokers
    ss_num = stoi(get_line);
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
    int l=0;
    a=0;
    pthread_t *threads = new pthread_t[pp_num+ss_num+order_num];
    while(j< pp_num){
        pthread_create(&threads[a], NULL, proper_private_thread, (void *)p_privates[j]);
        j++;
        a++;
    }
    while(k<ss_num){
        pthread_create(&threads[a], NULL, sneaky_smoker_thread, (void *)s_smokers[k]);
        k++;
        a++;
    }
    while(l<order_num){
        pthread_create(&threads[a], NULL, command_thread, (void *)commands[l]);
        l++;
        a++;
    }
    for(int a=0; a<pp_num+ss_num+order_num; a++){
        int e = pthread_join(threads[a], NULL);
        if(e){
            cerr << a << "/"<< e << "unable to join \n" ;
        }
    }


    return 0;
}