
//ͬ��ִ�У����ʧ����ع�
#include "stdafx.h"
#include <time.h>
#pragma warning(disable:4996)

#include "ServerSocket.h"
#include "ProxyServer.h"
#include "CDBServer.h"
#include "DebugLog.h"

static const unsigned char szBeginTrans[]=
{
	0x0E,0x01,0x00,0x22,0x00,0x00,0x01,0x00,
	0x16,0x00,0x00,0x00,0x12,0x00,0x00,0x00,
	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x01,0x00,0x00,0x00,0x05,0x00,
	0x02,0x00 
};
static const unsigned char szEndTrans[]=
{
	0x0E,0x01,0x00,0x22,0x00,0x00,0x01,0x00,
	0x16,0x00,0x00,0x00,0x12,0x00,0x00,0x00,
	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x01,0x00,0x00,0x00,0x07,0x00,
	0x00,0x00 
};
//����ȡ�����ݰ�
static const unsigned char szCancelReq[]=
{
	0x06,0x01,0x00,0x08,0x00,0x00,0x00,0x00
};
//ȡ��ȷ�ϻظ����ݰ�
static const unsigned char szCancelRsp[]=
{
	0x04,0x01,0x00,0x15,0x00,0x33,0x01,0x00,
	0xFD,0x20,0x00,0xFD,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00
};
static const unsigned char szErrPacket[]=
{
	0x04,0x01,0x00,0x15,0x00,0x33,0x01,0x00,
	0xFD,0x02,0x00,0xFD,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00
};


//��ȡ���ص�TransID�����������Ӧ��Ϣ��ʽ:
//<TDS header 8�ֽ�> + <E3>+<2�ֽڳ���>+<1�ֽ����� 08ΪtransID>+<1�ֽڴ�С>+<����>
static unsigned __int64 GetTransIDFromReply(unsigned char *szReply)
{
	if(szReply==NULL) return 0;
	unsigned char *ptrBegin=szReply+sizeof(tds_header);
	while(*ptrBegin++==0xE3)
	{
		unsigned short usLen=*(unsigned short *)ptrBegin; ptrBegin+=2;
		if(usLen==0x0B)
		{
			if(*ptrBegin==0x08) //0x08 0x08 <����Ϊ8�ֽ�ID> 0x00
				return *(unsigned __int64 *)(ptrBegin+2);
		}
		ptrBegin+=usLen;
	}
	return 0;
}

#define MAX_SYNCOND_TIMEOUT 10*1000	//10s���ȴ��ظ����ʱʱ��
//�����з��������Ϳ�ʼ�������󣬲���ȡ���������ص�����ID
bool CSQLClient :: BeginTransaction(bool bIncludeMainDB,unsigned char sta)
{
	unsigned int idx=0;
	QueryPacket query(m_idxRandDBSvr,0x0100007f,64);
	query.SetQueryData((unsigned char *)&szBeginTrans[0],sizeof(szBeginTrans));
	unsigned char *pquery=query.GetQueyDataPtr();
	*(pquery+1)=sta;
	
	HANDLE hMutex=m_proxysvr->m_hSynMutex;	//���л�ͬ��ִ��
	try{
		if(hMutex && WaitForSingleObject(hMutex, MAX_SYNCOND_TIMEOUT)==WAIT_TIMEOUT)
			throw -2 ;
		m_lastreply.ClearData(); //��ջظ�
		m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG); //���õȴ���Ϣ�ظ���ʶ
		if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
				throw -3;	//�������������Ϳ�ʼ��������
		//if(bIncludeMainDB){ //�������������Ϳ�ʼ��������
		//	m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG); //���õȴ���Ϣ�ظ���ʶ
		//	if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
		//		throw -3;
		//}else m_pRandDBSvr->SetUserTag(0);

		bool bSuccess=true; //�κ�һ����ͬ���ĸ��ط����������ȡʧ�ܶ�ֱ�ӽ����ͻ������ӣ������������Ƕ��
		long msTimeout=MAX_WAITREPLY_TIMEOUT; //���ʱ�ȴ�ʱ��ms
		//�����еĸ��ػ򱸷ݷ���������,0�ɹ� -1��ʱ
		int iresult=SendQueryToAll_rb(&query,msTimeout,false);
		if(iresult==0){
		for(idx=0;idx<m_vecDBConn.size();idx++)
		{//��ȡ���ص�TransID
			if(idx==m_idxRandDBSvr) continue; //��DB��������������ͬ��
			DBConn * pconn=m_vecDBConn[idx];
			if(pconn==NULL || !pconn->IsConnected()) continue;
			ReplyPacket &reply=pconn->GetLastReply();
			pconn->m_uTransactionID =(reply.GetReplyDataSize()!=0)?GetTransIDFromReply(reply.GetReplyDataPtr()):0;
			if(pconn->m_uTransactionID!=0) continue;
			bSuccess=false; break;
		} }else bSuccess=false;
		if(bSuccess) // && bIncludeMainDB)
		{//��ȡ���������������Ӧ
			m_uTransactionID=(m_lastreply.GetReplyDataSize()!=0)?GetTransIDFromReply(m_lastreply.GetReplyDataPtr()):0;
			bSuccess=(m_uTransactionID!=0);
		}
		if(!bSuccess) throw iresult;
		if(bIncludeMainDB) //�������������ؿ�ʼ��������ķ���
			m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
		 //m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
	}catch( int iresult)
	{
		if(hMutex) ReleaseMutex(hMutex);
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] ��ȡ��ʼ����������Ӧʧ�� (%d)��\r\n",iresult);
		m_pSocketUser->Close(true);	return false;//ǿ�ƹرտͻ�������
	}
	return true;
}

//�����з��������������ύ��ع�����
bool CSQLClient :: EndTransaction(bool bIncludeMainDB,bool bCommit)
{
	unsigned int idx=0;	
	QueryPacket query(m_idxRandDBSvr,0x0100007f,64);
	query.SetQueryData((unsigned char *)&szEndTrans[0],sizeof(szEndTrans));
	unsigned char *pquery=query.GetQueyDataPtr();
	unsigned char *pcommit=pquery+sizeof(tds_header)+TDS9_QUERY_START_LEN;
	*pcommit=(bCommit)?0x07:0x08;
	
	HANDLE hMutex=m_proxysvr->m_hSynMutex;	//���л�ͬ��ִ��
	try{
		m_lastreply.ClearData(); //��ջظ�
		unsigned __int64 *pTransID=(unsigned __int64 *)( pquery+sizeof(tds_header)+4+4+2);
		*pTransID=m_uTransactionID;
		m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG);
		if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
			throw -3; //��������������
		//if(bIncludeMainDB){ //��������������
		//	unsigned __int64 *pTransID=(unsigned __int64 *)( pquery+sizeof(tds_header)+4+4+2);
		//	*pTransID=m_uTransactionID;
		//	m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG);
		//	if(m_pRandDBSvr->Send(query.GetQueyDataPtr(),query.GetQueryDataSize())<=0)
		//		throw -3;
		//}else m_pRandDBSvr->SetUserTag(0);

		long msTimeout=MAX_WAITREPLY_TIMEOUT; //���ʱ�ȴ�ʱ��ms
		//�����еĸ��ػ򱸷ݷ���������
		int iresult=SendQueryToAll_rb(&query,msTimeout,true);
		if(bIncludeMainDB) ////�������������ؽ�������Ļظ�	
			m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
		m_uTransactionID=0;
		for(idx=0;idx<m_vecDBConn.size();idx++)
			if(m_vecDBConn[idx]!=NULL) m_vecDBConn[idx]->m_uTransactionID=0;
		if(iresult!=0) throw iresult;
	}catch(int iresult)
	{
		if(hMutex) ReleaseMutex(hMutex);
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] ��ȡ��������������Ӧʧ�� (%d)��\r\n",iresult);
		//ǿ�ƹرտͻ�������
		m_pSocketUser->Close(true); return false;
	}
	if(hMutex) ReleaseMutex(hMutex); return true;
}

//��ǰ���з�����ͬ��д����²������κ�һ��������ִ��ʧ������������������
bool CSQLClient :: SendQueryAndSynDB_rb(QueryPacket *plastquery)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	m_pRandDBSvr->SetUserTag(WAITREPLY_NOSEND_FLAG); //���õȴ���Ϣ�ظ���ʶ
	if(m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize())<=0)
	{//��������������ʧ�ܣ�ֱ�ӻ�Ӧִ�д�����Ϣ
		m_pSocketUser->Send((void*)szErrPacket,sizeof(szErrPacket));
		m_pRandDBSvr->SetUserTag(0); return false; 
	}
	for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue; //��DB��������������ͬ��
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn && pconn->IsConnected()) //��Ҫͬ���Ŀ϶�����д��������
			dbservers[idx].QueryCountAdd(true); //ִ�м���
	}
	
	long msTimeout=MAX_WAITREPLY_TIMEOUT; //���ʱ�ȴ�ʱ��ms
	int iresult=SendQueryToAll_rb(plastquery,msTimeout,false);
	if(iresult==0){ //���и��ػ򱸷ݷ�����ִ�гɹ������ȡ���������ķ��ز�ת�����ͻ���
		m_pSocketUser->Send(m_lastreply.GetReplyDataPtr(),m_lastreply.GetReplyDataSize());
		plastquery->m_staExec=m_lastreply.m_iresult; //��������ִ�н�� 0�ɹ� -1��ʱ...
		plastquery->m_nAffectRows=m_lastreply.m_nAffectRows;
		return true;
	}
	if(iresult==-1){ //��ʱ����������������
		plastquery->m_staExec=-1; //ִ�н����ʱ
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"[SQLProxy] ����ͬ��ִ�г�ʱ,�ͷ����ӣ�\r\n");
		if(m_proxysvr->m_hSynMutex) ReleaseMutex(m_proxysvr->m_hSynMutex);
		//ǿ�ƹرտͻ�������
		m_pSocketUser->Close(true); return false;
	}

	//����ͬ��ִ��ʧ�ܻ�ʱ����������δִ����ϳ�ʱ�ķ���������ȡ��ִ������
	SendCancelToAll_rb();
	DBConn * pconn=m_vecDBConn[iresult-1];
	ReplyPacket &lastreply=pconn->GetLastReply();//��ͻ��˻ظ�
	m_pSocketUser->Send(lastreply.GetReplyDataPtr(),lastreply.GetReplyDataSize());
	plastquery->m_staExec=lastreply.m_iresult; //ִ�н��ʧ��
	return false;
}

//��������Ч�ļ�Ⱥ������ת��query���Ա����ݿⱣ��ͬ��; lTimeout ��ʱʱ��ms
//�κ�һ�����ݿ�ͬ��ִ��ʧ�ܾ��������أ��ɹ�����0, -1��ʱ;����ͬ��ʧ�ܵ������������(�±�1��ʼ)
int CSQLClient :: SendQueryToAll_rb(QueryPacket *plastquery,unsigned int msTimeout,bool bEndTrans)
{
	unsigned int idx=0;
	int iresult,iNumsWaitforReply=1; //��Ҫͬ�������ݿ������������������
	std::vector<DBConn *> vecWaitforReply; //�ȴ�ִ�з��ص�conn����
	for(idx=0;idx<m_vecDBConn.size();idx++) vecWaitforReply.push_back(NULL);
	for(idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue;
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn==NULL || !pconn->IsConnected()) continue;
		if(bEndTrans && pconn->m_uTransactionID==0) continue;
		pconn->GetLastReply().ClearData();
		//�Ѿ��������ݿ�,�첽ִ������ͬ��Query
		if( pconn->ExcuteData(m_tdsver,plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize()) )
		{	
			vecWaitforReply[idx]=pconn; iNumsWaitforReply++; 
		}//�첽�ȴ�ִ�з���
	}
	if(iNumsWaitforReply==0) return 0; //û����Ҫͬ�������ݿ�
	long tStartTime=clock();
	while(true)
	{
		iresult=iNumsWaitforReply=0; //�ȴ�ִ�з��ص����Ӹ���
		if( m_pRandDBSvr->GetUserTag()!=0 ) iNumsWaitforReply++; //��������δִ�����
		for(idx=0;idx<vecWaitforReply.size();idx++)
		{
			DBConn * pconn=vecWaitforReply[idx];
			if(pconn==NULL) continue;
			iresult=pconn->GetStatus_Asyn();
			if(iresult!=DBSTATUS_PROCESS){ //��ִ����ϣ��ж��Ƿ�ִ�гɹ�
				vecWaitforReply[idx]=NULL;
				if(iresult!=DBSTATUS_SUCCESS) 
				{//��һ������ͬ��ִ��ʧ���򷵻�
					iresult=(idx+1);
					iNumsWaitforReply=0; break;
				}
			}else iNumsWaitforReply++; //δִ����ϣ������ȴ�����
		}//?for(idx=0;idx<vecWaitforReply.size();idx++)
		if(iNumsWaitforReply==0) break;
		if( (clock()-tStartTime)<msTimeout){ ::Sleep(1); continue; }
		iresult=-1; break; //��ʱ
	}//?while(true)
	return iresult;
}
void CSQLClient :: SendCancelToAll_rb()
{
	for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
	{//�����������ڴ����е����ݿⷢ��cancel
		if(idx==m_idxRandDBSvr) continue;
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn==NULL || !pconn->IsConnected()) continue;
		int iresult=pconn->GetStatus_Asyn();
		if(iresult==DBSTATUS_PROCESS) //���ڴ�����,����cancel��Ϣ
			pconn->ExcuteData(m_tdsver,(unsigned char *)szCancelReq,sizeof(szCancelReq));
	}
	if(m_pRandDBSvr->GetUserTag()!=0)
		m_pRandDBSvr->Send((void *)szCancelReq,sizeof(szCancelReq));
}

/*
#define SETDB_EXECFLAG(dbflags,idx,fl) { \
	unsigned int ii=idx<<1; \
	(dbflags) &=~(3<<ii); \
	(dbflags) |=( (fl)<<ii ); \
}
#define GETDB_EXECFLAG(dbflags,idx,fl) { \
	unsigned int ii=idx<<1; \
	fl= ( (dbflags)&(3<<ii) )>>ii; \
}
unsigned int CSQLClient :: WaitForReplys(std::vector<DBConn *> &vecWaitforReply,unsigned int msTimeout,unsigned int *pdbIndex)
{
	unsigned int idx,dbIndex=0,numErrDB=0;
	for(idx=0;idx<vecWaitforReply.size();idx++) //��ʼ��������ִ�б�ʶ
		if(vecWaitforReply[idx]!=NULL) SETDB_EXECFLAG(dbIndex,idx,0x10); 
	long tStartTime=clock();
	while(true)
	{
		int iNumsWaitforReply=0; //�ȴ�ִ�з��ص����Ӹ���
		for(idx=0;idx<vecWaitforReply.size();idx++)
		{
			DBConn * pconn=vecWaitforReply[idx];
			if(pconn==NULL) continue; //����ȴ�
			int iresult=pconn->GetStatus_Asyn();
			if(iresult!=DBSTATUS_PROCESS)
			{ //��ִ����ϣ��ж��Ƿ�ִ�гɹ�
				if(iresult==DBSTATUS_ERROR)
				{//������Ч
					SETDB_EXECFLAG(dbIndex,idx,0x11); 
					numErrDB++; //ͬ��ʧ�ܷ���������
				}else if(iresult!=DBSTATUS_SUCCESS)
				{ //ͬ��ִ��ʧ��
					SETDB_EXECFLAG(dbIndex,idx,0x01); //�����÷�����ʧ��
					numErrDB++; //ͬ��ʧ�ܷ���������
				}else //�����÷������ɹ�
					SETDB_EXECFLAG(dbIndex,idx,0x00);
				vecWaitforReply[idx]=NULL;
			}else //δִ����ϣ������ȴ�����
				iNumsWaitforReply++; 	
		}//?for(idx=0;idx<vecWaitforReply.size();idx++)
		if(iNumsWaitforReply==0) break;
		if( (clock()-tStartTime)<msTimeout){::Sleep(1); continue; }
		numErrDB+=iNumsWaitforReply; break; //��ʱ
	}//?while(true)
	if(pdbIndex!=NULL) *pdbIndex=dbIndex;
	return numErrDB;
} 
*/