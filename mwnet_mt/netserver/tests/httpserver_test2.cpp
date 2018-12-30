#include "../NetServer.h"

#include <stdint.h>
#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <boost/any.hpp>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <algorithm>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>
#include <memory>
#include <mutex>

#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wformat"


using namespace MWNET_MT::SERVER;


std::shared_ptr<MWNET_MT::SERVER::NetServer> m_pHttpServer;
// std::mutex g_srv_mtx;
// 
// std::shared_ptr<MWNET_MT::SERVER::NetServer> get_srv()
// {
// 	std::lock_guard<std::mutex> lock(g_srv_mtx);
// 	return m_pHttpServer;
// }

//������������ò���
struct NETSERVER_CONFIG
{
	void* pInvoker; 						/*�ϲ���õ���ָ��*/
	uint16_t listenport;					/*�����˿�*/
	int threadnum;							/*�߳���*/
	int idleconntime;						/*�������߿�ʱ��*/
	int maxconnnum; 						/*���������*/
	bool bReusePort;						/*�Ƿ�����SO_REUSEPORT*/
	pfunc_on_connection pOnConnection; 		/*��������ص��¼��ص�(connect/close/error etc.)*/
	pfunc_on_readmsg_tcp pOnReadMsg_tcp;	/*������յ����ݵ��¼��ص�-����tcpserver*/
	pfunc_on_readmsg_http pOnReadMsg_http;	/*������յ����ݵ��¼��ص�-����httpserver*/
	pfunc_on_sendok pOnSendOk; 	   			/*����������ɵ��¼��ص�*/	
	pfunc_on_highwatermark pOnHighWaterMark;/*��ˮλ�¼��ص�*/
	size_t nDefRecvBuf;						/*Ĭ�Ͻ��ջ���,��nMaxRecvBufһ��������ʵҵ�����ݰ���С��������,��ֵ����Ϊ�󲿷�ҵ������ܵĴ�С*/
	size_t nMaxRecvBuf;						/*�����ջ���,�����������ջ�����Ȼ�޷����һ��������ҵ�����ݰ�,���ӽ���ǿ�ƹر�,ȡֵʱ��ؽ��ҵ�����ݰ���С������д*/
	size_t nMaxSendQue; 					/*����ͻ������,��������������̵�δ���ͳ�ȥ�İ�������,��������ֵ,���᷵�ظ�ˮλ�ص�,�ϲ�������pfunc_on_sendok���ƺû���*/

	NETSERVER_CONFIG()
	{
		pInvoker 		= NULL;
		listenport 		= 5958;
		threadnum 		= 16;
		idleconntime	= 60*15;
		maxconnnum		= 1000000;
		bReusePort		= true;
		pOnConnection	= NULL;
		pOnReadMsg_tcp	= NULL;
		pOnReadMsg_http	= NULL;
		pOnSendOk		= NULL;
		nDefRecvBuf		= 4  * 1024;
		nMaxRecvBuf		= 32 * 1024;
		nMaxSendQue		= 64;
	}
};

void stdstring_format(std::string& strRet, const char *fmt, ...)
{
	va_list marker;
	va_start(marker, fmt);
	int nLength = vsnprintf(NULL, 0, fmt, marker);
	va_end(marker);
	if (nLength < 0) return;
	va_start(marker, fmt);
	strRet.resize(nLength + 1);
	int nSize = vsnprintf((char*)strRet.data(), nLength + 1, fmt, marker);
	va_end(marker);
	if (nSize > 0) 
	{
		strRet.resize(nSize);
	}
	else 
	{
		strRet = "";
	}
}


//////////////////////////////////////////////////////�ص�����//////////////////////////////////////////////////////////////

//��������صĻص�(����/�رյ�)
//����ֵ0:����ɹ� >0�����������ִ���򷴿ص���Ϣ(����) <0:�ײ������ǿ�ƹر�����
int func_on_connection_http(void* pInvoker, uint64_t conn_uuid, const boost::any& conn, const boost::any& sessioninfo, bool bConnected, const char* szIpPort)
{
	if (bConnected)
	{
		//printf("httpserver---%d-new link %ld-%s\n", m_pNetServer->GetCurrHttpConnNum(),sockid,szIpPort);
	}
	else
	{	
		//printf("httpserver---%d-link gone %ld-%s\n", m_pNetServer->GetCurrHttpConnNum(),sockid,szIpPort);
	}
	// ���ô���
	return 0;
}

//��ˮλ�ص�
int func_on_highwatermark(void* pInvoker, const uint64_t conn_uuid, const boost::any& conn, const boost::any& sessioninfo, size_t highWaterMark)
{
	printf("link:%ld, highwatermark:%d\n", conn_uuid, highWaterMark);
	// ���ô���
	return 0;
}

//��������ȡ����Ϣ�Ļص�
//����ֵ0:����ɹ� >0�����������ִ���򷴿ص���Ϣ(����) <0:�ײ������ǿ�ƹر�����
//expect:100-continue�ײ���Զ������Զ��ظ������������ϲ�ص����ϲ㲻��Ҫ����
int func_on_readmsg_http(void* pInvoker, uint64_t conn_uuid, const boost::any& conn, const boost::any& sessioninfo, const HttpRequest* pHttpReq)
{
	std::string strUrl = "";
	std::string strBody = "";
	pHttpReq->GetUrl(strUrl);	
	
	HttpResponse rsp;
	
	if (strUrl == "/push_report")
	{
		std::string strBody = "";

		// ȡ��BODY
		pHttpReq->GetBodyData(strBody);
		
		printf("###*****body:%s\n", strBody.c_str());
		// ����JSON

		// ����״̬�������
		// ..............
		
		// ��Ӧ
		rsp.SetKeepAlive(false);
		rsp.SetStatusCode(200);
		rsp.SetContentType("application/json");
		rsp.SetResponseBody("{\"ResCode\":1000,\"ResMsg\":\"�ɹ�\"}");
	}
	else
	{
		printf("######****************bad request:%s\n", strUrl.c_str());
		rsp.SetStatusCode(400);
	}

	boost::any params;
	if (m_pHttpServer)
		m_pHttpServer->SendHttpResponse(conn_uuid, conn, params, rsp);

	return 0;
}

//����������ɵĻص�
//����ֵ0:����ɹ� >0�����������ִ���򷴿ص���Ϣ(����) <0:�ײ������ǿ�ƹر�����
int func_on_sendok_http(void* pInvoker, const uint64_t conn_uuid, const boost::any& conn, const boost::any& sessioninfo, const boost::any& params)
{
	//printf("pfunc_on_sendok_http...%ld\n", sockid);
	// ���ô���
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void* StartHttpServer(void* p)
{
	if (m_pHttpServer)
	{
		m_pHttpServer.reset();
		m_pHttpServer = std::make_shared<MWNET_MT::SERVER::NetServer>(1);
	}
	::prctl(PR_SET_NAME, "RunHttpServer");
	NETSERVER_CONFIG* pConfig = reinterpret_cast<NETSERVER_CONFIG*>(p);
	if (pConfig)
	{
		m_pHttpServer->EnableHttpServerRecvLog();
		m_pHttpServer->EnableHttpServerSendLog();
		m_pHttpServer->StartHttpServer(pConfig->pInvoker, pConfig->listenport,
			pConfig->threadnum, pConfig->idleconntime,
			pConfig->maxconnnum, pConfig->bReusePort,
			pConfig->pOnConnection, pConfig->pOnReadMsg_http, pConfig->pOnSendOk, pConfig->pOnHighWaterMark,
			pConfig->nDefRecvBuf, pConfig->nMaxRecvBuf, pConfig->nMaxSendQue);
	}
	else
	{
		printf("httpserver start failed...\n");
	}
	printf("httpserver quit...dong!!!\n");
	return NULL;
}

int StopHttpServer(void* p)
{
	::prctl(PR_SET_NAME, "RunHttpServer");
	NETSERVER_CONFIG* pConfig = reinterpret_cast<NETSERVER_CONFIG*>(p);
	if (pConfig)
	{
		m_pHttpServer->EnableHttpServerRecvLog(false);
		m_pHttpServer->EnableHttpServerSendLog(false);
		m_pHttpServer->StopHttpServer();
	}
	else
	{
		printf("StopHttpServer start failed...\n");
		return -1;
	}
	printf("StopHttpServer quit...\n");
	return 0;
}


size_t get_executable_path(char* processdir, char* processname, size_t len)
{
    char* path_end = NULL;
    if(::readlink("/proc/self/exe", processdir,len) <=0) return -1;
    path_end = strrchr(processdir,  '/');
    if(path_end == NULL)  return -1;
    ++path_end;
    strcpy(processname, path_end);
    *path_end = '\0';
    return (size_t)(path_end - processdir);
}

#include <chrono>
#include <thread>
#include <memory>
#include <iostream>
#include <atomic>

std::atomic<int> g_srv_cnt(0);
std::atomic<int> g_first_init(0);

void init_srv(NETSERVER_CONFIG* pHttpConfig)
{
	auto t_spr = std::make_shared<std::thread>([=]()
	{
		g_srv_cnt++;
		StartHttpServer(reinterpret_cast<void*>(pHttpConfig));
		std::cout << "xxxxxxxxxxxxx--started!" << std::endl;
		g_srv_cnt--;
	});
	t_spr->detach();
}

void exit_srv(NETSERVER_CONFIG* pHttpConfig)
{
	auto t_spr = std::make_shared<std::thread>([=]()
	{

		StopHttpServer(reinterpret_cast<void*>(pHttpConfig));
		std::cout << "xxxxxxxxxxxxx--stop!!---------!" << std::endl;
	});
	t_spr->detach();
}

int main(int argc, char* argv[])
{
	char path[256] = {0};
    char processname[256] = {0};
    get_executable_path(path, processname, sizeof(path));
		
	::prctl(PR_SET_NAME, processname);

	NETSERVER_CONFIG* pHttpConfig = new NETSERVER_CONFIG();
	// �����˿�
	pHttpConfig->listenport = 8099;
	// �����߳�
	pHttpConfig->threadnum = 4;
	// �߿�
	pHttpConfig->idleconntime = 30;
	//http�ص�����
	pHttpConfig->pOnHighWaterMark = func_on_highwatermark;
	pHttpConfig->pOnConnection = func_on_connection_http;
	pHttpConfig->pOnReadMsg_http = func_on_readmsg_http;
	pHttpConfig->pOnSendOk = func_on_sendok_http;
	pHttpConfig->nDefRecvBuf = 4 * 1024;
	pHttpConfig->nMaxRecvBuf = 8 * 1024;
	pHttpConfig->nMaxSendQue = 512;


	// ���԰�������һ�ο���init������ִ�У�������һ���̣߳�
	{
		
		m_pHttpServer.reset();
		m_pHttpServer = std::make_shared<MWNET_MT::SERVER::NetServer>(1);

		auto t_spr = std::make_shared<std::thread>([=]()
		{
			while (1)
			{
				if (g_srv_cnt <= 0)
				{
					static bool inited(false);

					if (g_first_init > 0)
					{
						std::this_thread::sleep_for(std::chrono::seconds(3));
					}
					init_srv(pHttpConfig);
					// break;
				}
				std::this_thread::sleep_for(std::chrono::seconds(1));

			}
		});
		t_spr->detach();

	}

	while(1)
	{
		static int cnt(1);
		// if (0 == cnt++ % 20)
		if (cnt++ == 20)
		{
			exit_srv(pHttpConfig);
			g_first_init = 1;
		}
		
		std::cout << cnt << std::endl;
		sleep(1);
	}
}
