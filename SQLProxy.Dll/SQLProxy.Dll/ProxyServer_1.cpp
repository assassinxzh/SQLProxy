
#include "stdafx.h"
#include <time.h>
#pragma warning(disable:4996)

#include "ServerSocket.h"
#include "ProxyServer.h"
#include "CDBServer.h"
#include "DebugLog.h"

//dbconn���ݿ����ӶϿ�֪ͨ�ص���iEvent==0������ ==1�����Ͽ� ==2�쳣�Ͽ�
void CSQLClient :: dbconn_onStatus(int iEvent,CSQLClient *pclnt)
{
	CDBServer *dbservers=(CDBServer *)pclnt->m_proxysvr->m_dbservers;
	int idxRandDBSvr=pclnt->m_idxRandDBSvr;
	switch(iEvent)
	{
	case 0:
		dbservers[idxRandDBSvr].m_nDBConnNums++;
		break;
	case 1:
		dbservers[idxRandDBSvr].m_nDBConnNums--;
		break;
	case 2:
		dbservers[idxRandDBSvr].m_nDBConnNums--;
		//ǿ�ƹرտͻ������ӣ�����ͬ������
		if(pclnt->m_pSocketUser!=NULL) pclnt->m_pSocketUser->Close(true);
		break;
	}	
}
//dbconn�յ�db������������֪ͨ�ص�,��ĳ�����ݿ����Ӧ����ת�����ͻ��ˣ�ת����Ϻ��Զ��ر�
void CSQLClient :: dbconn_onData(tds_header *ptdsh,unsigned char *pData,CSQLClient *pclnt)
{	
	if(ptdsh->type ==TDS_REPLY && IS_LASTPACKET(ptdsh->status) )
	{//�ظ�Packet��Ϊ���һ��Packet
		int idxLoadDBSvr=pclnt->m_idxLoadDBSvr;
		DBConn * pconn=(idxLoadDBSvr>=0)?pclnt->m_vecDBConn[idxLoadDBSvr]:NULL;
		if(pconn) //�رյ�ǰ���ط�����������ת��
		pconn->SetEventFuncCB((FUNC_DBDATA_CB *)NULL);

		std::deque<QueryPacket *> &vecQueryData=pclnt->m_vecQueryData;
		if(!vecQueryData.empty() )
		{//�����ǰ�����ȴ���������Ӧ��Query/RPC�����������ȡ�������Query/RPC���󲢴ӵȴ��������Ƴ�
			QueryPacket *plastquery=vecQueryData.front();
			vecQueryData.pop_front();
			plastquery->m_msExec=clock()-plastquery->m_msExec; //ִ��ʱ��(ms)
			//plastquery->m_staExec=parse_reply(pData,pclnt->m_tdsver,&plastquery->m_nAffectRows);
			if(!pclnt->m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,pData,pclnt->m_tdsver)) delete plastquery;
		}
	}//if(tdsh.type ==TDS_REPLY && ) 
	//��dbconn�������ݿ�ķ�����Ϣת�����ͻ���
	pclnt->m_pSocketUser->Send(pData,ptdsh->size);
}

CSQLClient :: CSQLClient(CProxyServer *proxysvr)
: m_proxysvr(proxysvr)
{
	m_pSocketUser=NULL;
	m_pRandDBSvr=NULL;
	m_idxRandDBSvr=0;
	m_idxLoadDBSvr=0;
	m_bLoginDB=false;
	m_tdsver=0;
	m_uTransactionID=0;
}

CSQLClient :: ~CSQLClient()
{
	//���ȹر����ӣ�����m_pRandDBSvr��ISocketListener::SocketReceivedһֱ�ڵȴ�db reply���߳��޷�����
	for(unsigned int i=0;i<m_vecDBConn.size();i++)
	{
		DBConn *pconn=m_vecDBConn[i];
		if(pconn!=NULL) pconn->Close();
	}

	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	if(m_pRandDBSvr!=NULL){
		dbservers[m_idxRandDBSvr].m_nSQLClients--;
		delete m_pRandDBSvr;
	}
	//�ͷ����Ӷ���
	for(unsigned int i=0;i<m_vecDBConn.size();i++)
	{
		DBConn *pconn=m_vecDBConn[i];
		if(pconn!=NULL) delete pconn;
	}
	m_vecDBConn.clear();

	for(unsigned int i=0;i<m_vecQueryData.size();i++)
		delete m_vecQueryData[i];
	for(unsigned int i=0;i<m_vecTransData.size();i++)
		delete m_vecTransData[i];
}

void CSQLClient :: SetMainDBSocket(CSocket *pSocketUser,CSocket *pRandDBSvr,int idxRandDBSvr,int idxLoadDBsvr)
{
	m_pSocketUser=pSocketUser;
	m_pRandDBSvr=pRandDBSvr;
	m_idxRandDBSvr=idxRandDBSvr;
	m_idxLoadDBSvr=idxLoadDBsvr;
}
//��ʼ�����ݿ����Ӷ������Ӻ�̨��Ⱥ���ݿ�
void CSQLClient :: InitDBConn(unsigned char *login_packet,unsigned int nSize,tds_login *ptr_login)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	unsigned int i,iNumsSocketSvrs=0;//SQL��Ⱥ�������
	//��ʼ�����ݿ����Ӷ���
	for(i=0;i<MAX_NUM_SQLSERVER;i++,iNumsSocketSvrs++)
	{
		CDBServer &sqlsvr=dbservers[i];
		if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") break;
		if(sqlsvr.IsValid()){
			DBConn *pconn=new DBConn;
			if(pconn){
				tds_login_param &loginparams=pconn->LoginParams();
				std::string dbuser=sqlsvr.m_username,dbpswd=sqlsvr.m_userpwd,dbname=sqlsvr.m_dbname;
				loginparams.tdsversion =this->m_tdsver; //���úͿͻ���һ�µĵ�¼Э��
				if(ptr_login && dbuser==""){  //���û��ָ�����ݵ�¼�ʺ���������ÿͻ��˵�¼Э���е��ʺ�����
					dbuser=ptr_login->username; dbpswd=ptr_login->userpswd; }
				if(ptr_login && dbname=="") dbname=ptr_login->dbname;
				if(ptr_login) loginparams.tds_block_size=ptr_login->psz; //���úͿͻ���һ�µ�block size����
				pconn->Setdbinfo(sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport,dbname.c_str(),dbuser.c_str(),dbpswd.c_str());	
			}
			m_vecDBConn.push_back(pconn);
		}else m_vecDBConn.push_back(NULL);
	}//?for(i=0;i<MAX_NUM_SQLSERVER;i++)
	
	for(i=0;i<iNumsSocketSvrs;i++)
	{
		bool bconn=false;
		DBConn *pconn=m_vecDBConn[i];
		if(pconn==NULL || i==m_idxRandDBSvr) continue; 
		if(login_packet!=NULL && nSize!=0)
		{
			tds_login_param &params=pconn->LoginParams();
			if( ptr_login!=NULL && //�����жϵ�ǰ��¼�ʺ�������趨���Ƿ�һ�£������һ�������¸��ĵ�¼���ݰ���ĵ�¼��Ϣ
				(_stricmp(params.dbuser .c_str(),ptr_login->username.c_str())!=0 || params.dbpswd!=ptr_login->userpswd) )
			{
				unsigned char NewLoginPacket[512]; 
				unsigned int NewSize=write_login_new(NewLoginPacket,512,*ptr_login,NULL,params.dbuser .c_str(),params.dbpswd.c_str());
				if(NewSize!=0) bconn=pconn->Connect_tds(NewLoginPacket,NewSize);
			}else //ֱ��ת��ԭʼ��¼���ݰ����е�¼
				bconn=pconn->Connect_tds(login_packet,nSize);
		}else  //�����趨���ʺ�������Ϣͨ��dblib���ӷ������ݿ�
			bconn=pconn->Connect();
		if(bconn){
			CSQLClient::dbconn_onStatus(0,this);
			pconn->SetEventFuncCB((FUNC_DBSTATUS_CB *)CSQLClient::dbconn_onStatus,(INTPTR)this);
		}else dbservers[i].SetValid(1); //������������Ч�������ٴ�����
	}	
}

//���մ���SQL���������ص�����
bool CSQLClient :: SocketReceived(CSocket *pSocket)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	//IOutputStream *pOutStream=m_pSocketUser->GetOutputStream();
	CSocketStream *pInStream= (CSocketStream *)pSocket->GetInputStream();
	bool bNewReply=true; //����Ƿ�ʼһ����Ϣ�Ļظ�packet��ÿ��reply���ܰ������packets
	long lUserTag=0;	//���==WAITREPLY_FLAG˵�����첽�ȴ��ظ��Ĵ���
	while(true) //ѭ�����������ѽ��յ�����
	{
		unsigned int nSize=pInStream->GetInputSize(); //����δ�������ݴ�С
		unsigned int offset=pInStream->GetReadPos();
		unsigned char *pInbuffer=(unsigned char *)pInStream->GetInput()+offset; //δ����������ʼָ��
		if(nSize==0) break; //�����Ѵ������
		if(nSize<sizeof(tds_header)) break; //����δ�������,��������

		tds_header tdsh;
		unsigned char *pinstream=pInbuffer;
		pinstream+=parse_tds_header(pinstream,tdsh);
		if(tdsh.type<0x01 || tdsh.type >0x1f){	
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] ���ش��� - ��Ч��Э�����ݣ�t=0x%02X s=%d - %d\r\n",tdsh.type,tdsh.size,nSize);
			pInStream->Skip(nSize); break;
		}
		if(tdsh.size>nSize) break; //����δ�������,��������
		//���յ���һ������packet���������� begin====================================
		RW_LOG_DEBUG("[SQLProxy] Received %d reply data from %s:%d\r\n\t   type=0x%x status=%d size=%d\r\n",
							nSize,pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),tdsh.type ,tdsh.status ,tdsh.size);
		if( tdsh.type ==TDS_REPLY)
		{//�ظ���Ϣ
			if( (lUserTag=pSocket->GetUserTag())!=0)
			{//�����ȴ��ظ�����Ĵ���
				(bNewReply)?m_lastreply.SetReplyData(pInbuffer,tdsh.size):
							m_lastreply.AddReplyData(pInbuffer,tdsh.size);
				if( IS_LASTPACKET(tdsh.status) ){ //�ظ���Ϣ�����һ��packet
					m_lastreply.m_iresult=(int)parse_reply(pInbuffer,m_tdsver,&m_lastreply.m_nAffectRows);
					pSocket->SetUserTag(0); //�ѽ��յ��ظ���ȡ�������ȴ��ظ�����Ĵ����ʶ
					bNewReply=true; //��ʼһ���µĻظ����մ���
				}else bNewReply=false; //�������иûظ���packet������
				if(WAITREPLY_NOSEND_FLAG==lUserTag) //��Ӧ��Ϣ��ת��
				{
					//�ƶ����ݴ���ָ�룬�����Ѵ������ݴ�С
					pInStream->Skip(tdsh.size); continue;
				}
			}//?if(pSocket->GetUserTag()!=0)
			else if( IS_LASTPACKET(tdsh.status) && !m_vecQueryData.empty())
			{//��ǰ�����ȴ���������Ӧ��Query/RPC����������������ǻظ������һ��packet
				//ȡ�������Query/RPC���󲢴ӵȴ��������Ƴ�
				QueryPacket *plastquery=m_vecQueryData.front();// m_vecQueryData[0];
				m_vecQueryData.pop_front(); //.erase(m_vecQueryData.begin());
				plastquery->m_msExec=clock()-plastquery->m_msExec; //ִ��ʱ��(ms)
				//plastquery->m_staExec=parse_reply(pInbuffer,m_tdsver,&plastquery->m_nAffectRows);
				if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,pInbuffer,m_tdsver)) delete plastquery;
			}
		}//?if( tdsh.type ==TDS_REPLY)
		//���յ���һ������packet����������  end ====================================
		nSize=tdsh.size; //�Ѵ����ֽ�
		pInStream->Skip(nSize); //�ƶ����ݴ���ָ�룬�����Ѵ������ݴ�С
		m_pSocketUser->Send(pInbuffer,nSize);
	}//?while(true) 
	return true;
}

//ֱ��ת�����ݵ���������������0- �ɹ��ȵ���Ӧ��ִ�гɹ�������ʧ�� -1��ʱ
//msTimeOut==0 ���ȴ����ؽ����ͷ���ȴ�ָ����ms
int CSQLClient :: SendWaitRelpy(void *pData, unsigned int nSize,int msTimeOut)
{
	if(msTimeOut==0){ //���ȴ�����,��������
		m_pRandDBSvr->Send(pData,nSize);
		return 0;
	}
	int iresult=-1; //��ʱ
	DWORD dwTimeOut=(msTimeOut>0)?msTimeOut:(300*1000);
	if(pData!=NULL) m_pRandDBSvr->SetUserTag(WAITREPLY_FLAG);
	if(pData==NULL || m_pRandDBSvr->Send(pData,nSize)>0)
	{
		long tStartTime=clock();
		while(true)
		{
			if( (clock()-tStartTime)>dwTimeOut ) break; //��ʱ
			if( m_pRandDBSvr->GetUserTag()==0 )
			{//�Ѿ����յ��ظ�����
				iresult=m_lastreply.m_iresult;
				break;
			}
			::Sleep(1); //��ʱ,����
		}
	}else iresult=DBSTATUS_ERROR; //�������Ӵ���
	m_pRandDBSvr->SetUserTag(0);	return iresult;
}

inline int  CSQLClient :: GetInValidDBsvr()
{
	int idxSynDB=0;
	for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue; //��DB������
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn==NULL || !pconn->IsConnected())
			idxSynDB |=(0x01<<idx); //��Ǵ����ݿ�δ����ͬ��
	}
	return idxSynDB;
}
void CSQLClient :: TransactPacket(void *pData, unsigned int nSize,bool bRollback)
{
	unsigned char *pinstream=(unsigned char *)pData+sizeof(tds_header);
	pinstream=(IS_TDS9_QUERY_START(pinstream))?(pinstream+TDS9_QUERY_START_LEN):pinstream;
	unsigned short usTransFlag=*(unsigned short *)pinstream;
	if(usTransFlag==0x05)
	{//��ʼ����������
		//�������ͬ��ʧ�ܻع��������з��������Ϳ�ʼ��������
		unsigned char sta=*((unsigned char *)pData+1);
		if(!bRollback){//ֱ������������ת������������
			m_uTransactionID=1; //���������־��==0��������
			m_pRandDBSvr->Send(pData,nSize);
		}else BeginTransaction(true,sta);
		return;
	}
	//������������� 0x07�����ύ 0x08����ع�=======================
	int idxSynDB=-1,iTransmit=0;	//����ͬ��ִ���Ƿ�ɹ�
	QueryPacket *plastquery=(m_vecTransData.size()!=0)?m_vecTransData[0]:NULL;
	unsigned int msExecTransTime=(plastquery)?(clock()-plastquery->m_msExec):0; //����ִ��ʱ��
	//�����֧�ֻع�״̬��Query/RPC����ʱ�Ѿ���д���������ͬ����������������ύʱ�����ٽ�������ͬ������
	if(!bRollback)
	{ //���û�п���ͬ��ʧ�ܻع�����������ɹ���Ž���д����²�����ͬ��
		m_pRandDBSvr->Send(pData,nSize);//ֱ������������ת��������Ϣ,�����ܿ�Ľ�������
		if(usTransFlag==0x07 && m_proxysvr->m_iSynDBTimeOut>=0)
		{ //ֻ��������ɹ��ύ������ͬ���Ž��и������ݿ�ͬ��ִ�в���
			unsigned int msExecTime=(m_proxysvr->m_iSynDBTimeOut>0)?(m_proxysvr->m_iSynDBTimeOut*1000):0;
			for(unsigned int i=0;i<m_vecTransData.size();i++)
			{
				QueryPacket *plastquery=m_vecTransData[i];
				if(plastquery!=NULL && plastquery->IsNotSelectSQL(false) )
				{//����ͬ��д����²������
					iTransmit=SendQueryToAll(plastquery,msExecTime);
					if(iTransmit!=0 && iTransmit!=-1) break; //0�ɹ� -1��ʱ
				}
			}
		}
	}else{ //�������ͬ��ʧ�ܻع��������з��������Ϳ�ʼ���������Ϣ
		EndTransaction(true,(usTransFlag==0x07));
		idxSynDB=(usTransFlag==0x07)? GetInValidDBsvr():0;
	}
	m_uTransactionID=0; //�������������־
	//��������SQL������Ϣ����ӡ��������
	for(unsigned int i=0;i<m_vecTransData.size();i++)
	{
		plastquery=m_vecTransData[i];
		plastquery->m_bTransaction=true;
		plastquery->m_msExec=msExecTransTime; //����ִ��ʱ��
		plastquery->m_staExec =(usTransFlag==0x07)?0:usTransFlag; //����ִ��״̬
		plastquery->m_staSynExec =iTransmit;	//����ͬ��ִ��״̬
		if(idxSynDB!=-1) plastquery->m_idxSynDB=idxSynDB;
		plastquery->m_nMorePackets=m_vecTransData.size()-i-1;
		if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,NULL,0)) delete plastquery;
	}
	m_vecTransData.clear(); return;	
}

//���⸺��ģʽ�����е�SQL����������������ִ�С������д�������ͬ�����������ط�����ִ��
void CSQLClient :: SendQuery_mode1(QueryPacket *plastquery,bool bRollback)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	bool bNotSelectSQL=plastquery->IsNotSelectSQL(false);
	//���в����϶�����������ִ�У����������������+1
	dbservers[m_idxRandDBSvr].QueryCountAdd(bNotSelectSQL); //��������ִ�м���

	if(m_uTransactionID!=0){ //��ǰ�����������,ת����������ִ��
		m_vecTransData.push_back(plastquery); //����QueryPacket����
		if(bRollback && bNotSelectSQL) //����ǲ�ѯ����ҵ�ǰ��ͬ��ʧ�ܻع�ģʽ
			SendQueryAndSynDB_rb(plastquery);
		else m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}else if(bNotSelectSQL) //д����²���
	{
		if(bRollback && IS_TDS72_PLUS(m_tdsver))
		{
			if(BeginTransaction(false,0x01))
			{
				unsigned char *pquery=plastquery->GetQueyDataPtr();
				TDS_UINT8 *pTransID=(TDS_UINT8 *)( pquery+sizeof(tds_header)+4+4+2); 
				*pTransID=m_uTransactionID;
				*(pquery+1) &=0xF7; //ȡ��RESETCONNECTIONλ(0x08)
			}
			bool bCommit=SendQueryAndSynDB_rb(plastquery);
			EndTransaction(false,bCommit);
			if(bCommit) plastquery->m_idxSynDB= GetInValidDBsvr();
			plastquery->m_msExec=clock()-plastquery->m_msExec; //ִ��ʱ��(ms)
		}else //��ǰ������������д��SQL��䣬���ִ�гɹ���ͬ�����и��ػ򱸷ݷ�����
			SendQueryAndSynDB(plastquery);
		//���浽Query data����ӡ�������У����ʧ����ɾ���ͷ�
		if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,NULL,0)) delete plastquery;
	}else{ //��ѯ������ת������,���ȴ�����
		m_vecQueryData.push_back(plastquery); //����QueryPacket����
		m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}
}
//����ǲ�ѯ���������ת�������ط�����ִ�з���ת����������ִ��
void CSQLClient :: SendQuery_mode0(QueryPacket *plastquery,bool bRollback)
{
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	bool bNotSelectSQL=plastquery->IsNotSelectSQL(false);
	//�����д����²�������������ִ�У���ѯ�������ѡ��һ̨���ط�����ִ��
	if(m_uTransactionID!=0){ //��ǰ�����������,ת����������ִ��
		dbservers[m_idxRandDBSvr].QueryCountAdd(bNotSelectSQL); //ִ�м���
		m_vecTransData.push_back(plastquery); //����QueryPacket����
		if(bRollback && bNotSelectSQL) //����ǲ�ѯ����ҵ�ǰ��ͬ��ʧ�ܻع�ģʽ
			SendQueryAndSynDB_rb(plastquery);
		else m_pRandDBSvr->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}else if(bNotSelectSQL) //д����²���
	{
		dbservers[m_idxRandDBSvr].QueryCountAdd(true); //ִ�м���
		if(bRollback && IS_TDS72_PLUS(m_tdsver))
		{
			if(BeginTransaction(false,0x01))
			{
				unsigned char *pquery=plastquery->GetQueyDataPtr();
				TDS_UINT8 *pTransID=(TDS_UINT8 *)( pquery+sizeof(tds_header)+4+4+2); 
				*pTransID=m_uTransactionID;
				*(pquery+1) &=0xF7; //ȡ��RESETCONNECTIONλ(0x08)
			}
			bool bCommit=SendQueryAndSynDB_rb(plastquery);
			EndTransaction(false,bCommit);
			if(bCommit) plastquery->m_idxSynDB= GetInValidDBsvr();
			plastquery->m_msExec=clock()-plastquery->m_msExec; //ִ��ʱ��(ms)
		}else //��ǰ������������д��SQL��䣬���ִ�гɹ���ͬ�����и��ػ򱸷ݷ�����
			SendQueryAndSynDB(plastquery);
		//���浽Query data����ӡ�������У����ʧ����ɾ���ͷ�
		if(!m_proxysvr->m_psqlAnalyse->AddQuery(plastquery,NULL,0)) delete plastquery;
	}else
	{//��ѯ���������ѡ��һ̨���ط�����ִ��
		CSocket *psocket=m_pRandDBSvr;
		for(unsigned int idx=0;idx<m_vecDBConn.size();idx++)
		{
			m_idxLoadDBSvr=(m_idxLoadDBSvr+1) % m_vecDBConn.size();
			if(m_idxLoadDBSvr==m_idxRandDBSvr) break;
			DBConn * pconn=m_vecDBConn[m_idxLoadDBSvr];
			if(pconn==NULL || !pconn->IsConnected()) continue;
			pconn->SetEventFuncCB((FUNC_DBDATA_CB *)CSQLClient::dbconn_onData);
			psocket=pconn->GetSocket(); break;
		}
		plastquery->m_idxDBSvr =m_idxLoadDBSvr; //����ִ��SQL�����ݿ�����
		dbservers[m_idxLoadDBSvr].QueryCountAdd(false); //ִ�м���
		m_vecQueryData.push_back(plastquery); //����QueryPacket����
		psocket->Send(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize());
	}
}

//��ǰ������������д��SQL��䣬���ִ�гɹ���ͬ�����и��ػ򱸷ݷ�����
bool CSQLClient :: SendQueryAndSynDB(QueryPacket *plastquery)
{
	int msTimeout=MAX_WAITREPLY_TIMEOUT; //���ȴ�30s
	int iresult=SendWaitRelpy(plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize(),msTimeout);
	plastquery->m_msExec=clock()-plastquery->m_msExec; //ִ��ʱ��(ms)
	plastquery->m_staExec=iresult; //ִ�н�� 0�ɹ� -1��ʱ...
	if(iresult==0) plastquery->m_nAffectRows=m_lastreply.m_nAffectRows;
	if(iresult!=0) return false; //ִ��ʧ��
	if(m_proxysvr->m_iSynDBTimeOut<0) return true; //����ͬ��
	
	//�趨ͬ����ʱʱ��,���ȴ�ָ����ms
	unsigned int msExecTime=(m_proxysvr->m_iSynDBTimeOut>0)?(m_proxysvr->m_iSynDBTimeOut*1000):0;
	plastquery->m_staSynExec=SendQueryToAll(plastquery,msExecTime);	//�����еļ�Ⱥ������ת��query
	return true;
}
//��������Ч�ļ�Ⱥ������ת��query���Ա����ݿⱣ��ͬ��; lTimeout ��ʱʱ��ms
//ֱ������ͬ��ִ����ϻ�ʱ�ŷ��أ��ɹ�����0,-1��ʱ,�������һ��ͬ��ִ��ʧ�ܷ��صĴ�����
int CSQLClient :: SendQueryToAll(QueryPacket *plastquery,unsigned int msTimeout)
{
	unsigned int idx=0,nSynDB=0,nSynDB_OK=0; //��Ҫͬ�������ݿ������ͬ���ɹ��ĸ���
	std::vector<DBConn *> vecWaitforReply; //�ȴ�ִ�з��ص�conn����
	for(idx=0;idx<m_vecDBConn.size();idx++) vecWaitforReply.push_back(NULL);
	
	int  iSuccess =0;
	CDBServer *dbservers=(CDBServer *)m_proxysvr->m_dbservers;
	for(idx=0;idx<m_vecDBConn.size();idx++)
	{
		if(idx==m_idxRandDBSvr) continue; //��DB��������������ͬ��
		DBConn * pconn=m_vecDBConn[idx];
		if(pconn && pconn->IsConnected())
		{//�Ѿ��������ݿ�,�첽ִ������ͬ��Query
			if( pconn->ExcuteData(m_tdsver,plastquery->GetQueyDataPtr(),plastquery->GetQueryDataSize()) )
			{	vecWaitforReply[idx]=pconn; nSynDB++; 
				//��Ҫͬ���Ŀ϶�����д��������
				dbservers[idx].QueryCountAdd(true); //ִ�м���
			}//�첽�ȴ�ִ�з���
		}else
			plastquery->m_idxSynDB |=(0x01<<idx); //��Ǵ����ݿ�δ����ͬ��
	}
	if(nSynDB==0) return iSuccess; //û����Ҫͬ�������ݿ�
	
	//�ȴ�ת��Queryִ����ϣ����ж��Ƿ�ִ�гɹ�;��С��ʱʱ��Ϊ5S
	//time_t tStartTime=GetTickCount(); 
	long tStartTime=clock();
	long ulMaxWaitforReply=(msTimeout==0)?(MAX_WAITREPLY_TIMEOUT):msTimeout;
	while(true)
	{
		int iNumsWaitforReply=0; //�ȴ�ִ�з��ص����Ӹ���
		for(idx=0;idx<vecWaitforReply.size();idx++)
		{
			DBConn * pconn=vecWaitforReply[idx];
			if(pconn){
				int iStatus=pconn->GetStatus_Asyn();
				if(iStatus!=DBSTATUS_PROCESS){ //��ִ����ϣ��ж��Ƿ�ִ�гɹ�
					if(iStatus!=DBSTATUS_SUCCESS){ //ִ��ʧ��
						iSuccess=iStatus;
						plastquery->m_idxSynDB |=(0x01<<idx); //��Ǵ����ݿ�ͬ������
						dbservers[idx].SetValid(2);
						m_vecDBConn[idx]=NULL; delete pconn; 
					}else//����ͬ��ִ�гɹ�
						nSynDB_OK++; 
					vecWaitforReply[idx]=NULL;
				}else iNumsWaitforReply++; //δִ����ϣ������ȴ�����
			}
		}//?for(idx=0;idx<vecWaitforReply.size();idx++)
		if(iNumsWaitforReply<=0) break;
		if( (clock()-tStartTime)<ulMaxWaitforReply){ ::Sleep(1); continue; }
		iSuccess=-1; break; //��ʱ
	}//?while(true)
	plastquery->m_msSynExec=(int)(clock()-tStartTime);
	plastquery->m_nSynDB =nSynDB_OK+((nSynDB<<16) & 0xffff0000);
	return iSuccess;
}
