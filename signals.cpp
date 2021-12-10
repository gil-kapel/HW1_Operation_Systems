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
    cout<< "smash: got an alarm" <<endl;
    SmallShell& smash = SmallShell::getInstance();
    smash.removeFinishedJobs();
    for(auto &iter : smash.getTimedList().getList()){
        if(smash.getTimedList().getList().empty()) break;
        if(iter.getPid() != smash.getPid()){
            if(iter.getTimePassed() >= iter.getDuration()){
                if(!(iter.getIfFinish())){
                    if (kill(iter.getPid(), SIGKILL) == -1) ErrorHandling("kill");
                    cout << "smash: " << iter.getCmdLine() << " timed out!" << endl;
                    smash.getJobsList().removeJobByPid(iter.getPid());
                    
                }
                smash.getTimedList().removeTimeOutByPid(iter.getPid());
            }
        }
    }
}
