#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <unistd.h>
#include <map>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <ctime>
#include <fstream>
#include <chrono>
#include <thread>
#include <list>

using std::map;
using std::string;

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (50)
#define MAX_PROCCESS_SIMULTAINOUSLY (100)
#define MAX_PATH (100)
#define MAX_COMMAND_LENGTH (200)

const string WHITESPACE = " \n\r\t\f\v";
class SmallShell;

enum status{
    stopped, runningBG
};

void ErrorHandling(string syscall, bool to_exit = false);
int findRedirectionCommand(const string& cmd_line);
int findPipeCommand(const string& cmd_line);

class Command {
protected:
    string _cmd_line;
    bool is_stopped = false;
public:
    explicit Command(const char* cmd_line);
    virtual ~Command() = default;
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
    string getCmdLine() const {return _cmd_line;}
    void setCmdStatus(bool isStopped){if(isStopped) is_stopped = true; }
    bool isCmsStopped(){return is_stopped;}
    static bool isNumber(const string &str); // check if the string represent number

};

class BuiltInCommand : public Command {
protected:
    char* _args[COMMAND_MAX_ARGS];
    int _num_of_args;
public:
    explicit BuiltInCommand(const char* cmd_line);
    ~BuiltInCommand() override;
    int getNumOfArgs() const{ return _num_of_args;}
    const char* getArgs(int i) const{ return _args[i];}
};

class ExternalCommand : public Command {
public:
    bool _isBackground = false;
    ExternalCommand(const char* cmd_line) : Command(cmd_line) {};
    ~ExternalCommand() override = default;
    void execute() override;
};

class PipeCommand : public Command {
    string _first_cmd;
    string _second_cmd;
    bool _is_std_error = false;

public:
    explicit PipeCommand(const char* cmd_line);
    ~PipeCommand() override = default;
    void execute() override;
};

class RedirectionCommand : public Command {
    bool _append = false;
    string _cmd_to_exc;
    string _file;
public:
    explicit RedirectionCommand(const char* cmd_line);
    ~RedirectionCommand() override = default;
    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};

class HeadCommand : public BuiltInCommand {
    int _lines = 10;
    string _file_path;
public:
    explicit HeadCommand(const char* cmd_line);
    ~HeadCommand() override = default;
    void execute() override;
};

class TimeOutEntry{
    string _cmd_line;
    time_t _start_time;
    pid_t _cmd_pid;
    int _duration;
    bool _is_finish = false;
public:
    TimeOutEntry(string cmd, pid_t pid, int duration) : _cmd_line(cmd), _cmd_pid(pid), _start_time(time(nullptr)), _duration(duration) {};
    double getTimePassed() const {return difftime(time(nullptr), _start_time);}
    ~TimeOutEntry() = default;
    bool isTimeOut() const { return getTimePassed() >= _duration ;}
    int getDuration() const { return _duration;}
    string getCmdLine() {return _cmd_line;}
    pid_t getPid() const {return _cmd_pid;}
    int getTimeUntilKill() const {return _start_time + _duration - time(NULL);}
    bool getIfFinish() const {return _is_finish;}
    void setIfFinish(bool flag){_is_finish = flag;}
};

class TimeOutList{
    std::list<TimeOutEntry> _time_out_list;
public:
    std::list<TimeOutEntry>& getList() {return _time_out_list;}
    TimeOutList() = default;
    ~TimeOutList() = default;
    void addToList(string cmd, pid_t pid, int duration);
    void removeTimeOutByPid(pid_t pid);
    int getMinAlarm();
    TimeOutEntry& getTimeOutEntryByPid(pid_t pid);
};

class TimedOutCommand : public BuiltInCommand  {
public:
    explicit TimedOutCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~TimedOutCommand() override = default;
    void execute() override;
};


/*********************************************************************************************************************/
/*****************************************JOB LIST*******************************************************************/

class JobEntry {
    int _job_id;
    string _cmd_line;
    time_t _start_time;
    status _status; // status == 1 -> stopped other running in the bg
    pid_t _cmd_pid;
public:
    JobEntry(int job_id = -1, string cmd_line = "", pid_t pid = -1, time_t t = time(0), status s=runningBG) : _job_id(job_id), _cmd_line(cmd_line), _cmd_pid(pid), _start_time(t),_status(s) {};
    JobEntry(const JobEntry& job) { _job_id = job._job_id, _cmd_line = job._cmd_line, _cmd_pid = job._cmd_pid, _start_time = job._start_time,_status = job._status;}
    ~JobEntry() = default;
    int getJobId() const {return _job_id;}
    int getCmdPid() const {return _cmd_pid;}
    string getCmdLine() const {return _cmd_line; }
    double getSecondElapsed() const {return difftime(time(nullptr), _start_time);}
    status getStatus() const{ return _status;}
    void setStatus(status newStatus) { _status = newStatus; }
    string getCommand() const{return _cmd_line;}
};

class JobsList {
    map<int, JobEntry> _jobs = {};
//    int _num_of_jobs = 1;
    int _max_id_job;
public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(string cmd_line, pid_t pid, bool isStopped = false);
    void printJobsList();
    void killAllJobs(bool to_print = true);
    void removeJobById(int jobId){ _jobs.erase(jobId); }
    void removeJobByPid(int pid);
    JobEntry * getJobById(int jobId);
    int getMaxId() { return (_jobs.empty()) ? 1 : ( ((_jobs.rbegin())->first) +1 ); }
    map<int, JobEntry>& getJobs() {return _jobs;}
    int getLastJob();
    int getLastStoppedJob();
};

// #include <list>

// class TimedJobList{
//     list<time_t, JobEntry> _timed_jobs = {};
// public:
//     TimedJobList() = default;
//     ~TimedJobList() = default;
//     void addTimedJob(string cmd_line, pid_t pid, time_t duration);
//     void killAllJobs();
//     void removeJobById(int jobId){ _jobs.erase(jobId); }
//     list<time_t, JobEntry>& getJobs() {return _timed_jobs;}
// };

/*****************************************************************/
/***********************BUILD-IN COMMANDS*************************/

class ChPromptCommand : public BuiltInCommand {
public:
    explicit ChPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~ChPromptCommand() override = default;;
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~ShowPidCommand() override = default;;
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~GetCurrDirCommand() override = default;
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    explicit ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~ChangeDirCommand() override = default;
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
public:
    explicit JobsCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~JobsCommand() override = default;
    void execute() override;
};

class KillCommand : public BuiltInCommand {
public:
    explicit KillCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~KillCommand() override = default;
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
public:
    explicit ForegroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~ForegroundCommand() override = default;
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
public:
    explicit BackgroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~BackgroundCommand() override = default;
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
public:
    explicit QuitCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~QuitCommand() override = default;
    void execute() override;
};

/*********************************************************************************************************************/
/*******************************************SMALL SHELL**************************************************************/

class SmallShell {
private:
    string _name = "smash";
    JobsList _jobs_list;
    TimeOutList _timed_jobs_list;
    int _pid;
    SmallShell();
    string _last_path = "";
    string _curr_path;
    string _FG_cmd;
    pid_t _FG_pid = -1;
    pid_t _FG_Job_ID = -1;
    bool _is_redirection = false;

public:
    string _time_out_cmd = "";
    int _time_out_duration = -1;
    bool isQuit = false;
    Command* CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    void executeCommand(const char* cmd_line);
    // getter's and setter's
    void setName(const string& new_name){_name = new_name;}
    string getName() const {return _name;}
    int getPid() const {return _pid;}
    string getLastPath() const {return _last_path;}
    string getCurrPath() const {return _curr_path;}
    void setLastPath(string newLastPath){_last_path = newLastPath;}
    void setCurrPath(string newCurrPath){_curr_path = newCurrPath;}
    JobsList& getJobsList() {return _jobs_list;}
    TimeOutList& getTimedList() {return _timed_jobs_list;}
    string getFGCmd() const {return _FG_cmd;}
    void setFGCmd(string newFGCmd){ _FG_cmd = newFGCmd;}
    pid_t getFGpid() const {return _FG_pid;}
    pid_t getFGJobID() const {return _FG_Job_ID;}
    void setFGpid(pid_t newFGpid){ _FG_pid = newFGpid;}
    void setFGJobID(pid_t newFGJobID){ _FG_Job_ID = newFGJobID;}
    void removeFinishedJobs();


};


#endif //SMASH_COMMAND_H_
