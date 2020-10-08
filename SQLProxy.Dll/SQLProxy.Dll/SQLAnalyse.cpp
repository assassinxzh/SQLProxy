
#include "stdafx.h"
#include <process.h>
#include <time.h>
#include <sstream>		//ostringstream
#include <assert.h>
#include <regex>
#pragma warning(disable:4996)

#include "mytds.h"
#include "mytdsproto.h"
#include "CDBServer.h"
#include "Socket.h"
#include "SQLAnalyse.h"
#include "DebugLog.h"
extern //defined in DebugLog.cpp
int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps);

static unsigned int __stdcall ThreadProc(void *lpParameter)
{
	((SQLAnalyse *)lpParameter)->Run();
	return 0;
}

SQLAnalyse :: SQLAnalyse(void *dbservers)
{
	m_bStopped = true;
	m_hThread=NULL;
	m_dbservers=dbservers;
	m_hMutex = CreateMutex(NULL, false, NULL);
	
	m_iLogQuerys=0; 
	m_bPrintSQL=false;
	m_iSQLType=0;
	m_msExec=0;
	m_staExec=0;
	m_clntip=INADDR_NONE;
	m_pRegExpress=NULL;
}
SQLAnalyse :: ~SQLAnalyse()
{
	Stop();
	if(m_pRegExpress!=NULL) delete (std::tr1::wregex *)m_pRegExpress;
	if(m_hMutex!=NULL) CloseHandle(m_hMutex);
}

//����SQL��ӡ������˲���
void SQLAnalyse ::SetFilter(const char *szParam)
{
	std::map<std::string,std::string> maps;
	std::map<std::string,std::string>::iterator it;
	if(SplitString(szParam,' ',maps)>0)
	{
		m_bPrintSQL=false; //�ȹرմ�ӡ���
		::Sleep(300); //�ӳ٣���������ʹ��std::tr1::wregex����
		std::tr1::wregex *pRegExpress=(std::tr1::wregex *)m_pRegExpress;
		m_pRegExpress=NULL;
		m_clntip=INADDR_NONE; //������ip����SQL���
		if( (it=maps.find("sqltype"))!=maps.end() )
			m_iSQLType=atoi((*it).second.c_str());
		if( (it=maps.find("msexec"))!=maps.end() )
			m_msExec=atoi((*it).second.c_str());
		if( (it=maps.find("staexec"))!=maps.end() )
			m_staExec=atoi((*it).second.c_str());
		if( (it=maps.find("ipfilter"))!=maps.end() )
		{
			const char *ptr=(*it).second.c_str();
			while(*ptr==' ') ptr++;
			if(*ptr!=0) m_clntip=CSocket::Host2IP(ptr);
		}
		if( (it=maps.find("sqlfilter"))!=maps.end() )
		{
			const char *ptr=(*it).second.c_str();
			while(*ptr==' ') ptr++;
			if(*ptr!=0){
				// ���ʽѡ�� - ���Դ�Сд
				std::tr1::regex_constants::syntax_option_type fl = std::tr1::regex_constants::icase;
				std::wstring wstrFilter=MultibyteToWide((*it).second);
				m_pRegExpress=(new std::tr1::wregex(wstrFilter,fl));
			}
		}
		if(pRegExpress!=NULL) delete pRegExpress;
		m_bPrintSQL=true;
	}else m_bPrintSQL=false;
}

bool SQLAnalyse ::  Start()
{
	Stop();
	
	unsigned int id;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, ThreadProc, this, 0, &id);  //begin thread
	m_bStopped=(m_hThread!=0 && (long)m_hThread!=-1)?false:true;
	if(m_bStopped) m_hThread=NULL;
	return (!m_bStopped);
}

void SQLAnalyse :: Stop()
{
	m_bStopped = true;
	if (m_hThread != NULL)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread); m_hThread=NULL;
	}
	std::deque<QueryPacket *>::iterator it=m_QueryLists.begin();
	for(;it!=m_QueryLists.end();it++)
		delete (*it);
	m_QueryLists.clear();
}

bool SQLAnalyse::AddQuery(QueryPacket *pquery,void * pinstream,unsigned short tdsver)
{
	if(m_bStopped) return false;
	//if( -1==m_iLogQuerys &&	m_bPrintSQL)
	//	NULL; //���ָ����¼����sql��ָ���˴�ӡ����ӵ���������
	//else 
	if(!m_bPrintSQL && pquery->m_idxSynDB==0 )
		return false; //�������ӡSQL��û��δͬ�������ݿ�
		
	if(pinstream!=NULL)
		pquery->m_staExec=parse_reply(pinstream,tdsver,&pquery->m_nAffectRows);
	WaitForSingleObject(m_hMutex, INFINITE);
	if(m_QueryLists.size()>3000){
		std::deque<QueryPacket *>::iterator it=m_QueryLists.begin();
		for(;it!=m_QueryLists.end();it++)
			delete (*it);
		m_QueryLists.clear();
	}
	m_QueryLists.push_back(pquery);
	ReleaseMutex(m_hMutex);
	return true;
}

//ֻ�����ӵ���������ִ�гɹ��ķǲ�ѯ���Ż�������ݿ�ͬ��
//vecQuery - ��������ִ�гɹ���Query (������ѯ���)
void SQLAnalyse::LogQuerys(std::vector<QueryPacket *> &vecQuery,bool bSynErr)
{
	QueryPacket *pquery=NULL;
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	if(bSynErr){ //������¼ͬ��ʧ�ܵ�Querys
		for(unsigned int idx=0;idx<vecQuery.size();idx++)
		{
			pquery=vecQuery[idx];
			if(pquery->m_idxSynDB==0) continue; //����ͬ���Ļ�ͬ���ɹ�Query		
			for(unsigned int n=0;n<MAX_NUM_SQLSERVER;n++)
			{
				CDBServer &sqlsvr=dbservers[n];
				if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") break; 
				if( ((0x01<<n) & pquery->m_idxSynDB)==0 )continue;
				sqlsvr.SaveQueryLog(pquery->GetQueyDataPtr(),pquery->GetQueryDataSize());
			}
		}
		return;
	}
	//��¼����������ӡ�����Query Data(������)
	for(unsigned int idx=0;idx<vecQuery.size();idx++)
	{
		pquery=vecQuery[idx];
		CDBServer &sqlsvr=dbservers[pquery->m_idxDBSvr];
		sqlsvr.SaveQueryLog(pquery->GetQueyDataPtr(),pquery->GetQueryDataSize());
	}
}

void SQLAnalyse::PrintQuerys(std::vector<QueryPacket *> &vecQuery,int LogLevel)
{
	QueryPacket *pquery=NULL;
	std::ostringstream outstream;
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	//-1��¼����������ӡ�����Query Data(������)
	if(-1==m_iLogQuerys) LogQuerys(vecQuery,false);

	if(vecQuery[0]->m_bTransaction)
	{
		pquery=vecQuery[0]; int idxDBSvr=pquery->m_idxDBSvr;
		bool bNotSelectSQL=false; //�Ƿ�����Ҫͬ���ķǲ�ѯ���
		RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d ��ʼ������...\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport);
		for(unsigned int i=0;i<vecQuery.size();i++){
			vecQuery[i]->PrintQueryInfo(outstream); bNotSelectSQL |=vecQuery[i]->IsNotSelectSQL(false); }
		if(pquery->m_staExec==0){
			RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d ��������� '�ύ'����ʱ: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport,pquery->m_msExec);
		}else if(pquery->m_staExec==0x08){//����ع� Rollback
			RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d ��������� '�ع�'����ʱ: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport,pquery->m_msExec);
		}else{
			RW_LOG_PRINT(outstream,"[SQLProxy] %s:%d ��������� '0x%02X'����ʱ: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),dbservers[idxDBSvr].m_svrport,pquery->m_staExec,pquery->m_msExec);
		}
		if(pquery->m_nSynDB!=0)
		{//��Ҫͬ�������ݿ������ͬ���ɹ��ĸ���
			unsigned int nSynDB=((pquery->m_nSynDB & 0xffff0000)>>16),nSynDB_OK=(pquery->m_nSynDB & 0x0000ffff); 
			if(pquery->m_staSynExec!=0){
				if(pquery->m_staSynExec==-1)
					RW_LOG_PRINT(outstream,"[SQLProxy] ���ݿ�ͬ��ִ��ʧ��, ��Ӧ��ʱ - %d/%d\r\n",nSynDB_OK,nSynDB);
				else if(pquery->m_staSynExec==-2)
					RW_LOG_PRINT(outstream,"[SQLProxy] ���ݿ�ͬ��ִ��ʧ��, �����ѶϿ� - %d/%d\r\n",nSynDB_OK,nSynDB);
				else{
					unsigned short tds_token=(pquery->m_staSynExec & 0x0000ffff),bit_flags=((pquery->m_staSynExec & 0xffff0000)>>16);
					RW_LOG_PRINT(outstream,"[SQLProxy] ���ݿ�ͬ��ִ��ʧ��, Token=%02X %02X - %d/%d\r\n",tds_token, bit_flags,nSynDB_OK,nSynDB);
				}
			}else 
				RW_LOG_PRINT(outstream,"[SQLProxy] ���ݿ�ͬ��ִ�гɹ�����ʱ:%d ms - %d/%d\r\n",pquery->m_msSynExec,nSynDB_OK,nSynDB);
			if(pquery->m_idxSynDB!=0) //��ͬ��ʧ�ܵ����ݿ⣬ĳλ��1��˵���÷�����ͬ��ʧ��
			{
				RW_LOG_PRINT(outstream,0,"[SQLProxy] δͬ����ͬ��ʧ�ܵ����ݿ�:");
				for(unsigned int n=0;n<MAX_NUM_SQLSERVER;n++)
				{
					CDBServer &sqlsvr=dbservers[n];
					if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="" ) break; 
					if( ((0x01<<n) & pquery->m_idxSynDB)==0 )continue;
					RW_LOG_PRINT(outstream," %s:%d",sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport);
				}
				RW_LOG_PRINT(outstream,0,"\r\n");
			}//?if(pquery->m_idxSynDB!=0) //��ͬ��ʧ�ܵ����ݿ⣬ĳλ��1��˵���÷�����ͬ��ʧ��
		}
	}else //��������
	{ 
		for(unsigned int idx=0;idx<vecQuery.size();idx++)
		{
			pquery=vecQuery[idx]; int idxDBSvr=pquery->m_idxDBSvr;
			bool bNotSelectSQL=pquery->IsNotSelectSQL(false);
			pquery->PrintQueryInfo(outstream);
			unsigned short tds_token=(pquery->m_staExec & 0x0000ffff),bit_flags=((pquery->m_staExec & 0xffff0000)>>16);
			if(pquery->m_staExec==0)
			{//д���ѯ�ɹ�
				if(pquery->m_nAffectRows!=(unsigned int)-1)
					RW_LOG_PRINT(outstream,"[SQLProxy] ������(%s:%d) %sִ�гɹ�����ʱ: %d ms����Ӱ��(����)����: %d\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"д��":"��ѯ",pquery->m_msExec,pquery->m_nAffectRows);
				else
					RW_LOG_PRINT(outstream,"[SQLProxy] ������(%s:%d) %sִ�гɹ�����ʱ: %d ms\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"д��":"��ѯ",pquery->m_msExec);
			}else if(pquery->m_staExec==-1)
				RW_LOG_PRINT(outstream,"[SQLProxy] ������(%s:%d) %sִ��ʧ��, ��Ӧ��ʱ\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							 dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"д��":"��ѯ");
			else //д���ѯʧ��
				RW_LOG_PRINT(outstream,"[SQLProxy] ������(%s:%d) %sִ��ʧ��,Token=%02X %02X\r\n",dbservers[idxDBSvr].m_svrhost.c_str(),
							dbservers[idxDBSvr].m_svrport, bNotSelectSQL?"д��":"��ѯ",tds_token, bit_flags);
			if(pquery->m_nSynDB!=0) 
			{ //��Ҫͬ�������ݿ������ͬ���ɹ��ĸ���
				unsigned int nSynDB=((pquery->m_nSynDB & 0xffff0000)>>16),nSynDB_OK=(pquery->m_nSynDB & 0x0000ffff); 
				if(pquery->m_staSynExec!=0){
					if(pquery->m_staSynExec==-1)
						RW_LOG_PRINT(outstream,"[SQLProxy] ���ݿ�ͬ��ִ��ʧ��, ��Ӧ��ʱ - %d/%d\r\n",nSynDB_OK,nSynDB);
					else{
						tds_token=(pquery->m_staSynExec & 0x0000ffff),bit_flags=((pquery->m_staSynExec & 0xffff0000)>>16);
						RW_LOG_PRINT(outstream,"[SQLProxy] ���ݿ�ͬ��ִ��ʧ��, Token=%02X %02X - %d/%d\r\n",tds_token, bit_flags,nSynDB_OK,nSynDB);
					}
				}else 
					RW_LOG_PRINT(outstream,"[SQLProxy] ���ݿ�ͬ��ִ�гɹ�����ʱ:%d ms - %d/%d\r\n",pquery->m_msSynExec,nSynDB_OK,nSynDB);
				if(pquery->m_idxSynDB!=0) //��ͬ��ʧ�ܵ����ݿ⣬ĳλ��1��˵���÷�����ͬ��ʧ��
				{
					RW_LOG_PRINT(outstream,0,"[SQLProxy] δͬ����ͬ��ʧ�ܵ����ݿ�:");
					for(unsigned int n=0;n<MAX_NUM_SQLSERVER;n++)
					{
						CDBServer &sqlsvr=dbservers[n];
						if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="" ) break; 
						if( ((0x01<<n) & pquery->m_idxSynDB)==0 )continue;
						RW_LOG_PRINT(outstream," %s:%d",sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport);
					}
					RW_LOG_PRINT(outstream,0,"\r\n");
				}//?if(pquery->m_idxSynDB!=0) //��ͬ��ʧ�ܵ����ݿ⣬ĳλ��1��˵���÷�����ͬ��ʧ��
			}
		}//?for(idx=0;idx<vecQuery.size();idx++)
	}
	
	long outlen=outstream.tellp(); //��ӡ�����������Ϣ
	if(outlen>0) RW_LOG_PRINT((LOGLEVEL)LogLevel,outlen,outstream.str().c_str());
}

bool SQLAnalyse::MatchQuerys(std::vector<QueryPacket *> &vecQuery)
{
	if(m_clntip!=INADDR_NONE){ //ָ���˿ͻ���ip
		if(vecQuery[0]->m_clntip!=m_clntip) return false;
	}
	if(m_pRegExpress==NULL) return true;
	std::tr1::wregex *pRegExpress=(std::tr1::wregex *)m_pRegExpress;
	std::tr1::wsmatch ms; //������ҽ��
	for(unsigned int idx=0;idx<vecQuery.size();idx++)
	{
		std::wstring w_strsql=vecQuery[idx]->GetQuerySQL(NULL);
		if(regex_search(w_strsql, ms, *pRegExpress))
			return true; //���ҵ�
	}
	return false;
}

void SQLAnalyse::Run(void)
{
	QueryPacket *pquery=NULL;
	std::vector<QueryPacket *> vecQuery;
	while (!m_bStopped)
	{
		WaitForSingleObject(m_hMutex, INFINITE);
		while(!m_QueryLists.empty()){
			pquery=m_QueryLists.front();
			m_QueryLists.pop_front();
			if(pquery==NULL) break;
			vecQuery.push_back(pquery);
			if(pquery->m_nMorePackets==0) break; //Ҫ������������һ����
		}
		ReleaseMutex(m_hMutex);
		if(vecQuery.empty()) { ::Sleep(100); continue; }
		
		pquery=vecQuery[0];
		if(pquery->m_staSynExec!=0) //ͬ��ִ��ʧ�ܵ�ʼ�մ�ӡ���(ERROR/INFO)
			PrintQuerys(vecQuery,(pquery->m_staSynExec==-1)?LOGLEVEL_INFO:LOGLEVEL_ERROR);
		else if(m_bPrintSQL)
		{	
			bool bPrintQuery=false;
			bool bNotSelectSQL=pquery->IsNotSelectSQL(false);
			if( m_iSQLType==0) bPrintQuery=true;
			else if( (m_iSQLType & 4)!=0) //ָ�����������
			{//��������������ͷ������������
				if( pquery->m_bTransaction ||  //����
					( (bNotSelectSQL && (m_iSQLType & 2)!=0 ) ||		//д��
					 (!bNotSelectSQL && (m_iSQLType & 1)!=0 ) 			//��ѯ
					) ) bPrintQuery=true;
			}else{ //û��ָ��������񣬽���������������ķ��������
				if(!pquery->m_bTransaction &&  //������
				   ( (bNotSelectSQL && (m_iSQLType & 2)!=0 ) ||			//д��
					 (!bNotSelectSQL && (m_iSQLType & 1)!=0 ) 			//��ѯ
					) ) bPrintQuery=true;
			}
			
			if(bPrintQuery)	
			{
				if(pquery->m_msExec>=m_msExec)						//ִ��ʱ��
				{
					if( m_staExec==0 ||								//ִ��״̬ ȫ��
					   (pquery->m_staExec==0 && (m_staExec & 1)!=0) ||  //�ɹ�
					   (pquery->m_staExec!=0 && (m_staExec & 2)!=0) )   //ʧ��
					{
//======================================��ӡ��� Beign================================================================
							if(MatchQuerys(vecQuery)) PrintQuerys(vecQuery,LOGLEVEL_ERROR);
//======================================��ӡ���  End ================================================================
					}//��ӡִ�гɹ���ʧ�ܵ����
				}//��ӡָ��ִ��ʱ���SQL
			}//��ӡָ�����͵�SQL  ��ѯ��д�룬����
		}//?else if(m_bPrintSQL)
		//ֻ�����ӵ���������ִ�гɹ��ķǲ�ѯ���Ż�������ݿ�ͬ��������δͬ����ͬ�������SQL��¼
		if(0==pquery->m_staExec && 1==m_iLogQuerys) LogQuerys(vecQuery,true);
		for(unsigned int idx=0;idx<vecQuery.size();idx++) delete vecQuery[idx];
		vecQuery.clear();
	}//?while (!m_bStopped)
}

//=================ReplyPacket class==========================================
ReplyPacket::ReplyPacket()
{
	m_tdsver=0;
	m_preplydata=NULL;
	m_datasize=m_nAllocated=0;

	m_iresult=0;
	m_nAffectRows=0;
}
ReplyPacket::~ReplyPacket()
{
	if(m_preplydata!=NULL)
		delete[] m_preplydata;
}
void ReplyPacket::SetReplyData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_preplydata && m_nAllocated<nsize)
	{
		delete[] m_preplydata; m_preplydata=NULL;
	}
	if(m_preplydata==NULL){
		m_datasize=0;
		m_nAllocated=(nsize<4096)?4096:nsize;
		if( (m_preplydata=new unsigned char[m_nAllocated])==NULL ) 
			return;
	}
	::memcpy(m_preplydata,ptrData,nsize);
	m_datasize=nsize;
}
void ReplyPacket::AddReplyData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_preplydata && 
		(m_nAllocated-m_datasize)>= nsize )
	{
		::memcpy(m_preplydata+m_datasize,ptrData,nsize);
		m_datasize+=nsize; return;
	}
	
	m_nAllocated+=nsize;
	unsigned char *preplydata=new unsigned char[m_nAllocated];
	if(preplydata==NULL) return;
	if(m_preplydata!=NULL) ::memcpy(preplydata,m_preplydata,m_datasize);
	::memcpy(preplydata+m_datasize,ptrData,nsize);
	
	delete[] m_preplydata;
	m_preplydata=preplydata; m_datasize+=nsize;	
}

//=================QueryPacket class==========================================
QueryPacket::QueryPacket(int idxDBSvr,unsigned long ipAddr,unsigned int nAlloc)
: m_idxDBSvr(idxDBSvr),m_clntip(ipAddr)
{
	
	m_bNotSelectSQL=true;
	m_atExec=0;	//��ʼִ��ʱ��(s)
	m_msExec=0;	//ִ��ʱ��ms,Ĭ�ϳ�ʼ��Ϊ��ʼʱ��(ms)
	m_msSynExec=0;
	m_nAffectRows=(unsigned int)-1;
	m_staExec=0;		//ִ��״̬ 0 �ɹ�,����ʧ��״̬�롣
	m_staSynExec=0;	//ͬ��ִ��״̬ 0 �ɹ�
	m_nSynDB=m_idxSynDB=0;
	m_bTransaction=false; //�Ƿ�Ϊ����packet
	m_nMorePackets=0; //ָ����������N��packet��һ���������������	
	
	if( (m_nAllocated=nAlloc)!=0) 
	{
		if( (m_pquerydata=new unsigned char[m_nAllocated])==NULL )
			m_nAllocated=0;
	}else m_pquerydata=NULL;
	m_datasize=0;
}

QueryPacket::~QueryPacket()
{
	if(m_pquerydata!=NULL)
		delete[] m_pquerydata;
}

void QueryPacket::SetQueryData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_pquerydata && m_nAllocated<nsize)
	{
		delete[] m_pquerydata; m_pquerydata=NULL;
	}
	if(m_pquerydata==NULL){
		m_datasize=0;
		//m_nAllocated=( *(ptrData+1)!=0)?nsize:8*1024; //yyc removed 2017-11-05
		//yyc modify 2017-11-05 �����ж������⣬Ӧ�ø��ݽ�������־Ϊ�ж϶������ֽ��ж�
		m_nAllocated=IS_LASTPACKET(*(ptrData+1))?nsize:8*1024;
		if(m_nAllocated<nsize) m_nAllocated=nsize;
		if( (m_pquerydata=new unsigned char[m_nAllocated])==NULL )
			return;
	}
	m_datasize=nsize;
	::memcpy(m_pquerydata,ptrData,nsize);
	m_atExec=time(NULL);	 //��¼��ʼ��ʼִ��ʱ��(s)
	m_msExec=clock(); //ִ��ʱ��ms,Ĭ�ϳ�ʼ��Ϊ��ʼʱ��(ms)
}
void QueryPacket::AddQueryData(unsigned char *ptrData,unsigned int nsize)
{
//	if(ptrData==NULL || nsize==0) return;
	if( m_pquerydata && 
		(m_nAllocated-m_datasize)>= nsize )
	{
		::memcpy(m_pquerydata+m_datasize,ptrData,nsize);
		m_datasize+=nsize; return;
	}
	
	m_nAllocated+=nsize;
	unsigned char *pquerydata=new unsigned char[m_nAllocated];
	if(pquerydata==NULL) return;
	if(m_pquerydata!=NULL) ::memcpy(pquerydata,m_pquerydata,m_datasize);
	::memcpy(pquerydata+m_datasize,ptrData,nsize);
	
	delete[] m_pquerydata;
	m_pquerydata=pquerydata;
	m_datasize+=nsize; return;	
}

bool QueryPacket::IsNotSelectSQL(bool bParseQuery)
{
	//if(m_datasize==0 || m_pquerydata==NULL) return false;
	if(m_pquerydata==NULL) return false;
	if(!bParseQuery) return m_bNotSelectSQL;
	wchar_t * qry_strsql=NULL; //��ѯ����洢��������
	
	tds_header tdsh; 
	unsigned char *pinstream=m_pquerydata;
	pinstream+=parse_tds_header(pinstream,tdsh);
	pinstream=(IS_TDS9_QUERY_START(pinstream))?(pinstream+TDS9_QUERY_START_LEN):pinstream;
	if(tdsh.type ==TDS_QUERY)
	{
		qry_strsql=(wchar_t *)pinstream; //��ѯ�����ʼ
	}else if(tdsh.type ==TDS_RPC)
	{
		unsigned short qry_strsql_len=0;//��ѯ����洢���������ֽڳ���
		unsigned short sp_flag=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
		if(sp_flag==0xFFFF){
			//0xFFFF+INT16(���ñ��)+INT16(Flags)+INT16+INT8(����Type) + INT16(�ֽڳ���) +INT8[5](collate info TDS7.1+)+ INT16(�ֽڳ���)
			pinstream+=0x0E; //skip 14�ֽ�
			qry_strsql_len=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
			qry_strsql=(wchar_t *)pinstream; //��ѯ�����ʼ
		}else
		{	//sp_flagΪ�洢��������UCS���ȣ����ֽڳ���
			qry_strsql_len=sp_flag*sizeof(short); //�洢���������ֽڳ���
			qry_strsql=(wchar_t *)pinstream; //�洢����������ʼ
		}
	}
	if(qry_strsql==NULL) return false;
	while(*qry_strsql==L' ') qry_strsql++; //ȥ��ǰ���ո�
	return m_bNotSelectSQL=(_wcsnicmp(qry_strsql,L"select ",7)!=0);
}
//����sql����ַ���(unicode����)
static void  GetSqlParamDef(void *ptr_param,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params);
static void GetSqlParamVal(void *ptr_param_1,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params);
std::wstring QueryPacket::GetQuerySQL(void *pvecsql_params)
{
	if(m_datasize==0 || m_pquerydata==NULL) 
		return std::wstring();
	wstring w_strsql; //����sql���(unicode����)

	unsigned int nCount=0;
	unsigned char *ptrData=m_pquerydata;
	while(nCount<m_datasize)
	{
		tds_header tdsh; 
		unsigned char *pinstream=ptrData;
		pinstream+=parse_tds_header(pinstream,tdsh);
		pinstream=(IS_TDS9_QUERY_START(pinstream))?(pinstream+TDS9_QUERY_START_LEN):pinstream;
		if(tdsh.type ==TDS_QUERY)
		{
			wchar_t * qry_strsql=(wchar_t *)pinstream; //��ѯ�����ʼ
			//��ѯ��䳤��(�ֽڳ���)
			unsigned int qry_strsql_len=tdsh.size-(pinstream-ptrData);
			w_strsql.append(qry_strsql,(qry_strsql_len>>1) );
		}else if(tdsh.type ==TDS_RPC)
		{
			wchar_t * qry_strsql=NULL;	//��ѯ����洢��������
			unsigned short qry_strsql_len=0;//��ѯ����洢���������ֽڳ���
			unsigned short qry_param_def_len=0; //�������Ͷ����ַ����ֽڳ���
			//����������ʽΪ @������ ����(����),@������ ����(����),...Ʃ�� @a1 char(10),@a2 varchar(20), @a3 int...
			unsigned char * qry_param_def=NULL;	//unicode �ַ���
			unsigned short qry_param_val_len=0; //����ֵ���岿���ֽڳ���
			//��ʽ:INT8(����������) + ������(UCS) + INT8(Flags) + INT8(��������Type) + INT8(�����ֽڳ���) + INT8(option)+....
			unsigned char * qry_param_val=NULL;

			unsigned short sp_flag=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
			if(sp_flag==0xFFFF){
				//0xFFFF+INT16(���ñ��)+INT16(Flags)+INT16+INT8(����Type) + INT16(�ֽڳ���) +INT8[5](collate info TDS7.1+)+ INT16(�ֽڳ���)
				pinstream+=0x0E; //skip 14�ֽ�
				qry_strsql_len=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
				qry_strsql=(wchar_t *)pinstream; //��ѯ�����ʼ
				//����sql��䲿�ֺͿս�����'\0'
				pinstream+=(qry_strsql_len+0x02);
				//��������������Ϣ0x0A�ֽ�; INT8(����Type) + INT16(�ֽڳ���) +INT8[5](collate info TDS7.1+)+ INT16(�ֽڳ���)
				pinstream+=0x08; //skip
				qry_param_def_len=*((unsigned short *)pinstream); pinstream+=sizeof(unsigned short);
				qry_param_def=pinstream;
				pinstream+=qry_param_def_len; //skip �������Ͷ��岿��
				//����Ϊ������ֵ��Ϣ
				qry_param_val=pinstream;
				qry_param_val_len=tdsh.size-(pinstream-ptrData);
			}else
			{	//sp_flagΪ�洢��������UCS���ȣ����ֽڳ���
				qry_strsql_len=sp_flag*sizeof(short); //�洢���������ֽڳ���
				qry_strsql=(wchar_t *)pinstream; //�洢����������ʼ
				//����sql��䲿�ֺͿս�����'\0'
				pinstream+=(qry_strsql_len+0x02);
				//����Ϊ������ֵ��Ϣ
				qry_param_val=pinstream;
				qry_param_val_len=tdsh.size-(pinstream-ptrData);
			}
			assert(qry_strsql_len<0x7fff);
			w_strsql.append(qry_strsql,(qry_strsql_len>>1) );
			if(pvecsql_params!=NULL)
			{//���洫��sql���Ĳ�����Ϣ
				std::vector<tds_sql_param *> &vecsql_params=*(std::vector<tds_sql_param *> *)pvecsql_params;
				if(qry_param_def!=NULL)
					GetSqlParamDef(qry_param_def,qry_param_def_len,vecsql_params);
				if(qry_param_val!=NULL)
					GetSqlParamVal(qry_param_val,qry_param_val_len,vecsql_params);
			}//?if(pvecsql_params!=NULL)
		}

		nCount+=tdsh.size;
		ptrData+=tdsh.size;
	}//?while(nCount<m_datasize)
	return w_strsql;
}

void QueryPacket::PrintQueryInfo(std::ostream &os)
{
	if(m_datasize==0 || m_pquerydata==NULL)
		return;
	//���洫��sql���Ĳ�����Ϣ
	std::vector<tds_sql_param *> vecsql_params; 
	//����sql���(unicode����)
	wstring w_strsql=GetQuerySQL(&vecsql_params); 
	if(w_strsql[0]==0) return;
	
	struct tm * ltime=localtime(&m_atExec);
	char szDateTime[64]; //��ӡsql���ִ��ʱ��
	sprintf(szDateTime,"[%04d-%02d-%02d %02d:%02d:%02d] from %s %s\r\n",
		(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, ltime->tm_hour,
		ltime->tm_min, ltime->tm_sec,CSocket::IP2A(m_clntip),
		IsNotSelectSQL(false)?"*":"");
	os<<szDateTime;
	printUnicodeStr(w_strsql.c_str(),w_strsql.length(),os,true);
	
	std::vector<tds_sql_param *>::iterator it=vecsql_params.begin();
	//��ӡ���� @������ ��������(����) ֵ\r\n
	for(;it!=vecsql_params.end();it++)
	{
		tds_sql_param *p=*it;
		if(p!=NULL) print_sqlparam(*p,os);
	}
}

//����������ʽΪ @������ ����(����),@������ ����(����),...Ʃ�� @a1 char(10),@a2 varchar(20),...
//����������Ϊunicode�ַ�����param_len����Ϊ�ֽڳ���
static void  GetSqlParamDef(void *ptr_param,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params)
{
	unsigned int nCount=0;
	wchar_t *param=(wchar_t *)ptr_param;
	wchar_t *ptr_next=param;
	while(true)
	{
		if(*ptr_next==L',' || 
			nCount>=param_len )
		{
			tds_sql_param *p=new tds_sql_param;
			if(p==NULL) break; //�ڴ����ʧ��
			unsigned int nSize=(ptr_next-param);
			if(parse_sqlparam_def(param,nSize,*p)==0){ delete p; break;}
			vecsql_params.push_back(p);
			
			if(nCount>=param_len) break;
			param=ptr_next+1;
		}
		ptr_next++; nCount+=sizeof(wchar_t);
	}
}

//��ʽ: 1�ֽ� ����������+������+9�ֽ�(δ֪����)+2�ֽ�ֵ����+ֵ
//param_len����Ϊ�ֽڳ���
static void GetSqlParamVal(void *ptr_param_1,unsigned int param_len,std::vector<tds_sql_param *> &vecsql_params)
{
	unsigned int nCount=0;
	unsigned char *ptr_param=(unsigned char *)ptr_param_1;
	while(nCount<param_len)
	{
		tds_sql_param *p1=new tds_sql_param;
		if(p1==NULL) return;
		unsigned int nSkip=parse_sqlparam_val(ptr_param,*p1);
		if(nSkip==0){ delete p1; break; }
		nCount+=nSkip; ptr_param+=nSkip;

		std::vector<tds_sql_param *>::iterator it=vecsql_params.begin();
		for(;it!=vecsql_params.end();it++)
		{
			tds_sql_param *p0=*it;
			if(p0 && p0->name==p1->name)
			{
				p1->type_name=p0->type_name;
				*it=p1; delete p0;
				break;
			}
		}
		if(it==vecsql_params.end())
			vecsql_params.push_back(p1);
	}//?while(nCount<param_len)
}
