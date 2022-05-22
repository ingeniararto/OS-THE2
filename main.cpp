#include <iostream>
#include "hw2_output.h"
#include <string.h>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
using namespace std;

unsigned long row, col;
struct timeval start_time;

template <typename T>
struct _2DimensonalArray
{
    inline void init_arr()
    {
        arr = new T[row * col];
    }
    T *arr;
    T *operator[](size_t p_row)
    {
        return arr + (col * p_row);
    }
    inline void zero_init_arr()
    {
        arr = new T[row * col]{};
    }
};
_2DimensonalArray<int> grid;
_2DimensonalArray<pair<bool, unsigned int>> cell_occupied; // is cell occupied by proper private
_2DimensonalArray<bool> s_cell_occupied;                   // is cell occupied by sneaky smoker

vector<pair<uint8_t, u_int8_t>> map = { // mapping for clockwise matrix traversal
    make_pair(0, 0),
    make_pair(0, 1),
    make_pair(0, 2),
    make_pair(1, 2),
    make_pair(2, 2),
    make_pair(2, 1),
    make_pair(2, 0),
    make_pair(1, 0)};

// mutexes
pthread_mutex_t lockers_queue = PTHREAD_MUTEX_INITIALIZER;           // lock for occupation
pthread_mutex_t smoking = PTHREAD_MUTEX_INITIALIZER;                 // lock for increasing cigbutts
pthread_mutex_t condition_lock = PTHREAD_MUTEX_INITIALIZER;          // lock for cond timedwait
pthread_mutex_t continue_condition_lock = PTHREAD_MUTEX_INITIALIZER; // lock for cond timedwait

pthread_cond_t command_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t continue_command_condition = PTHREAD_COND_INITIALIZER;

enum command_type
{
    EMPTY,
    BREAK,
    CONTINUE,
    STOP,
};

command_type flag = EMPTY;

typedef struct p_private
{
    int gid;
    int s_i; // row position
    int s_j; // column position
    long t_g;
    int n_g;
    int where;
    vector<pair<int, int>> position;
} p_private;

typedef struct s_smoker
{
    int sid;
    long t_s;
    int n_s;
    int where;
    vector<pair<int, int>> position;
    vector<int> cignum;
} s_smoker;

typedef struct command
{
    int time;
    command_type type;
} command;

p_private *create_p_private(char *line)
{
    p_private *new_private = new p_private();
    new_private->gid = stoi(strtok(line, " "));
    new_private->s_i = stoi(strtok(NULL, " "));
    new_private->s_j = stoi(strtok(NULL, " "));
    new_private->t_g = stoi(strtok(NULL, " "));
    new_private->n_g = stoi(strtok(NULL, " "));
    new_private->where = 0;
    return new_private;
}

s_smoker *create_s_smoker(char *line)
{
    s_smoker *new_smoker = new s_smoker();
    new_smoker->sid = stoi(strtok(line, " "));
    new_smoker->t_s = stoi(strtok(NULL, " "));
    new_smoker->n_s = stoi(strtok(NULL, " "));
    new_smoker->where = 0;
    return new_smoker;
}

static long get_timestamp(struct timeval start_time) // for time difference
{
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return (cur_time.tv_sec - start_time.tv_sec) * 1000000 + (cur_time.tv_usec - start_time.tv_usec);
}

int commandwait(long ms)
{
    struct timespec wait;
    struct timespec curr_time;
    int status;
    timespec_get(&curr_time, TIME_UTC);
    wait.tv_sec = curr_time.tv_sec + ms / 1000;
    wait.tv_nsec = curr_time.tv_nsec + (ms % 1000) * 1000000;
    if (wait.tv_nsec >= 1000000000)
    {
        wait.tv_nsec -= 1000000000;
        wait.tv_sec++;
    }
    pthread_mutex_lock(&condition_lock);
    status = pthread_cond_timedwait(&command_condition, &condition_lock, &wait);
    pthread_mutex_unlock(&condition_lock);
    if (status = ETIMEDOUT && !flag)
    {
        printf("Done\n");
        return 0;
    }
    else if (flag == BREAK)
    {
        return BREAK;
    }
    else if (flag == STOP)
    {
        return STOP;
    }
}

int us_commandwait(long us)
{
    struct timespec wait;
    struct timespec curr_time;
    int status;
    timespec_get(&curr_time, TIME_UTC);
    wait.tv_sec = curr_time.tv_sec + us / 1000;
    wait.tv_nsec = curr_time.tv_nsec + us;
    if (wait.tv_nsec >= 1000000000)
    {
        wait.tv_nsec -= 1000000000;
        wait.tv_sec++;
    }
    pthread_mutex_lock(&condition_lock);
    status = pthread_cond_timedwait(&command_condition, &condition_lock, &wait);
    pthread_mutex_unlock(&condition_lock);
    if (status = ETIMEDOUT && !flag)
    {
        return 0;
    }
    else if (flag == BREAK)
    {
        return BREAK;
    }
    else if (flag == STOP)
    {
        return STOP;
    }
}

void break_handler(bool did_locked, int p_row, int p_col, int a, int b, int gid)
{
    hw2_notify(PROPER_PRIVATE_TOOK_BREAK, gid, 0, 0);
    if (did_locked)
    {
        pthread_mutex_lock(&lockers_queue);
        for (int j = p_row; j < a; j++)
        {
            for (int k = p_col; k < b; k++)
            {
                cell_occupied[j][k].first = false;
            }
        }
        pthread_mutex_unlock(&lockers_queue);
    }
    pthread_mutex_lock(&continue_condition_lock);
    pthread_cond_wait(&continue_command_condition, &continue_condition_lock);
    pthread_mutex_unlock(&continue_condition_lock);
    hw2_notify(PROPER_PRIVATE_CONTINUED, gid, 0, 0);
}

int gather(int i, int j, int a, int b, long time, int gid)
{ // cigbut gathering
    hw2_notify(PROPER_PRIVATE_ARRIVED, gid, i, j);
    int command;
    for (int row = i; row < a; row++)
    {
        for (int col = j; col < b; col++)
        {
            while (grid[row][col])
            {
                if (command = commandwait(time) == BREAK)
                {
                    // cerr << "go to handler" << endl;
                    break_handler(true, i, j, a, b, gid);
                    return 1;
                }
                else if (command == STOP)
                {
                    // stop
                    return 2;
                }
                grid[row][col]--;
                hw2_notify(PROPER_PRIVATE_GATHERED, gid, row, col);
            }
        }
    }
    hw2_notify(PROPER_PRIVATE_CLEARED, gid, 0, 0);
    for (int row = i; row < a; row++)
    {
        for (int col = j; col < b; col++)
        {
            cell_occupied[row][col].first = false;
        }
    }
    return 0;
}

int smoke(int i, int j, long time, int sid, int cignum)
{
    hw2_notify(SNEAKY_SMOKER_ARRIVED, sid, i + 1, j + 1);
    int command;
    while (cignum)
    {
        for (int num = 0; num < 8 && cignum; num++)
        {
            if ((map[num].first) == 1 && (map[num].second) == 1)
            {
                continue;
            }
            if (command = commandwait(time) == STOP)
            {
                // cerr << "stop" << endl;
                return 2;
            }
            pthread_mutex_lock(&smoking);
            grid[i + map[num].first][j + map[num].second]++;
            pthread_mutex_unlock(&smoking);
            hw2_notify(SNEAKY_SMOKER_FLICKED, sid, i + map[num].first, j + map[num].second);
            cignum--;
        }
    }

    hw2_notify(SNEAKY_SMOKER_LEFT, sid, 0, 0);
    pthread_mutex_lock(&lockers_queue);
    for (int row = i; row < i + 3; row++)
    {
        for (int col = j; col < j + 3; col++)
        {
            cell_occupied[row][col].second--;
        }
    }
    pthread_mutex_unlock(&lockers_queue);
    s_cell_occupied[i + 1][j + 1] = false;

    return 0;
}

void *proper_private_thread(void *pprivate)
{
    p_private *proper_private = (struct p_private *)(pprivate);
    hw2_notify(PROPER_PRIVATE_CREATED, proper_private->gid, 0, 0);
    int a = proper_private->s_i;
    int b = proper_private->s_j;
    int status = 0;
    long time = proper_private->t_g;
    int gid = proper_private->gid;
    for (int i = 0; i < proper_private->n_g; i++)
    {
        int p_row = proper_private->position[i].first;
        int p_col = proper_private->position[i].second;
        bool can_go_in;
    go_back_and_check: // label
        can_go_in = true;
        for (int j = p_row; j < p_row + a; j++)
        {
            for (int k = p_col; k < p_col + b; k++)
            {
                if (flag == BREAK)
                {
                    break_handler(false, p_row, p_col, p_row + a, p_col + b, proper_private->gid);
                    goto go_back_and_check;
                }
                else if (flag == STOP)
                {
                    can_go_in = false;
                    status = 2;
                    goto end;
                }
                if (cell_occupied[j][k].first || cell_occupied[j][k].second)
                {
                    can_go_in = false;
                    j = p_row + a;
                    break;
                }
            }
        }
        if (can_go_in)
        {
            pthread_mutex_lock(&lockers_queue);
            for (int j = p_row; j < p_row + a; j++)
            {
                for (int k = p_col; k < p_col + b; k++)
                {
                    if (flag == BREAK)
                    {
                        pthread_mutex_unlock(&lockers_queue);
                        break_handler(false, p_row, p_col, p_row + a, p_col + b, proper_private->gid);
                        goto go_back_and_check;
                    }
                    else if (flag == STOP)
                    {
                        pthread_mutex_unlock(&lockers_queue);
                        can_go_in = false;
                        status = 2;
                        goto end;
                    }
                    if (cell_occupied[j][k].first || cell_occupied[j][k].second)
                    {
                        can_go_in = false;
                        j = p_row + a;
                        break;
                    }
                }
            }
            if (!can_go_in)
            {
                pthread_mutex_unlock(&lockers_queue);
                goto go_back_and_check;
            }
            for (int j = p_row; j < p_row + a; j++)
            {
                for (int k = p_col; k < p_col + b; k++)
                {
                    if (flag == BREAK)
                    {
                        pthread_mutex_unlock(&lockers_queue);
                        break_handler(true, p_row, p_col, p_row + a, p_col + b, proper_private->gid);
                        goto go_back_and_check;
                    }
                    else if (flag == STOP)
                    {
                        pthread_mutex_unlock(&lockers_queue);
                        can_go_in = false;
                        status = 2;
                        goto end;
                    }
                    cell_occupied[j][k].first = true;
                }
            }
            pthread_mutex_unlock(&lockers_queue);
            status = gather(p_row, p_col, p_row + a, p_col + b, time, gid);
            if (status == 1)
            { // break and continue
                goto go_back_and_check;
            }
            else if (status == 2)
            { // stop
                goto end;
            }
        }
        else if (status != 2)
        {
            goto go_back_and_check;
        }
    }
end:
    if (status == 2)
    {
        hw2_notify(PROPER_PRIVATE_STOPPED, proper_private->gid, 0, 0);
        pthread_exit(NULL);
    }
    hw2_notify(PROPER_PRIVATE_EXITED, proper_private->gid, 0, 0);
    pthread_exit(NULL);
}

void *sneaky_smoker_thread(void *ssmoker)
{
    s_smoker *sneaky_smoker = (struct s_smoker *)ssmoker;
    hw2_notify(SNEAKY_SMOKER_CREATED, sneaky_smoker->sid, 0, 0);
    struct timeval start_time;
    int status = 0;
    gettimeofday(&start_time, NULL);
    long time = sneaky_smoker->t_s;
    int sid = sneaky_smoker->sid;
    int i, j, k;
    for (int i = 0; i < sneaky_smoker->n_s; i++)
    {
        int p_row = sneaky_smoker->position[i].first;
        int p_col = sneaky_smoker->position[i].second;
        int cignum = sneaky_smoker->cignum[i];
        bool can_go_in;
    go_back_and_check:
        can_go_in = true;
        for (j = p_row - 1; j < p_row + 2; j++)
        {
            for (k = p_col - 1; k < p_col + 2; k++)
            {
                if (flag == STOP)
                {
                    status = 2;
                    goto end;
                }
                if (cell_occupied[j][k].first)
                {
                    can_go_in = false;
                    j = p_row + 2;
                    break;
                }
            }
        }
        if (s_cell_occupied[p_row][p_col])
        {
            can_go_in = false;
        }
        if (can_go_in)
        {
            pthread_mutex_lock(&lockers_queue);
            for (j = p_row - 1; j < p_row + 2; j++)
            {
                for (k = p_col - 1; k < p_col + 2; k++)
                {
                    if (flag == STOP)
                    {
                        pthread_mutex_unlock(&lockers_queue);
                        status = 2;
                        goto end;
                    }
                    if (cell_occupied[j][k].first)
                    {
                        can_go_in = false;
                        j = p_row + 2;
                        break;
                    }
                }
            }
            if (s_cell_occupied[p_row][p_col])
            {
                can_go_in = false;
            }
            if (!can_go_in)
            {
                pthread_mutex_unlock(&lockers_queue);
                goto go_back_and_check;
            }
            for (j = p_row - 1; j < p_row + 2; j++)
            {
                for (k = p_col - 1; k < p_col + 2; k++)
                {
                    if (flag == STOP)
                    {
                        pthread_mutex_unlock(&lockers_queue);
                        status = 2;
                        goto end;
                    }
                    cell_occupied[j][k].second++;
                }
            }
            s_cell_occupied[p_row][p_col] = true;
            pthread_mutex_unlock(&lockers_queue);
            status = smoke(p_row - 1, p_col - 1, time, sid, cignum);
            if (status == 2)
            {
                goto end;
            }
        }
        else
        {
            goto go_back_and_check;
        }
    }
end:
    if (status == 2)
    {
        hw2_notify(SNEAKY_SMOKER_STOPPED, sneaky_smoker->sid, 0, 0);
        pthread_exit(NULL);
    }
    hw2_notify(SNEAKY_SMOKER_EXITED, sneaky_smoker->sid, 0, 0);
    pthread_exit(NULL);
}

void *command_thread(void *commands_vector)
{
    vector<command *> *commands = (vector<command *> *)commands_vector;
    long time;
    command *command;
    for (int i = 0; i < commands->size(); i++)
    {
        command = (*commands)[i];
        while ((time = get_timestamp(start_time)) < (command->time) * 1000)
            ;
        switch (command->type)
        {
        case BREAK:

            hw2_notify(ORDER_BREAK, 0, 0, 0);
            pthread_mutex_lock(&condition_lock);
            pthread_cond_broadcast(&command_condition);
            pthread_mutex_unlock(&condition_lock);
            flag = BREAK;

            break;
        case CONTINUE:

            hw2_notify(ORDER_CONTINUE, 0, 0, 0);
            pthread_mutex_lock(&continue_condition_lock);
            pthread_cond_broadcast(&continue_command_condition);
            pthread_mutex_unlock(&continue_condition_lock);
            flag = CONTINUE;

            break;
        case STOP:
            hw2_notify(ORDER_STOP, 0, 0, 0);
            pthread_mutex_lock(&condition_lock);
            pthread_cond_broadcast(&command_condition);
            pthread_mutex_unlock(&condition_lock);
            flag = STOP;

            break;
        default:
            break;
        }
    }
    pthread_exit(NULL);
}

int main()
{

    string get_line;
    char *line;
    int pp_num = 0;
    int order_num = 0;
    int ss_num = 0;

    vector<p_private *> p_privates;
    p_private *new_private;

    vector<s_smoker *> s_smokers;
    s_smoker *new_ssmoker;

    vector<command *> commands;
    command *new_command;

    getline(cin, get_line);
    line = new char[get_line.length() + 1];
    copy(get_line.begin(), get_line.end(), line);
    line[get_line.length()] = '\n';

    char *first_part = strtok(line, " "); // taking row and column number
    row = stoi(first_part);
    col = stoi(strtok(NULL, " "));

    grid.init_arr();
    cell_occupied.zero_init_arr();
    s_cell_occupied.zero_init_arr();

    for (int i = 0; i < row; i++)
    { // initializing grid
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';

        char *cigbutt = strtok(line, " ");
        for (int j = 0; j < col; j++)
        {
            grid[i][j] = stoi(cigbutt);
            cigbutt = strtok(NULL, " ");
        }
    }

    getline(cin, get_line); // starting parsing proper privates
    pp_num = stoi(get_line);

    for (int i = 0; i < pp_num; i++)
    {
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';
        new_private = create_p_private(line);
        for (int i = 0; i < new_private->n_g; i++)
        {
            getline(cin, get_line);
            free(line);
            line = new char[get_line.length() + 1];
            copy(get_line.begin(), get_line.end(), line);
            line[get_line.length()] = '\n';
            int p_row = stoi(strtok(line, " "));
            int p_col = stoi(strtok(NULL, " "));
            new_private->position.push_back(make_pair(p_row, p_col));
        }
        p_privates.push_back(new_private);
    }

    getline(cin, get_line); // starting parsing commands
    order_num = stoi(get_line);

    for (int i = 0; i < order_num; i++)
    {
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';
        new_command = new command();
        new_command->time = stoi(strtok(line, " "));
        char *type = strtok(NULL, " ");
        if (type[0] == 'c')
        {
            new_command->type = CONTINUE;
        }
        else if (type[0] == 'b')
        {
            new_command->type = BREAK;
        }
        else
        {
            new_command->type = STOP;
        }
        commands.push_back(new_command);
    }

    getline(cin, get_line); // starting parsing sneaky smokers
    ss_num = stoi(get_line);

    for (int i = 0; i < ss_num; i++)
    {
        getline(cin, get_line);
        free(line);
        line = new char[get_line.length() + 1];
        copy(get_line.begin(), get_line.end(), line);
        line[get_line.length()] = '\n';
        new_ssmoker = create_s_smoker(line);
        for (int i = 0; i < new_ssmoker->n_s; i++)
        {
            getline(cin, get_line);
            free(line);
            line = new char[get_line.length() + 1];
            copy(get_line.begin(), get_line.end(), line);
            line[get_line.length()] = '\n';
            int p_row = stoi(strtok(line, " "));
            int p_col = stoi(strtok(NULL, " "));
            new_ssmoker->position.push_back(make_pair(p_row, p_col));
            new_ssmoker->cignum.push_back(stoi(strtok(NULL, " ")));
        }
        s_smokers.push_back(new_ssmoker);
    }

    // start processes
    hw2_init_notifier();
    gettimeofday(&start_time, NULL);
    int j = 0;
    int k = 0;
    int l = 0;
    int a = 0;
    pthread_t *threads = new pthread_t[pp_num + ss_num + order_num];
    while (j < pp_num)
    {
        pthread_create(&threads[a], NULL, proper_private_thread, (void *)p_privates[j]);
        j++;
        a++;
    }
    while (k < ss_num)
    {
        pthread_create(&threads[a], NULL, sneaky_smoker_thread, (void *)s_smokers[k]);
        k++;
        a++;
    }
    pthread_create(&threads[a], NULL, command_thread, (void *)&commands);
    a++;
    for (int a = 0; a < pp_num + ss_num + 1; a++)
    {
        int e = pthread_join(threads[a], NULL);
        if (e)
        {
            cerr << a << "/" << e << "unable to join \n";
        }
    }

    /*for(int x=0; x<row; x++){
        for (int y = 0; y < col; y++)
        {
            cout << x << "," << y << ":" << grid[x][y] << endl;
        }

    }*/

    return 0;
}