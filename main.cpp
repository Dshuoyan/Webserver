#include"threadpool.h"
#include<iostream>
#include <unistd.h>
#include"http_conn.h"
#include"lst_timer.h"
#include "sql_connection.h"

using namespace std;

#define MAX_FD 65536           //最大文件描述符
#define MAX_EVENT_NUMBER 65536 //最大事件数
#define TIMESLOT 5             //最小超时单位

int m_close_log;
//这三个函数在http_conn.cpp中定义，改变链接属性
extern void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
extern void removefd(int epollfd, int fd);
extern int setnonblocking(int fd);

const char* doc_root = "/home/lighthouse/mywebserver/root";
//const char* doc_root = "/home/china/root";
//定时器链表
static sort_timer_lst timer_lst;
static int epollfd = 0;
static int pipefd[2];

//信号处理函数
void sig_handler(int sig)
{
	//为保证函数的可重入性，保留原来的errno
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
}

void addsig(int sig, void(handler)(int), bool restart = true)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart)
		sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
	timer_lst.tick();
	alarm(TIMESLOT);
}

//定时器回调函数，删除非活动连接在socket上的注册事件，并关闭连接
void cb_func(client_data* user_data)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
	assert(user_data);
	close(user_data->sockfd);
	http_conn::m_user_count--;
	//LOG_INFO("close fd %d", user_data->sockfd);
	//Log::get_instance()->flush();
}





void show_error(int connfd, const char* info)
{
	send(connfd, info, strlen(info), 0);
	close(connfd);
}



int main(int argc, char* argv[])
{
	
	//int m_index = 0;
	//参数检查
	if (argc < 3)
	{
		//LOG_DEBUG("参数太少\n");
		return 0;
	}

	//接收参数
	//char* ip = argv[1];
	int port = atoi(argv[1]);
	m_close_log = atoi(argv[2]);

/*
	//创建守护进程
	pid_t pid;
	int i;
	//创建子进程，退出父进程。变成孤儿进程被init进程收养 子进程在后台运行
	if ((pid = fork()) < 0) //fork失败
	{
		perror("fork");
		exit(-1);
	}
	else if (pid > 0)//父进程退出
	{
		exit(0);
	}
	//(2)---setsid使子进程独立。摆脱会话控制、摆脱原进程组控制、摆脱终端控制
	if (-1 == setsid())
	{
		printf("setsid error\n");
		exit(1);
	}
	//通过再次创建子进程结束当前进程，使进程不再是会话首进程来禁止进程重新打开控制终端
	pid = fork();
	if (pid == -1)
	{
		printf("fork error\n");
		exit(1);
	}
	else if (pid)
	{
		exit(0);
	}

	umask(0); //更改文件权限 文件权限值&(~umask)
	chdir("/home/lighthouse/mywebserver"); //更改当前工作目录 chdir("/");
	for (i = 0; i < 3; i++) //关闭打开的文件描述符
	{
		close(i);
	}*/

	//创建数据库连接池
	connection_pool* connPool = connection_pool::GetInstance();
	connPool->init("localhost", "root", "123456", "dsy123", 3306, 8,m_close_log);
	/*********************************/


	//创建线程池
	threadpool<http_conn> *pool = NULL;
	pool = new threadpool<http_conn>(connPool);//将数据库连接池指针放入

	//创建保存http连接的节点
	http_conn* users = new http_conn[100];
	//初始化数据库读取表
	users->initmysql_result(connPool);
	assert(users);

	//初始化日志
	//Log::get_instance()->init("./",m_close_log);

	addsig(SIGPIPE, SIG_IGN);//忽略SIGPIPE信号


	int listenfd = socket(PF_INET, SOCK_STREAM, 0);//创建套接字
	assert(listenfd >= 0);

	//这种方式下，在调用closesocket的时候不会立刻返回，内核会延迟一段时间，这个时间就由l_linger得值来决定。
	//如果超时时间到达之前，发送完未发送的数据(包括FIN包)并得到另一端的确认，closesocket会返回正确，socket描述符优雅性退出。
	//否则，closesocket会直接返回 错误值，未发送数据丢失，socket描述符被强制性退出。需要注意的时，如果socket描述符被设置为非堵塞型，则closesocket会直接返回值。
	//struct linger tmp = { 1, 0 };//socket为阻塞，并且缓冲区有未发送出去的数据，则阻塞1的时间或数据发送完才返回
	//setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

	int ret = 0;
	struct sockaddr_in address;//地址加端口
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;


	//address.sin_addr.s_addr = inet_addr(ip);//转换为网络字节序
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);
	
	ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	if (ret != 0)
	{
		//LOG_DEBUG("bind error");
	}
	assert(ret >= 0);
	ret = listen(listenfd, 5);
	assert(ret >= 0);

	//创建内核事件表
	epoll_event events[MAX_EVENT_NUMBER];
	epollfd = epoll_create(5);
	assert(epollfd != -1);
	//将负责接受新连接的套接字加入监听集合
	addfd(epollfd, listenfd, false,1);
	http_conn::m_epollfd = epollfd;
	

	//创建信号处理函数与主循环的
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
	assert(ret != -1);
	setnonblocking(pipefd[1]);
	addfd(epollfd, pipefd[0], false, 0);//管道加入监听集合
	addsig(SIGALRM, sig_handler, false);
	addsig(SIGTERM, sig_handler, false);//设置信号处理函数
	client_data* users_timer = new client_data[MAX_FD];//定时器用户数据数组
	
	bool stop_server = false;
	bool timeout = false;
	alarm(TIMESLOT);
	while (!stop_server)
	{
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if (number < 0 && errno != EINTR)
		{
			break;
		}
		for (int i = 0; i < number; i++)
		{			
			int sockfd = events[i].data.fd;
			//处理新的连接
			if (sockfd == listenfd)
			{
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				while (1)
				{
					//LOG_DEBUG("新的连接，currConnect: %d\n",http_conn::m_user_count);
					//cout << "新连接" << endl;
					int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
					if (connfd < 0)
					{
						//LOG_ERROR("%s:errno is:%d", "accept error", errno);
						break;
					}
					if (http_conn::m_user_count >= MAX_FD)
					{
						show_error(connfd, "Internal server busy");
						//LOG_ERROR("%s", "Internal server busy");
						break;
					}

					users[connfd].public_init(connfd, client_address, doc_root, 1,m_close_log);//创建http节点			
																				   //初始化client_data数据
					//创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
					users_timer[connfd].address = client_address;
					users_timer[connfd].sockfd = connfd;

					util_timer* timer = new util_timer;//创建当前连接的定时器节点
					timer->user_data = &users_timer[connfd];
					timer->cb_func = cb_func;//设置定时器

					time_t cur = time(NULL);
					timer->expire = cur + 3 * TIMESLOT;//设置超时时间
					users_timer[connfd].timer = timer;
					timer_lst.add_timer(timer);//往链表中添加定时器
				 }
				continue;
			}
			else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))//连接异常关闭连接
			{
				//服务器关闭连接
				util_timer* timer = users_timer[sockfd].timer;
				timer->cb_func(&users_timer[sockfd]);//移除epoll监听,关闭连接
				if (timer)
				{
					timer_lst.del_timer(timer);//定时器链表中删除链表
				}
			}
			//处理信号
			else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
			{
				int sig;
				char signals[1024];
				ret = recv(pipefd[0], signals, sizeof(signals), 0);
				if (ret <= 0)
				{
					continue;
				}
				else
				{
					for (int i = 0; i < ret; i++)//多次触发的信号都会留在管道中
					{
						switch (signals[i])
						{
						case SIGALRM:
						{
							timeout = true;
							break;
						}
						case SIGTERM:
						{
							stop_server = true;
						}
						}
					}
				}
			}
			//处理客户连接上收到的数据服务器端关闭连接，移除对应的定时器
			else if (events[i].events & EPOLLIN)						//新的请求
			{
				util_timer* timer = users_timer[sockfd].timer;
				if (users[sockfd].read_once())							//新的请求读取成功
				{
					pool->append(users + sockfd);
					if (timer)
					{
						time_t cur = time(NULL);
						timer->expire = cur + 3 * TIMESLOT;
						//LOG_INFO("%s", "adjust timer once");
						//Log::get_instance()->flush();
						timer_lst.adjust_timer(timer);
					}
				}
				else
				{//读取失败，关闭连接，移除定时器
					timer->cb_func(&users_timer[sockfd]);
					if (timer)
					{
						timer_lst.del_timer(timer);
					}
				}
			}
			else if (events[i].events & EPOLLOUT)
			{
				util_timer* timer = users_timer[sockfd].timer;
				if (users[sockfd].write())							//发送响应报文之后是短连接或发送失败，则关闭连接
				{
					//若有数据传输，则将定时器往后延迟3个单位,并对新的定时器在链表上的位置进行调整
					if (timer)
					{
						time_t cur = time(NULL);
						timer->expire = cur + 3 * TIMESLOT;
						//LOG_INFO("%s", "adjust timer once");
						//Log::get_instance()->flush();
						timer_lst.adjust_timer(timer);
					}
				}
				else
				{
					timer->cb_func(&users_timer[sockfd]);
					if (timer)
					{
						timer_lst.del_timer(timer);
					}
				}
			}
		}

	}

	close(epollfd);
	close(listenfd);
	close(pipefd[1]);
	close(pipefd[0]);
	delete[] users;
	delete[] users_timer;
	delete pool;
	return 0;
}
