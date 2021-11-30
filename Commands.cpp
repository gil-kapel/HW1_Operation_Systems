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
    if (idx == string::npos) {
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

void ErrorHandling(string syscall){ /* General error message and exit -1 for syscall faliure*/
    string error_msg = "smash error: " + syscall + " failed";
    perror(error_msg.c_str());
    exit(-1);
}

/*********************************************************************************************************************/
/***************************Pipe and Redirection classification functions*********************************************/

int findRedirectionCommand(const char* cmd_line){ /* return the index of the first > sign,
 will return str::npos if there isn't any > sign*/
    const string str(cmd_line);
    return str.find_first_of(">");
}

bool isAppendRedirection(const char* cmd_line){
    const string str(cmd_line);
    return (str.find_first_of(">>") < string::npos);
}

int findPipeCommand(const char* cmd_line){ /* return the index of the first | sign, 
will return str::npos if there isn't any | sign */
    const string str(cmd_line);
    return str.find_first_of("|");
}

bool isErrorPipe(const char* cmd_line){
    const string str(cmd_line);
    return (str.find_first_of("|&") < string::npos);
}

/**********************************************************************************************************************/
/*****************************************SmallShell IMPLEMENTATION****************************************************/
/**********************************************************************************************************************/

SmallShell::SmallShell(){
    _pid = getpid();
    _curr_path = getcwd(const_cast<char*>(_curr_path.c_str()), MAX_PATH);
    _last_path = _curr_path;
}

SmallShell::~SmallShell() {
    _jobs_list.killAllJobs();
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line); // external commands must fork
    if(!cmd){
        return;
    }
    if(_isBackgroundComamnd(cmd_line)){
        smash.getJobsList().addJob(cmd);
    }
    smash.getJobsList().removeFinishedJobs();
    cmd->execute();
    delete cmd;
}

Command * SmallShell::CreateCommand(const char* cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    char* args_array[COMMAND_MAX_ARGS];
    int arg_num = _parseCommandLine(cmd_line, args_array);
    string firstWord;
    char* tempWord;
    if( args_array[0] != NULL ) {
        firstWord = firstWord.substr(0, firstWord.find_first_of(" \n"));
        tempWord = const_cast<char*>(firstWord.c_str());
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
    _removeBackgroundSign(tempWord);
    firstWord = tempWord;

    /*Built in commands handle*/
    if (firstWord.compare("chprompt") == 0){
        return new ChPromptCommand(cmd_line);
    }

    else if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }

    else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    }

    else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line);
    }

    else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(cmd_line);
    }

    else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line);
    }

    else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line);
    }

    else if (firstWord.compare("bg") == 0) {
        return new BackgroundCommand(cmd_line);
    }
    
    if(firstWord == "quit") {
        return new QuitCommand(cmd_line);
    }

    /********special commands*******/

    /*Redirection and pipe commands handle*/

    unsigned re_index = findRedirectionCommand(cmd_s.c_str());
    if(re_index != string::npos && re_index > 0){
        string first_cmd = cmd_s.substr(0, re_index - 1);
        string file_path = cmd_s.substr(re_index + 1, string::npos);
        if(isAppendRedirection(cmd_s.c_str())){
            file_path = cmd_s.substr(re_index + 2);
            return new RedirectionCommand(cmd_line, file_path, first_cmd, true);
        }
        else return new RedirectionCommand(cmd_line, file_path, first_cmd, false);
    }
    unsigned pipe_index = findPipeCommand(cmd_s.c_str());
    if(pipe_index < string::npos && pipe_index > 0){
        string fisrt_cmd = cmd_s.substr(0, pipe_index - 1);
        string second_cmd = cmd_s.substr(pipe_index + 1);
        if(isErrorPipe(cmd_s.c_str())){
            second_cmd = cmd_s.substr(pipe_index + 2);
            return new PipeCommand(cmd_line, fisrt_cmd, second_cmd, true, smash);
        }
        else return new PipeCommand(cmd_line, fisrt_cmd, second_cmd, false, smash);
    }

    return new ExternalCommand(cmd_line);
}

/**********************************************************************************************************************/
/*****************************************Command IMPLEMENTATION****************************************************/
/**********************************************************************************************************************/

Command::Command(const char* cmd_line):_cmd_line(cmd_line){
    _pid = getpid();
}

bool Command::isNumber(const string &str){
    return str.find_first_not_of("0123456789") == string::npos;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    _num_of_args = _parseCommandLine(cmd_s.c_str(), _args);
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
    char* argv[] = {bash_cmd, flag, cmd_c};

    pid_t ext_pid = fork();
    if(ext_pid == -1) ErrorHandling("fork");
    if(ext_pid == 0){ //child process
        if(setpgrp() == -1) ErrorHandling("setpgrp");
        if(execv(bash_cmd, argv) == -1 ) ErrorHandling("execv");
    }
    // father process
    else if(_isBackgroundComamnd(cmd_s.c_str())) return;
    else {
        int ret = waitpid(ext_pid, nullptr, 0);
        if(ret == -1) ErrorHandling("waitpid");
    }
}

void PipeCommand::execute(){
    int fd[2];
    int ret = pipe(fd);
    if(ret == -1) ErrorHandling("pipe");
    int index = 1;
    if(is_std_error) index = 2;
    Command* command;
    int first_pid = fork();
    if(first_pid == -1) ErrorHandling("fork");
    if(first_pid == 0){
        if(setpgrp() == -1) ErrorHandling("setpgrp");
        if(dup2(fd[1], index) == -1) ErrorHandling("dup2");
        if(close(fd[0]) == -1) ErrorHandling("close");
        if(close(fd[1]) == -1) ErrorHandling("close");
        command = smash.CreateCommand(first_cmd.c_str());
    }
    else{
        int sec_pid = fork();
        if(sec_pid == -1) ErrorHandling("fork");
        if(sec_pid == 0){
            if(setpgrp() == -1) ErrorHandling("setpgrp");
            if(dup2(fd[0], 0) == -1) ErrorHandling("dup2");
            if(close(fd[0]) == -1) ErrorHandling("close");
            if(close(fd[1]) == -1) ErrorHandling("close");
            command = smash.CreateCommand(sec_cmd.c_str());
        }
    }
    if(first_pid == 0 || sec_pid == 0) {
        if(command == nullptr) exit(-1);
        command->execute();
        delete command;
        exit(0);
    }
    if(close(fd[0]) == -1) ErrorHandling("close");
    if(close(fd[1]) == -1) ErrorHandling("close");
    waitpid(first_pid, nullptr, 0);
    waitpid(sec_pid, nullptr, 0);
}

void RedirectionCommand::execute(){
    Command* command;
    int pid = fork();
    if(pid == -1) ErrorHandling("fork");
    if(pid == 0){
        if(setpgrp() == -1) ErrorHandling("setpgrp");
        int std = dup(1);
        if(std == -1) ErrorHandling("dup");
        if(close(1) == -1) ErrorHandling("close");
        if(isAppended){
            if(open(file_path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666) == -1) ErrorHandling("open");
        }
        else if(open(file_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666) == -1) ErrorHandling("open");
        command = smash.CreateCommand(this->s_cmd.c_str());
        command->execute();
        delete command;
        if (close(1) == -1) ErrorHandling("close");
        if (dup(std) == -1) ErrorHandling("dup");
        if (close(std) == -1) ErrorHandling("close");
        exit(0);
    }

    wait(nullptr);
}

/*********************************************************************************************************************/
/****************************************JOBS LIST CLASS IMPLEMENTATION***********************************************/
/********************************************************************************************************************/

void JobsList::addJob(Command *cmd, bool isStopped) {
    removeFinishedJobs();
    //todo if job exist
    status s = (isStopped) ? stopped : runningBG;
    int new_id_job = getMaxId();
    _jobs[new_id_job] = JobEntry(new_id_job, cmd, time(nullptr), s);
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for(const auto &iter : _jobs){
        cout << "[" << iter.first << "]" << " " << iter.second.getCmdLine() << " : " << iter.second.getCommand()->getPid()
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
        pid_t pid_to_kill = iter.second.getCommand()->getPid();
        if (kill(pid_to_kill, SIGKILL) == -1) {
            perror("smash error: kill failed");
        }
        cout << pid_to_kill << ": " << iter.second.getCmdLine() << endl;
    }
    _jobs.clear();
}

void JobsList::removeFinishedJobs() {
    pid_t pid_to_remove = 1;
    for(auto& iter : _jobs){
        pid_t to_find = iter.second.getCommand()->getPid();
        pid_t temp = waitpid(to_find, NULL, WNOHANG);
        if(temp == -1) {
            perror("smash error: waitpid failed");
        }
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
    if(this->getNumOfArgs() == 0){
        smash.setName("smash");
    }
    else{
        smash.setName(this->getArgs(0));
    }
}

void ShowPidCommand::execute() {
    cout << "smash pid is" << smash.getPid() << endl;
}

void GetCurrDirCommand::execute() { //todo why not to use in cuurent path of smash class
    char cwd[COMMAND_MAX_ARGS];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
        cout << string(cwd) << endl;
    }
    else{
        perror("smash error: getcwd failed");
    }
}

void ChangeDirCommand::execute() {
    if(_num_of_args > 1){
       cerr << "smash error: cd: too many arguments" << endl;
       return;
    }
    if(_args[0] == "-" && smash.getLastPath() == ""){
        cerr << "smash error: cd: OLDPWD not set" << endl;
        return;
    }
    if(chdir(*_args) == -1) {
        perror("smash error: chdir failed");
        return;
    }
    smash.setLastPath(smash.getCurrPath());
    smash.setCurrPath(string(*_args));
}

void JobsCommand::execute() {
    smash.getJobsList().printJobsList();
}

void KillCommand::execute() {
    if(_num_of_args != 2 ){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    // check arg 1
    string arg1 = _args[0];
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
    string arg2 = _args[1];
    if(!isNumber(arg2)){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    int job_id_get_sig = stoi(arg1);
    if(smash.getJobsList().getJobById(job_id_get_sig) == nullptr){
        cerr << "smash error: kill: job-id " << job_id_get_sig << " does not exist" << endl;
        return;
    }
    pid_t pid_get_sig = smash.getJobsList().getJobs()[job_id_get_sig].getCommand()->getPid();

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
    if(_num_of_args == 0 && smash.getJobsList().getJobs().empty()){
        cerr<<"smash error: fg: jobs list is empty"<<endl;
        return;
    }
    if(_num_of_args > 1 ){
        cerr << "smash error: fg: invalid arguments" << endl;
        return ;
    }
    int job_id_to_fg;
    if(_num_of_args == 1) {
        // check arg
        string str_job_id = _args[0];
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
    else if(_num_of_args == 0){
        job_id_to_fg = smash.getJobsList().getLastJob();
    }

    pid_t pid_to_fg = smash.getJobsList().getJobs()[job_id_to_fg].getCommand()->getPid();
    cout << smash.getJobsList().getJobs()[job_id_to_fg].getCmdLine() << " : " << pid_to_fg << endl;

    if(kill(pid_to_fg, SIGCONT) == -1) {
        perror("smash error: kill failed");
        return;
    }
    smash.getJobsList().removeJobById(job_id_to_fg);
    if(waitpid(pid_to_fg, NULL, WUNTRACED) == -1) { /// todo - check about the WNOHANG option
        perror("smash error: waitpid failed");
        return;
    }
}

void BackgroundCommand::execute() {
    if(_num_of_args > 1 ){
        cerr << "smash error: bg: invalid arguments" << endl;
        return ;
    }
    int job_id_to_running;
    if(_num_of_args == 1) {
        // check arg
        string str_job_id = _args[0];
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
    else if(_num_of_args == 0){
        job_id_to_running = smash.getJobsList().getLastStoppedJob();
        if(job_id_to_running == 0){
            cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
            return;
        }
    }
    pid_t pid_to_running = smash.getJobsList().getJobs()[job_id_to_running].getCommand()->getPid();
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
}
