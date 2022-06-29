#include "sql_connection.h"

//从数据库连接池中取出一个可用的连接
MYSQL* connection_pool::GetConnection()
{
    MYSQL* con = NULL;

    if (0 == connList.size())
        return NULL;

    reserve.wait();
    lock.lock();

    con = connList.front();//获得头节点数据
    connList.pop_front();//去掉取出的头节点

    --FreeConn;//空闲连接数减少
    ++CurConn;//已使用连接数增加

    lock.unlock();
    return con;

}

connection_pool::connection_pool()
{
    this->CurConn = 0;
    this->FreeConn = 0;
}

connection_pool::~connection_pool()
{
    this->DestroyPool();
}


bool connection_pool::ReleaseConnection(MYSQL* conn)
{
    if (NULL == conn)
        return false;

    lock.lock();

    connList.push_back(conn);
    ++FreeConn;
    --CurConn;

    lock.unlock();

    reserve.post();
    return true;
}

//获取空闲连接
int connection_pool::GetFreeConn()
{
    return this->FreeConn;
}

void connection_pool::DestroyPool()
{
    lock.lock();
    if (connList.size() > 0)
    {
        list<MYSQL*>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)//遍历链表，关闭数据库连接
        {
            MYSQL* con = *it;
            mysql_close(con);
        }
        CurConn = 0;  //当前已使用的连接数
        FreeConn = 0; //当前空闲的连接数
        connList.clear();

        lock.unlock();
    }
    lock.unlock();
}

//获取数据库连接池
connection_pool* connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

    //连接池初始化,创建数据库连接放入链表，初始化数据库信息，根据实际连接的数量赋值
void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn,int close_log)
{
    this->url = url;
    this->Port = Port;
    this->User = User;
    this->PassWord = PassWord;
    this->DatabaseName = DataBaseName;
    this->m_close_log = close_log;
    lock.lock();
    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL* con = NULL;//数据库连接
        con = mysql_init(con);//创建标识符，用于索引连接


        if (con == NULL)
        {
            cout << "Error:" << mysql_error(con);
            exit(1);
        }
        //建立数据库连接
        con = mysql_real_connect(con, url.c_str(), User.c_str(),PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);

        if(con == NULL) 
        {
            cout << "Error: " << mysql_error(con);
            exit(1);
        }
        //添加连接到链表
        connList.push_back(con);
        ++FreeConn;//空闲的连接数增加
    }

    reserve = sem(FreeConn);//信号量通过=运算符进行拷贝操作
    this->MaxConn = FreeConn;//初始化最大连接数量
    lock.unlock();
}


//使用二级指针的目的是为了
connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* connPool)
{
    *SQL = connPool->GetConnection();
    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}
