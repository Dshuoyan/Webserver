#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include "locker.h"
//#include "Log.h"
#include "sql_connection.h"
class http_conn
{
public:
    static const int FILENAME_LEN = 200;//设置读取文件名称的大小
    static const int READ_BUFFER_SIZE = 2048;//设置读缓冲区大小m_read_buf
    static const int WRITE_BUFFER_SIZE = 1024;//设置写缓冲区大小m_write_buf
    enum METHOD//http请求方法
    {
        GET = 0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH
    };
    enum CHECK_STATE//主状态机状态
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE//报文解析状态码(结果)，报文请求类型
    {
        NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void public_init(int sockfd, sockaddr_in addr, const char* root, int TRIGMode,int close_log);//监听文件描述符，连接的地址信息，触发的模式LT or ET
    void close_conn(bool real_close = true);//关闭http连接
    void process();//处理客户请求,解析读到的数据并生成对应的响应报文
    bool read_once();//将socket中的数据读到m_read_buf中，供后续解析使用
    bool write();//响应报文写入函数，将生成的http响应报文写入m_write_buf
    sockaddr_in* get_address()//获取保存的客户端信息
    {
        return &m_address;
    }
    void initmysql_result(class connection_pool* connPool);

private:
    void init();                                //初始化http连接，供公共接口init使用
    HTTP_CODE process_read();                   //从m_read_buf读取，并处理请求报文
    bool process_write(HTTP_CODE ret);          //向m_write_buf中写入响应报文
    HTTP_CODE parse_request_line(char* text);   //主状态机解析报文中请求行数据
    HTTP_CODE parse_headers(char* text);        //主状态机解析报文中的请求头数据
    HTTP_CODE parse_content(char* text);        //主状态机解析报文中的请求内容
    HTTP_CODE do_request();                     //生成请求报文

    //get_line用于将指针后移，指向未处理的字符串
    char* get_line() { return m_read_buf + m_start_line; };

    LINE_STATUS parse_line();//从状态机读取一行数据，分析是请求报文的那一段
    void unmap();

    //根据响应报文格式，生成对应8个部分，以下函数均有do_request调用
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;
    int writeTaskState;  //给任务函数判断是写，还是读，读为0, 写为1

private:
    int m_sockfd;//连接的套接字
    sockaddr_in m_address;

    //存储读取的请求报文请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];

    //缓冲区中m_read_buf中数据最后一个字节的下一个位置
    int m_read_idx;
    //从状态机将要在m_read_buf中读取的下一个位置m_checked_idx。parse_line函数读取m_read_buf中的的数据并维护m_checked
    int m_checked_idx;
    //m_read_buf中即m_start_line是每一个数据行在m_read_buf中的起始位置
    int m_start_line;

    //储存响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    //因为m_write_idx表示为待发送文件的定位点
    int m_write_idx;

    //主状态机的状态
    CHECK_STATE m_check_state;
    //保存解析得到的方法
    METHOD m_method;

    //以下为解析请求报文中对应的6个变量
    char m_real_file[FILENAME_LEN];//保存资源文件在服务器的完整文件名
    char* m_url;//保存解析得到的url
    char* m_version;
    char* m_host;
    int m_content_length;//解析请求头部内容长度字段
    bool m_linger;//http请求头中的coonect:一行可以设置对应的tcp是长连接还是短连接
    
    char* m_file_address;//获取服务器上的文件地址,将需要传送的文件映射到内存中,该变量用来保存映射后的地址
    struct stat m_file_stat;//保存请求资源的是否正常的信息，通过stat函数判断请求的资源是否能正常访问
    struct iovec m_iv[2];//io向量机制iovec
    int m_iv_count;
    int cgi;        //是否启用的POST
    char* m_string; //存储发过来的账号密码user=123&passwd=123
    int bytes_to_send;//剩余发送字节数
    int bytes_have_send;//已发送字节数
    const char* doc_root;

    int m_TRIGMode;//触发模式选择，ET,LT. triggered mode 1为Edge triggered
    int m_close_log;
};

#endif


