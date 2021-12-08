#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t fgPid = smash.getFGpid();
    JobsList job_list = smash.getJobsList();
    if(fgPid > 0 ){
        // job_list.addJob(cmd, fgPid, true);
        if(kill(fgPid ,SIGSTOP) == -1) ErrorHandling("kill");
        cout << "smash: process " << fgPid << " was stopped" << endl;
    }
    else return;
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t fgPid = smash.getFGpid();
    if(fgPid > 0){
        if(kill(fgPid ,SIGKILL) == -1) ErrorHandling("kill");
        cout << "smash: process " << fgPid << " was killed" << endl;
        smash.setFGpid(-1);
        smash.setFGJobID(-1);
        smash.setFGCmd("");
    }
}

void alarmHandler(int sig_num) {
    cout << "smash got an alarm" << endl;
    SmallShell& smash = SmallShell::getInstance();
    JobEntry* timed_job = smash.getTimedJobsList().getJobById(smash.getTimedJobsList().getMaxId());
    pid_t alarm_pid = timed_job->getCmdPid();
    if(alarm_pid == smash.getFGpid()){
        if(kill(alarm_pid, SIGKILL) == -1) ErrorHandling("kill");
        cout << "smash: " << timed_job->getCmdLine() << " timed out!" << endl;
        smash.setFGpid(-1);
        smash.setFGJobID(-1);
        smash.setFGCmd("");
    }
}
