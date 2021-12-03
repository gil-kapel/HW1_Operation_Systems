#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {

    struct sigaction alarm, ctrlc, ctrlz;
    
    alarm.sa_flags = SA_RESTART;
    sigemptyset(&alarm.sa_mask);
    alarm.sa_handler = alarmHandler;

    ctrlc.sa_flags = SA_RESTART;
    sigemptyset(&ctrlc.sa_mask);
    ctrlc.sa_handler = ctrlCHandler;

    ctrlz.sa_flags = SA_RESTART;
    sigemptyset(&ctrlz.sa_mask);
    ctrlz.sa_handler = ctrlZHandler;

    if(sigaction(SIGTSTP, &ctrlz , nullptr) == -1) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(sigaction(SIGINT, &ctrlc, nullptr) == -1) {
        perror("smash error: failed to set ctrl-C handler");
    }
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
    }
    
    return 0;
}