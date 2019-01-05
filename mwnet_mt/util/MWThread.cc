#include "MWThread.h"
#include <mwnet_mt/base/Thread.h>

using namespace mwnet_mt;
using namespace MWTHREAD;

typedef std::shared_ptr<mwnet_mt::Thread> ThreadPtr;

namespace MWTHREAD
{
Thread::Thread(const THREAD_FUNC& thrfunc, const std::string& thrname)
{
	// ����һ���߳�
	ThreadPtr pThr(new mwnet_mt::Thread(thrfunc, thrname));
	m_any = pThr;
}

Thread::~Thread()
{
	
}

// �����ɹ������߳�ID,����ʧ�ܷ���0
pid_t Thread::StartThread()
{
	pid_t t = 0;
	ThreadPtr pThr(boost::any_cast<ThreadPtr>(m_any));
	if (pThr)
	{
		pThr->start();
		if (pThr->started())
		{
			t = pThr->tid();
		}
	}
	return t;
}

// return pthread_join()
int Thread::JoinThread()
{
	int nRet = -1;
	ThreadPtr pThr(boost::any_cast<ThreadPtr>(m_any));
	if (pThr && pThr->started())
	{ 
		nRet = pThr->join();
	}
	return nRet;
}

// �����߳�����
const char* Thread::ThreadName()
{
	ThreadPtr pThr(boost::any_cast<ThreadPtr>(m_any));
	if (pThr)
	{
		return pThr->name().c_str();
	}
	return "";
}

}
