#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

int main() {
    printf("Running appBlock2\n");
    sleep(5);
    
    kill(getpid(), SIGINT);
    exit(0);
}

