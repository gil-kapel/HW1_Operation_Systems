#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#define PATH_MAX 20
#endif

string _ltrim(const std::string& s) // returns the left part of the string (after the first whitespace)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s) // returns the right part of the string (before the first whitespace)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s) // returns the last readable string that was written
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

// TODO: Add your implementation for classes in Commands.h 

void ShowPidCommand::execute() {
    cout << "smash pid is " << this->getPid(); << endl;
}

void GetCurrDirCommand::execute() {
    char dir_path[MAX_PATH];
    cout << getcwd(dir_path, MAX_PATH); << endl;
 }

void ChangeDirCommand::execute() { 
  string cmd_s = _trim(string(this->_cmd_line));
  string path = cmd_s.substr(1, cmd_s.find_last_of(WHITESPACE));
  string path_arr[PATH_MAX] ={nullptr};
  stringstream ssin(path);
  int args_count = _parseCommandLine(path, path_arr);
  string curr_path = getcwd(curr_path, PATH_MAX);

  if(path.compare("-") == 0){
      if(plastPwd == nullptr) cout << "smash error: cd: OLDPWD not set" << endl;
      else chdir(this->plastPwd);
      return;
  }
  else if(args_count > 1) cout << "smash error: cd: too many arguments" << endl; //won't happen
  else if(chdir(path_arr[0]) == -1){
    string error_msg = "smash error: " + "not finished - need to add the syscall type " + "failed";
    perror(error_msg);
  }
}

void ExternalCommand::execute(){
  char* argv[] = {"/bin/bash", "-c", this->clean_cmd.c_str(), nullptr};
  int res = execv("/bin/bash", argv);
  if(res == -1){
      perror("smash error: execv failed");
      exit(-1);
  }
  exit(0);
}

void RedirectionCommand::execute() {}

void QuitCommand::execute() {}

void JobsCommand::execute() {}

void KillCommand::execute() {}



void JobsList::addJob(Command* cmd, bool isStopped = false){
    if(isStopped) job_list.insert(JobList::JobEntry(job_list.size() + 1, getPidByCmd(cmd), Stopped)); // need to implement the getPidByCmd Function!!
    else job_list.insert(JobList::JobEntry(job_list.size() + 1, getPidByCmd(cmd), Background));
}

auto printJob = [](const JobList::JobEntry& job){
  if(job.IsStoppedStatus()){
    cout << "";   //implementation
    }
  };

void JobsList::printJobsList(){
    std::for_each(job_list.begin(), job_list.end(), printJob);
}

auto killJob = [](const JobList::JobEntry& job){
  //implementation
}

void JobsList::killAllJobs(){
    std::for_each(job_list.begin(), job_list.end(), killJob);
}

auto removejob = [](const JobList::JobEntry& job){
  //implementation
}

void JobsList::removeFinishedJobs(){
    std::for_each(job_list.begin(), job_list.end(), removejob);
}

JobEntry* JobsList::getJobById(int jobId){
  if(job_list.size() > jobId && job_list[jobId-1].getJobId() > 0) return &job_list[jobId - 1];
  else return nullptr;
}

void JobsList::removeJobById(int jobId){
  if(job_list[jobId - 1].getJobId() < 0) return;
  else job_list.erase(job_list[jobId - 1]);
}

JobEntry* JobsList::getLastJob(int* lastJobId){
  return job_list.pop_back();
}

JobEntry* JobsList::getLastStoppedJob(int *jobId){
  do{
    JobList::JobEntry iter = job_list.pop_back();
  }
  while(iter.IsStoppedStatus() == false || iter == job_list.begin());
  if(iter.IsStoppedStatus()) return &iter;
  else return nullptr;
}


SmallShell::SmallShell() {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {

  string cmd_s = _trim(string(cmd_line));
  string cmd_array[COMMAND_MAX_ARGS] = {nullptr};
  int args_num = _parseCommandLine(cmd_s, cmd_array);
  if (cmd_array[0].compare("chprompt") == 0){
    if(args_num == 1){
      this->changePrompt("smash> ");
    }
    else this->changePrompt(cmd_s.find_last_not_of(WHITESPACE, 1) + "> ");
  }

  else if (cmd_array[0].compare("pwd") == 0){
    return new GetCurrDirCommand(cmd_line);
  }

  else if (cmd_array[0].compare("showpid") == 0){
    return new ShowPidCommand(cmd_line);
  }

  else if (cmd_array[0].compare("cd") == 0){
      if (args_num > 1){
        cout << "smash error: cd: too many arguments" << endl;
      }
      else return new ChangeDirCommand(cmd_line);
  }
  
  else if (cmd_array[0].compare("jobs") == 0){}

  else if (cmd_array[0].compare("kill") == 0){}

  else if (cmd_array[0].compare("fg") == 0){}

  else if (cmd_array[0].compare("bg") == 0){}

  else if (cmd_array[0].compare("quit") == 0){}

  else {
      return new ExternalCommand(cmd_line, cmd_s.c_str());
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  Command* cmd = CreateCommand(cmd_line);
  cmd->execute();

  // Please note that you must fork smash process for some commands (e.g., external commands....)
}