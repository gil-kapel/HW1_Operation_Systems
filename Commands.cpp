#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include "Commands.h"

SmallShell& smash = SmallShell::getInstance();
using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx > MAX_COMMAND_LENGTH) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

void ErrorHandling(string syscall, bool to_exit){ /* General error message and exit -1 for syscall faliure*/
    string error_msg = "smash error: " + syscall + " failed";
    perror(error_msg.c_str());
    if(to_exit) exit(-1);
    else return;
}

/**********************************************************************************************************************/
/*****************************************SmallShell IMPLEMENTATION****************************************************/
/**********************************************************************************************************************/

SmallShell::SmallShell() {
    _pid = getpid();
    char dir[MAX_PATH];
    getcwd(dir,MAX_PATH);
    _curr_path = dir;
    _last_path = dir;
}

SmallShell::~SmallShell() {
    _jobs_list.killAllJobs();
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line); // external commands must fork
    if(cmd == nullptr) return;
    smash.getJobsList().removeFinishedJobs();
    if(_isBackgroundComamnd(cmd_line)){
        cmd->execute();
    }
    else{
        smash.setFGCmd(cmd);
        smash.setFGpid(getpid());
        // smash.setFGJobID(-1);
        cmd->execute();
        smash.setFGCmd(nullptr);
        smash.setFGpid(-1); // NO ONE RUNNING IN FG
        delete cmd;
    }
}

Command * SmallShell::CreateCommand(const char* cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    char* args_array[COMMAND_MAX_ARGS];
    int arg_num = _parseCommandLine(cmd_line, args_array);
    string firstWord;
    char tempWord[MAX_COMMAND_LENGTH];
    if( args_array[0] != nullptr ) {
        firstWord = cmd_s.substr(0, cmd_s.find_first_of(" "));
        strcpy(tempWord, firstWord.c_str());
        _removeBackgroundSign(tempWord);
        firstWord = tempWord;
    }
    else {
        for(int i = 0; i < arg_num; i++) {
            free(args_array[i]);
        }
        return nullptr;
    }
    // free
    for(int i = 0; i < arg_num; i++) {
        free(args_array[i]);
    }

    /*Built in commands handle*/
    if (firstWord.compare("chprompt") == 0) return new ChPromptCommand(cmd_line);

    else if (firstWord.compare("pwd") == 0) return new GetCurrDirCommand(cmd_line);

    else if (firstWord.compare("showpid") == 0) return new ShowPidCommand(cmd_line);

    else if (firstWord.compare("cd") == 0) return new ChangeDirCommand(cmd_line);

    else if (firstWord.compare("jobs") == 0) return new JobsCommand(cmd_line);

    else if (firstWord.compare("kill") == 0) return new KillCommand(cmd_line);

    else if (firstWord.compare("fg") == 0) return new ForegroundCommand(cmd_line);

    else if (firstWord.compare("bg") == 0) return new BackgroundCommand(cmd_line);
    
    if(firstWord == "quit") return new QuitCommand(cmd_line);

    /**********SPECIAL COMMAND*********/
    unsigned re_index = findRedirectionCommand(cmd_s.c_str());
    if(re_index <= MAX_COMMAND_LENGTH && re_index > 0) return new RedirectionCommand(cmd_line);

    unsigned pipe_index = findPipeCommand(cmd_s.c_str());
    if(pipe_index <= MAX_COMMAND_LENGTH && pipe_index > 0) return new PipeCommand(cmd_line);
    
    return new ExternalCommand(cmd_line);
}

/**********************************************************************************************************************/
/*****************************************Command IMPLEMENTATION****************************************************/
/*********************************************************************************************************************/

Command::Command(const char* cmd_line):_cmd_line(cmd_line){
}

bool Command::isNumber(const string &str){
    return str.find_first_not_of("0123456789") == string::npos;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    _num_of_args = _parseCommandLine(cmd_line, _args);
}

BuiltInCommand::~BuiltInCommand(){
    for(int i = 0; i < _num_of_args; i++) {
        free(_args[i]);
    }
}
void ExternalCommand::execute() {
    char bash_cmd[] = {"/bin/bash"};
    char flag[] = {"-c"};
    string cmd_s = _trim(string(_cmd_line));
    char cmd_c[MAX_COMMAND_LENGTH];
    strcpy(cmd_c, cmd_s.c_str());
    char *argv[] = {bash_cmd, flag, cmd_c, nullptr};

    pid_t ext_pid = fork();
    if (ext_pid == -1) ErrorHandling("fork");
    if (ext_pid == 0) { //child proce
        setpgrp();
        if (execv(bash_cmd, argv) == -1) ErrorHandling("execv");
    } else { // parent process
        int wstatus;
        if(_isBackgroundComamnd(cmd_c)) smash.getJobsList().addJob(this, ext_pid);
        else if (waitpid(ext_pid, &wstatus, WUNTRACED) == -1) return ErrorHandling("waitpid");
        if (WIFSTOPPED(wstatus)) smash.getJobsList().addJob(this, ext_pid,  true);
    }
}

/*********************************************************************************************************************/
/****************************************JOBS LIST CLASS IMPLEMENTATION***********************************************/
/********************************************************************************************************************/

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped, bool was_at_list) {
    removeFinishedJobs();
    status s = (isStopped) ? stopped : runningBG;
    if(was_at_list){
        int old_job_id = smash.getFGJobID();
        _jobs[old_job_id] = JobEntry(old_job_id, cmd, pid, time(nullptr), s);
    }
    else{
        int new_id_job = getMaxId();
        _jobs[new_id_job] = JobEntry(new_id_job, cmd, pid, time(nullptr), s);
    }
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for(const auto &iter : _jobs){
        cout << "[" << iter.first << "]" << " " << iter.second.getCmdLine() << " : " << iter.second.getCmdPid()
             << " " << iter.second.getSecondElapsed() << " secs";
        if(iter.second.getStatus() == stopped){
            cout << "(stopped)" << endl;
        }
        if(iter.second.getStatus() == runningBG){
            cout << endl;
        }
    }
}
void JobsList::killAllJobs() {
    for(auto &iter : _jobs){
        pid_t pid_to_kill = iter.second.getCmdPid();
        if (kill(pid_to_kill, SIGKILL) == -1) ErrorHandling("kill");
        cout << pid_to_kill << ": " << iter.second.getCmdLine() << endl;
    }
    _jobs.clear();
}

void JobsList::removeFinishedJobs() {
    for(auto& iter : _jobs){
        pid_t to_find = iter.second.getCmdPid();
        pid_t temp = waitpid(-1, nullptr, WNOHANG);
        if(temp == -1) ErrorHandling("waitpid");
        if(temp == to_find){
            _jobs.erase(iter.first);
        }
    }
}
int JobsList::getLastJob() {
    if(_jobs.empty()) return 0;
    int last_id_job = 0;
    for(auto &iter : _jobs){
        int temp_id_job = iter.first;
        if(temp_id_job > last_id_job){
            last_id_job = temp_id_job;
        }
    }
    return last_id_job;
}

int JobsList::getLastStoppedJob() {
    if(_jobs.empty()) return 0;
    int last_id_job = 0;
    for(auto &iter : _jobs){
        int temp_id_job = iter.first;
        if(iter.second.getStatus() == stopped && (temp_id_job > last_id_job)){
            last_id_job = temp_id_job;
        }
    }
    return last_id_job;
}

JobEntry *JobsList::getJobById(int jobId){
    if(_jobs.find(jobId) == _jobs.end()) return nullptr;
    return &_jobs[jobId];
}


/*********************************************************************************************************************/
/**************************************BUILD-IN COMMAND IMPLEMENTATION************************************************/
/*********************************************************************************************************************/

void ChPromptCommand::execute() {
    if(this->getNumOfArgs() == 1){
        smash.setName("smash");
    }
    else{
        smash.setName(this->getArgs(1));
    }
}

void ShowPidCommand::execute() {
    cout << "smash pid is" << smash.getPid() << endl;
}

void GetCurrDirCommand::execute() { //todo why not to use in cuurent path of smash class
    char cwd[MAX_PATH];
    if(getcwd(cwd, sizeof(cwd)) == nullptr) ErrorHandling("getcwd");
    cout << string(cwd) << endl;
}

void ChangeDirCommand::execute() {
    if(_num_of_args > 2){
       cerr << "smash error: cd: too many arguments" << endl;
       return;
    }
    string path = _args[1];
    if(path.compare("-") == 0){
        if(smash.getLastPath() == ""){
            cerr << "smash error: cd: OLDPWD not set" << endl;
            return;
        }
        else{
            string tmp = smash.getLastPath();
            smash.setLastPath(smash.getCurrPath());
            smash.setCurrPath(tmp);
            if(chdir(tmp.c_str()) == -1) {
                perror("smash error: chdir failed");
                return;
            }
            return;
        }
    }
    if(chdir(_args[1]) == -1) {
        perror("smash error: chdir failed");
        return;
    }
    smash.setLastPath(smash.getCurrPath());
    smash.setCurrPath(string(_args[1]));
}

void JobsCommand::execute() {
    smash.getJobsList().printJobsList();
}

void KillCommand::execute() {
    if(_num_of_args != 3 ){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    // check arg 1
    string arg1 = _args[1];
    if(arg1[0] != '-'){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    arg1 = arg1.substr(1, arg1.size() - 1);
    int signum = stoi(arg1);
    if(!isNumber(arg1) || signum > 31 || signum < 1 ){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    //check arg 2
    string arg2 = _args[2];
    if(!isNumber(arg2)){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    int job_id_get_sig = stoi(arg2);
    if(smash.getJobsList().getJobById(job_id_get_sig) == nullptr){
        cerr << "smash error: kill: job-id " << job_id_get_sig << " does not exist" << endl;
        return;
    }
    pid_t pid_get_sig = smash.getJobsList().getJobs()[job_id_get_sig].getCmdPid();

    // functionality
    if(kill(pid_get_sig, signum) == -1){
        perror("smash error: kill failed");
        return;
    }
    else{
        if(signum == SIGKILL){
            // can call removeFinishJob instead.
            smash.getJobsList().removeJobById(job_id_get_sig);
            if(waitpid(pid_get_sig, NULL, WNOHANG) == -1) {
                perror("smash error: waitpid failed");
                return;
            }
        }
        if(signum == SIGSTOP || signum == SIGTSTP){
            smash.getJobsList().getJobs()[job_id_get_sig].setStatus(stopped);
            return;
        }
        if(signum == SIGCONT){
            smash.getJobsList().getJobs()[job_id_get_sig].setStatus(runningBG);
        }
    }
    cout<< "signal number " << signum <<" was sent to pid "<< job_id_get_sig << endl;
}

void ForegroundCommand::execute() {
    if(_num_of_args == 1 && smash.getJobsList().getJobs().empty()){
        cerr<<"smash error: fg: jobs list is empty"<<endl;
        return;
    }
    if(_num_of_args > 2 ){
        cerr << "smash error: fg: invalid arguments" << endl;
        return ;
    }
    int job_id_to_fg;
    if(_num_of_args == 2) {
        // check arg
        string str_job_id = _args[1];
        if (!isNumber(str_job_id)) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        job_id_to_fg = stoi(str_job_id);
        if (job_id_to_fg <= 0 || !smash.getJobsList().getJobById(job_id_to_fg)) {
            cerr << "smash error: fg: job-id " << job_id_to_fg << " does not exist" << endl;
            return;
        }
    }
    else if(_num_of_args == 1){
        job_id_to_fg = smash.getJobsList().getLastJob();
    }
    JobEntry job = smash.getJobsList().getJobs()[job_id_to_fg];
    Command* cmd = job.getCommand();
    pid_t pid_to_fg = job.getCmdPid();
    cout << job.getCommand()->getCmdLine() << " : " << pid_to_fg << endl;

    // if(kill(pid_to_fg, SIGCONT) == -1) {
    //     perror("smash error: kill failed");
    //     return;
    // }
    smash.setFGCmd(job.getCommand());
    smash.setFGpid(pid_to_fg);
    smash.setFGJobID(job_id_to_fg);
    smash.getJobsList().removeJobById(job_id_to_fg);
    if(waitpid(pid_to_fg, nullptr, WUNTRACED) == -1) { /// todo - check about the WNOHANG option
        perror("smash error: waitpid failed");
        delete cmd;
        return;
    }
    smash.setFGCmd(nullptr);
    smash.setFGpid(-1); // NO ONE RUNNING IN FG
    smash.setFGJobID(-1);
    delete cmd;

}


void BackgroundCommand::execute() {
    if(_num_of_args > 2 ){
        cerr << "smash error: bg: invalid arguments" << endl;
        return ;
    }
    int job_id_to_running;
    if(_num_of_args == 2) {
        // check arg
        string str_job_id = _args[1];
        if (!isNumber(str_job_id)) {
            cerr << "smash error: bg: invalid arguments" << endl;
            return;
        }
        job_id_to_running = stoi(str_job_id);
        if (job_id_to_running <= 0 || !smash.getJobsList().getJobById(job_id_to_running)) {
            cerr << "smash error: bg: job-id " << job_id_to_running << " does not exist" << endl;
            return;
        }
        if(smash.getJobsList().getJobs()[job_id_to_running].getStatus() == runningBG){
            cerr << "smash error: bg: job-id " << job_id_to_running << " is already running in the background" << endl;
            return;
        }
    }
    else if(_num_of_args == 1){
        job_id_to_running = smash.getJobsList().getLastStoppedJob();
        if(job_id_to_running == 0){
            cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
            return;
        }
    }
    pid_t pid_to_running = smash.getJobsList().getJobs()[job_id_to_running].getCmdPid();
    cout << smash.getJobsList().getJobs()[job_id_to_running].getCmdLine() << " : " << pid_to_running << endl;
    if(kill(pid_to_running, SIGCONT) == -1) {
        perror("smash error: kill failed");
        return;
    }
    smash.getJobsList().getJobs()[job_id_to_running].setStatus(runningBG);
}

void QuitCommand::execute() {
    if (_num_of_args == 1 && (strcmp(_args[0], "kill") == 0)) {
        int size = smash.getJobsList().getJobs().size();
        cout << "smash: sending SIGKILL signal to " << size << " jobs:" << endl;
        smash.getJobsList().killAllJobs();
        cout << "Linux-shell:";
        // todo delete smash ?
    }
    exit(0);

}

/**********************************************************************************************************************/
/***************************************SPECIAL COMMANDS IMPLEMENTATION************************************************/
/*********************************************************************************************************************/

/*****************redirection*****************/

int findRedirectionCommand(const string& cmd_line){ /* return the index of the first > sign,
 will return str::npos if there isn't any > sign*/
    return cmd_line.find_first_of(">");
}
bool isAppendRedirection(const char* cmd_line){
    const string str(cmd_line);
    return (str.find_first_of(">>") < string::npos);
}


RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line){
    string cmd_s = _trim(string(cmd_line));
    int re_index = findRedirectionCommand(cmd_s);
    _cmd_to_exc = _trim(cmd_s.substr(0, re_index - 1));
    if(isAppendRedirection(cmd_s.c_str())){
            _file = _trim(cmd_s.substr(re_index + 2));
            _append = true;
    }
    else{ //_append = false by default
        _file = _trim(cmd_s.substr(re_index + 1));
    }
}

void RedirectionCommand::execute() { //todo try and catch
    Command *command = smash.CreateCommand(_cmd_to_exc.c_str());
    int old_std_out = dup(1);
    if (old_std_out == -1) {
        delete command;
        ErrorHandling("dup");
    }
    if (close(1) == -1) {
        delete command;
        ErrorHandling("close");
    }
    int fd_file;

    if (_append) {
        fd_file = open(_file.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
    } else {
        fd_file = open(_file.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    }
    if (fd_file == -1) {
        delete command;
        // return to old std out
        if (dup(old_std_out) == -1) ErrorHandling("dup");
        if (close(old_std_out) == -1) ErrorHandling("close");
        ErrorHandling("open");
    }
    command->execute();
    delete command;
    // return to old std out
    if (dup(old_std_out) == -1) ErrorHandling("dup");
    if (close(old_std_out) == -1) ErrorHandling("close");
}
//    if(dynamic_cast<ExternalCommand*>(command)){ // external command
//
//    }
//    else{ //build-in command
//
//
//    }

/************** pipe ********************/

int findPipeCommand(const string& cmd_line){ /* return the index of the first | sign,
will return str::npos if there isn't any | sign */
    return cmd_line.find_first_of("|");
}

bool isErrorPipe(const char* cmd_line){
    const string str(cmd_line);
    return (str.find_first_of("|&") < string::npos);
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    int pipe_index = findPipeCommand(cmd_s);
    _first_cmd = cmd_s.substr(0, pipe_index - 1);
    if(isErrorPipe(cmd_s.c_str())) {
        _is_error_pipe = true;
        _second_cmd = _trim(cmd_s.substr(pipe_index + 2));
    }
    else{
        _second_cmd = _trim(cmd_s.substr(pipe_index + 1));
    }
}

void PipeCommand::execute() {

}

void HeadCommand::execute() {
    if(_num_of_args == 1){
        cerr<<"smash error: head: not enough arguments"<<endl;
        return;
    }
    if(_num_of_args == 2){
        char buff[10];
        if(openat(3, _args[1], O_RDONLY) == -1) ErrorHandling("open");
        if(write(3, buff, 10)) ErrorHandling("write");
        if(read(1, buff, 10)) ErrorHandling("read");
    }
    if(_num_of_args == 3){
        int buffer_size = stoi(_args[1]);
        char buff[buffer_size];
        if(openat(3, _args[2], O_RDONLY) == -1) ErrorHandling("open");
        if(read(3, buff, buffer_size)) ErrorHandling("read");
        if(write(1, buff, buffer_size)) ErrorHandling("write");

    }
    else{
        cerr<<"smash error: head: too many arguments"<<endl;
        return;
    }
}





/**********************************************************************************************************************/
/*********************************************************************************************************************/

