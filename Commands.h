#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <algorithm>

using namespace std;
#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define SIM_MAX_PROCESSES (100)
    
enum state {Foreground = 1, Background = 2, Stopped = 3};
const string WHITESPACE = " \n\r\t\f\v";
class SmallShell;

class Command{
protected:
    string _cmd_line;
    const char* _args[COMMAND_MAX_ARGS];
    int _num_of_args;
    pid_t _pid;

    Command(const char* cmd_line){}
    virtual ~Command();
    virtual void execute() = 0;
    // virtual void prepare();
    // virtual void cleanup();
public:
    pid_t getPid() { return _pid;}
    int getNumOfArgs() { return _num_of_args;}
    const char* getArgs(int i) { return _args[i];}
    string getCmdLine() { return _cmd_line; }
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command{
public:
    BuiltInCommand(const char* cmd_line): Command(cmd_line){}
    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
    string clean_cmd; //cmd without WHitespace signs
public:
    ExternalCommand(const char* cmd_line, string clean_cmd): Command(cmd_line), clean_cmd(clean_cmd){}
    virtual ~ExternalCommand() {}
    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
 public:
    PipeCommand(const char* cmd_line): Command(cmd_line){}
    virtual ~PipeCommand() {}
    void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
    explicit RedirectionCommand(const char* cmd_line);
    virtual ~RedirectionCommand() {}
    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
    string* plastPwd;
public:
    ChangeDirCommand(const char* cmd_line, char** plastPwd): BuiltInCommand(cmd_line){
        this->plastPwd = new std::string(plastPwd);
    }
    virtual ~ChangeDirCommand() {}
    void execute() override;
    string* getPlastPwd(){return plastPwd;}
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
    GetCurrDirCommand(const char* cmd_line): BuiltInCommand(cmd_line){}
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
    ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line){
        jobs = new JobsList(jobs);
    }
    virtual ~QuitCommand() {}
    void execute() override;
    JobsList* getJobsList(){return jobs;}
};

class JobsList {
public:
    class JobEntry{
        Command* cmd;
        int jobID;
        pid_t PID;
        bool isRunning;
        time_t jobTime;
    public:
        JobEntry(Command* cmd = nullptr, int jobID = -1, int PID = -1, bool isRunning = false): 
                 cmd(cmd), jobID(jobID), PID(PID), isRunning(isRunning){time(&jobTime);}
        ~JobEntry() = default;
        bool isJobRunning(){return isRunning;}
        pid_t getPID(){ return PID;}
        int getJobID(){ return jobID;}
        time_t getJobTime() {return jobTime};
        string getCmdLine(){return cmd->getCmdLine()};
        bool activate();
        string getTime();
        Command* getCommand(){return cmd;}
};
private:
    vector<JobEntry> job_list;
public:
    JobsList(){job_list = vector<JobEntry>(SIM_MAX_PROCESSES)};
    ~JobsList() = default;
    void addJob(Command* cmd, bool isStopped = false, int pid;
    void printJobsList();
    void killAllJobs(); 
    void removeFinishedJobs(); 
    JobEntry* getJobById(int jobId);
    void removeJobById(int jobId); 
    JobEntry* getLastJob(int* lastJobId); 
    JobEntry* getLastStoppedJob(int *jobId); 
    int getListSize(){return job_list.size();}
    int getPid_ByJobId(int jobId);
    int getJobId_ByPid(int pid);
    int findMaxValue();
    int findMaxValueOfStopped();
    bool activateJob(JobEntry* job);
    bool checkIfJobExist(int pid);
    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
JobsList* jobs;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs);
    virtual ~JobsCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const char* cmd_line, JobsList* jobs);
    virtual ~KillCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class HeadCommand : public BuiltInCommand {
 public:
    HeadCommand(const char* cmd_line);
    virtual ~HeadCommand() {}
    void execute() override;
};

class SmallShell {
    JobsList* jobs;
    SmallShell();
    string prompt;
    string previos_dir;
    int PID;
    Command* cmd;
    // int fgPid;
    // int fgJobId;
public:
    std::list<TimedJob> timedList;
    TimedJob checkWhoTimedOut();
    int getNewAlarmTime();
    JobsList* getJobList();
    std::list<TimedJob>* getTimedList();
    void changePrompt(const std::string& otherPrompt);
    std::string getPrompt();
    void changeLastDir(std::string dir);
    std::string getLastDir();
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    ~SmallShell() = default;
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    void executeCommand(const char* cmd_line);
    // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
