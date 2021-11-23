#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
using namespace std;
#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

const string WHITESPACE = " \n\r\t\f\v";

class Command{
    string cmd_line;
public:
    Command(const char* cmd_line): cmd_line(cmd_line); {}
    virtual ~Command();
    virtual void execute() = 0;
    // virtual void prepare();
    // virtual void cleanup();
    std::string getCmdLine() { return cmd_line; }
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command{
public:
    BuiltInCommand(const char* cmd_line): Command(cmd_line){}
    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line): Command(cmd_line){}
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
    
#include <vector>
#include <algorithm>
enum state {Foreground = 1, Background = 2, Stopped = 3};

class JobsList {
public:
    class JobEntry{
        int jobID;
        int PID;
        state command_state;
    public:
        JobEntry(int jobID, int PID, state command_state = Stopped) : jobID(jobID), PID(PID), command_state(command_state){}
        ~JobEntry() = default;
        state getJobState(){return command_state;}
        bool IsStoppedStatus(){return command_state == Stopped;}
        int getPID(){ return PID;}
        int getJobID(){ return jobID;}
};
private:
    std::vector<JobEntry> job_list;
public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(Command* cmd, bool isStopped = false);
    void printJobsList();
    void killAllJobs(); 
    void removeFinishedJobs(); 
    JobEntry* getJobById(int jobId);
    void removeJobById(int jobId); 
    JobEntry* getLastJob(int* lastJobId); 
    JobEntry* getLastStoppedJob(int *jobId); 
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
 private:
    JobsList* jobs;
    SmallShell();
 public:
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)        = delete; // disable copy ctor
    void operator=(SmallShell const&)    = delete; // disable = operator
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
