#include <stdlib.h>
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/reg.h>
#include <unistd.h>

#include "mysql_connection.h"
#include "mysql_driver.h"
#include "cppconn/driver.h"
#include "cppconn/exception.h"
#include "cppconn/resultset.h"
#include "cppconn/statement.h"

char* hostname = nullptr;
char* username = nullptr;
char* passwd = nullptr;
char* dbname = nullptr;

const int COMPILE_TIME = 6;
const int COMPILE_MEM  = 128;
const int MB = 1048576;
const int FILE_SIZE = 10;
const int OFFSET_OLE = 1024;

const char* MYSQL_ERROR_LOG = "mysqlError.log";
const char* SYSTEM_ERROR_LOG = "systemError.log";

int compile(const char* sid);
void updateSubmitStatus(const char* sid, std::string result);
void judge(const char* sid, const char* pid);
std::string checkResult(std::string dataOut, const char* userOutFileName);
void run(int timeLimit, int memLimit, int& usedTime, const char* dataIn, const char* userOut, const char* errOut);

int main(int argc, char **argv)
{
    if (argc != 7) {
        std::cout << "argv num error:" << argc << "\n";
        return 1;
    }

	const char *sid = argv[1];
	const char *pid = argv[2];
    hostname = argv[3];
	username = argv[4];
	passwd = argv[5];
	dbname = argv[6];


	int status = compile(sid);
    
	if (status == -1) {// system error
		updateSubmitStatus(sid, "SYSTEM_ERROR");
	} else if (status != 0) {// compile error
		updateSubmitStatus(sid, "COMPILING_ERROR");
	} else {// compile success
		judge(sid, pid);
	}

	return 0;
}

int compile(const char* sid)
{
	updateSubmitStatus(sid, "COMPILING...");

	const char *compilelog = "compile.log";
	int time_limit = COMPILE_TIME;
	int memory_limit = COMPILE_MEM*MB;

	pid_t pid = fork();
	if (pid == -1) {// fork error
		std::ofstream fout(SYSTEM_ERROR_LOG, std::ios_base::out | std::ios_base::app);
		fout << "Error: fork() file when compile.\n";
		fout << "In file " << __FILE__ << " function (" << __FUNCTION__ << ")"
			<<" , line: " << __LINE__ << "\n";
		fout.close();
		return -1;
	} else if (pid != 0) {// parent
		int status;
		waitpid(pid, &status, 0);
		return status;
	} else {// child
		struct rlimit lim;
		lim.rlim_cur = lim.rlim_max = time_limit;
		setrlimit(RLIMIT_CPU, &lim);

		alarm(0);
		alarm(time_limit);

		lim.rlim_cur = lim.rlim_max = memory_limit;
		setrlimit(RLIMIT_AS, &lim);

		lim.rlim_cur = lim.rlim_max = FILE_SIZE*MB;
    	setrlimit(RLIMIT_FSIZE, &lim);

		freopen(compilelog,"w",stderr);

		execlp("g++", "g++", "-Wall", "-fno-asm", "-lm", "--static", "-std=c++11"
		,"main.cpp", "-o", "main", NULL);

		exit(0);
	}
}



void judge(const char* sid, const char* pid)
{
	updateSubmitStatus(sid, "RUNING...");

	const char* dataIn = "data.in";
	const char* userOut = "user.out";
	const char* errOut = "err.out";

	int time_limit = COMPILE_TIME;
	int memory_limit = COMPILE_MEM*MB;

	sql::Driver *driver = nullptr;
    sql::Connection *conn = nullptr;
    sql::Statement *st = nullptr;
    sql::ResultSet *rs = nullptr;

	try {
		driver = get_driver_instance();

		conn = driver->connect(hostname, username, passwd);
        conn->setSchema(dbname);

        st = conn->createStatement();

        sql::SQLString sql = "Select * from problem where pid = "+std::string(pid)+" ;";
        rs = st->executeQuery(sql);

		std::string result("ACCEPT");

		while (rs->next() && result == "ACCEPT") {
			std::string inputData = rs->getString("input");
			std::string outputData = rs->getString("output");

			std::ofstream fout(dataIn);
			fout << inputData;
			fout.close();

			int usedTime = 0;
			pid_t pid = fork();

			if (pid == 0) {// child
				run(time_limit, memory_limit,usedTime,dataIn,userOut,errOut);
				exit(0);
			} else if (pid != -1) {// parent
				waitpid(pid,NULL,0);
			} else {// system error
				std::ofstream fout(SYSTEM_ERROR_LOG, std::ios_base::out | std::ios_base::app);
				fout << "# ERR fork in " << __FILE__;
				fout << "function is (" << __FUNCTION__ << ") in line " << __LINE__ << "\n";
				fout.close();

				result = "SYSTEM_ERROR";
			}

			if (result == "ACCEPT") {
				result = checkResult(outputData, userOut);
				updateSubmitStatus(sid, result);
			}
		}

		delete rs;
		delete st;
		delete conn;
	} catch (sql::SQLException& e){
		std::ofstream fout(MYSQL_ERROR_LOG, std::ios_base::out | std::ios_base::app);

		fout << "# ERR: SQLException in " << __FILE__;
		fout << "# ERR: " << e.what();
		fout << " (MySQL error code: " << e.getErrorCode();
		fout << ", SQLState: " << e.getSQLState() << " )" << "\n";

		fout.close();

		updateSubmitStatus(sid, "SYSTEM_ERROR");
	}
}

std::string checkResult(std::string dataOut, const char* userOutFileName)
{
	std::ifstream fin(userOutFileName);
	std::string line;
	std::string userOut = "";

	while (getline(fin,line)) {
		userOut += line+"\n";
	}
	fin.close();

	// compare
	int dLen = dataOut.length();
	int uLen = userOut.length();

	std::string result = "ACCEPT";
	if (uLen >= (dLen<<1)+OFFSET_OLE) 
		result = "OUTPUT_LIMIT_EXCEEDED";
	else if (uLen!=dLen || dataOut.compare(userOut)!=0) 
		result = "WRONG_ANSWER";

	return result;
}

void run(int timeLimit, int memLimit, int& usedTime, const char* dataIn, const char* userOut, const char* errOut)
{
	freopen(dataIn, "r", stdin);
	freopen(userOut, "w", stdout);
	freopen(errOut, "w", stderr);

	struct rlimit lim;
	lim.rlim_cur = lim.rlim_max = timeLimit;
	setrlimit(RLIMIT_CPU, &lim);
	alarm(0);
	alarm(timeLimit);

	lim.rlim_cur = lim.rlim_max = FILE_SIZE*MB;
    setrlimit(RLIMIT_FSIZE, &lim);

	lim.rlim_cur = lim.rlim_max = memLimit;
	setrlimit(RLIMIT_AS, &lim);

	execl("./main", "main",NULL);
	exit(0);
}

void updateSubmitStatus(const char* sid, std::string result)
{
	sql::Driver *driver = nullptr;
    sql::Connection *conn = nullptr;
    sql::Statement *st = nullptr;

	try {
		driver = sql::mysql::get_driver_instance();
		conn = driver->connect(hostname, username, passwd);
        conn->setSchema(dbname);

        st = conn->createStatement();

        sql::SQLString sql = "update status set status = \""+ result +"\" where sid = "+sid+";";
		st->executeQuery(sql);

		delete st;
        delete conn;
	} catch (sql::SQLException &e) {
		
	}
}