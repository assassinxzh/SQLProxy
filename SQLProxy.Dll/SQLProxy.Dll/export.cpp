// export.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include <time.h>
#include <process.h>
#pragma warning(disable:4996)
#pragma warning(disable:4244)

#include "export.h"
#include "dbclient.h"
#include "CDBServer.h"
#include "ServerSocket.h"
#include "ProxyServer.h"
#include "DebugLog.h"

const char * _PROJECTNAME_="SQLProxy"; //DebugLog��־�����ʶ
#define SQL_PORT 1433
static CDBServer g_dbservers[MAX_NUM_SQLSERVER];
static CProxyServer  g_proxysvr(g_dbservers);
static SQLAnalyse	g_sqlanalyse(g_dbservers);
static CServerSocket g_socketsvr;

extern "C"
SQLPROXY_API void _stdcall SetLogLevel(HWND hWnd,int LogLevel)
{
	if(LogLevel>=LOGLEVEL_DEBUG && LogLevel<=LOGLEVEL_NONE)
		RW_LOG_SETLOGLEVEL((LOGLEVEL)LogLevel);
	if((int)hWnd==1){
		RW_LOG_SETSTDOUT();
	}else if(hWnd!=NULL){
		 RW_LOG_SETHWND((long)hWnd);
	}else RW_LOG_SETDVIEW();
}

//���ü�Ⱥ���ݿ���Ϣ���������в����Լ�SQL��ӡ������˲���
extern "C"
SQLPROXY_API void _stdcall SetParameters(int SetID,const char *szParam)
{
	if(SetID==1)
		g_proxysvr.SetBindServer(szParam);
	else if(SetID==2)
		g_sqlanalyse.SetFilter(szParam);
}

//��ȡ��̨ĳ��ȺSQL����״̬ idx==-1��ȡSQL��Ⱥ������� ==-8��������Query�� ==-9��ǰ����ģʽ����==-10���ع���ģʽ
//proid==0SQL�����Ƿ��������� ==1����SQL����ǰ�����ͻ������� ==3����SQL����ǰ���ش����� ==4д�봦���� 
extern "C"
SQLPROXY_API INTPTR _stdcall GetDBStatus(int idx,int proid)
{
	if(idx==-8) return g_sqlanalyse.Size();
	if(idx==-9) return (INTPTR)g_proxysvr.GetLoadMode();
	if(idx==-10) return g_proxysvr.m_iLoadMode;

	return (INTPTR)g_proxysvr.GetDBStatus(idx,proid);
}

//����SQLProxy�������
extern "C"
SQLPROXY_API bool _stdcall StartProxy(int iport)
{
	g_proxysvr.m_psqlAnalyse=&g_sqlanalyse;
	g_socketsvr.SetListener(&g_proxysvr);
	g_sqlanalyse.Start();
	if(iport<=0) iport=SQL_PORT;
	if (!g_socketsvr.Open(iport,0) )
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"SQL���ݿ⼯Ⱥ��������ʧ�ܣ��˿�(%d)\r\n",iport);
		return false;
	}
	hostent *localHost = gethostbyname("");
	RW_LOG_PRINT(LOGLEVEL_ERROR,0,"SQLProxy Copyright (C) 2014 yyc; email:yycnet@163.com\r\n");
	RW_LOG_PRINT(LOGLEVEL_ERROR,"SQL���ݿ⼯Ⱥ���������� (%s:%d)\r\n",inet_ntoa (*(struct in_addr *)*localHost->h_addr_list),iport);
	RW_LOG_PRINT(LOGLEVEL_ERROR,"�˼�Ⱥ�� %d ̨��̨���ݿ⣬��ǰѡ��ģʽΪ: %s;",g_proxysvr.GetDBNums(),g_proxysvr.GetLoadMode());
	if(g_proxysvr.m_iSynDBTimeOut>=0){
		if(g_proxysvr.m_bRollback){
			(g_proxysvr.m_hSynMutex!=NULL)?
				RW_LOG_PRINT(LOGLEVEL_ERROR,0," ����ͬ��ʧ�ܻع�;"):
				RW_LOG_PRINT(LOGLEVEL_ERROR,0," ����ͬ��ʧ�ܻع�;");
		}
		RW_LOG_PRINT(LOGLEVEL_ERROR,  " ͬ����ʱ: %dms\r\n", g_proxysvr.m_iSynDBTimeOut*1000);
	}else RW_LOG_PRINT(LOGLEVEL_ERROR,0," ͬ����ֹ\r\n");
	return true;
}
extern "C"
SQLPROXY_API void _stdcall StopProxy()
{
	//Socket closeʱ�����WaitForSingleObject�ȴ�socket�Ĺ����߳̽�����
	//��socket�Ĺ����߳��ڽ���ǰ�����ISocketListener�����SocketClosed����
	//SocketClosed�л���ǰUI�̵߳Ĵ���ͨ��SendMessage����ر���Ϣ��SendMessage��һ������ʽ����
	//��UI�̴߳�ʱ����WaitForSingleObject����������2���߳����������޷���Ӧ;
	//�����Ҫ�ر���Ϣ�������(���������������Ϣʱ�Żᵼ�´�����)
	LOGLEVEL ll=RW_LOG_SETLOGLEVEL(LOGLEVEL_NONE);
	g_socketsvr.Close();
	RW_LOG_SETLOGLEVEL(ll); //�ָ�
	g_sqlanalyse.Stop();
	for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
		CDBServer &dbsvr=g_dbservers[idx];
		if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
		dbsvr.CloseQueryLog();
	}
}

static unsigned int __stdcall ExecFileData(void *args);
extern "C"
SQLPROXY_API int _stdcall ProxyCall(int callID,INTPTR lParam)
{
	switch(callID)
	{
	case 1:
		return ExecFileData((void *)lParam);
	}
	return 0;
}

//�򿪼�¼�ļ���ִ�У��򿪳ɹ�����0������ʧ��
//!!!�˺���sqlAnalyse����Ĺ����̺߳�ISocketListener�Ľ������ݴ����̶߳��������־���
//��������־���ȴ�������ҵ�ǰ����ʱ�ڴ����UI�̵߳��ã������������������Ҫ�����������̵߳���
static unsigned int __stdcall ExecFileData(void *args)
{
	std::string fname; fname.assign((const char *)args);
	const char *queryfile=fname.c_str();
	FILE *fp=::fopen(queryfile,"rb");
	if(fp==NULL){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] �򿪼�¼�ļ�ʧ�� - %s\r\n",queryfile); 
		return 1;
	}
	//���ݿ���Ϣ�������ʺ��Լ�Ĭ��TDS�汾
	std::string hostname,dbname,dbuser,dbpswd;
	unsigned short hostport=0, tdsver=0;
	int block_size=8192;

	char tmpbuf[128]; unsigned short usLen=0;
	::fread(tmpbuf,1,4,fp);
	if(strcmp(tmpbuf,"sql")!=0){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] ����ļ�¼�ļ���ʽ - %s\r\n",queryfile); 
		::fclose(fp); return 2;
	}
	::fread(tmpbuf,1,2,fp);						//host
	usLen=*(unsigned short *)tmpbuf;
	if(usLen>0x7f) { ::fclose(fp); return 3; }
	::fread(tmpbuf,1,usLen,fp);
	hostname.assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//port
	hostport=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,2,fp);						//dbname
	usLen=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,usLen,fp);
	dbname .assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//username
	usLen=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,usLen,fp);
	dbuser.assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//userpswd
	usLen=*(unsigned short *)tmpbuf;
	::fread(tmpbuf,1,usLen,fp);
	dbpswd.assign(tmpbuf);
	::fread(tmpbuf,1,2,fp);						//tdsv
	tdsver=*(unsigned short *)tmpbuf; //Ĭ��Э��汾
	if(tdsver<0x700 || tdsver>0x703) tdsver=0x703;

	DBConn dbconn; 
	int lTimeout=15; //Queryִ�н�����ʱ�ȴ�ʱ��s
	int numSuccess=0,numFailed=0; //ִ�м���
	//�趨���ݿ����ӵ�¼��Ϣ
	tds_login_param &loginparams=dbconn.LoginParams();
	loginparams.tdsversion =tdsver; 
	loginparams.tds_block_size=block_size;
	dbconn.Setdbinfo(hostname.c_str(),hostport,dbname.c_str(),dbuser.c_str(),dbpswd.c_str());
	if(!dbconn.Connect()) { ::fclose(fp); return 4; }
	
	int idxRandDBSvr=MAX_NUM_SQLSERVER-1; //����SQL����ʱ��ӡ���
	g_dbservers[idxRandDBSvr].Init();
	g_dbservers[idxRandDBSvr].m_svrhost=hostname;
	g_dbservers[idxRandDBSvr].m_svrport=hostport;
	g_dbservers[idxRandDBSvr].m_dbname=dbname;
	g_dbservers[idxRandDBSvr].m_username=dbuser;
	g_dbservers[idxRandDBSvr].m_userpwd=dbpswd;

	bool bActive=g_sqlanalyse.Active();
	if(!bActive) g_sqlanalyse.Start(); //����SQL������ӡ����

	//ѭ����ȡquery data��ִ��=================begin================
	QueryPacket * plastquery=NULL;
	unsigned char *poutbuf=new unsigned char [block_size]; 
	while(!::feof(fp) && poutbuf!=NULL)
	{
		size_t nReadBytes=::fread(poutbuf,1,sizeof(tds_header),fp);
		if(nReadBytes<sizeof(tds_header)) break; //��������
		bool bLastpacket=(*(poutbuf+1)!=0x0); //�Ƿ�ΪQuery�����һ����
		unsigned short nSize= *(poutbuf+2)*256+*(poutbuf+3);
		if(nSize>block_size) break;
		nReadBytes=fread(poutbuf+sizeof(tds_header),1,(nSize-sizeof(tds_header)),fp);
		if(nReadBytes<(nSize-sizeof(tds_header)) ) 
			break; //��������
		if(plastquery==NULL){
			plastquery=new QueryPacket(idxRandDBSvr,0x0100007f);
			if(plastquery==NULL) break;
			plastquery->SetQueryData(poutbuf,nSize);
			plastquery->IsNotSelectSQL(true); //�ж��Ƿ�Ϊ��select���
		}else plastquery->AddQueryData(poutbuf,nSize);
		if(!dbconn.IsConnected() || !dbconn.ExcuteData(0,poutbuf,nSize)) break;
		if(bLastpacket){//Query�����һ��Packet,�ȴ���������Ӧ
			//Queryִ�н����-1��ʱ
			plastquery->m_staExec=dbconn.WaitForResult(NULL,0,lTimeout);
			plastquery->m_msExec=clock()-plastquery->m_msExec; //ִ��ʱ��(ms)
			(plastquery->m_staExec==0)?++numSuccess:++numFailed;
			if(!g_sqlanalyse.AddQuery(plastquery,NULL,0)) delete plastquery;
			plastquery=NULL; //��ӵ�SQL��������ӡ������
		}
	}//?while(!::feof(fp) && poutbuf!=NULL)
	delete[] poutbuf;
	if(plastquery!=NULL) delete plastquery;
	//ѭ����ȡquery data��ִ��=================End================
	while(g_sqlanalyse.Size()!=0) ::Sleep(100); //�ȴ�ֱ����ӡ�������
	RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] �򿪼�¼�ļ� %s �ɹ�\r\n���� Query Data to %s:%d ִ�гɹ���%d ִ��ʧ�ܣ�%d\r\n",
								queryfile,hostname.c_str(),hostport,numSuccess,numFailed);
	
	dbconn.Close(); 
	if(!bActive) g_sqlanalyse.Stop(); //ֹͣSQL������ӡ����
	::fclose(fp); return 0;
}
