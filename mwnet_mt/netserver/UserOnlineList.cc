#include "UserOnlineList.h"
#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <list>
#include <atomic>
#include <mutex>

namespace MWNET_MT
{
namespace SERVER
{

#ifndef CHECK_CMD_VALUE_INVALID
#define CHECK_CMD_VALUE_INVALID(cmd,maxcmdcnt) {\
			(cmd<0||cmd>=maxcmdcnt)?cmd=0:1;\
		}
#endif

UserOnlineList::UserOnlineList(int maxcmdcnt/*����ж��ٸ�����*/)
	: maxcmdcnt_(maxcmdcnt)
{
}

UserOnlineList::~UserOnlineList()
{
}

// ���������б������ڸ��û����˻�Ӧ���Ժ󣬲����յ������sendok�Ļص��󣬲ſ��Ե��ã�
// useruuid:ָ����ҵ������Ωһ��ʶһ���û���ݵ�ID
// userinfo:����Ϊany����,���ڱ����������͵��û���Ϣ.���Խ�ҵ�����û���Ϣ������ָ����������б�
// useruuid,userinfo��Ҫ��ɽṹ�������ӽ����󣬵���settcpsession���������䱣��
// conn_uuid,connΪ��������ڱ�ʶһ��������ݵ���������
// ����ֵ,0:�ɹ�,��0:ʧ��
int UserOnlineList::AddToOnlineList(uint64_t useruuid, const boost::any& userinfo, uint64_t conn_uuid, const boost::any& conn, const std::string& ipport)
{
	int ret = 0;

	MWTIMESTAMP::Timestamp tNow = MWTIMESTAMP::Timestamp::Now();

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	//�û�,���ڸ���
	if (itUser != mapUserOnlineInfo_.end())
	{		
		itUser->second->pUserInfo_->useruuid_ 	 = useruuid;
		itUser->second->pUserInfo_->userinfo_ 	 = userinfo;
		itUser->second->pUserStat_->last_active_ = tNow;

		std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
		//����,���ڸ���
		if (itConn != itUser->second->mapConnInfo_.end())
		{
			itConn->second->conn_uuid_ 					= conn_uuid;
			itConn->second->conn_ 	  					= conn;
			itConn->second->pConnStat_->last_active_	= tNow;
		}
		//����,�����ڲ���
		else
		{
			ConnInfoPtr pConnInfo(new tConnInfo);
			pConnInfo->conn_uuid_	= conn_uuid;
			pConnInfo->conn_		= conn;
			pConnInfo->ipport_		= ipport;
			pConnInfo->pConnStat_.reset(new tStatistics);
			pConnInfo->pConnStat_->last_active_ = tNow;
			pConnInfo->vConnCmdStat_.resize(maxcmdcnt_);
			for (int i = 0; i < maxcmdcnt_; ++i)
			{
				pConnInfo->vConnCmdStat_[i].reset(new tStatistics);
			}
			itUser->second->mapConnInfo_.insert(std::make_pair(conn_uuid, pConnInfo));
		}
	}
	//�û�,�����ڲ���
	else
	{
		UserOnlineInfoPtr pUserOnlineInfo(new tUserOnlineInfo);
		UserInfoPtr pUserInfo(new tUserInfo);
		pUserInfo->useruuid_ 		= useruuid;
		pUserInfo->userinfo_ 		= userinfo;
		pUserOnlineInfo->pUserInfo_ = pUserInfo;
		pUserOnlineInfo->pUserStat_.reset(new tStatistics);
		pUserOnlineInfo->pUserStat_->last_active_ = tNow;
		pUserOnlineInfo->vMinWndConn.resize(maxcmdcnt_);
		pUserOnlineInfo->vUserCmdStat_.resize(maxcmdcnt_);
		for (int i = 0; i < maxcmdcnt_; ++i)
		{
			pUserOnlineInfo->vUserCmdStat_[i].reset(new tStatistics);
		}
		ConnInfoPtr pConnInfo(new tConnInfo);
		pConnInfo->conn_uuid_	= conn_uuid;
		pConnInfo->conn_		= conn;
		pConnInfo->ipport_		= ipport;
		pConnInfo->pConnStat_.reset(new tStatistics);
		pConnInfo->pConnStat_->last_active_ = tNow;
		pConnInfo->vConnCmdStat_.resize(maxcmdcnt_);
		for (int j = 0; j < maxcmdcnt_; ++j)
		{
			pConnInfo->vConnCmdStat_[j].reset(new tStatistics);
		}
		pUserOnlineInfo->mapConnInfo_.insert(std::make_pair(conn_uuid, pConnInfo));

		mapUserOnlineInfo_.insert(std::make_pair(useruuid, pUserOnlineInfo));		
	}
	
	return ret;
}

// �������б������
// ���ӶϿ�ʱ��ص�session��Ϣ����ȡ��useruuid����������б����
// ����ֵ,0:�ɹ�,��0:ʧ��
int UserOnlineList::DelFromOnlineList(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn)
{
	int ret = 0;
	
	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
		if (itConn != itUser->second->mapConnInfo_.end())
		{
			itUser->second->mapConnInfo_.erase(itConn);
		}
	}
		
	return ret;
}

// ��ȡ�������
// ���һ���û��ж�����ӣ�����ѡ��:��������������С�ĺ�ҵ���ȴ���Ӧ��С������
// ����ֵ,0:�ɹ�,1:�޿�������,����:����ʧ��					conn_uuid,connΪ���ص��������
int UserOnlineList::GetBestConn(uint64_t useruuid, uint64_t& conn_uuid, boost::any& conn, int cmd)
{
	int ret = 1;

	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);
	
	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		//û�п�������
		if (!itUser->second->mapConnInfo_.empty())
		{
			ConnInfoPtr pConnInfo(itUser->second->vMinWndConn[cmd].pConn_.lock());
			//���û�����ӣ�ȡ�����һλ������
			if (nullptr == pConnInfo)
			{
				std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.begin();
				itUser->second->vMinWndConn[cmd].minwnd_ = itConn->second->vConnCmdStat_[cmd]->slidewndcnt_;
				itUser->second->vMinWndConn[cmd].pConn_  = itConn->second;
				conn_uuid = itConn->second->conn_uuid_;
				conn 	  = itConn->second->conn_;
			}
			else
			{
				conn_uuid = pConnInfo->conn_uuid_;
				conn 	  = pConnInfo->conn_;
			}
			ret = 0;	
		}
	}
	
	return ret;
}

// ��ȡ�û��ܵ�������������
// ����ֵ,�û��ܵ�������������
size_t UserOnlineList::GetUserOnlineConnCnt(uint64_t useruuid)
{
	size_t cnt = 0;

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		cnt = itUser->second->mapConnInfo_.size();
	}
	
	return cnt;
}

// �����ܵĽ�����(ֻ������)
void UserOnlineList::UpdateTotalRecvCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd)
{
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);	

	MWTIMESTAMP::Timestamp tNow = MWTIMESTAMP::Timestamp::Now();

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		++itUser->second->pUserStat_->totalrecvcnt_;
		itUser->second->pUserStat_->last_active_ = tNow;//ֻ�н��յ�������Ϊ�ǻ�Ծ
		++itUser->second->vUserCmdStat_[cmd]->totalrecvcnt_;
		itUser->second->vUserCmdStat_[cmd]->last_active_ = tNow;//ֻ�н��յ�������Ϊ�ǻ�Ծ
		std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
		if (itConn != itUser->second->mapConnInfo_.end())
		{
			++itConn->second->pConnStat_->totalrecvcnt_;
			itConn->second->pConnStat_->last_active_ = tNow;//ֻ�н��յ�������Ϊ�ǻ�Ծ
			++itConn->second->vConnCmdStat_[cmd]->totalrecvcnt_;
			itConn->second->vConnCmdStat_[cmd]->last_active_ = tNow;//ֻ�н��յ�������Ϊ�ǻ�Ծ
		}
	}
}

// ��ȡ�ܽ����� connuuid��0��ʾ�������ӣ�cmd��maxcmdcnt��ʾ��������
// ����ֵ,�����������ܽ�����
size_t UserOnlineList::GetTotalRecvCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd)
{
	size_t cnt = 0;

	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);
	
	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		if (0 == conn_uuid)
		{
			if (maxcmdcnt_ == cmd)
			{
				cnt = itUser->second->pUserStat_->totalrecvcnt_;
			}
			else
			{
				cnt = itUser->second->vUserCmdStat_[cmd]->totalrecvcnt_;
			}
		}
		else
		{
			std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
			if (itConn != itUser->second->mapConnInfo_.end())
			{
				if (maxcmdcnt_ == cmd)
				{
					cnt = itConn->second->pConnStat_->totalrecvcnt_;
				}
				else
				{
					cnt = itConn->second->vConnCmdStat_[cmd]->totalrecvcnt_;
				}
			}
		}		
	}

	return cnt;
}

// �����ܵķ�����(ֻ������)
void UserOnlineList::UpdateTotalSendCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd)
{
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		++itUser->second->pUserStat_->totalsendcnt_;
		++itUser->second->vUserCmdStat_[cmd]->totalsendcnt_;
		std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
		if (itConn != itUser->second->mapConnInfo_.end())
		{
			++itConn->second->pConnStat_->totalsendcnt_;
			++itConn->second->vConnCmdStat_[cmd]->totalsendcnt_;
		}
	}
}

// ��ȡ�ܷ����� connuuid��0��ʾ�������ӣ�cmd��maxcmdcnt��ʾ��������
// ����ֵ,�����������ܽ�����
size_t UserOnlineList::GetTotalSendCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd)
{
	size_t cnt = 0;
	
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);
	
	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		if (0 == conn_uuid)
		{
			if (maxcmdcnt_ == cmd)
			{
				cnt = itUser->second->pUserStat_->totalsendcnt_;
			}
			else
			{
				cnt = itUser->second->vUserCmdStat_[cmd]->totalsendcnt_;
			}
		}
		else
		{
			std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
			if (itConn != itUser->second->mapConnInfo_.end())
			{
				if (maxcmdcnt_ == cmd)
				{
					cnt = itConn->second->pConnStat_->totalsendcnt_;
				}
				else
				{
					cnt = itConn->second->vConnCmdStat_[cmd]->totalsendcnt_;
				}
			}
		}		
	}

	return cnt;
}

// �������������͵�����������sendmsgwithtcpserver�ɹ������inc��onsendok/onsenderr�����dec��
void UserOnlineList::UpdateWaitSndCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd, enumCntOpt opt)
{
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		itUser->second->pUserStat_->waitsndcnt_ += opt;
		itUser->second->vUserCmdStat_[cmd]->waitsndcnt_ += opt;
		std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
		if (itConn != itUser->second->mapConnInfo_.end())
		{
			itConn->second->pConnStat_->waitsndcnt_ += opt;
			itConn->second->vConnCmdStat_[cmd]->waitsndcnt_ += opt;
		}
	}
}

// ��ȡ���������͵����� connuuid��0��ʾ�������ӣ�cmd��maxcmdcnt��ʾ��������
// ����ֵ,���������͵�����
size_t UserOnlineList::GetWaitSndCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd)
{
	size_t cnt = 0;
	
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		if (0 == conn_uuid)
		{
			if (maxcmdcnt_ == cmd)
			{
				cnt = itUser->second->pUserStat_->waitsndcnt_;
			}
			else
			{
				cnt = itUser->second->vUserCmdStat_[cmd]->waitsndcnt_;
			}
		}
		else
		{
			std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
			if (itConn != itUser->second->mapConnInfo_.end())
			{
				if (maxcmdcnt_ == cmd)
				{
					cnt = itConn->second->pConnStat_->waitsndcnt_;
				}
				else
				{
					cnt = itConn->second->vConnCmdStat_[cmd]->waitsndcnt_;
				}
			}
		}		
	}

	return cnt;
}

// ����ҵ�����ݴ���Ӧ��������onsendok���غ����inc��onrecvmsg���غ����dec��
void UserOnlineList::UpdateWaitRspCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd, enumCntOpt opt)
{
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		itUser->second->pUserStat_->waitrspcnt_ += opt;
		itUser->second->vUserCmdStat_[cmd]->waitrspcnt_ += opt;
		std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
		if (itConn != itUser->second->mapConnInfo_.end())
		{
			itConn->second->pConnStat_->waitrspcnt_ += opt;
			itConn->second->vConnCmdStat_[cmd]->waitrspcnt_ += opt;
		}
	}
}

// ��ȡҵ�����ݴ���Ӧ������ connuuid��0��ʾ�������ӣ�cmd��maxcmdcnt��ʾ��������
// ����ֵ,ҵ�����ݴ���Ӧ������
size_t UserOnlineList::GetWaitRspCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd)
{
	size_t cnt = 0;

	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		if (0 == conn_uuid)
		{
			if (maxcmdcnt_ == cmd)
			{
				cnt = itUser->second->pUserStat_->waitrspcnt_;
			}
			else
			{
				cnt = itUser->second->vUserCmdStat_[cmd]->waitrspcnt_;
			}
		}
		else
		{
			std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
			if (itConn != itUser->second->mapConnInfo_.end())
			{
				if (maxcmdcnt_ == cmd)
				{
					cnt = itConn->second->pConnStat_->waitrspcnt_;
				}
				else
				{
					cnt = itConn->second->vConnCmdStat_[cmd]->waitrspcnt_;
				}
			}
		}		
	}

	return cnt;
}

// ������ռ�ô��ڼ���������sendmsgwithtcpserver֮ǰinc,�����ú����dec,�����óɹ�,�ȴ��ص�,onsenderr�ص�/onrecvmsg�ص�ʱdec��
void UserOnlineList::UpdateSlideWndCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd, enumCntOpt opt)
{
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		itUser->second->pUserStat_->slidewndcnt_ += opt;
		itUser->second->vUserCmdStat_[cmd]->slidewndcnt_ += opt;
		std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
		if (itConn != itUser->second->mapConnInfo_.end())
		{
			itConn->second->pConnStat_->slidewndcnt_ += opt;
			size_t slidewnd = (itConn->second->vConnCmdStat_[cmd]->slidewndcnt_ += opt);
			//���滬��������С������
			if (slidewnd < itUser->second->vMinWndConn[cmd].minwnd_)
			{
				//������С����ֵ
				itUser->second->vMinWndConn[cmd].minwnd_ = slidewnd;	
				//�����Ӧ���ӵ���ָ��
				itUser->second->vMinWndConn[cmd].pConn_  = itConn->second;
			}
		}		
	}
}

// ��ȡ��ռ�ô��������� connuuid��0��ʾ�������ӣ�cmd��maxcmdcnt��ʾ��������
// ����ֵ,��ռ�ô���������
size_t UserOnlineList::GetSlideWndCnt(uint64_t useruuid, uint64_t conn_uuid, const boost::any& conn, int cmd)
{
	size_t cnt = 0;
	
	CHECK_CMD_VALUE_INVALID(cmd, maxcmdcnt_);

	std::lock_guard<std::mutex> lock(mutex_);
	std::map<uint64_t, UserOnlineInfoPtr>::iterator itUser = mapUserOnlineInfo_.find(useruuid);
	if (itUser != mapUserOnlineInfo_.end())
	{
		if (0 == conn_uuid)
		{
			if (maxcmdcnt_ == cmd)
			{
				cnt = itUser->second->pUserStat_->slidewndcnt_;
			}
			else
			{
				cnt = itUser->second->vUserCmdStat_[cmd]->slidewndcnt_;
			}
		}
		else
		{
			std::map<uint64_t, ConnInfoPtr>::iterator itConn = itUser->second->mapConnInfo_.find(conn_uuid);
			if (itConn != itUser->second->mapConnInfo_.end())
			{
				if (maxcmdcnt_ == cmd)
				{
					cnt = itConn->second->pConnStat_->slidewndcnt_;
				}
				else
				{
					cnt = itConn->second->vConnCmdStat_[cmd]->slidewndcnt_;
				}
			}
		}		
	}

	return cnt;
}

}
}
