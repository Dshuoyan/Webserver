#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "locker.h"

using namespace std;
/*���������ź��������ݿ���������*/
/*���ݿ����*/
/*���ݿ���������*/
class connection_pool
{
public:
	MYSQL* GetConnection();				//��ȡ���ݿ�����
	bool ReleaseConnection(MYSQL* conn); //�ͷ�����
	int GetFreeConn();					 //��ȡ����
	void DestroyPool();					 //������������

	//����ģʽ
	static connection_pool* GetInstance();

	void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn,int close_log);

	connection_pool();
	~connection_pool();

private:
	unsigned int MaxConn;  //���������
	unsigned int CurConn;  //��ǰ��ʹ�õ�������
	unsigned int FreeConn; //��ǰ���е�������
	int m_close_log;

private:
	locker lock;
	list<MYSQL*> connList;//���ӳ�
	sem reserve;

private:
	string url;			 //������ַ
	string Port;		 //���ݿ�˿ں�
	string User;		 //��½���ݿ��û���
	string PassWord;	 //��½���ݿ�����
	string DatabaseName; //ʹ�����ݿ���
};


class connectionRAII {
public:
	connectionRAII(MYSQL** con, connection_pool* connPool);
	~connectionRAII();

private:
	MYSQL* conRAII;
	connection_pool* poolRAII;
};
#endif