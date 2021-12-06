#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include "Commands.h"

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

SmallShell& smash = SmallShell::getInstance();

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
    unsigned int idx = str.find_last_of("&");
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

inline void ErrorHandling(string syscall, bool to_exit){ /* General error message and exit -1 for syscall faliure*/ 
    string error_msg = "smash error: " + syscall + " failed";
    perror(error_msg.c_str());
    if(to_exit) exit(-1);
    else return;
}

enum cmd_type{ builtIn, external, special, timedout};
cmd_type FindCmdType(Command* cmd);

/**********************************************************************************************************************/
/*****************************************SmallShell IMPLEMENTATION****************************************************/
/**********************************************************************************************************************/

SmallShell::SmallShell() {
    _pid = getpid();
    char dir[MAX_PATH];
    getcwd(dir,MAX_PATH);
    _curr_path = dir;
}

SmallShell::~SmallShell() {
    bool to_print = false;
    _jobs_list.killAllJobs(to_print);
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line); // external commands must fork
    if(cmd == nullptr) return;
    smash.getJobsList().removeFinishedJobs();
    if(FindCmdType(cmd) == timedout){
        cmd->execute();
        delete cmd;
    }
    else if(_isBackgroundComamnd(cmd->getCmdLine().c_str())){
        cmd->execute();
    }
    else{
        smash.setFGCmd(cmd);
        smash.setFGpid(getpid());
        // smash.setFGJobID(-1);
        cmd->execute();
        if(cmd->isCmsStopped()) return;
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
        firstWord = _trim(firstWord);
    }
    else {
        for(int i = 0; i < arg_num; i++) free(args_array[i]);
        return nullptr;
    }
    // free
    for(int i = 0; i < arg_num; i++) free(args_array[i]);

    /**********SPECIAL COMMAND*********/
    unsigned re_index = findRedirectionCommand(cmd_s.c_str());
    unsigned pipe_index = findPipeCommand(cmd_s.c_str());

    if(re_index <= MAX_COMMAND_LENGTH && re_index > 0) return new RedirectionCommand(cmd_line);

    else if(pipe_index <= MAX_COMMAND_LENGTH && pipe_index > 0) return new PipeCommand(cmd_line);


    /*Built in commands handle*/
    if (firstWord.compare("chprompt") == 0) return new ChPromptCommand(cmd_line);

    else if (firstWord.compare("pwd") == 0) return new GetCurrDirCommand(cmd_line);

    else if (firstWord.compare("showpid") == 0) return new ShowPidCommand(cmd_line);

    else if (firstWord.compare("cd") == 0) return new ChangeDirCommand(cmd_line);

    else if (firstWord.compare("jobs") == 0) return new JobsCommand(cmd_line);

    else if (firstWord.compare("kill") == 0) return new KillCommand(cmd_line);

    else if (firstWord.compare("fg") == 0) return new ForegroundCommand(cmd_line);

    else if (firstWord.compare("bg") == 0) return new BackgroundCommand(cmd_line);
    
    else if(firstWord == "quit") return new QuitCommand(cmd_line);

    else if (firstWord.compare("head") == 0) return new HeadCommand(cmd_line);

    else if(firstWord == "timeout") return new TimedOutCommand(cmd_line, time(0)); /*Error handling?*/

    else return new ExternalCommand(cmd_line);
}

/**********************************************************************************************************************/
/*****************************************Command IMPLEMENTATION****************************************************/
/*********************************************************************************************************************/

Command::Command(const char* cmd_line):_cmd_line(cmd_line){}

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
    if (ext_pid == -1) return ErrorHandling("fork");
    if (ext_pid == 0) { //child process
        setpgrp();
        _removeBackgroundSign(argv[2]);
        if (execv(bash_cmd, argv) == -1){
            ErrorHandling("execv", true);
        }
    } else { // parent process
        int wstatus;
        if(_isBackgroundComamnd(cmd_c)) smash.getJobsList().addJob(this, ext_pid);
        else{
            smash.setFGCmd(this);
            smash.setFGpid(ext_pid);
            if (waitpid(ext_pid, &wstatus, WUNTRACED) == -1) return ErrorHandling("waitpid");
            else if (WIFSTOPPED(wstatus)){
                smash.getJobsList().addJob(this, ext_pid, true);
                smash.setFGCmd(nullptr);
                smash.setFGpid(-1);
                this->setCmdStatus(true);
            }
        }
    }
}

/*********************************************************************************************************************/
/****************************************JOBS LIST CLASS IMPLEMENTATION***********************************************/
/********************************************************************************************************************/

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    status s = (isStopped) ? stopped : runningBG;
    if(smash.getFGJobID() != -1){
        int old_job_id = smash.getFGJobID();
        _jobs[old_job_id] = JobEntry(old_job_id, cmd, pid, time(nullptr), s);
        smash.setFGJobID(-1);
        smash.setFGCmd(nullptr);
        smash.setFGpid(-1);
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
void JobsList::killAllJobs(bool to_print) {
    if(!_jobs.empty()){
        for(auto &iter : _jobs){
            if(iter.first == 0) break;
            pid_t pid_to_kill = iter.second.getCmdPid();
            if (kill(pid_to_kill, SIGKILL) == -1) return ErrorHandling("kill");
            if(to_print) cout << pid_to_kill << ": " << iter.second.getCmdLine() << endl;
            delete iter.second.getCommand();
        }
        _jobs.clear();
        _max_id_job = 0;
    }
}

void JobsList::removeFinishedJobs() {
    for(auto& iter : _jobs){
        if(iter.first == 0) break;
        pid_t to_find = iter.second.getCmdPid();
        pid_t temp = waitpid(to_find, nullptr, WNOHANG);
        if(temp == -1) return ErrorHandling("waitpid");
        if(temp == to_find){
            _jobs.erase(iter.first);
        }
    }
}
int JobsList::getLastJob() {
    if(_jobs.empty()) return 0;
    int last_id_job = 0;
    for(auto &iter : _jobs){
        if(iter.first == 0) break;
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
        if(iter.first == 0) break;
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
    cout << "smash pid is " << smash.getPid() << endl;
}

void GetCurrDirCommand::execute() { //todo why not to use in cuurent path of smash class
    char cwd[MAX_PATH];
    if(getcwd(cwd, sizeof(cwd)) == nullptr) return ErrorHandling("getcwd");
    cout << string(cwd) << endl;
}

void ChangeDirCommand::execute() {
    if(_num_of_args > 2){
       cerr << "smash error: cd: too many arguments" << endl;
       return;
    }
    if(_num_of_args == 1) return;
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
            if(chdir(tmp.c_str()) == -1) return ErrorHandling("chdir");
            return;
        }
    }
    if(path.compare("..") == 0){
        string currPath = smash.getCurrPath();
        smash.setLastPath(currPath);
        if(currPath.compare("/home") != 0) smash.setCurrPath(currPath.substr(0, currPath.find_last_of("/")));
        if(chdir(smash.getCurrPath().c_str()) == -1) return ErrorHandling("chdir");
        return;
    }
    else if(chdir(_args[1]) == -1) return ErrorHandling("chdir");
    string currPath = smash.getCurrPath();
    if(path.find("home") == string::npos){
        path = currPath + "/" + path;
    }
    smash.setLastPath(smash.getCurrPath());
    smash.setCurrPath(path);
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

    if(kill(pid_to_fg, SIGCONT) == -1) return ErrorHandling("kill");

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
    if (_num_of_args == 2 && (strcmp(_args[1], "kill") == 0)) {
        int size = smash.getJobsList().getJobs().size();
        cout << "smash: sending SIGKILL signal to " << size << " jobs:" << endl;
        smash.getJobsList().killAllJobs();
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
    return (str.find_first_of(">>") != string::npos);
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
        return ErrorHandling("dup");
    }
    if (close(1) == -1) {
        delete command;
        return ErrorHandling("close");
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
        if (dup(old_std_out) == -1) return ErrorHandling("dup");
        if (close(old_std_out) == -1) return ErrorHandling("close");
        return ErrorHandling("open");
    }
    command->execute();
    delete command;
    // return to old std out
    if (close(fd_file) == -1) return ErrorHandling("close");
    if (dup(old_std_out) == -1) return ErrorHandling("dup");
    if (close(old_std_out) == -1) return ErrorHandling("close");

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
    return (str.find_first_of("|&") > MAX_COMMAND_LENGTH);
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    int pipe_index = findPipeCommand(cmd_s);
    _first_cmd = cmd_s.substr(0, pipe_index - 1);
    if(isErrorPipe(cmd_s.c_str())) {
        _is_std_error = true;
        _second_cmd = _trim(cmd_s.substr(pipe_index + 2));
    }
    else{
        _second_cmd = _trim(cmd_s.substr(pipe_index + 1));
    }
    
    // _removeBackgroundSign(_first_cmd);
    // _removeBackgroundSign(_second_cmd);

}

void PipeCommand::execute() {
//    Command* first_command = smash.CreateCommand(_first_cmd.c_str());
//
//    Command* second = smash.CreateCommand(_second_cmd.c_str());
    int fd[2];
    if(pipe(fd) == -1) return ErrorHandling("pipe");

    // save parameter to restore
    int old_std_in = dup(0);
    int old_std_out = dup(1);
    int old_std_error = dup(2);

    pid_t pid = fork();
    if(pid == -1) return ErrorHandling("fork");
    Command* first_command;
    Command* second_command;
    int index;
    if(_is_std_error) index = 2;
    else index = 1;

    if(pid == 0){ //son
        if(setpgrp() == -1) return ErrorHandling("setpgrp");
        if(dup2(fd[1],index) == -1) return ErrorHandling("dup2");
        if(close(fd[0]) == -1) return ErrorHandling("close");
        if(close(fd[1]) == -1) return ErrorHandling("close");
        first_command = smash.CreateCommand(_first_cmd.c_str());
        if(dynamic_cast<BuiltInCommand*>(first_command) != nullptr && 
           _first_cmd.substr(0, _first_cmd.find_first_of(WHITESPACE)).compare("head") != 0){ //build in command
            first_command->execute();
        }
        else{ // external command
            char bash_cmd[] = {"/bin/bash"};
            char flag[] = {"-c"};
            char cmd_c[MAX_COMMAND_LENGTH];
            strcpy(cmd_c, _first_cmd.c_str());
            char *argv[] = {bash_cmd, flag, cmd_c, nullptr};
            if (execv(bash_cmd, argv) == -1) return ErrorHandling("execv");
        }
        delete first_command;
        exit(0);
    }
    else{ // father
        if (waitpid(pid, nullptr, WUNTRACED) == -1)  return ErrorHandling("waitpid");
        if(dup2(fd[0], 0) == -1) return ErrorHandling("dup2");
        if(close(fd[0]) == -1) return ErrorHandling("close");
        if(close(fd[1]) == -1) return ErrorHandling("close");
        second_command = smash.CreateCommand(_second_cmd.c_str());
        second_command->execute();
        // char bash_cmd[] = {"/bin/bash"};
        // char flag[] = {"-c"};
        // char cmd_c[MAX_COMMAND_LENGTH];
        // strcpy(cmd_c, _second_cmd.c_str());
        // char *argv[] = {bash_cmd, flag, cmd_c, nullptr};
        // if (execv(bash_cmd, argv) == -1) return ErrorHandling("execv");
        delete second_command;
    }

    // restore STDIN, STDOUT, STDERROR
    if(dup2(old_std_in, 0) == -1) return ErrorHandling("dup2");
    if(dup2(old_std_out, 1) == -1) return ErrorHandling("dup2");
    if(dup2(old_std_error, 2) == -1) return ErrorHandling("dup2");
}

HeadCommand::HeadCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    if(_num_of_args == 3 && !isNumber(_args[1])) _lines = -1;
    else if(_num_of_args == 3){
        _lines = abs(stoi(_args[1]));
    }
}
void HeadCommand::execute() {
    if(_lines == -1) return;
    if(_num_of_args == 1){
        cerr<<"smash error: head: not enough arguments"<<endl;
        return;
    }
    int file_index = 2;
    if(_num_of_args == 2) file_index = 1;
    int fd_open = open(_args[file_index], O_RDONLY);
    if(fd_open == -1) return ErrorHandling("open");
    int counter_lines = 1;
    while(counter_lines <= _lines){
        char* buff = new char [100];
        int bytes = read(fd_open, buff, 100);
        if(bytes == -1 ) return ErrorHandling("read");
        int i;
        for(i = 0 ; i < bytes ; ++i){
            if(buff[i] == '\n'){
                counter_lines++;
                if(counter_lines > _lines) break;
            }
        }
        if(write(1, buff, i+1) == -1) return ErrorHandling("write");
        delete[] buff;
        if(bytes < 100 || i < 100) break; // end of file or read the lines we needed
    }
}


TimedOutCommand::TimedOutCommand(const char* cmd_line, time_t start): Command(cmd_line), start_time(start){
    string cmd_s = _trim(string(cmd_line));
    char* args_array[MAX_COMMAND_LENGTH];
    int arg_num = _parseCommandLine(cmd_line, args_array);
    /*-------------Errors handling??--------------------*/
    _timed_cmd = cmd_s.substr(2,arg_num).c_str();
    duration = stoi(args_array[1]);
    for(int i = 0; i < arg_num; i++) free(args_array[i]);
}


cmd_type FindCmdType(Command* cmd){
    string firstWord = cmd->getCmdLine().substr(0,1);
    unsigned re_index = findRedirectionCommand(cmd->getCmdLine().c_str());
    unsigned pipe_index = findPipeCommand(cmd->getCmdLine().c_str());
    if (firstWord.compare("chprompt") == 0 || firstWord.compare("pwd") == 0 || firstWord.compare("showpid") == 0 ||
        firstWord.compare("cd") == 0 || firstWord.compare("jobs") == 0 || firstWord.compare("kill") == 0 ||
        firstWord.compare("fg") == 0 || firstWord.compare("bg") == 0 || firstWord == "quit")  return builtIn;
    
    else{
        if(re_index <= MAX_COMMAND_LENGTH && re_index > 0) return special;
        else if(pipe_index <= MAX_COMMAND_LENGTH && pipe_index > 0) return special;
        else if(firstWord == "timeout") return timedout;
    }
    return external;
}


void TimedOutCommand::execute(){
    smash.getTimedJobsList().removeFinishedJobs();
    int child_pid = fork();
    if(child_pid == -1) return ErrorHandling("fork");
    else if(child_pid == 0){
        if(setpgrp() == -1) return ErrorHandling("setpgrp");
        smash.executeCommand(_timed_cmd.c_str());
    }
    else{
        smash.getTimedJobsList().addJob(this, child_pid);
        while(isTimeOut(start_time, time(0)) == false);
        if(kill(child_pid, SIGALRM) == -1) return ErrorHandling("kill");        
    }
}



/**********************************************************************************************************************/
/*********************************************************************************************************************/

