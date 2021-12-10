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
    // unsigned int idx = str.find_last_of("&");
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

inline void ErrorHandling(string syscall, bool to_exit){ /* General error message and exit -1 for syscall faliure*/ 
    string error_msg = "smash error: " + syscall + " failed";
    perror(error_msg.c_str());
    if(to_exit) exit(-1);
    else return;
}

enum cmd_type{ builtIn, external, special, timedout};

/**********************************************************************************************************************/
/*****************************************SmallShell IMPLEMENTATION****************************************************/
/**********************************************************************************************************************/

SmallShell::SmallShell() {
    _pid = getpid();
    char dir[MAX_PATH];
    getcwd(dir,MAX_PATH);
    _curr_path = dir;
}

SmallShell::~SmallShell() = default;

void SmallShell::removeFinishedJobs() {
    if(smash.getJobsList().getJobs().empty()) return;
    for(auto &iter : smash.getJobsList().getJobs()){
        pid_t to_find = iter.second.getCmdPid();
        if(to_find == 0) break;
        int temp = waitpid(to_find, NULL, WNOHANG);
        if(temp == -1) return ErrorHandling("waitpid");
        else if(temp == to_find){
            smash.getTimedList().getTimeOutEntryByPid(temp).setIfFinish(true);
            smash.getJobsList().getJobs().erase(iter.first);
        }
    }

}

void SmallShell::executeCommand(const char *cmd_line) {
    smash.removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line); // external commands must fork
    if(cmd == nullptr) return;
    else if(_isBackgroundComamnd(cmd->getCmdLine().c_str())){
        cmd->execute();
        delete cmd;
    }
    else{
        smash.setFGCmd(cmd->getCmdLine());
        smash.setFGpid(getpid());
        // smash.setFGJobID(-1);
        cmd->execute();
        smash.setFGCmd("");
        smash.setFGpid(-1); // NO ONE RUNNING IN FG
        delete cmd;
    }
    if(isQuit) exit(0);
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

    else if(firstWord == "timeout") return new TimedOutCommand(cmd_line); 

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
    _isBackground = _isBackgroundComamnd(this->getCmdLine().c_str());
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
        if (execv(bash_cmd, argv) == -1) ErrorHandling("execv", true);
    } else { // parent process
        int duration = smash._time_out_duration;
        if (duration != -1) {
            smash.getTimedList().addToList(smash._time_out_cmd, ext_pid, duration);
        }
        int wstatus;
        if(_isBackground){
            if(duration != -1 ){
                smash.getJobsList().addJob(smash._time_out_cmd, ext_pid);
            }
            else{
                smash.getJobsList().addJob(this->getCmdLine(), ext_pid);
            }
        }
        else{
            smash.setFGCmd(cmd_s);
            smash.setFGpid(ext_pid);
            if (waitpid(ext_pid, &wstatus, WUNTRACED) == -1) return ErrorHandling("waitpid");
            if (WIFSTOPPED(wstatus)){
                smash.getJobsList().addJob(this->getCmdLine(), ext_pid, true);
                smash.setFGCmd("");
                smash.setFGpid(-1);
                this->setCmdStatus(true);
            }
            if( WIFSIGNALED(wstatus) || WIFEXITED(wstatus)){
                smash.getTimedList().getTimeOutEntryByPid(ext_pid).setIfFinish(true);
            }
        }
    }
}

/*********************************************************************************************************************/
/****************************************JOBS LIST CLASS IMPLEMENTATION***********************************************/
/********************************************************************************************************************/

void JobsList::addJob(string cmd_line, pid_t pid, bool isStopped) {
    smash.removeFinishedJobs();
    status s = (isStopped) ? stopped : runningBG;
    int job_id;
    JobEntry job_entry;
    if(smash.getFGJobID() != -1){
        job_id = smash.getFGJobID();
        job_entry  = JobEntry(job_id, cmd_line, pid, time(nullptr), s);
        smash.setFGJobID(-1);
        smash.setFGCmd("");
        smash.setFGpid(-1);
    }
    else{
        job_id = getMaxId();
        job_entry  = JobEntry(job_id, cmd_line, pid, time(nullptr), s);
    }
    _jobs.insert(pair<int, JobEntry>(job_id, job_entry));
}

void JobsList::printJobsList() {
    smash.removeFinishedJobs();
    for(const auto &iter : _jobs){
        cout << "[" << iter.first << "]" << " " << iter.second.getCmdLine() << " : " << iter.second.getCmdPid()
             << " " << iter.second.getSecondElapsed() << " secs";
        if(iter.second.getStatus() == stopped){
            cout << " (stopped)" << endl;
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
        }
        _jobs.clear();
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

void JobsList::removeJobByPid(int pid) {
    if(_jobs.empty()) return;
    int find_job_id_to_erase = 0;
    for(auto  &iter : _jobs){
        if(iter.second.getCmdPid() == pid){
            find_job_id_to_erase = iter.first;
        }
    }
    if(find_job_id_to_erase != 0) _jobs.erase(find_job_id_to_erase);
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
    if(!isNumber(arg1)){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    //check arg 2
    string arg2 = _args[2];
    if(arg2[0] == '-' && isNumber(arg2.substr(1))){
        int job_id_get_sig = stoi(arg2);
        cerr << "smash error: kill: job-id " << job_id_get_sig << " does not exist" << endl;
        return;
    }
    if(!isNumber(arg2)){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    int job_id_get_sig = stoi(arg2);
    if(smash.getJobsList().getJobById(job_id_get_sig) == nullptr){
        cerr << "smash error: kill: job-id " << job_id_get_sig << " does not exist" << endl;
        return;
    }
    JobEntry& job = smash.getJobsList().getJobs()[job_id_get_sig];
    pid_t pid_get_sig = job.getCmdPid();

    // functionality
    int signum = stoi(arg1);
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
            // job.setStatus(stopped);
            cout<< "signal number " << signum <<" was sent to pid "<< job.getCmdPid() << endl;
            return;
        }
        if(signum == SIGCONT){
            job.setStatus(runningBG);
        }
    }
    cout<< "signal number " << signum <<" was sent to pid "<< job.getCmdPid() << endl;
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
        string arg = _args[1];
        if(arg[0] == '-' && isNumber(arg.substr(1))){
            job_id_to_fg = stoi(arg);
            cerr << "smash error: fg: job-id " << job_id_to_fg << " does not exist" << endl;
            return;
        }
        if (!isNumber(arg)) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        job_id_to_fg = stoi(arg);
        if (job_id_to_fg <= 0 || !smash.getJobsList().getJobById(job_id_to_fg)) {
            cerr << "smash error: fg: job-id " << job_id_to_fg << " does not exist" << endl;
            return;
        }
    }
    else if(_num_of_args == 1){
        job_id_to_fg = smash.getJobsList().getLastJob();
    }
    JobEntry job = smash.getJobsList().getJobs()[job_id_to_fg];
    pid_t pid_to_fg = job.getCmdPid();
    cout << job.getCommand() << " : " << pid_to_fg << endl;

    if(kill(pid_to_fg, SIGCONT) == -1) return ErrorHandling("kill");

    smash.setFGCmd(job.getCommand());
    smash.setFGpid(pid_to_fg);
    smash.setFGJobID(job_id_to_fg);
    smash.getJobsList().removeJobById(job_id_to_fg);
    int wstatus;
    if(waitpid(pid_to_fg, &wstatus, WUNTRACED) == -1) { /// todo - check about the WNOHANG option
        perror("smash error: waitpid failed");
        return;
    }
    if (WIFSTOPPED(wstatus)){
        smash.getJobsList().addJob(job.getCommand(), pid_to_fg, true);
        smash.setFGCmd("");
        smash.setFGpid(-1);
        this->setCmdStatus(true);
    }
}


void BackgroundCommand::execute() {
    if(_num_of_args > 2 ){
        cerr << "smash error: bg: invalid arguments" << endl;
        return ;
    }
    int job_id_to_running;
    if(_num_of_args == 2) {
        // check arg
        string arg = _args[1];
        if(arg[0] == '-' && isNumber(arg.substr(1))){
            job_id_to_running = stoi(arg);
            cerr << "smash error: bg: job-id " << job_id_to_running << " does not exist" << endl;
            return;
        }
        if (!isNumber(arg)) {
            cerr << "smash error: bg: invalid arguments" << endl;
            return;
        }
        job_id_to_running = stoi(arg);
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
    else{
        smash.getJobsList().killAllJobs(false);
    }
    smash.isQuit = true;
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
    int index = findRedirectionCommand(str);
    return (str[index + 1] == '>');
}


RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line){
    string cmd_s = _trim(string(cmd_line));
    int re_index = findRedirectionCommand(cmd_s);
    _cmd_to_exc = _trim(cmd_s.substr(0, re_index));
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

/************** pipe ********************/

int findPipeCommand(const string& cmd_line){ /* return the index of the first | sign,
will return str::npos if there isn't any | sign */
    return cmd_line.find_first_of("|");
}

bool isErrorPipe(const char* cmd_line){
    const string str(cmd_line);
    int index = str.find_first_of("|&");
    return (str[index + 1] == '&');
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
    // remove background signs from both commands
    
    char tempWord[MAX_COMMAND_LENGTH];
    strcpy(tempWord, _first_cmd.c_str());
    _removeBackgroundSign(tempWord);
    _first_cmd = tempWord;

    strcpy(tempWord, _second_cmd.c_str());
    _removeBackgroundSign(tempWord);
    _second_cmd = tempWord;

}

void PipeCommand::execute() {
    int fd[2];
    if(pipe(fd) == -1) return ErrorHandling("pipe");

    // save parameter to restore
    int old_std_in = dup(0);
    int old_std_out = dup(1);
    int old_std_error = dup(2);
    int index;
    if(_is_std_error) index = 2;
    else index = 1;
    Command* first_command;
    Command* second_command;

    // left side
    pid_t first_pid = fork();
    if(first_pid == -1) return ErrorHandling("fork");
    if(first_pid == 0){ // first_son
        if(setpgrp() == -1) return ErrorHandling("setpgrp");
        if(close(fd[0]) == -1) return ErrorHandling("close");
        if(dup2(fd[1],index) == -1) return ErrorHandling("dup2");
        if(close(fd[1]) == -1) return ErrorHandling("close");
        first_command = smash.CreateCommand(_first_cmd.c_str());
        if(dynamic_cast<BuiltInCommand*>(first_command) != nullptr ) { //build in command
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
        if(dup2(old_std_out, 1) == -1) return ErrorHandling("dup2");
        if(dup2(old_std_error, 2) == -1) return ErrorHandling("dup2");
        exit(0);
    }
    pid_t sec_pid = fork();
    if(sec_pid == -1) return ErrorHandling("fork");
    if(sec_pid == 0){
        if(setpgrp() == -1) return ErrorHandling("setpgrp");
        if(close(fd[1]) == -1) return ErrorHandling("close"); // close the side of pipe that doesnt need
        if(dup2(fd[0], 0) == -1) return ErrorHandling("dup2");
        if(close(fd[0]) == -1) return ErrorHandling("close");
        second_command = smash.CreateCommand(_second_cmd.c_str());
        if(dynamic_cast<BuiltInCommand*>(second_command) != nullptr ) { //build in command
            second_command->execute();
        }
        else{ // external command
            char bash_cmd[] = {"/bin/bash"};
            char flag[] = {"-c"};
            char cmd_c[MAX_COMMAND_LENGTH];
            strcpy(cmd_c, _second_cmd.c_str());
            char *argv[] = {bash_cmd, flag, cmd_c, nullptr};
            if (execv(bash_cmd, argv) == -1) return ErrorHandling("execv");
        }
        delete second_command;
        if(dup2(old_std_in, 0) == -1) return ErrorHandling("dup2");
        exit(0);
    }

    if(close(fd[1]) == -1) return ErrorHandling("close");
    if(close(fd[0]) == -1) return ErrorHandling("close");
    if (waitpid(first_pid, nullptr, WUNTRACED) == -1)  return ErrorHandling("waitpid");
    if (waitpid(sec_pid, nullptr, WUNTRACED) == -1)  return ErrorHandling("waitpid");

}

HeadCommand::HeadCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    if(_num_of_args == 3){
        _lines = abs(stoi(_args[1]));
    }
}
void HeadCommand::execute() {
    // not check if have too many argument and if have 2 argument the second one must be name of file
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
        if(bytes == -1) return ErrorHandling("read");
        int i;
        for(i = 0 ; i < bytes ; ++i){
            if(buff[i] == '\n'){
                counter_lines++;
                if(counter_lines > _lines) break;
            }
        }
        if(bytes == i){
            if(write(1, buff, i) == -1) return ErrorHandling("write");
        }
        else if(write(1, buff, i+1) == -1) return ErrorHandling("write");
        delete[] buff;
        if(bytes < 100) break; // end of file or read the lines we needed
    }
}


void TimeOutList::addToList(string cmd, pid_t pid, int duration) {
    this->getList().emplace_back(cmd, pid, duration);
}


//cmd_type FindCmdType(const string& cmd_line){
//    string firstWord = cmd_line.substr(0,cmd_line.find_first_of(WHITESPACE));
//    unsigned re_index = findRedirectionCommand(cmd_line.c_str());
//    unsigned pipe_index = findPipeCommand(cmd_line.c_str());
//    if (firstWord.compare("chprompt") == 0 || firstWord.compare("pwd") == 0 || firstWord.compare("showpid") == 0 ||
//        firstWord.compare("cd") == 0 || firstWord.compare("jobs") == 0 || firstWord.compare("kill") == 0 ||
//        firstWord.compare("fg") == 0 || firstWord.compare("bg") == 0 || firstWord == "quit")  return builtIn;
//
//    else{
//        if(re_index <= MAX_COMMAND_LENGTH && re_index > 0) return special;
//        else if(pipe_index <= MAX_COMMAND_LENGTH && pipe_index > 0) return special;
//        else if(firstWord == "timeout") return timedout;
//    }
//    return external;
//}

void TimeOutList::removeTimeOutByPid(pid_t pid) {
    for (auto it = _time_out_list.begin() ; it != _time_out_list.end() ; ++ it) {
        if ((*it).getPid() == pid) {
            _time_out_list.erase(it);
            return;
        }
    }
}

int TimeOutList::getMinAlarm() {
    if (_time_out_list.empty()) return 0;
    int min = _time_out_list.front().getTimeUntilKill();
    for (auto &it : _time_out_list) {
        int temp = it.getTimeUntilKill();
        if (temp < min)
            min = temp;
    }
    return min;
}

TimeOutEntry& TimeOutList::getTimeOutEntryByPid(pid_t pid) {
    for (auto &it : _time_out_list) {
        if (it.getPid() == pid) {
            return it;
        }
    }
}

void TimedOutCommand::execute(){
    if(_num_of_args < 3 ){
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }
    // check arg 1
    string arg1 = _args[1];
    arg1 = arg1.substr(0, arg1.size() - 1);
    if(!isNumber(arg1)){
        cerr << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    int index = getCmdLine().find(_args[2]);
    string cmd = _trim(this->getCmdLine().substr(index));
    int duration = stoi(_args[1]);
    int curr_min_alarm = smash.getTimedList().getMinAlarm();
    if(duration < curr_min_alarm || curr_min_alarm == 0){
        alarm(duration);
    }
    Command* command = smash.CreateCommand(cmd.c_str());
    smash._time_out_duration = duration;
    smash._time_out_cmd = _cmd_line;
    command->execute();
    delete command;
    smash._time_out_duration = -1;
    smash._time_out_cmd = "";

//    smash.removeFinishedJobs();
//    if(FindCmdType(_timed_cmd) != external){
//        smash.executeCommand(_timed_cmd.c_str());
//        return;
//    }
//    char cmd_line[MAX_COMMAND_LENGTH];
//    strcpy(cmd_line, _timed_cmd.c_str());
//    _removeBackgroundSign(cmd_line);
//    ExternalCommand* timed_cmd = new ExternalCommand(cmd_line);
//
//    int child_pid = fork();
//    if(child_pid == -1) return ErrorHandling("fork");
//    else if(child_pid == 0){
//        if(setpgrp() == -1) return ErrorHandling("setpgrp", true);
//        int grand_pid = fork();
//        if(grand_pid == -1) return ErrorHandling("fork");
//        else if(grand_pid == 0){
//            timed_cmd->execute();
//            delete timed_cmd;
//            exit(0);
//        }
//        while(isTimeOut(start_time, time(0)) == false && !wait(nullptr));
//        if(isTimeOut(_start_time, time(0)) == true){
//            if(kill(grand_pid, SIGALRM) == -1) return ErrorHandling("kill");
//        }
//        exit(0);
//    }
//    else{
//        if(_isBackgroundComamnd(_timed_cmd.c_str())) smash.getJobsList().addJob(timed_cmd, child_pid);
//        else{
//            int wstatus;
//            smash.setFGCmd(timed_cmd);
//            smash.setFGpid(child_pid);
//            if (waitpid(child_pid, &wstatus, WUNTRACED) == -1) return ErrorHandling("waitpid");
//            else if (WIFSTOPPED(wstatus)){
//                smash.getJobsList().addJob(timed_cmd, child_pid, true);
//                smash.setFGCmd(nullptr);
//                smash.setFGpid(-1);
//                timed_cmd->setCmdStatus(true);
//            }
//        }
//    }
}



/**********************************************************************************************************************/
/*********************************************************************************************************************/

