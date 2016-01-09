#include <iostream>
#include <string.h>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/mount.h>
#include <grp.h>
#include <sys/capability.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <bits/unique_ptr.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <thread>
#include "common.h"

using namespace std;
#define pivot_root(new_root, put_old) syscall(SYS_pivot_root,new_root,put_old)

char *increment_address(const char *address_string) {
    // convert the input IP address to an integer
    in_addr_t address = inet_addr(address_string);

    // add one to the value (making sure to get the correct byte orders)
    address = ntohl(address);
    address += 1;
    address = htonl(address);

    // pack the address into the struct inet_ntoa expects
    struct in_addr address_struct;
    address_struct.s_addr = address;

    // convert back to a string
    return inet_ntoa(address_struct);
}


class Arguments {
public:
    int pid;
    string netid = "";
    bool daemonize = false;
    int cpu_perc = 100;
    string imagePath = "";
    string net = "";
    string cmd = "";
    vector<char *> args;
};

int pipe_fd[2];

void net_P(Arguments *args);

void net_C(Arguments *args);

#define STACK_SIZE (1024 * 1024)
static char container_stack[STACK_SIZE];

int container_main(void *arg) {
    Arguments *args = (Arguments *) arg;
    int err;

    char c;
    close(pipe_fd[1]);
    read(pipe_fd[0], &c, 1);

    check_result(mount("proc", (args->imagePath + "/proc").c_str(), "proc", 0, NULL), "MOUNT PROC");
    check_result(mount("none", (args->imagePath + "/tmp").c_str(), "tmpfs", 0, NULL), "MOUNT NONE");


    check_result(chdir(args->imagePath.c_str()), "CHDIR");

    check_result(mount("sandbox-dev", "dev", "tmpfs",
                       MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME,
                       "size=64k,nr_inodes=16,mode=755"), "sand box dev");

    mknod("dev/null", S_IFREG | 0666, 0);
    mknod("dev/zero", S_IFREG | 0666, 0);
    mknod("dev/random", S_IFREG | 0666, 0);
    mknod("dev/urandom", S_IFREG | 0666, 0);

    check_result(mount("/dev/null", "dev/null", NULL, MS_BIND, NULL), "null");
    check_result(mount("/dev/zero", "dev/zero", NULL, MS_BIND, NULL), "zero");;
    check_result(mount("/dev/urandom", "dev/random", NULL, MS_BIND, NULL), "random");;
    check_result(mount("/dev/urandom", "dev/urandom", NULL, MS_BIND, NULL), "urandom");;

    check_mkdir(mkdir("sys", 0755), "mkdir /sys");
    check_result(mount("/sys", "sys", "/sys", MS_BIND | MS_REC | MS_RDONLY, NULL), "mount /sys");

    check_mkdir(mkdir("dev/shm", 0755), "mkdir dev/shm");
    check_result(mount("tmpfs", "dev/shm", "tmpfs", MS_NOSUID | MS_NODEV | MS_STRICTATIME, "mode=1777"),
                 "mount /dev/shm");

    struct stat sb;
    if (stat("/dev/mqueue", &sb) == 0 && S_ISDIR(sb.st_mode)) {
        check_mkdir(mkdir("dev/mqueue", 0755), "mkdir dev/mqueue");
        check_result(mount("/dev/mqueue", "dev/mqueue", NULL, MS_BIND, NULL), "mount /dev/mqueue");
    }

    string new_root_path = args->imagePath;
    string old_root_path = args->imagePath + "/old";
    check_mkdir(mkdir("./old", 0777), "mkdir /old");


    check_result(mount(new_root_path.c_str(), new_root_path.c_str(), "bind", MS_BIND | MS_REC, NULL), "pivot");
    check_result(pivot_root(new_root_path.c_str(), old_root_path.c_str()), "pivot");


//    err = chroot(".");
//    if (err < 0) {
//        printf("CHROOT error %d\n %s", err, strerror(errno));
//        exit(EXIT_FAILURE);
//    }
    err = chdir("/bin");
    if (err < 0) {
        printf("CHDIR error %d\n %s", err, strerror(errno));
        exit(EXIT_FAILURE);
    }

    check_result(umount2("/old", MNT_DETACH), "umount")

    setgroups(0, NULL);
//    check_result(setresgid(0, 0, 0), "gid")

//    check_result(setgid(0), "check gid");

    const char *name = "container";
    err = sethostname(name, strlen(name));
    if (err != 0) {
        printf("hostname error %d\n %s", err, strerror(errno));
        exit(10);
    }

    umask(0);

    int sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    net_C(args);

    /* Redirect standard files to /dev/null */
    if (args->daemonize) {
        freopen("/dev/null", "r", stdin);
        freopen("/tmp/err", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }


    execv(args->cmd.c_str(), &args->args[0]);
    exit(0);
}


int main(int argc, char *argv[]) {
    const int gid = getgid(), uid = getuid();

    Arguments *args = new Arguments();
    args->netid = "Net" + to_string(getpid());

    for (int i = 1; i < argc; ++i) {
        char *cur = argv[i];
        if (strcmp(cur, "-d") == 0) {
            args->daemonize = true;
        } else if (strcmp(cur, "--cpu") == 0) {
            args->cpu_perc = atoi(argv[++i]);
        } else if (strcmp(cur, "--net") == 0) {
            args->net = argv[++i];
        } else if (args->imagePath.empty()) {
            args->imagePath = cur;
        } else if (args->cmd.empty()) {
            args->cmd = cur;
            args->args.push_back(cur);
        } else {
            args->args.push_back(cur);
        }
    }
    int err;


    pipe(pipe_fd);

    int pid;

    int nses = CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD | CLONE_NEWUSER | CLONE_NEWNET;
    pid = clone(container_main, container_stack + STACK_SIZE, nses, args);
    args->pid = pid;

    check_result(pid, "CLONE");

//    printf("Container: PID = %ld, eUID = %ld;  eGID = %ld, UID=%ld, GID=%ld\n", (long) getpid(),
//           (long) geteuid(), (long) getegid(), (long) getuid(), (long) getgid());

    int mappid = pid;
    set_uid_map(mappid, 0, uid, 1);
    string cmd11 = "echo deny >> /proc/" + to_string(mappid) + "/setgroups";
    system(cmd11.c_str());
    set_gid_map(mappid, 0, gid, 1);


    const string cgroup_base_dir = "/tmp/cgroup";
    const string system_path = cgroup_base_dir + "/cpu";
    const string cg_path = system_path + "/" + to_string(pid);

    const string cmd = "mkdir -p " + cg_path;
    check_result(system(cmd.c_str()), "system call mkdir ");
    string chcmd = "chown 1000:1000 -R " + cg_path;
    check_result(system(chcmd.c_str()), "chown");

    const string s1 = "echo 100000 >> " + cg_path + "/cpu.cfs_period_us";
    check_result(system(s1.c_str()), "echo period");

    int newv = (100000 / 100) * args->cpu_perc * thread::hardware_concurrency();
    const string s2 = "echo " + to_string(newv) + " >> " + cg_path + "/cpu.cfs_quota_us";
    check_result(system(s2.c_str()), "echo quota");

    string s3 = "echo " + to_string(pid) + " >> " + cg_path + "/tasks";
    check_result(system(s3.c_str()), "echo tasks");

    net_P(args);

    ofstream f("containers", ofstream::app | ofstream::out);
    f << pid << endl;
    cout << pid << endl;
    f.close();

    close(pipe_fd[1]);


    if (!args->daemonize) {
        waitpid(pid, NULL, 0);
        remove_from_list(pid);
    }
    _exit(0);
    return 0;
}


void net_P(Arguments *args) {
    if (args->net.empty()) {
        return;
    }
    int pid = args->pid;
    const char *nedid = args->netid.c_str();

    char cmd[120];

    check_result(sprintf(cmd, "sudo ip link add name u-%s-0 type veth peer name u-%s-1", nedid, nedid), cmd);
    check_result(system(cmd), "system call ip link add ");


    check_result(sprintf(cmd, "sudo ip link set u-%s-1 netns %d", nedid, pid), cmd);
    check_result(system(cmd), "system call ip link set ");

    check_result(sprintf(cmd, "sudo ip link set u-%s-0 up", nedid), cmd);
    check_result(system(cmd), "system call ip link set ");

    check_result(sprintf(cmd, "sudo ip addr add %s/24 dev u-%s-0", increment_address(args->net.c_str()), nedid), cmd);
    check_result(system(cmd), "sudo ip set");

}

void net_C(Arguments *args) {
    if (args->net.empty()) {
        return;
    }


    check_result(system("ip link set lo up"), "set lo");
    string com = "ip link set u-" + args->netid + "-1 up";
    check_result(system(com.c_str()), "ip link up");

    const char *hostip = args->net.c_str();
    com = string("ip addr add ") + hostip + "/24 dev u-" + args->netid + "-1";
    check_result(system(com.c_str()), "ip up");

}