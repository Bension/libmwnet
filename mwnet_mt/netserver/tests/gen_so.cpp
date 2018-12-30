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

#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wformat"


using namespace MWNET_MT::SERVER;


MWNET_MT::SERVER::NetServer* m_pHttpServer = NULL;

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
		bReusePort		= false;
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
	::prctl(PR_SET_NAME,"RunHttpServer");
	NETSERVER_CONFIG* pConfig = reinterpret_cast<NETSERVER_CONFIG*>(p);
	if (pConfig)
	{
		m_pHttpServer->EnableHttpServerRecvLog();
		m_pHttpServer->EnableHttpServerSendLog();
		printf("httpserver:%u start success...\n",pConfig->listenport);
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
	printf("httpserver quit...\n");
	return NULL;
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

int test_so()
{
/*
	char path[256] = {0};
    char processname[256] = {0};
    get_executable_path(path, processname, sizeof(path));
		
	::prctl(PR_SET_NAME, processname);
*/
// ���԰�������һ�ο���init������ִ�У�������һ���̣߳�
{
	NETSERVER_CONFIG* pHttpConfig = new NETSERVER_CONFIG();
	// �����˿�
	pHttpConfig->listenport = 8099;
	// �����߳�
	pHttpConfig->threadnum  = 4;
	// �߿�
	pHttpConfig->idleconntime = 30;
	//http�ص�����
	pHttpConfig->pOnHighWaterMark = func_on_highwatermark;
	pHttpConfig->pOnConnection	= func_on_connection_http;
	pHttpConfig->pOnReadMsg_http= func_on_readmsg_http;
	pHttpConfig->pOnSendOk		= func_on_sendok_http;
	pHttpConfig->nDefRecvBuf	= 4 * 1024;
	pHttpConfig->nMaxRecvBuf	= 8 * 1024;
	pHttpConfig->nMaxSendQue	= 512;
	
	m_pHttpServer = new MWNET_MT::SERVER::NetServer(1);

	//����httpserver	
	pthread_t ntid;	
	int err = pthread_create(&ntid, NULL, StartHttpServer, reinterpret_cast<void*>(pHttpConfig));
	if (err != 0) printf("StartHttp can't create thread: %s\n", strerror(err));
	pthread_detach(ntid);
}
	/*
	while(1)
	{
		sleep(1);
	}
	*/
}
