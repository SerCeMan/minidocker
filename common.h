#pragma once

#include <string.h>
#include <fstream>
#include <vector>
#include <err.h>
#include <dirent.h>
#include <sys/stat.h>

using namespace std;

#define check_result(value, message) \
if (value < 0){ \
    printf("%s get error: %d %s \n", message, value,  strerror(errno)); \
    exit(value); \
} \


int result;
int errCode;

#define check_mkdir(value, message) \
result = value; \
errCode = errno; \
if(result < 0 && errCode != EEXIST){ \
    printf("%s get error return %d and msg is %s \n", message, result,  strerror(errCode)); \
    exit(errCode); \
} \


void set_map(char *file, int inside_id, int outside_id, int len) {
    FILE *mapfd = fopen(file, "w");
    if (NULL == mapfd) {
        perror("open file error");
        printf("set_map error %s", file);
        exit(0);
    }
    fprintf(mapfd, "%d %d %d\n", inside_id, outside_id, len);
    fclose(mapfd);
}

void set_uid_map(pid_t pid, int inside_id, int outside_id, int len) {
    char file[256];
    sprintf(file, "/proc/%d/uid_map", pid);
    set_map(file, inside_id, outside_id, len);
}

void set_gid_map(pid_t pid, int inside_id, int outside_id, int len) {
    char file[256];
    sprintf(file, "/proc/%d/gid_map", pid);
    set_map(file, inside_id, outside_id, len);
}


void remove_from_list(int id) {
    ifstream fin("containers");
    vector<string> lines;
    string line;
    while (getline(fin, line)) {
        if (atoi(line.c_str()) != id) {
            lines.push_back(line);
        }
    }
    fin.close();
    ofstream fout("containers");
    fout << "";
    for (int i = 0; i < lines.size(); ++i) {
        fout << lines[i] << endl;
    }
    fout.close();
//    string cmd = "rm -rf /tmp/cgroup/cpu/" + to_string(id);
//    system(cmd.c_str());
    string cmd2 = "rmdir /tmp/cgroup/cpu/" + to_string(id);
    system(cmd2.c_str());
    string cmd = "sudo rm -rf /tmp/cgroup/cpu/" + to_string(id);
    system(cmd.c_str());
}