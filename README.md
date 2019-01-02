# onlinejudgecore

OJ的后台判题模块，polling轮询mysql数据库，如果有QUEUE状态的题目就交给启动一个docker运行judge判题，将结果写回数据库。

安全方面，只限制了进程的一些资源(setrlimit, getrlimit)，没有用ptrace追踪进程、获取进程的各种状态。

主要参考了[onlineJudge](https://github.com/mufeng964497595/onlineJudge)。
