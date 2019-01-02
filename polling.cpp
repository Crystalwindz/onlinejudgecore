#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <cstring>
#include <fstream>
#include <queue>
#include <fcntl.h>

#include "mysql_connection.h"
#include "mysql_driver.h"
#include "cppconn/driver.h"
#include "cppconn/exception.h"
#include "cppconn/resultset.h"
#include "cppconn/statement.h"

struct Submission {
    int sid;
    int pid;
    std::string code;

    Submission(int &s, int &p, std::string &c):
    sid(s), pid(p), code(c) {}
};

std::queue<Submission> submissionQueue;

bool WriteCode2File(const char *workdir, std::string &code);

int main(int argc, char **argv)
{
    freopen("polling.log", "a+", stdout);

    // command hostname username passwd dbname judge_shell workdir
    if (argc != 7) {
        std::cout << "argv num error:" << argc << "\n";
        return 1;
    }

    sql::Driver *driver = nullptr;
    sql::Connection *conn = nullptr;
    sql::Statement *st = nullptr;
    sql::ResultSet *rs = nullptr;

    try {
        driver = sql::mysql::get_driver_instance();

        while (true) {
            conn = driver->connect(argv[1], argv[2], argv[3]);
            conn->setSchema(argv[4]);

            st = conn->createStatement();

            sql::SQLString sql = "Select * from status where status = \"QUEUE\"";
            rs = st->executeQuery(sql);

            while (rs->next()) {
                int sid = rs->getInt("sid");
                int pid = rs->getInt("pid");
                std::string code = rs->getString("usercode");

                submissionQueue.emplace(sid, pid, code);
            }

            delete rs;
            delete st;
            delete conn;

            while (!submissionQueue.empty()) {
                Submission s = submissionQueue.front();
                submissionQueue.pop();

                printf("Judge a problem: pid %d sid %d\n",s.pid,s.sid);

                if (!WriteCode2File(argv[6], s.code)) {
                    std::cout << "Write code into file error!\n";
                    return 1;
                }

                pid_t pid = fork();
                if (pid == 0) {// child
                    execl(argv[5], argv[5], argv[6], std::to_string(s.sid).c_str(), 
                    std::to_string(s.pid).c_str(), 
                    argv[1], argv[2], argv[3], argv[4], NULL);
                    exit(1);
                } else if (pid != -1) {
                    waitpid(pid, NULL, 0);
                } else {
                    std::cout << "Fork error!\n";
                    return 1;
                }
            }

            usleep(100);
        }

    } catch (sql::SQLException &e) {
        std::cout << "# ERR: SQLException in "<< __FILE__ 
        << "(" << __FUNCTION__ <<") on line " << __LINE__ <<"\n"
        << "# ERR: " << e.what()
        << " (MySQL error code:" << e.getErrorCode()
        << ", SQLState: " << e.getSQLStateCStr() << ")\n";
    }

    return 0;
}

bool WriteCode2File(const char *workdir, std::string &code)
{
    char path[1024];
    sprintf(path, "%s/%s", workdir, "main.cpp");
    
    printf("Write code to path: %s\n",path);
    std::ofstream fout(path);

    if (!fout) {
        fout.close();
        return false;
    }

    fout << code;
    fout.close();
    return true;
}