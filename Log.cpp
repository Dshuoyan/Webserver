#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "Log.h"
#include <pthread.h>
using namespace std;
Log::Log()
{
	m_count = 0;//日志行数记录
	m_is_async = false;
}

Log::~Log()
{
	if (m_fp != NULL)
	{
		fclose(m_fp);
	}
}

bool Log::init(const char* file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
	//printf("init log \n");
	if (m_is_async == true && max_queue_size>0)
	{
		//printf("11111一切正常2\n");
		m_is_async = true;
		m_log_queue = new block_queue<string>(max_queue_size);
		pthread_t tid;
		//flush_log_thread为回调函数,这里表示创建线程异步写日志
		int ret = pthread_create(&tid, NULL, flush_log_thread, NULL);
	}
	m_split_lines = split_lines;
	m_buf = new char[log_buf_size];
	//printf("22222222一切正常2\n");
	memset(m_buf, '\0', m_log_buf_size);
	m_split_lines = split_lines;
	
	time_t t = time(NULL);
	struct tm* sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;

	const char* p = strrchr(file_name, '/');//查找'/'最后出现的位置
	char log_full_name[256] = { 0 };
	
	m_close_log = close_log;//日志开关
	//相当于自定义日志名
	//若输入的文件名没有/，则直接将时间+文件名作为日志名
	if (p == NULL)
	{
		snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
		printf("%s\n",log_full_name);
	}
	else
	{
		//将/的位置向后移动一个位置，然后复制到logname中
		//p - file_name + 1是文件所在路径文件夹的长度
		//dirname相当于./
		strcpy(log_name, p + 1);
		strncpy(dir_name, file_name, p - file_name + 1);
		//后面的参数跟format有关
		snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
	}
		/*
		C 库函数 FILE *fopen(const char *filename, const char *mode) 使用给定的模式 mode
		打开 filename 所指向的文件。
		"r"	    打开一个用于读取的文件。该文件必须存在。
		"w"	    创建一个用于写入的空文件。如果文件名称与已存在的文件相同，则会删除已有文件的内容，文件被视为一个新的空文件。
		"a"	    追加到一个文件。写操作向文件末尾追加数据。如果文件不存在，则创建文件。
		"r+"	打开一个用于更新的文件，可读取也可写入。该文件必须存在。
		"w+"	创建一个用于读写的空文件。
		a+"	打开一个用于读取和追加的文件
		*/
		m_fp = fopen(log_full_name, "a");
		if (m_fp == NULL)
		{
			printf("打开失败\n");
			return false;
		}
		//printf("init end\n");
		return true;
}

void Log::write_log(int level, const char* format, ...)
{
	struct timeval now = { 0,0 };
	gettimeofday(&now, NULL);//获取当前时间,从1970至今的秒数 struct timeval{time_t tv_sec,suseconds_t tv_usec};
	time_t t = now.tv_sec;
	struct tm* sys_tm = localtime(&t);//将time_t类型的数据转为tm按本地时区转换为tm类型的结构体
	struct tm my_tm = *sys_tm;			//localtime返回的是一个指针，保存指针指向内存里的数据
	char s[16] = { 0 };					//保存日志级别字符串

	switch (level)
	{
	case 0:
		strcpy(s, "[debug]:");
		break;
	case 1:
		strcpy(s, "[info]:");
		break;
	case 2:
		strcpy(s, "[warn]:");
		break;
	case 3:
		strcpy(s, "[erro]:");
		break;
	default:
		strcpy(s, "[info]:");
		break;
	}


	//写入一个log，对m_count++, m_split_lines最大行数
	m_mutex.lock();
	m_count++;

	//不是今天或者当前负责记录的日志已超过最大行数
	if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
	{
		char new_log[256] = { 0 };
		fflush(m_fp);//将缓冲区数据写入日志文件
		fclose(m_fp);//关闭原有的日志
		char tail[16] = { 0 };//保存日志名中的时间部分

		//格式化日志名中的时间部分
		snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
		
		//如果是时间不是今天,则创建今天的日志，更新m_today和m_count
		if (m_today != my_tm.tm_mday)
		{
			snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
			m_today = my_tm.tm_mday;
			m_count = 0;
		}
		else//超过了最大行，在之前的日志名基础上加后缀, m_count/m_split_lines
		{
			snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
		}
		m_fp = fopen(new_log, "a");
	}

	m_mutex.unlock();

	va_list valst;
	va_start(valst, format);

	string log_str;
	m_mutex.lock();

	//写入的具体时间内容格式
	//时间格式化，snprintf成功返回写字符的总数，其中不包括结尾的null字符
	int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
		my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
		my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

	//内容格式化，用于向字符串中打印数据、数据格式用户自定义，返回写入到字符数组str中的字符个数(不包含终止符)
	int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);//写入具体内容
	m_buf[n + m] = '\n';
	m_buf[n + m + 1] = '\0';

	log_str = m_buf;
	m_mutex.unlock();

	//若m_is_async为true表示异步，默认为同步
	//若异步,则将日志信息加入阻塞队列,同步则加锁向文件中写
	if (m_is_async && !m_log_queue->full())
	{
		m_log_queue->push(log_str);
	}
	else
	{
		m_mutex.lock();
		fputs(log_str.c_str(), m_fp);
		m_mutex.unlock();
	}
	va_end(valst);
}

void Log::flush(void)
{
	m_mutex.lock();
	//强制刷新写入流缓冲区
	fflush(m_fp);
	m_mutex.unlock();
}
