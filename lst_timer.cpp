#include "lst_timer.h"
#include<iostream>
sort_timer_lst::~sort_timer_lst()//析构，释放所有节点
{
	util_timer* tmp = head;
	while(tmp)
	{
		head = head->next;
		delete tmp;
		tmp = head;
	}
}

void sort_timer_lst::add_timer(util_timer* timer)
{
	//输入参数检查
	if (!timer)
	{
		return;
	}
	//链表是否无节，第一次插入
	if (!head)
	{
		head = tail = timer;
		return;
	}
	if (timer->expire < head->expire)
	{
		timer->next = head;
		head->prev = timer;
		head = timer;
		return;
	}
	add_timer(timer, head);
}

//调整定时器，任务发生变化时，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer* timer)
{
	if (!timer)
	{
		return;
	}
	util_timer* tmp = timer->next;
	//被调整的定时器在链表尾部
	//定时器超时值仍然小于下一个定时器超时值，不调整
	if (!tmp || (timer->expire < tmp->expire))
	{
		return;
	}

	//被调整定时器是链表头结点，将定时器取出，重新插入
	if (timer == head)
	{
		head = head->next;
		head->prev = NULL;
		timer->next = NULL;
		add_timer(timer, head);
	}
	else//被调整定时器在内部，将定时器取出，重新插入
	{
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		add_timer(timer, timer->next);
	}
}

void sort_timer_lst::del_timer(util_timer* timer)
{
	if (!timer)
	{
		return;
	}

	//链表中只有一个定时器，需要删除该定时器
	if ((timer == head) && (timer == tail))
	{
		delete timer;
		head = NULL;
		tail = NULL;
		return;
	}
	//被删除的定时器为头节点
	if (timer == head)
	{
		head = head->next;
		head->prev = NULL;
		delete timer;
		return;
	}

	//被删除的定时器为尾结点
	if (timer == tail)
	{
		tail = tail->prev;
		tail->next = NULL;
		delete timer;
		return;
	}

	//被删除的定时器在链表内部，常规链表结点删除
	timer->prev->next = timer->next;
	timer->next->prev = timer->prev;
	delete timer;
}

//定时任务处理函数
void sort_timer_lst::tick()
{
	if (!head)
	{
		return;
	}
	time_t cur = time(NULL);
	util_timer* tmp = head;

	while (tmp)
	{
		//链表容器为升序排列
		//当前时间小于定时器的超时时间，后面的定时器也没有到期
		if (cur < tmp->expire)
		{
			break;
		}

		//当前定时器到期，则调用回调函数，执行定时事件
		tmp->cb_func(tmp->user_data);

		//将处理后的定时器从链表容器中删除，并重置头结点
		head = tmp->next;
		if (head)
		{
			head->prev = NULL;
		}
		delete tmp;
		tmp = head;
	}
}
//中间和尾部插入节点
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
	util_timer* prev = lst_head;
	util_timer* tmp = prev->next;
	while (tmp)
	{
		if (timer->expire < tmp->expire)
		{
			prev->next = timer;
			timer->next = tmp;
			tmp->prev = timer;
			timer->prev = prev;
			break;
		}
		prev = tmp;
		tmp = tmp->next;
	}
	//遍历完发现，目标定时器需要放到尾结点处
	if (!tmp)
	{
		prev->next = timer;
		timer->prev = prev;
		timer->next = NULL;
		tail = timer;
	}
}
