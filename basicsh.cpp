// OS LAB ASSIGNMENT 2, GROUP 6
// basicsh - A UNIX-based C++ shell
// Gaurav Madkaikar (19CS30018)
// Girish Kumar (19CS30019)

// Header Files
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <dirent.h>
#include <termios.h>
#include <chrono>
#include <ctime>

using namespace std;

#define PATH_LEN 200
#define MAXLEN 10
#define SUCC_CODE 1
#define FAIL_CODE 0

// Global variable declarations
bool freeParent = false;
int loc = 0;
pid_t curr_process = -1;
int ctrlPrompt = 0, ctrlNewline = 0, bg = 0;
string input;
deque<string> historyContainer;

// Function prototype declarations
void enableSettings();
void disableSettings();
bool commandPrompt(int);
int executeInterface(vector<string> &);
int execute_cd(vector<string> &);
int execute_exit(vector<string> &);
int execute_help(vector<string> &);
int execute_history(vector<string> &);
int execute_multiWatch(string &);
int executeMain(vector<string> &, int, int);
void cmd_split(vector<string> &, string &);

// built-in commands for parent process
vector<string> inBuiltCmds{"cd", "help", "exit", "history"};

// functions to execute for built-in commands
int (*builtInFuncs[])(vector<string> &){
    &execute_cd,
    &execute_help,
    &execute_exit,
    &execute_history};
#define MULTIWATCH 100

// ----------------------------- SIGNAL HANDLERS -----------------------------
// CTRL+C
void sigintHandler(int sig_num)
{
    disableSettings();

    kill(curr_process, SIGKILL);
    signal(SIGINT, sigintHandler);

    cout << "\n";
    enableSettings();
    input.clear();
    return;
}
// CRTL+Z
void sigstpHandler(int sig_num)
{
    kill(curr_process, SIGTSTP);
    freeParent = true;
    
    disableSettings();
    input.clear();
    cout << "\n";

    signal(SIGTSTP, sigstpHandler);
    return;
}

// ----------------------------- IN-BUILT COMMANDS -----------------------------
// Multiwatch command
int execute_multiWatch(string &command)
{
    vector<int> file_descriptors;
    vector<string> indCmds;
    int pos1 = command.find('['), pos2 = command.find(']'), pos3 = command.find(' ');
    if ((pos1 == -1) || (pos2 == -1) || (pos3 == -1))
    {
        cout << "\nERROR: Invalid syntax\n";
        return -1;
    }
    cout << "\n";
    string mainCmd = command.substr(0, pos3), temp;
    string tmp;
    int numCmds;

    for (int i = pos1 + 1; i < pos2; i++)
    {
        if (command[i] == ',')
        {
            indCmds.push_back(tmp);
            tmp.clear();
            continue;
        }
        tmp.push_back(command[i]);
    }
    indCmds.push_back(tmp);
    numCmds = indCmds.size();
    file_descriptors.resize(numCmds);

    // Clear the buffer
    fflush(stdout);

    // Execute parallely
    pid_t pid, waiting_pid;
    int status = 0;
    for (int i = 0; i < numCmds; i++)
    {
        string process_id = to_string(getpid());
        string filename = ".temp." + process_id + ".txt";

        auto end_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
        cout << "<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n";
    
        vector<string> args;
        cmd_split(args, indCmds[i]);
        pid = fork();
        // Execute child
        if (pid == 0)
        {
            // Create file
            string process_id = to_string(getpid());
            string filename = ".temp." + process_id + ".txt";
            file_descriptors[i] = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
            string buffer = to_string((i + 1)) + ". " + indCmds[i] + "      Timestamp: " + string(ctime(&end_time)) + "\n";
            cout << buffer << "\n";
            char *wrbuffer = strdup("<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n");
            int bytes = write(file_descriptors[i], buffer.c_str(), buffer.size());
            bytes = write(file_descriptors[i], wrbuffer, strlen(wrbuffer));

            // execute command
            executeMain(args, 1, file_descriptors[i]);
        }
        else if (pid > 0)
        {
            // Signal Handlers
            signal(SIGINT, sigintHandler);
            signal(SIGTSTP, SIG_DFL);
            do
            {
                waiting_pid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
        else
        {
            cout << "\x1b[31mERROR: fork() call failed!\x1b[0m\n";
            exit(-1);
        }
        char *wrbuffer = strdup("->->->->->->->->->->->->->->->->->->->\n");
        int bytes = write(file_descriptors[i], wrbuffer, 40);

        close(file_descriptors[i]);
    }
    return 1;
}

// Signal handlers for catching ^C and ^Z inside main()
void sh1(int sig_num)
{
    // CTRL+C
    signal(SIGINT, sh1);
    disableSettings();
    cout << "\n";

    bool prompt = commandPrompt(2);
    ctrlPrompt = 1;

    input.clear();
    return;
}

void sh2(int sig_num)
{
    // CTRL+Z
    signal(SIGTSTP, sh2);
    disableSettings();
    cout << "\n";

    bool prompt = commandPrompt(2);
    ctrlPrompt = 1;

    enableSettings();
    return;
}
// ---------------------- END ----------------------

// Utility Functions
string get_curr_path()
{
    string resPath;
    char *check, str[PATH_LEN];
    check = getcwd(str, sizeof(str));
    if (check == NULL)
    {
        cout << "\x1b[31mERROR: Cannot get current working directory!\x1b[0m\n";
        return resPath = "";
    }
    resPath = string(str);
    return resPath;
}

int find_occurrence(const string &str, int n = 3)
{
    size_t pos = 0;
    int cnt = 1;
    while (cnt != n)
    {
        pos++;
        pos = str.find('/', pos);
        if (pos == string::npos)
            return -1;
        cnt++;
    }
    return pos;
}

string trim(const string &str, const string &ws = " \t")
{
    const auto start = str.find_first_not_of(ws);
    if (start == string::npos)
        return "";

    const auto end = str.find_last_not_of(ws);
    const auto reqLength = end - start + 1;
    return str.substr(start, reqLength);
}

void line_split(string &line, vector<string> &cmdList, char delim = '|')
{
    int len = line.size();
    int single_app_st, single_app_end, double_app_st, double_app_end;
    int start = 0;
    int end = line.find(delim);
    string cmd, trimCmd;
    while (end != -1)
    {
        // Ignore spaces in strings enclosed within ' '
        single_app_st = line.find('\'', start);
        if (single_app_st != -1)
        {
            single_app_end = line.find('\'', single_app_st + 1);
            if ((single_app_end != -1) && (end >= single_app_st) && (end <= single_app_end))
            {
                end = line.find(delim, end + 1);
                continue;
            }
        }
        // Ignore spaces in strings enclosed within " "
        double_app_st = line.find('\'', start);
        if (double_app_st != -1)
        {
            double_app_end = line.find('\'', double_app_st + 1);
            if ((double_app_end != -1) && (end >= double_app_st) && (end <= double_app_end))
            {
                end = line.find(delim, end + 1);
                continue;
            }
        }
        // Ignore explicitly mentioned spaces
        if (line[end - 1] == '\\')
        {
            end = line.find(delim, end + 1);
            continue;
        }
        cmd = line.substr(start, end - start);
        trimCmd = trim(cmd);
        if (trimCmd.size() > 0)
            cmdList.push_back(trimCmd);
        start = end + 1;
        end = line.find(delim, start);
    }
    cmd = line.substr(start, end - start);
    trimCmd = trim(cmd);
    if (trimCmd.size() > 0)
        cmdList.push_back(trimCmd);

    return;
}

int LCSubstr(string &str1, string &str2)
{
    int N = str1.size(), M = str2.size(), res = 0;
    int LCSuff[N + 1][M + 1];

    for (int i = 0; i <= N; i++)
    {
        for (int j = 0; j <= M; j++)
        {
            if (i == 0 || j == 0)
            {
                LCSuff[i][j] = 0;
            }

            else if (str1[i - 1] == str2[j - 1])
            {
                LCSuff[i][j] = LCSuff[i - 1][j - 1] + 1;
                res = max(res, LCSuff[i][j]);
            }
            else
                LCSuff[i][j] = 0;
        }
    }
    return res;
}

void cmd_split(vector<string> &tokens, string &cmd)
{
    int numFields = 0;
    int len = cmd.size(), numTokens;

    cmd = trim(cmd);
    line_split(cmd, tokens, ' ');
    numTokens = tokens.size();

    return;
}

int execSingleCommand(vector<string> &cmdList)
{
    // Single commands need no piping
    vector<string> tokenList;
    cmd_split(tokenList, cmdList[0]);

    // trim sides of each input string
    for (auto &str : tokenList)
        str = trim(str, "\'\"");

    // string is non-empty
    if ((int)tokenList.size() > 0)
    {
        int ret_code = executeInterface(tokenList);
        if (ret_code == -1)
        {
            return -1;
        }
    }
    return 0;
}

int execPipedCommands(vector<string> &cmdList)
{
    int numCmd = (int)cmdList.size();

    int IO_fd[2], inp_fd = 0, ret_code, i;
    int err = 0;
    for (i = 0; i < numCmd - 1; i++)
    {
        if (pipe(IO_fd) == -1)
        {
            cout << "\x1b[31mERROR: Pipe cannot be created!\x1b[0m\n";
            err = 1;
            return -1;
        }
        else
        {
            vector<string> tokenList;
            cmd_split(tokenList, cmdList[i]);
            int numTokens = tokenList.size();
            
            // Write to IO_fd[1]
            if (numTokens > 0)
                ret_code = executeMain(tokenList, inp_fd, IO_fd[1]);
            else
            {
                cout << "\x1b[31mERROR: Invalid syntax!\x1b[0m\n";
                err = 1;
                return -1;
            }
            close(IO_fd[1]);
            // set the new input fd
            inp_fd = IO_fd[0];
            
        }
    }
    if (!err)
    {
        vector<string> tokenList;
        cmd_split(tokenList, cmdList[i]);
        int numTokens = tokenList.size();
       
        if (numTokens > 0)
            ret_code = executeMain(tokenList, inp_fd, 1);
        else
        {
            cout << "\x1b[31mERROR: Invalid syntax!\x1b[0m\n";
            err = 1;
            return -1;
        }
    }
    return 0;
}

int execute_cd(vector<string> &args)
{
    if ((int)args.size() == 1)
    {
        cout << "\n\x1b[31mERROR: No argument provided!\x1b[0m\n";
        return 0;
    }
    else if ((int)args.size() > 2)
    {
        cout << "\n\x1b[31mERROR: Too many arguments provided!\x1b[0m\n";
        return 0;
    }
    const char *dir_name = args[1].c_str();
    if (chdir(dir_name) != 0)
    {
        cout << "\n\x1b[31mERROR: Directory not found!\x1b[0m\n";
        return 0;
    }
    cout << "\n";
    return 1;
}

void restore_history()
{
    char *buffer;
    string tempVar;
    FILE *fptr = fopen("hist.txt", "w");
    for (int i = 0; i < (int)historyContainer.size(); i++)
    {
        tempVar = historyContainer[i];
        tempVar += "\n";
        fputs(tempVar.c_str(), fptr);
    }
    fclose(fptr);
}

int execute_exit(vector<string> &args)
{
    // disba;();
    restore_history();
    cout << "\n\n\e[1;91mExiting basicsh ...\x1b[0m\n\n";
    return -1;
}

int execute_help(vector<string> &args)
{
    cout << "\nbasicsh Help Page\n";
    cout << "In-built commands:\n";
    for (int i = 0; i < inBuiltCmds.size(); i++)
    {
        cout << inBuiltCmds[i] << "\n";
    }
    cout << "Type man <command> to know about a command\n";
    cout << "Type man to know about other commands\n";

    return 1;
}

int execute_history(vector<string> &args)
{
    cout << "\n";
    for (int i = 0; i < historyContainer.size(); i++)
    {
        cout << i + 1 << ". " << historyContainer[i] << "\n";
    }
    return 1;
}

int executeInterface(vector<string> &tokenList)
{
    int numTokens = tokenList.size();
    if (numTokens == 0)
        return 0;
    // compare command with a built-in command
    // need to execute separately
    for (int i = 0; i < inBuiltCmds.size(); i++)
    {
        if (inBuiltCmds[i] == tokenList[0])
            return (*builtInFuncs[i])(tokenList);
    }
    // if no built-in command call executeMain
    return executeMain(tokenList, 0, 1);
    // }
}


int executeMain(vector<string> &args, int inp_fd, int out_fd)
{
    pid_t pid, waiting_pid;
    int num_args = args.size();
    int status;

    // Fork()
    pid = fork();
    char *arg;

    // child process
    if (pid == 0)
    {
        // Redirect stdin to inp_fd
        if (inp_fd != 0)
        {
            dup2(inp_fd, 0);
            close(inp_fd);
        }
        // Redirect stdout to out_fd
        if (out_fd != 1)
        {
            dup2(out_fd, 1);
            close(out_fd);
        }

        int size = 0;
        bool flg = false, nflg = false;
        
        // Take a flg to check for priinting newline
        for (int i = 0; i < num_args; i++)
        {
            if ((!flg) && ((args[i] == "&") || (args[i] == "<") || (args[i] == ">") || (args[i] == ">>")))
                size = i, flg = true; 
            if (args[i] == "<")       // input redirection
            {
                nflg = true;
                int read_fd = open(args[i + 1].c_str(), O_RDONLY);
                dup2(read_fd, 0);
            }
            if (args[i] == ">")   // output redirection
            {
                nflg = true;
                int write_fd = open(args[i + 1].c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
                dup2(write_fd, 1);
            }
            else if (args[i] == ">>")
            {
                nflg = true;
                // open file to write
                int write_fd = open(args[i + 1].c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
                dup2(write_fd, 1);
            }
        }
        if (!flg)
            size = num_args;
        char **execArgs, *cmd;
        execArgs = (char **)malloc(sizeof(char *) * (size + 1));
        if (execArgs == NULL)
        {
            cout << "\x1b[31mERROR: Cannot allocate memory!\x1b[0m\n";
            exit(EXIT_FAILURE);
        }
        
        int k;
        for (k = 0; k < size; k++)
            execArgs[k] = strdup(args[k].c_str());
        execArgs[k] = NULL;
        cmd = execArgs[0];
        
        if (!nflg)
            cout << endl;
        
        // catch ctrl+C
        signal(SIGINT, sigintHandler);
        int i = 0;
        while (!freeParent && !i)
            signal(SIGTSTP, SIG_DFL), i = 1;

        if (execvp(execArgs[0], execArgs) == -1)
        {
            cout << "\x1b[31mERROR: Command cannot be executed!\x1b[0m\n";
            exit(1);
        }
    }
    if (pid > 0) // parent process
    {
        if (args.back() != "&")
            sleep(0.1);
        curr_process = pid;
        signal(SIGINT, sigintHandler);
        signal(SIGTSTP, sigstpHandler); // Use SIG_DFL for default behaviour (Terminal will exit)
        // Wait until last character is &
        if (args.back() != "&")
        {
            do
            {
                waiting_pid = waitpid(pid, &status, WUNTRACED);
            } while ((!freeParent) && !WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }

    // Catch errors in forking the process
    else
    {
        cout << "\x1b[31mERROR: fork() call failed!\x1b[0m\n";
        exit(1);
    }
    
    return 0;
}

bool commandPrompt(int a)
{
    string user, cwd;
    user = string(getenv("USER"));
    cwd = get_curr_path();
    if (cwd == "")
        return false;
    // Insert newline if process runs in background
    if (bg)
    {
        cout << "\n";
    }
    // Command-Prompt
    cout << "\033[1;32m" << user << "@basicsh\x1b[0m:\e[1;34m" << cwd.replace(0, find_occurrence(cwd), "~") << "\x1b[0m [" << a << "] $ ";
    return true;
}
// Implememnt auto-complete
string auto_complete(string line)
{
    int len = line.size();
    reverse(line.begin(), line.end());
    string key, prev_cmd;
    auto pos = line.find(' ');
    if (pos != string::npos)
    {
        key = line.substr(0, pos);
        prev_cmd = line.substr(pos);
    }
    else
    {
        key = line;
        prev_cmd = "";
    }
    reverse(key.begin(), key.end());
    reverse(prev_cmd.begin(), prev_cmd.end());
    
    vector<string> match;
    struct dirent *d;
    DIR *dr;
    dr = opendir(".");
    if (dr != NULL)
    {
        // cout<<"List of Files & Folders:-\n";
        for (d = readdir(dr); d != NULL; d = readdir(dr))
        {
            string file(d->d_name);
            if (file.find(key) == 0)
            {
                match.push_back(file);
            }
        }
        closedir(dr);
       
        if ((int)match.size() == 1)
        {
            
            int tmp = 0, req_sz = (int)line.size();
            
            while (tmp < (int)req_sz)
            {
                cout << "\b \b";
                tmp++;
            }
            
            prev_cmd += match[0];
            prev_cmd += " ";
            cout << prev_cmd;
        }
        else if ((int)match.size() > 1)
        {
            cout << "\n";
            for (int i = 0; i < match.size(); i++)
            {
                cout << i + 1 << ". " << match[i] << " ";
            }
            cout << "\n";
            
            int optn;
            cin >> optn;
            char nl = getchar();
            if ((optn > 0) && (optn <= (int)match.size()))
            {
                prev_cmd += match[optn - 1];
            }
            else
            {
                cout << "ERROR: Invalid Option!\n";
                return line;
            }
            commandPrompt(4);
            prev_cmd += " ";
            cout << prev_cmd;
        }
        else
        {
            return line;
        }
    }
    else
        cout << "\nError Occurred!";
    
    return prev_cmd;
}
int histCnt = 0;
#define MAXHISTORYMEM 1000
void alignHistory()
{
    int lineCnt = historyContainer.size();
    if (lineCnt <= MAXHISTORYMEM)
    {
        return;
    }
    while ((int)historyContainer.size() > MAXHISTORYMEM)
    {
        historyContainer.pop_front();
    }
    return;
}

// Take characters one by one
static struct termios oldt, newt, copy_old;
int settingsFlg = 0;
void enableSettings()
{
    /* tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt */
    tcgetattr(STDIN_FILENO, &oldt);
    /*now the settings will be copied*/
    newt = oldt;
    if (!settingsFlg)
        copy_old = oldt, settingsFlg = 1;

    /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
    newt.c_lflag &= ~(ICANON | ECHO);

    /*Those new settings will be set to STDIN
    TCSANOW tells tcsetattr to change attributes immediately. */
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void disableSettings()
{
    /*restore the old settings*/
    tcsetattr(STDIN_FILENO, TCSANOW, &copy_old);
}

// Valid printing characters
bool valid(char ch)
{
    if (ch >= 32 && ch <= 127)
        return true;
    else
        return false;
}

string searchHistory(string &key)
{
    string finalStr;
    int lineCnt = 0;
    FILE *fptr1 = fopen("hist.txt", "r"), *fptr2;
    char buffer[200];
    vector<string> pipedCmds, args;

    bool flg = false;
    for (int i = (int)historyContainer.size() - 1; i >= 0; i--)
    {
        if (key == historyContainer[i])
        {
            commandPrompt(5);
            cout << historyContainer[i] << " ";
            finalStr = historyContainer[i];
            finalStr += "";
            flg = true;
            break;
        }
    }
    // No exact match
    if (!flg)
    {
        int maxMatch = 0, currMatch = 0;
        for (int i = (int)historyContainer.size() - 1; i >= 0; i--)
        {
            currMatch = LCSubstr(historyContainer[i], key);
            if (currMatch > max(2, maxMatch))
            {
                finalStr = historyContainer[i];
                maxMatch = currMatch;
            }
        }
        if (maxMatch == 0)
        {
            // commandPrompt(5);
            cout << "No match for search term in history!\n";
            finalStr = "";
        }
        else
        {
            commandPrompt(6);
            finalStr += " ";
            cout << finalStr;
        }
    }
    return finalStr;
}

// Extract history from hist.txt
void extractHistory()
{
    FILE *fptr;
    if ((fptr = fopen("./hist.txt", "r+")) == NULL)
    {
        if ((fptr = fopen("./hist.txt", "w+")) == NULL)
        {
            cout << "ERROR: Cannot access history!\n";
            return;
        }
    }
    char ch;
    string tmp;
    while ((ch = fgetc(fptr)) != EOF)
    {
        if (ch == '\n')
        {
            historyContainer.push_back(tmp);
            tmp.clear();
        }
        else
        {
            tmp.push_back(ch);
        }
    }
    fclose(fptr);
}

int main(int argc, char *argv[])
{
    // Main signal handlers
    signal(SIGINT, sh1);
    signal(SIGTSTP, sh2);

    char ch;
    extractHistory();

    cout << "\n+++++++++++++++++++++++++++++\n\n    \033[1;32mWelcome to basicsh!\033[0m\n\n+++++++++++++++++++++++++++++\n";
    cout << "Developed by:\n\e[1;91m- Gaurav Madkaikar (19CS30018)\n- Girish Kumar (19CS30019)\x1b[0m\n";
    while (1)
    {
        enableSettings();
        if (!ctrlPrompt)
        {
            if (ctrlNewline)
                cout << "\n";
            bool prompt = commandPrompt(1);
            if (!prompt)
                continue;
            ctrlNewline = 0;
        }
        
        int numCmd;
        string line;
        vector<string> cmdList, tokenList;

        char ch;
        // Read input line one-by-one
        while (1)
        {
            bg = 0;
            ch = getchar();
            if ((ch == '\n') || (ch == EOF))
            {
                // disableSettings();
                break;
            }
            // backspace
            else if (ch == '\177')
            {
                disableSettings();
                if ((int)input.size() > 0)
                {
                    // for(int i=0;i<3;i++)
                    cout << "\b \b";
                    input.pop_back();
                }
                enableSettings();
            }
            else if (ch == '\t')
            {
                if ((int)input.size() == 0)
                    continue;
                disableSettings();
                
                input = auto_complete(input);
                
                enableSettings();
            }
            else if ((int)ch == 18)
            {
                disableSettings();
                cout << "\nEnter search term: ";
                string srch;
                cin >> srch;
                char capture;
                capture = getchar();
                input = searchHistory(srch);
                if (input.size() == 0)
                    break;
                enableSettings();
            }
            else if ((int)ch == 12)
            {
                disableSettings();
                cout << "\e[1;1H\e[2J";
                input = "";
                break;
            }
            else if (valid(ch))
            {
                if (ch == '&')
                {
                    bg = 1;
                }
                cout << ch;
                input.push_back(ch);
            }
        }
        // Disable terminal settings
        disableSettings();
        
        line = trim(input);
        ctrlPrompt = 0;
        string append_line = line + '\n';
        if (line.size() > 0)
        {
            historyContainer.push_back(line);
            alignHistory();
        }
        if (line.size() == 0)
        {
            ctrlNewline = 1;
            fflush(stdout);
            continue;
        }
        // Clear input line
        input.clear();
        
        if ((line.find(">") != string::npos) || (line.find(">>") != string::npos) || (line.find("<") != string::npos))
        {
            ctrlNewline = 1;
        }

        // Multiwatch
        if (line.find("multiWatch") != string::npos)
        {
            int err_code = execute_multiWatch(line);
            if (err_code == -1)
            {
                cout << "\nERROR: Command cannot be executed!\n";
            }
            continue;
        }
        // Extract individual commands and store its count
        line_split(line, cmdList);

        numCmd = (int)cmdList.size();
        int err_code;
        // Execute based on number of commands
        if (numCmd > 0)
        {
            if (numCmd == 1)
            {
                err_code = execSingleCommand(cmdList);
            }
            else if (numCmd > 1)
            {
                err_code = execPipedCommands(cmdList);
            }
        }
        if (err_code == -1)
            break;

        fflush(stdout);
    }

    return 0;
}