#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    Command* cmd = smash.getFGCmd();
    pid_t fgPid = smash.getFGpid();
    JobsList job_list = smash.getJobsList();
    if(fgPid > 0){
        if(kill(fgPid ,SIGSTOP) == -1) ErrorHandling("kill");
        cout << "smash: process " << fgPid << " was stopped" << endl;
        job_list.addJob(cmd, fgPid, true, true);
    }
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t fgPid = smash.getFGpid();
    if(fgPid > 0){
        if(kill(fgPid ,SIGKILL) == -1) ErrorHandling("kill");
        cout << "smash: process " << fgPid << " was killed" << endl;
    }
}

void alarmHandler(int sig_num) {
    cout << "smash got an alarm" << endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t alarm_pid = smash.getFGpid();
    if(alarm_pid > 0){
        if(kill(alarm_pid, SA_RESTART ,SIGKILL) == -1) ErrorHandling("kill");
        cout << "smash: " << cmd << " timed out!" << endl;
    }
}
