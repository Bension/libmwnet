#ifndef __REQUEST_MANAGER_H__
#define __REQUEST_MANAGER_H__

#include "CurlRequest.h"
#include <mutex>
#include <map>
#include <list>

namespace curl
{
#define MAX_TOTAL_REQUESTING 	10000
#define MAX_HOST_REQUESTING 	100
#define MAX_WAIT_REQUEST 		50000

// ������
class HttpRequesting
{
public:
	static HttpRequesting& GetInstance()
	{
	    static HttpRequesting instance;
    	return instance;
	}
	~HttpRequesting(){};
public:
	void setMaxSize(int total_host, int single_host);
	CurlRequestPtr find(CURL* c);
	CurlRequestPtr get(CURL* c);
	void add(const CurlRequestPtr& request);
	void remove(const CurlRequestPtr& request);
	bool isFull();
private:
	HttpRequesting();
private:
	// ����ܲ�����
	int max_total_requesting_;
	// ���host������
	int max_host_requesting_;
	// ���������е�����
	std::mutex mutex_requesting_;
	std::map<uint64_t, CurlRequestPtr> requestings_;	
};

// �ȴ�����
class HttpWaitRequest
{
public:
	static HttpWaitRequest& GetInstance()
	{
		static HttpWaitRequest instance;
		return instance;
	}
	~HttpWaitRequest(){};
public:
	void setMaxSize(int max_wait);
	bool isFull();
	bool add(const CurlRequestPtr& request);
	CurlRequestPtr get();
private:
	HttpWaitRequest();
private:
	// ���ȴ�������
	int max_wait_request_;
	// �ȴ����͵�����
	std::mutex mutex_wait_request_;
	std::list<CurlRequestPtr> wait_requests_;
};

// �ѻ��յ�����
class HttpRecycleRequest
{
public:
	static HttpRecycleRequest& GetInstance()
	{
		static HttpRecycleRequest instance;
		return instance;
	}
	~HttpRecycleRequest(){};
public:
	CurlRequestPtr get();
	void recycle(const CurlRequestPtr& request);
private:
	HttpRecycleRequest(){};
private:
	// �ѻ��յ�����
	std::mutex mutex_recycle_request_;
	std::list<CurlRequestPtr> recycle_requests_;
};

} // end namespace curl


#endif // __REQUEST_MANAGER_H__
