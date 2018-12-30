#ifndef MWNET_MT_CURL_HTTPCLIENT_H
#define MWNET_MT_CURL_HTTPCLIENT_H

#include <stdint.h>
#include <stdio.h>
#include <boost/any.hpp>
#include <string>
#include <memory>
#include <atomic>

namespace MWNET_MT
{
namespace CURL_HTTPCLIENT
{
//content-type
enum HTTP_CONTENT_TYPE { CONTENT_TYPE_XML, CONTENT_TYPE_JSON, CONTENT_TYPE_URLENCODE, CONTENT_TYPE_UNKNOWN };
//POST/GET ...
enum HTTP_REQUEST_TYPE { HTTP_REQUEST_POST, HTTP_REQUEST_GET, HTTP_REQUEST_UNKNOWN };

class HttpRequest
{
public:
	HttpRequest(const std::string& strUrl,      /*
												����url,�����get����,���ǰ����������ڵ�����������;
											    �����get����,��������url,��:http://ip/domain:port/v1/func
											    */
				   bool bKeepAlive=false, 	    /*�Ƿ񱣳ֳ�����,��:Connection�Զ�׷��Keep-Alive,��:�Զ�׷��Close*/
				   HTTP_REQUEST_TYPE req_type=HTTP_REQUEST_POST);/*ָ���Ƿ�post����get����*/
	~HttpRequest();
public:

	//����HTTP����body����
	//�ڲ��õ���swap���������Բ���û�м�const
	//����ֵ:void
	void	SetBody(std::string& strBody);

	//��������ͷ 
	//���ж��ͷ����Ҫ���ã����ö�μ���
	//����ֵ:void
	void	SetHeader(const std::string& strField, const std::string& strValue);
	
	//����User-Agent  
	//�粻���ã��Զ�ȡĬ��ֵ��Ĭ��ֵΪ:curl�汾��(curl_version()�����ķ���ֵ)
	//Ҳ����ʹ��SetHeader����
	//����ֵ:void
	void	SetUserAgent(const std::string& strUserAgent);

	//����content-type
	//Ҳ����ʹ��SetHeader����
	void	SetContentType(const std::string& strContentType);

	//���ó�ʱʱ��
	//conn_timeout:���ӳ�ʱʱ��
	//entire_timeout:��������ĳ�ʱ��
	void	SetTimeOut(int conn_timeout=2, int entire_timeout=5);

	//��ȡ���������Ωһreq_uuid
	uint64_t GetReqUUID() const;
	
///////////////////////////////////////////////////////////////////////////////////////
	//�����ڲ�����,���ã�
	///////////////////////////////////////////////////
	void	SetContext(const boost::any& context);
	const boost::any& GetContext() const;
///////////////////////////////////////////////////
private:
	boost::any any_;
	bool keep_alive_;
	HTTP_REQUEST_TYPE req_type_;
};

class HttpResponse
{
public:
	HttpResponse(int nRspCode,int nErrCode,
					const std::string& strErrDesc, 
					const std::string& strContentType, 
					std::string& strHeader, 
					std::string& strBody, 
					double total, double connect, double namelookup_time);
	~HttpResponse();
public:
	//��ȡ״̬�룬200��400��404 etc.
	int    GetRspCode() const;

	//��ȡ�������
	int	   GetErrCode() const;

	//��ȡ��������
	void   GetErrDesc(std::string& strErrDesc);
	
	//��ȡconetnettype���磺text/json��application/x-www-form-urlencoded��text/xml
	void   GetContentType(std::string& strContentType);

	//��ȡ��Ӧ���İ�ͷ
	void   GetHeader(std::string& strHeader);

	//��ȡ��Ӧ���İ���
	void   GetBody(std::string& strBody);

	//��ȡ��ʱ
	void   GetTimeConsuming(double& total, double& connect, double& namelookup);
private:
	int m_nRspCode;
	int m_nErrCode;
	std::string m_strErrDesc;
	std::string m_strContentType;
	std::string m_strHeader;
	std::string m_strBody;
	double total_time_;
	double connect_time_;
	double namelookup_time_;
	//����ʱ���
	//.....
};

typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
typedef std::shared_ptr<HttpResponse> HttpResponsePtr;

/**************************************************************************************
* Function      : pfunc_onmsg_cb
* Description   : �ص����������Ӧ��ص��¼��ص�(��Ϣ��Ӧ����ʱ)
* Input         : [IN] void* pInvoker						�ϲ���õ���ָ��
* Input         : [IN] uint64_t req_uuid					SendHttpRequest�����Ĳ���HttpRequestPtr�ĳ�Ա����GetReqUUID��ֵ
* Input         : [IN] const boost::any& params				SendHttpRequest�����Ĳ���params
* Input         : [IN] const HttpResponsePtr& response		HttpResponse��Ӧ��
**************************************************************************************/
typedef int(*pfunc_onmsg_cb)(void* pInvoker, uint64_t req_uuid, const boost::any& params, const HttpResponsePtr& response);

class CurlHttpClient
{
public:
	CurlHttpClient();
	~CurlHttpClient();

public:
	//��ʼ����������curlhttpclient���������ڣ�ֻ�ɵ���һ��
	bool InitHttpClient(void* pInvoker,				/*�ϲ���õ���ָ��*/
						pfunc_onmsg_cb pFnOnMsg, 	/*���Ӧ��ص��¼��ص�(recv)*/
						bool enable_ssl=false,		/*�Ƿ����ssl.�粻��Ҫhttps,���Բ�����*/
						int nIoThrNum=4,			/*IO�����߳���*/
						int nMaxReqQueSize=50000,   /*����������ֵ,������ֵ�ᱻ����*/
						int nMaxTotalWaitRsp=1000,	/*����������Ӧ��������,�������������彫������*/
						int nMaxHostWaitRsp=100,	/*����host����������Ӧ������,������������host��������*/
						int nMaxTotalConns=0,		/*curl multi������CURLMOPT_MAX_TOTAL_CONNECTIONS ��ȡĬ��ֵ,��������*/
						int nMaxHostConns=0,		/*curl multi������CURLMOPT_MAX_HOST_CONNECTIONS ��ȡĬ��ֵ,��������*/	
						int nMaxConns=0);			/*curl multi������CURLMOPT_MAXCONNECTS ��ȡĬ��ֵ,��������*/

	//��ȡHttpRequest
	HttpRequestPtr GetHttpRequest(const std::string& strUrl,     	/*
																	����url,�����get����,���ǰ����������ڵ�����������;
											    					�����get����,��������url,��:http://ip/domain:port/v1/func
											    					*/
				   					bool bKeepAlive=false, 	   	    /*�Ƿ񱣳ֳ�����,��:Connection�Զ�׷��Keep-Alive,��:�Զ�׷��Close*/
				   					HTTP_REQUEST_TYPE req_type=HTTP_REQUEST_POST);/*ָ���Ƿ�post����get����*/

	//����http����
	int  SendHttpRequest(const boost::any& params, 			/*ÿ������Я������������,�ص�����ʱ�᷵��*/
							const HttpRequestPtr& request);	/*������http��������,ÿ������ǰ����GetHttpRequest,�����*/
	
	// ��ȡ����Ӧ����
	int  GetWaitRspCnt();

	void ExitHttpClient();
private:
	void Start();
	void DoneCallBack(const boost::any& _any, int errCode, const char* errDesc, double total_time, double connect_time, double namelookup_time);	
private:
	void* m_pInvoker;
	// �ص�����
	pfunc_onmsg_cb m_pfunc_onmsg_cb;
	// mainloop
	//boost::any main_loop_;
	// ioloop
	boost::any io_loop_;
	// latch
	boost::any latch_;
	// httpclis
	std::vector<boost::any> httpclis_;
	// ��������
	std::atomic<uint64_t> total_req_;
	// ����Ӧ������
	std::atomic<int> wait_rsp_;
	int MaxTotalConns_;
	int MaxHostConns_;
	int MaxConns_;
	int MaxTotalWaitRsp_;
	int MaxHostWaitRsp_;
	int IoThrNum_;
	int MaxReqQueSize_;
};

}
}

#endif
