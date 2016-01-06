#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <vector>
#include <string.h>
#include <grp.h>
#include <sys/wait.h>
#include "common.h"

using namespace std;

int pipe_fd[2];

int main(int argc, char *argv_MAIN[]) {
    using namespace std;
    int pid = atoi(argv_MAIN[1]);
    vector<char *> args;
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv_MAIN[i]);
    }
    vector<string> nsnames = {"user", "ipc", "uts", "net", "pid", "mnt"};

//    printf("Container: PID = %ld, eUID = %ld;  eGID = %ld, UID=%ld, GID=%ld\n", (long) getpid(),
//           (long) geteuid(), (long) getegid(), (long) getuid(), (long) getgid());

    pipe(pipe_fd);

    const int gid = getgid(), uid = getuid();

    // cg enter
    string cgroup_base_dir = "/tmp/cgroup";
    string system_path = cgroup_base_dir + "/cpu";
    string cg_path = system_path + "/" + to_string(pid);

    string s3 = "echo " + to_string(getpid()) + " >> " + cg_path + "/tasks";
    check_result(system(s3.c_str()), "echo tasks");


    for (auto &ns_name : nsnames) {
        stringstream ns_file_path_ss;
        ns_file_path_ss << "/proc/" << pid << "/ns/" << ns_name;
        string ns_file_path_str = ns_file_path_ss.str();
        int ns_fd = open(ns_file_path_str.c_str(), O_RDONLY, O_CLOEXEC);
        if (ns_fd == -1) {
            perror("ns_fd");
            return 1;
        }

        check_result(setns(ns_fd, 0), "setns");
        check_result(close(ns_fd), "close ns");;
    }

    stringstream root_path;
    root_path << "/proc/" << pid << "/" << "root";
    int fd = open(root_path.str().c_str(), O_RDONLY);
    int err = fchdir(fd);
    if (err != 0) {
        printf("CHDIR error %d\n %s", err, strerror(errno));
        exit(EXIT_FAILURE);
    }


//    printf("Container: PID = %ld, eUID = %ld;  eGID = %ld, UID=%ld, GID=%ld\n", (long) getpid(),
//           (long) geteuid(), (long) getegid(), (long) getuid(), (long) getgid());

    err = chroot(".");
    if (err < 0) {
        printf("CHROOT error %d\n %s", err, strerror(errno));
        exit(EXIT_FAILURE);
    }
    err = chdir("/bin");
    if (err < 0) {
        printf("CHDIR error %d\n %s", err, strerror(errno));
        exit(EXIT_FAILURE);
    }

    int cpid = fork();
    if (cpid == 0) {

        setgroups(0, NULL);
        setgid(0);
        setuid(0);


        err = execvp(args[0], &args[0]);
        if (err < 0) {
            printf("EXEC error %d\n %s", err, strerror(errno));
            exit(EXIT_FAILURE);
        }

    } else {
        close(pipe_fd[1]);
        waitpid(cpid, NULL, 0);
    }
    return 0;
}