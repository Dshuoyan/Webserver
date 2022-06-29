#include "sql_connection.h"

//�����ݿ����ӳ���ȡ��һ�����õ�����
MYSQL* connection_pool::GetConnection()
{
    MYSQL* con = NULL;

    if (0 == connList.size())
        return NULL;

    reserve.wait();
    lock.lock();

    con = connList.front();//���ͷ�ڵ�����
    connList.pop_front();//ȥ��ȡ����ͷ�ڵ�

    --FreeConn;//��������������
    ++CurConn;//��ʹ������������

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

//��ȡ��������
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
        for (it = connList.begin(); it != connList.end(); ++it)//���������ر����ݿ�����
        {
            MYSQL* con = *it;
            mysql_close(con);
        }
        CurConn = 0;  //��ǰ��ʹ�õ�������
        FreeConn = 0; //��ǰ���е�������
        connList.clear();

        lock.unlock();
    }
    lock.unlock();
}

//��ȡ���ݿ����ӳ�
connection_pool* connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

    //���ӳس�ʼ��,�������ݿ����ӷ���������ʼ�����ݿ���Ϣ������ʵ�����ӵ�������ֵ
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
        MYSQL* con = NULL;//���ݿ�����
        con = mysql_init(con);//������ʶ����������������


        if (con == NULL)
        {
            cout << "Error:" << mysql_error(con);
            exit(1);
        }
        //�������ݿ�����
        con = mysql_real_connect(con, url.c_str(), User.c_str(),PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);

        if(con == NULL) 
        {
            cout << "Error: " << mysql_error(con);
            exit(1);
        }
        //������ӵ�����
        connList.push_back(con);
        ++FreeConn;//���е�����������
    }

    reserve = sem(FreeConn);//�ź���ͨ��=��������п�������
    this->MaxConn = FreeConn;//��ʼ�������������
    lock.unlock();
}


//ʹ�ö���ָ���Ŀ����Ϊ��
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
