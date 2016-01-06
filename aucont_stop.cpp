#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "common.h"

void remove_from_list(int id);

using namespace std;


int main(int argc, char *argv[]) {
    int id = atoi(argv[1]);
    int signum = 15;
    if (argc > 2) {
        signum = atoi(argv[2]);
    }
    string cmd = "sudo kill -" + to_string(signum) + " " + to_string(id);
    check_result(system(cmd.c_str()), "KILL FAIL");

    sleep(1);
    remove_from_list(id);
    return 0;
}