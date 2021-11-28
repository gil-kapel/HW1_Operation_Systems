#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <unistd.h>
#include <map>
#include <sys/types.h>
#include <fcntl.h>
// #include <sys/wait.h>
#include <signal.h>

using std::map;
using std::string;

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define MAX_PATH (20)
    
const string WHITESPACE = " \n\r\t\f\v";
class SmallShell;

enum status{
    stopped, runningBG
}; /* why not bool? */

class Command {
protected:
// TODO: Add your data members
    string _cmd_line;
    const char* _args[COMMAND_MAX_ARGS];
    int _num_of_args;
    pid_t _pid;

    Command(const char* cmd_line);
    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
public:
    virtual ~Command();
    virtual void execute() = 0;
    pid_t getPid() const {return _pid;}
    int getNumOfArgs() const{return _num_of_args;}
    const char* getArgs(int i)const{ return _args[i];}
    string getCmdLine() const {return _cmd_line;}
    bool isNumber(string n); // check if the string represent number

};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line);
    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line, string clean_cmd): Command(cmd_line){}
    virtual ~ExternalCommand() {}
    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
    string first_cmd;
    string sec_cmd;
    bool is_std_error;
    SmallShell& smash;
 public:
    PipeCommand(const char* cmd_line, string first_cmd, string sec_cmd, bool is_std_error, SmallShell& s):
                            Command(cmd_line), first_cmd(first_cmd), sec_cmd(sec_cmd), is_std_error(is_std_error), smash(s){}
    virtual ~PipeCommand() {}
    void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
    string file_path;
    string s_cmd;
    bool isAppended;
 public:
    explicit RedirectionCommand(const char* cmd_line, string file_path, string s_cmd, bool isAppended): 
                                            Command(cmd_line), file_path(file_path), s_cmd(s_cmd), isAppended(isAppended){}
    virtual ~RedirectionCommand() {}
    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};

class HeadCommand : public BuiltInCommand {
public:
    HeadCommand(const char* cmd_line);
    virtual ~HeadCommand() {}
    void execute() override;
};

/*********************************************************************************************************************/
/*****************************************JOB LIST*******************************************************************/

class JobEntry {
    int _job_id;
    Command* _command;
    time_t _start_time;
    status _status; // status == 1 -> stopped other running in the bg
public:
    JobEntry(int job_id, Command* command, time_t t, status s=runningBG) : _job_id(job_id), _command(command), _start_time(t),_status(s) {};
    JobEntry(const JobEntry& job) { _job_id = job._job_id, _command = job._command, _start_time = job._start_time,_status = job._status;}
    ~JobEntry() = default;
    int getJobId() const {return _job_id;}
    string getCmdLine() const {return _command->getCmdLine();}
    double getSecondElapsed() const {return difftime(time(nullptr), _start_time);}
    status getStatus() const{ return _status;}
    void setStatus(status newStatus) {_status = newStatus;}
    Command* getCommand() const{return _command;}
};

class JobsList {
    map<int, JobEntry> _jobs = {};
//    int _num_of_jobs = 1;
    int _max_id_job = 0;
public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(Command* cmd, bool isStopped = false);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    void removeJobById(int jobId){ _jobs.erase(jobId); }
    JobEntry * getJobById(int jobId);
    int getMaxId() { _max_id_job++; return _max_id_job; }
    map<int, JobEntry>& getJobs() {return _jobs;}
    int getLastJob();
    int getLastStoppedJob();
};


/*****************************************************************/
/***********************BUILD-IN COMMANDS*************************/

class ChPromptCommand : public BuiltInCommand {
public:
    explicit ChPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~ChPromptCommand() override = default;
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~ShowPidCommand() override = default;
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
    // TODO: Add your data members
public:
    explicit ForegroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~ForegroundCommand() override = default;
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    explicit BackgroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~BackgroundCommand() override = default;
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
    explicit QuitCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    ~QuitCommand() override = default;
    void execute() override;
};

/*********************************************************************************************************************/
/*******************************************SMALL SHELL**************************************************************/

class SmallShell {
private:
    // TODO: Add your data members
    string _name = "smash";
    JobsList _jobs_list;
    int _pid;
    SmallShell();
    string _last_path = "";
    string _curr_path = "";
public:
    Command *CreateCommand(const char* cmd_line);
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

};

#endif //SMASH_COMMAND_H_
