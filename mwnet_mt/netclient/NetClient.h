#ifndef MWNET_MT_NETCLIENT_H
#define MWNET_MT_NETCLIENT_H

#include <stdint.h>
#include <stdio.h>
#include <boost/any.hpp>
#include <string>
#include <sstream>
#include <memory>
#include <mutex>
#include <utility>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <vector>
#include <list>

namespace MWNET_MT
{
namespace CLIENT
{
//content-type
enum HTTP_CONTENT_TYPE { CONTENT_TYPE_XML=1, CONTENT_TYPE_JSON, CONTENT_TYPE_URLENCODE, CONTENT_TYPE_UNKNOWN };
//POST/GET ...
enum HTTP_REQUEST_TYPE { HTTP_REQUEST_POST=1, HTTP_REQUEST_GET, HTTP_REQUEST_UNKNOWN };

class HttpRequest
{
public:
	HttpRequest();
	HttpRequest(const std::string& strReqIp, int nReqPort = 80);
	~HttpRequest();
public:
	//�Ƿ���Ҫ��������,��connection��keep-alive����close
	//����ֵ:void
	void	SetKeepAlive(bool bKeepLive);

	bool    GetKeepAlive() const;

	//����post��������
	//����ֵ:void
	void	SetContent(const std::string& strContent);

	//��������url
	//void
	//˵��:��:post����,POST /sms/std/single_send HTTP/1.1 ,/sms/std/single_send����URL
	//	   ��:get����,
	void	SetUrl(const std::string& strUrl);

	//��������ͻ��˵�IP
	//����ֵ:����ͻ��˵�IP
	void	SetRequestIp(const std::string& strIp);

	//��������ͻ��˵Ķ˿�
	//����ֵ:void
	void	SetRequestPort(int nPort);

	//����User-Agent
	//����ֵ:void
	void	SetUserAgent(const std::string& strUserAgent);

	//����content-type
	//����ֵ:CONTENT_TYPE_XML ���� xml,CONTENT_TYPE_JSON ���� json,CONTENT_TYPE_URLENCODE ���� urlencode,CONTENT_TYPE_UNKNOWN ���� δ֪����
	//˵��:��ֻ�ṩ������
	void	SetContentType(int nContentType);

	//����host
	//void
	void	SetHost(const std::string& strHost);

	//����http���������
	//ֵ:HTTP_REQUEST_POST ���� post����; HTTP_REQUEST_GET ���� get���� ;
	void	SetRequestType(int nRequestType);

	int		GetTimeOut()const;

	//���ó�ʱ
	void	SetTimeOut(int nTimeOut);

public:
	//��ȡ����ͻ��˵�IP
	//����ֵ:void
	void GetRequestIp(std::string& strIp) const;

	//��ȡ����ͻ��˵Ķ˿�
	//����ֵ:int
	int	GetRequestPort(void) const;

	void GetRequestStr(std::string& strRequestStr) const;

	void GetContentTypeStr(int nContentType, std::string& strContentType) const;

private:
	std::string m_strIp;
	int m_nPort;
	bool m_bKeepLive;
	int m_nContentType;
	std::string m_strContent;
	std::string m_strUrl;
	std::string m_strUserAgent;
	std::string m_strHost;
	int m_nRequestType;
	int m_nTimeOut;
};

class HttpResponse
{
public:
	HttpResponse();
	~HttpResponse();
public:
	//��ȡ״̬�룬200��400��404 etc.
	int GetStatusCode(void) const;

	//��ȡconetnettype���磺text/json��application/x-www-form-urlencoded��text/xml
	void GetContentType(std::string& strContentType) const;

	//���������Ƿ񱣳֣�bKeepAlive=1����:Keep-Alive��bKeepAlive=0������:Close
	bool GetKeepAlive() const;

	//��ȡ��Ӧ���İ���
	size_t GetResponseBody(std::string& strRspBody) const;

public:
	//.......................................................
	//!!!���ĸ������ϲ�����ֹ����!!!

	int		ParseHttpRequest(const char* data, size_t len, bool& over);

	//���ý�����
	void	ResetParser();

	//�����ѽ��յ�������
	void	SaveIncompleteData(const char* pData, size_t nLen);

	//����һЩ���Ʊ�־
	void	ResetSomeCtrlFlag();

	//����������ǰ�������ֽ���
	void	IncCurrTotalParseSize(size_t nSize);

	//��ȡ��ǰ�ѽ������ֽ���
	size_t	GetCurrTotalParseSize() const;

	//���õ�ǰ�ѽ������ֽ���
	void	ResetCurrTotalParseSize();

	//��ȡָ��
	void*	GetParserPtr();

	void GetContentTypeStr(int nContentType, std::string& strContentType) const;
	//.......................................................
private:
	void* pParser;
	size_t m_nCurrTotalParseSize;

	bool m_bIncomplete;
	std::string m_strRequest;
	int m_nDefErrCode; //Ĭ�ϴ������
};

/**************************************************************************************
* Function      : pfunc_on_msg_httpclient
* Description   : �ص����������Ӧ��ص��¼��ص�(��Ϣ��Ӧ����ʱ)
* Input         : [IN] void* pInvoker						�ϲ���õ���ָ��
* Input         : [IN] uint64_t req_uuid					�ϲ���ûỰ����ID
* Input         : [IN] const boost::any& params				�ϲ���ô������
* Input         : [IN] const HttpResponse& httpResponse		HttpResponse��Ӧ��
* Output        :
* Return        :
* Author        : Zhengbs     Version : 1.0    Date: 2018��01��15��
* Modify        : ��
* Others        : ��
**************************************************************************************/
typedef int(*pfunc_on_msg_httpclient)(void* pInvoker, uint64_t req_uuid, const boost::any& params, const HttpResponse& httpResponse);


//��������ȡ����Ϣ�Ļص�(������tcpclient)
//����ֵ0:����ɹ� >0�����������ִ���򷴿ص���Ϣ(����) <0:�ײ������ǿ�ƹر�����
//nReadedLen���ڿ��Ʋа�ճ��������ǲа�:������0�������ճ��:�������������Ĵ�С*�������ĸ���
typedef int(*pfunc_on_readmsg_tcpclient)(void* pInvoker, uint64_t conn_uuid, const boost::any& param, const char* szMsg, int nMsgLen, int& nReadedLen);

typedef int(*pfunc_on_connection)(void* pInvoker, uint64_t conn_uuid, const boost::any& param, bool bConnected, const char* szIpPort);

typedef int(*pfunc_on_sendok)(void* pInvoker, const uint64_t conn_uuid, const boost::any& param);

typedef int(*pfunc_on_highwatermark)(void* pInvoker, const uint64_t conn_uuid, size_t highWaterMark);

typedef int(*pfunc_on_heartbeat)(uint64_t conn_uuid);

class tag_cli_mgr;
class HttpClientManager;
typedef std::shared_ptr<HttpClientManager> HttpClientManagerPtr;
class NetClient
	: public std::enable_shared_from_this<NetClient>
{
public:
	NetClient();
	~NetClient();

	// ����ʹ�õ�����Ҳ���Բ�ʹ�ã��粻ʹ��ֱ��net NetClient();
	static NetClient& Instance()
	{
		static auto instance_spr = std::make_shared<NetClient>();
		return *instance_spr.get();
	}

public:
	bool InitHttpClient(void* pInvoker,			/*�ϲ���õ���ָ��*/
						int nThreadNum,			/*�߳���*/
						int idleconntime,		/*�������߿�ʱ��*/
						int nMaxHttpClient,		/*��󲢷���*/
						pfunc_on_msg_httpclient pFnOnMessage 	/*���Ӧ��ص��¼��ص�(recv)*/,
						size_t nDefRecvBuf,		//�ײ���ջ��������õĴ�С
						size_t nMaxRecvBuf,		//�ײ���ջ��������Ĵ�С
						size_t nMaxSendQue);	//�ײ㷢���������������ݰ�����
	int  SendHttpRequest(uint64_t req_uuid, const boost::any& params, const HttpRequest& httpRequest);
	void ExitHttpClient();

public:
	bool InitTcpClient(void* pInvoker,			/*�ϲ���õ���ָ��*/
		int nThreadNum,			/*�߳���*/
		int idleconntime,		/*�������߿�ʱ��*/
		int nMaxTcpClient,		/*��󲢷���*/
		std::string ip,
		uint16_t  port,
		pfunc_on_connection pFnOnConnect,
		pfunc_on_sendok pFnOnSendOk,
		pfunc_on_readmsg_tcpclient pFnOnMessage, 	/*���Ӧ��ص��¼��ص�(recv)*/
		size_t nDefRecvBuf,// = 4 * 1024,		/*Ĭ�Ͻ���buf*/
		size_t nMaxRecvBuf,// = 4 * 1024,		/*������buf*/
		size_t nMaxSendQue,// = 512			/*����Ͷ���*/
		uint64_t& cli_uuid, /*���ص�uuid*/
		size_t nMaxWnd /*��󴰿ڴ�С*/
	);
	int  SendTcpRequest(uint64_t cli_uuid, uint64_t cnn_uuid/*cnn_uuid = 0ʱ�� ϵͳ�Զ�ѡһ�����ӷ���*/, 
		const boost::any& params, const char* szMsg, size_t nMsgLen);
	void ExitTcpClient(uint64_t cli_uuid);
	void SetHeartbeat(pfunc_on_heartbeat pFnOnHeartbeat, double interval = 10.0);

protected:
	bool InitTcpClient_bak(void* pInvoker,			/*�ϲ���õ���ָ��*/
		int nThreadNum,			/*�߳���*/
		int idleconntime,		/*�������߿�ʱ��*/
		int nMaxTcpClient,		/*��󲢷���*/
		std::string ip,
		uint16_t  port,
		pfunc_on_connection pFnOnConnect,
		pfunc_on_sendok pFnOnSendOk,
		pfunc_on_readmsg_tcpclient pFnOnMessage, 	/*���Ӧ��ص��¼��ص�(recv)*/
		size_t nDefRecvBuf,// = 4 * 1024,		/*Ĭ�Ͻ���buf*/
		size_t nMaxRecvBuf,// = 4 * 1024,		/*������buf*/
		size_t nMaxSendQue,// = 512			/*����Ͷ���*/
		uint64_t& cli_uuid, /*���ص�uuid*/
		size_t nMaxWnd /*��󴰿ڴ�С*/
	);
	int  SendTcpRequest_bak(uint64_t cli_uuid, uint64_t cnn_uuid/*cnn_uuid = 0ʱ�� ϵͳ�Զ�ѡһ�����ӷ���*/,
		const boost::any& params, const char* szMsg, size_t nMsgLen);
	void ExitTcpClient_bak(uint64_t cli_uuid);


public:
	std::shared_ptr<tag_cli_mgr> m_climgr;
	std::mutex m_cli_lock;
private:
	HttpClientManagerPtr m_httpClientPtr;
	boost::any m_timerid_heartbeat;
	
};


//�ص��ӿ�
typedef void(*CallBackRecv)(const void* /* pInvokePtr */, \
	int /* nLinkId */, \
	int /* command */, \
	int /* ret */, \
	const char* /* data */, \
	size_t /* len */, \
	int& /* nReadedLen */);
//����
struct Params
{
	Params()
		: nWnd(512)
		, nDefBuf(4 * 1024)
		, nMaxBuf(4 * 1024 * 1024)
		, nMaxQue(512)
		, bAutoRecon(true)
		, nAutoReconTime(10)
		, nThreads(1)
		, bMayUseBlockOpration(true)
		, nClients(1)
	{
	}
	int nWnd;//��������
	int nDefBuf;	// Ĭ�ϻ���
	int nMaxBuf;	//��󻺳�
	int nMaxQue;	// ����Ͷ���
	bool bAutoRecon;//�Ƿ��Զ�����
	int nAutoReconTime;//�Զ��������(s)
	int nThreads;//�̸߳���
	bool bMayUseBlockOpration;	// �Ƿ���õ���������
	int nClients;	// �ͻ��˸���
};

class MwCli;
class XTcpClient
{
public:
	XTcpClient();
	~XTcpClient() {};

	static XTcpClient& Instance()
	{
		static XTcpClient instance;
		return instance;
	}
public:
	//��ʼ���ӿ�
	int Init(const void* pInvokePtr, const Params& params, CallBackRecv cb);
	//����ʼ���ӿ�
	int UnInit();
	//����ʱ���ò���
	void SetRunParam(const Params& params);
	//��������, > 0 nLinkId, ����ʧ��, moder��0�첽,1ͬ����, 
	int ApplyNewLink(const std::string ip, int port, int moder, int ms_timeout);
	//�ر�����
	int CloseLink(int nLinkId);
	//��������
	int ReConnLink(int nLinkId);
	//����״̬������ӿڲ�̫���ף���Ҫ�Լ�����CallBackRecv�ж�
	bool LinkState(int nLinkId);
	//��ȡĳ�����ӵĵ�ǰ�����С
	size_t GetWaitBufSize(int nLinkId);
	//��ȡ���л����С
	size_t GetAllWaitBufSize();
	//��ȡ��ǰ��ʹ�ö��ٴ���
	size_t GetUseWnd(int nLinkId);
	//���ͽӿ�
	int Send(int nLinkId, const char* data, size_t len);

private:
	const void* m_pInvokePtr;
	const Params m_params;
	std::shared_ptr<MwCli> m_mwcli_spr;
};


}
}

#endif