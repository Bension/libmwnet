#include "CurlManager.h"
#include "RequestManager.h"
#include <mwnet_mt/base/Atomic.h>
#include <mwnet_mt/base/Logging.h>
#include <assert.h>
#include <signal.h>
//#include <sys/poll.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/cdefs.h>

using namespace curl;
// ������Ϣ��ͳ����
mwnet_mt::AtomicUint64 totalCount_;

void CurlManager::initialize(Option opt)
{
	curl_global_init(opt == kCURLnossl ? CURL_GLOBAL_NOTHING : CURL_GLOBAL_SSL);
	//curl_global_init(CURL_GLOBAL_ALL);
}

void CurlManager::uninitialize()
{
	curl_global_cleanup();
}

int createEventfd()
{
	int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (evtfd < 0)
	{
		LOG_SYSERR << "Failed in eventfd";
		abort();
	}
	return evtfd;
}

CurlManager::CurlManager(long maxtotalconns, long maxhostconns, long maxconns)
	: wakeupFd_(createEventfd()),
	  curlm_(CHECK_NOTNULL(curl_multi_init())),
	  runningHandles_(0),
	  prevRunningHandles_(0),
	  stopped_(false),
	  MaxTotalConns_(maxtotalconns),
	  MaxHostConns_(maxhostconns),
	  MaxConns_(maxconns)
{
	curl_multi_setopt(curlm_, CURLMOPT_SOCKETFUNCTION, &CurlManager::curlmSocketOptCb);
	curl_multi_setopt(curlm_, CURLMOPT_SOCKETDATA, this);
	curl_multi_setopt(curlm_, CURLMOPT_TIMERFUNCTION, &CurlManager::curlmTimerCb);
	curl_multi_setopt(curlm_, CURLMOPT_TIMERDATA, this);

	// curl 7.30���ϰ汾��֧��
	if (maxtotalconns > 0) curl_multi_setopt(curlm_, CURLMOPT_MAX_TOTAL_CONNECTIONS, maxtotalconns);
	if (maxhostconns > 0) curl_multi_setopt(curlm_, CURLMOPT_MAX_HOST_CONNECTIONS, maxhostconns);
	if (maxconns > 0) curl_multi_setopt(curlm_, CURLMOPT_MAXCONNECTS, maxconns);
}

CurlManager::~CurlManager()
{
	curl_multi_cleanup(curlm_);
}

int CurlManager::runCurlEvLoop(const std::shared_ptr<CountDownLatch>& latch)
{
	int nRet = -1;

	// ��ʼ��evbase
	if(NULL != (evInfo_.evbase = event_base_new()))
	{
		// ע�ỽ���¼��������¼�ѭ��
		nRet = event_assign(&evInfo_.wakeup_event, evInfo_.evbase, wakeupFd_, EV_READ|EV_PERSIST, &CurlManager::onRecvWakeUpNotify, this);
		if (0 != nRet) 
		{
			LOG_INFO << "event_assign wakeup event error:" << nRet;
			return nRet;
		}
		nRet = event_add(&evInfo_.wakeup_event, NULL);
		if (0 != nRet) 
		{
			LOG_INFO << "event_add wakeup event error:" << nRet;
			return nRet;
		}

		// ע��һ����ʱ�¼�,����curl�ĳ�ʱ����
		nRet = evtimer_assign(&evInfo_.timer_event, evInfo_.evbase, &CurlManager::onRecvEvTimerNotify, this);
		if (0 != nRet) 
		{
			LOG_INFO << "evtimer_assign timer event error:" << nRet;
			return nRet;
		}
		
		latch->countDown();
		
		// �����¼�ѭ��(������....������event_base_loopbreak() or event_base_loopexit() ʱ�ŷ���)
		nRet = event_base_dispatch(evInfo_.evbase);
		if (0 != nRet) 
		{
			LOG_INFO << "event_base_dispatch return:" << nRet;
		}
		
		// ѭ���˳�ʱ����evloop
		event_del(&evInfo_.wakeup_event);
		close(wakeupFd_);
		event_del(&evInfo_.timer_event);
		event_base_free(evInfo_.evbase);
	}

	return nRet;
}

int CurlManager::curlmSocketOptCb(CURL* c, int fd, int what, void* userp, void* socketp)
{
	LOG_DEBUG << "CurlManager::curlmSocketOptCb";

	if (userp)
	{
		CurlManager* cm = static_cast<CurlManager*>(userp);
		cm->handleCurlmSocketOptCb(c, fd, what, socketp);
	}
	
	return 0;
}

void CurlManager::handleCurlmSocketOptCb(CURL* c, int fd, int what, void* socketp)
{
	const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };
	LOG_DEBUG << "CurlManager::curlm_opt_socket_callback_inloop [" 
			  << this << "] [" << socketp << "]- fd = " << fd
			  << " what = " << whatstr[what];
	
	SockInfo* sinfo = static_cast<SockInfo*>(socketp);

	if (what == CURL_POLL_REMOVE && !sinfo)
	{
		remsock(sinfo);
	}
	else
	{
		if (!sinfo)
		{
			addsock(fd, c, what, &evInfo_);
		}
		else
		{
			setsock(sinfo, fd, c, what, &evInfo_);
		}
	}

}

void CurlManager::addsock(int fd, CURL* c, int action, EvLoopInfo* evInfo)
{
	SockInfo* sinfo = static_cast<SockInfo*>(malloc(sizeof(SockInfo)));
	if(!sinfo)
	{
		LOG_ERROR << "SockInfo* sinfo = (SockInfo *)malloc(sizeof(SockInfo)) FAIL!";
		return;
	}
	
	memset(sinfo, 0, sizeof(SockInfo));
	
	//sinfo->evInfo = evInfo;
	
	setsock(sinfo, fd, c, action, evInfo);
	
	curl_multi_assign(curlm_, fd, sinfo);
}

/* Assign information to a SockInfo structure */
void CurlManager::setsock(SockInfo* sinfo, int fd, CURL* c, int action, EvLoopInfo* evInfo)
{
	short evtp = static_cast<short>((action&CURL_POLL_IN?EV_READ:0)|(action&CURL_POLL_OUT?EV_WRITE:0)|EV_PERSIST);
	
	sinfo->fd = fd;
	sinfo->action = action;
	sinfo->easy = c;
	
	event_del(&sinfo->ev);
	event_assign(&sinfo->ev, evInfo->evbase, fd, evtp, onRecvEvOptNotify, this);
	event_add(&sinfo->ev, NULL);
}

/* Clean up the SockInfo structure */
void CurlManager::remsock(SockInfo* sinfo)
{
	if (sinfo)
	{
		event_del(&sinfo->ev);
		free(sinfo);
	}
}

int CurlManager::curlmTimerCb(CURLM* curlm, long ms, void* userp)
{
	LOG_DEBUG << "CurlManager::curlmTimerCb";
	
	if (userp)
	{
		CurlManager* cm = static_cast<CurlManager*>(userp);
		cm->handleCurlmTimerCb(ms);
	}
	return 0;
}

void CurlManager::handleCurlmTimerCb(long ms)
{
	struct timeval timeout;
	timeout.tv_sec = ms / 1000;
	timeout.tv_usec = (ms % 1000)*1000;
	
	if (ms == 0)
	{
		curl_multi_socket_action(curlm_, CURL_SOCKET_TIMEOUT, 0, &runningHandles_);
	}
	else if (ms == -1)
	{
		evtimer_del(&evInfo_.timer_event);
	}
	else
	{
		evtimer_add(&evInfo_.timer_event, &timeout);
	}
}

void CurlManager::onRecvEvTimerNotify(int fd, short evtp, void *arg)
{
	if (arg)
	{
		CurlManager* cm = static_cast<CurlManager*>(arg);
		cm->handleEvTimerCb();
	}
}

void CurlManager::handleEvTimerCb()
{
	curl_multi_socket_action(curlm_, CURL_SOCKET_TIMEOUT, 0, &runningHandles_);
	check_multi_info();
}

void CurlManager::onRecvEvOptNotify(int fd, short evtp, void *arg)
{
	if (arg)
	{
		CurlManager* cm = static_cast<CurlManager*>(arg);
		cm->handleEvOptCb(fd, evtp);
	}
}

void CurlManager::handleEvOptCb(int fd, short evtp)
{
	int action = (evtp & EV_READ ? CURL_POLL_IN : 0) | (evtp & EV_WRITE ? CURL_POLL_OUT : 0);

	curl_multi_socket_action(curlm_, fd, action, &runningHandles_);

	check_multi_info();
	
	if (runningHandles_ <= 0)
	{
		if (evtimer_pending(&evInfo_.timer_event, NULL))
		{
			evtimer_del(&evInfo_.timer_event);
		}
	}
}

CurlRequestPtr CurlManager::getRequest(const std::string& url, bool bKeepAlive, int req_type)
{
	uint64_t req_uuid = totalCount_.incrementAndGet();
	
	CurlRequestPtr p = HttpRecycleRequest::GetInstance().get();
	
	if (nullptr == p)
	{
		p.reset(new CurlRequest(url, req_uuid, bKeepAlive, req_type));
	}
	else
	{
		p->initCurlRequest(url, req_uuid, bKeepAlive, req_type);
	}

	LOG_DEBUG << "getRequest requuid:" << req_uuid;
		
	return p;
}

void CurlManager::notifySendRequest()
{
	uint64_t one = 1;
	write(wakeupFd_, &one, sizeof one);
}

void CurlManager::onRecvWakeUpNotify(int fd, short evtp, void *arg)
{
	if (arg)
	{
		CurlManager* cm = static_cast<CurlManager*>(arg);
		cm->handleWakeUpCb();
	}
}

void CurlManager::handleWakeUpCb()
{	
	uint64_t one = 1;
	read(wakeupFd_, &one, sizeof one);

	// ���Ѻ󣬴Ӵ���������ȡ���ݷ���
	checkNeedSendRequest();
}

// ��������
int CurlManager::sendRequest(const CurlRequestPtr& request)
{
	int nRet = 0;

	// �����������
	if (!HttpWaitRequest::GetInstance().add(request))
	{
		nRet = 1;
	}
	
	// ֪ͨ����
	notifySendRequest();

	return nRet;
}

void CurlManager::sendRequestInLoop(const CurlRequestPtr& request)
{
	// �ȼ��뷢����
	HttpRequesting::GetInstance().add(request);
	// ��������
	request->request(curlm_);
}

void CurlManager::check_multi_info()
{
	LOG_DEBUG << "check_multi_info::prevRunningHandles_ = " << prevRunningHandles_ << ", runningHandles_ = " << runningHandles_;
	if (prevRunningHandles_ > runningHandles_ || runningHandles_ == 0)
	{
		CURLMsg* msg = NULL;
		int left = 0;
		while ((msg = curl_multi_info_read(curlm_, &left)) != NULL)
		{
			if (msg->msg == CURLMSG_DONE)
			{
				CURL* c = msg->easy_handle;
				CURLcode retCode = msg->data.result;
				
				CurlRequestPtr request = HttpRequesting::GetInstance().find(c);
				if (request && request->getCurl() == c)
				{
					LOG_DEBUG <<"check_multi_info::" << request.get() << " ========>>>>done:" << curl_easy_strerror(retCode);
					request->done(retCode, curl_easy_strerror(retCode));

					afterRequestDone(request);
				}
			}
		}
		
		// ����Ƿ�����Ҫ���͵�����
		checkNeedSendRequest();
	}
	
	prevRunningHandles_ = runningHandles_;

	// û���ڵȴ�������¼�ʱ�ſ����˳�
	if (runningHandles_ <= 0 && stopped_)
	{
		event_base_loopbreak(evInfo_.evbase);
	}
}


void CurlManager::afterRequestDone(const CurlRequestPtr& request)
{
	// �������Ƴ�������
	removeMultiHandle(request);
	// ��������,����������ǻ������û���ֱ���ͷ�
	recycleRequest(request);
}

// �������Ƴ�������
void CurlManager::removeMultiHandle(const CurlRequestPtr& request)
{
	request->removeMultiHandle(curlm_);
}

// ��������,����������ǻ������û���ֱ���ͷ�
void CurlManager::recycleRequest(const CurlRequestPtr& request)
{
	// ������ӷ����ж����Ƴ�
	HttpRequesting::GetInstance().remove(request);

	// ��������
	HttpRecycleRequest::GetInstance().recycle(request);
	// �����ӣ��������գ������ã���
	//.............
}

// ����Ƿ�����Ҫ���͵�����
void CurlManager::checkNeedSendRequest()
{
	CurlRequestPtr request = nullptr;
	// ����Ƿ�����Ҫ���͵�����....
	while (!HttpRequesting::GetInstance().isFull() 
		&& (request=HttpWaitRequest::GetInstance().get()) != nullptr)
	{
		sendRequestInLoop(request);
	}
}
