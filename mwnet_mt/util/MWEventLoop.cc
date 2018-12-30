#include "MWEventLoop.h"
#include <mwnet_mt/net/EventLoopThread.h>
#include <mwnet_mt/net/EventLoop.h>
#include <mwnet_mt/base/Thread.h>
#include <mwnet_mt/net/TimerId.h>

#include <stdint.h>
#include <stdio.h>

using namespace mwnet_mt;
using namespace mwnet_mt::net;
using namespace MWEVENTLOOP;

typedef std::shared_ptr<EventLoopThread> EventLoopThreadPtr;

namespace MWEVENTLOOP
{
EventLoop::EventLoop(const std::string& loopname)
{
	// ����һ���߳̽�loop�����߳���
	EventLoopThreadPtr pThr(new EventLoopThread(NULL, loopname));
	pThr->startLoop();
	m_any = pThr;
}

EventLoop::~EventLoop()
{
	
}

// ��ѭ��������һ��
void EventLoop::RunOnce(const EVENT_FUNCTION& evfunc)
{
	EventLoopThreadPtr pThr(boost::any_cast<EventLoopThreadPtr>(m_any));
	if (pThr)
	{
		pThr->getloop()->runInLoop(evfunc);
	}
}
/*
// ��ĳһ��ʱ������һ��
void CEventLoop::RunAt()
{
	EventLoop* pLoop = reinterpret_cast<EventLoop*>(m_pLoop);
	pLoop->runAt(const Timestamp & time,TimerCallback && cb);
}
*/
// ��ʱһ��ʱ�������һ��
TIMER_ID EventLoop::RunAfter(double delay, const EVENT_FUNCTION& evfunc)
{
	TimerId timer_id;
	
	EventLoopThreadPtr pThr(boost::any_cast<EventLoopThreadPtr>(m_any));
	if (pThr)
	{
		timer_id = pThr->getloop()->runAfter(delay, evfunc);
	}

	boost::any _any = timer_id;
	
	return _any;
}

// ÿ��һ��ʱ������һ��(��ʱ��)
TIMER_ID EventLoop::RunEvery(double interval, const EVENT_FUNCTION& evfunc)
{
	TimerId timer_id;
	
	EventLoopThreadPtr pThr(boost::any_cast<EventLoopThreadPtr>(m_any));
	if (pThr)
	{
		timer_id = pThr->getloop()->runEvery(interval, evfunc);
	}

	boost::any _any = timer_id;
	
	return _any;
}

// ȡ����ʱ
void EventLoop::CancelTimer(TIMER_ID timerid)
{
	EventLoopThreadPtr pThr(boost::any_cast<EventLoopThreadPtr>(m_any));
	if (pThr)
	{
		pThr->getloop()->cancel(boost::any_cast<TimerId>(timerid));
	}
}

// �˳��¼�ѭ��
void EventLoop::QuitLoop()
{
	EventLoopThreadPtr pThr(boost::any_cast<EventLoopThreadPtr>(m_any));
	if (pThr)
	{
		pThr->getloop()->runInLoop(std::bind(&mwnet_mt::net::EventLoop::quit, pThr->getloop()));
	}	
}

}

