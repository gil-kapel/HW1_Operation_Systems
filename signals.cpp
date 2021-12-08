#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;
extern  SmallShell& smash;

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    pid_t fgPid = smash.getFGpid();
    JobsList job_list = smash.getJobsList();
    if(fgPid > 0 ){
        // job_list.addJob(cmd, fgPid, true);
        if(kill(fgPid ,SIGSTOP) == -1) return ErrorHandling("kill");
        cout << "smash: process " << fgPid << " was stopped" << endl;
    }
    else return;
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    pid_t fgPid = smash.getFGpid();
    if(fgPid > 0){
        if(kill(fgPid ,SIGKILL) == -1) return ErrorHandling("kill");
        cout << "smash: process " << fgPid << " was killed" << endl;
        smash.setFGpid(-1);
        smash.setFGJobID(-1);
        smash.setFGCmd("");
    }
}

void alarmHandler(int sig_num) {
    smash.removeFinishedJobs();
    cout << "smash: got an alarm" << endl;
    for(auto &it : smash.getTimedList().getList()){
        pid_t pid_to_alarm =it.getPid();
        if(it.getDuration() == it.getTimePassed()){
            cout << "smash: " << it.getCmdLine() << " timed out!" << endl;
            if(!it.getIfFinish()){
                if(kill(pid_to_alarm, SIGKILL) == -1) return ErrorHandling("kill");
            }
            smash.getTimedList().removeTimeOutByPid(pid_to_alarm);
        }
    }
}
