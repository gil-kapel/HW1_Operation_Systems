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

void ShowPidCommand::execute() override{
    cout << "smash pid is " << /* getSmashPID() */ << endl;
}

void GetCurrDirCommand::execute() override{
    cout << getcwd() << endl;
 }

void ChangeDirCommand::execute() override{ //implementation//
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  char* dir[PATH_MAX];
  string curr_path = getcwd(dir, PATH_MAX);
  if(curr_path == nullptr);
  else if(firstWord.compare("-") == 0){
      if(last_cd_is_empty) cout << "smash error: cd: OLDPWD not set" << endl;
      chdir(plastPwd);
      return;
  }
  else if(chdir(cmd_s) == error_code_number) print error = perror();
}

void RedirectionCommand::execute() override{}

void QuitCommand::execute() override{}

void JobsCommand::execute() override{}

void KillCommand::execute() override{}

void JobsList::addJob(Command* cmd, bool isStopped = false){
    if(isStopped) job_list.insert(JobList::JobEntry(job_list.size() + 1, getPidByCmd(cmd), Stopped)); // need to implement the getPidByCmd Function!!
    else job_list.insert(JobList::JobEntry(job_list.size() + 1, getPidByCmd(cmd), Background));
}

auto printJob = [](const JobList::JobEntry& job){
  if(job.IsStoppedStatus()){
    cout << "";   //implementation
    }
  }

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
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  int num_args = ???; 


  if (firstWord.compare("chprompt") == 0){}

  else if (firstWord.compare("pwd") == 0){
    return new GetCurrDirCommand(cmd_line);
  }

  else if (firstWord.compare("showpid") == 0){
    return new ShowPidCommand(cmd_line);
  }

  else if (firstWord.compare("cd") == 0){
      if (num_args > 1){
        cout << "smash error: cd: too many arguments" << endl;
      }
      else return new ChangeDirCommand(cmd_line);
  }
  .....
  else if (firstWord.compare("jobs") == 0){}

  else if (firstWord.compare("kill") == 0){}

  else if (firstWord.compare("fg") == 0){}

  else if (firstWord.compare("bg") == 0){}

  else if (firstWord.compare("quit") == 0){}

  else {
    return new ExternalCommand(cmd_line);
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  // Command* cmd = CreateCommand(cmd_line);
  // cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}