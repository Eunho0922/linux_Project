#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>

#define MAX_STR 100
#define BLOCK_COUNT 10
#define LOGDIR "./log"
#define LOGFILE "restart.txt"

char* trim(const char* s);

typedef struct {
    char name[20];
    char restart_count[10];
    char start_time[30];
    char reason[50];
    int int_restart;
} SwInfo;

typedef struct {
    char SwBlock[20];
    char App_para1[20];
    char App_para2[20];
    char App_para3[20];
} SwParam;

typedef struct {
    pid_t pids[BLOCK_COUNT];
    pid_t dpid;
    int p_no;
    SwInfo sw_info[BLOCK_COUNT];
    SwParam sw_param[BLOCK_COUNT];
} SwManager;

char* gettime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t t = tv.tv_sec;
    struct tm* ptm = localtime(&t);
    static char str[1024];
    sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d",
            ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
            ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    return str;
}

void InitStruct(SwManager* info) {
    info->p_no = 0;
    info->dpid = 0;
    for (int i = 0; i < BLOCK_COUNT; i++) {
        info->pids[i] = 0;
        memset(&info->sw_info[i], 0, sizeof(SwInfo));
        memset(&info->sw_param[i], 0, sizeof(SwParam));
    }
}

void readFileList(SwManager* info) {
    FILE* fp = fopen("./blockList.txt", "r");
    if (!fp) {
        perror("Failed to open blockList.txt");
        exit(EXIT_FAILURE);
    }
    char str[MAX_STR];
    int swno = 0;

    while (fgets(str, MAX_STR, fp)) {
        str[strlen(str) - 1] = '\0';
        char* token = strtok(str, ";");
        strcpy(info->sw_param[swno].SwBlock, token);

        int i = 0;
        while ((token = strtok(NULL, ";")) != NULL) {
            token = trim(token);
            if (i == 0) strcpy(info->sw_param[swno].App_para1, token);
            else if (i == 1) strcpy(info->sw_param[swno].App_para2, token);
            else if (i == 2) strcpy(info->sw_param[swno].App_para3, token);
            i++;
        }
        swno++;
    }
    fclose(fp);
    info->p_no = swno;
}

void FileLogger(SwInfo* list) {
    mkdir(LOGDIR, 0755);
    FILE* fp = fopen(LOGDIR "/" LOGFILE, "a");
    if (fp) {
        fprintf(fp, "Process: %s | Restart Count: %s | Start Time: %s | Reason: %s\n",
                list->name, list->restart_count, list->start_time, list->reason);
        fclose(fp);
    }
}

void SpawnBlock(SwManager* info) {
    for (int i = 0; i < info->p_no; i++) {
        strcpy(info->sw_info[i].name, info->sw_param[i].SwBlock);
        sprintf(info->sw_info[i].reason, "Init.");
        sprintf(info->sw_info[i].restart_count, "%d", 0);
        strcpy(info->sw_info[i].start_time, gettime());

        FileLogger(&info->sw_info[i]);

        pid_t pid = fork();
        if (pid > 0) {
            info->pids[i] = pid;
        } else if (pid == 0) {
            execl(info->sw_param[i].SwBlock, info->sw_param[i].SwBlock,
                  info->sw_param[i].App_para1,
                  info->sw_param[i].App_para2,
                  info->sw_param[i].App_para3, NULL);
            exit(EXIT_FAILURE);
        }
    }
}

void respawnProcess(SwManager* info, int index) {
    pid_t pid = fork();
    if (pid > 0) {
        info->pids[index] = pid;
        strcpy(info->sw_info[index].start_time, gettime());
    } else if (pid == 0) {
        execl(info->sw_param[index].SwBlock, info->sw_param[index].SwBlock,
              info->sw_param[index].App_para1,
              info->sw_param[index].App_para2,
              info->sw_param[index].App_para3, NULL);
        exit(EXIT_FAILURE);
    }
}

int FindIndex(SwManager* info) {
    for (int i = 0; i < info->p_no; i++) {
        if (info->dpid == info->pids[i]) {
            return i;
        }
    }
    return -1;
}

void LogPrint(SwManager* info) {
    printf(" _____________________________________________________________________________\n");
    printf("| Process Name | Restart Count |       Start Time        |        Reason        |\n");
    printf("|______________|_______________|_________________________|_______________________|\n");
    for (int i = 0; i < info->p_no; i++) {
        printf("| %12s | %13s | %23s | %22s |\n",
               info->sw_info[i].name,
               info->sw_info[i].restart_count,
               info->sw_info[i].start_time,
               info->sw_info[i].reason);
    }
    printf("|______________|_______________|_________________________|_______________________|\n");
}

char* trim(const char* s) {
    while (isspace(*s)) s++;
    char* r = strdup(s);
    char* end = r + strlen(r) - 1;
    while (end > r && isspace(*end)) end--;
    *(end + 1) = '\0';
    return r;
}

int main() {
    SwManager manager;
    InitStruct(&manager);
    readFileList(&manager);
    SpawnBlock(&manager);

    while (1) {
        int status;
        manager.dpid = waitpid(-1, &status, 0);
        if (manager.dpid > 0) {
            int index = FindIndex(&manager);
            manager.sw_info[index].int_restart++;
            sprintf(manager.sw_info[index].restart_count, "%d", manager.sw_info[index].int_restart);

            if (WIFEXITED(status)) {
                sprintf(manager.sw_info[index].reason, "EXIT(%d)", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                sprintf(manager.sw_info[index].reason, "SIGNAL(%d).%s",
                        WTERMSIG(status), strsignal(WTERMSIG(status)));
            }

            FileLogger(&manager.sw_info[index]);
            respawnProcess(&manager, index);
            LogPrint(&manager);
        }
    }

    return 0;
}
