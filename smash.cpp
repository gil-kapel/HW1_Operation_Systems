#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

extern SmallShell& smash;

int main(int argc, char* argv[]) {

    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    
    struct sigaction alarm;
    
    alarm.sa_flags = SA_RESTART;
    sigemptyset(&alarm.sa_mask);
    alarm.sa_handler = alarmHandler;

    if(sigaction(SIGALRM , &alarm, nullptr) == -1) {
        perror("smash error: failed to set alarm handler");
    }

    //TODO: setup sig alarm handler
    SmallShell& smash = SmallShell::getInstance();

    while(true) {
        std::cout << smash.getName() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
        if(smash.isQuit == true) exit(0);
    }
    
    return 0;
}