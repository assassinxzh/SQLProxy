

#include "stdafx.h"
#include <io.h>
#include <time.h>
#pragma warning(disable:4996)

#include "CDBServer.h"
#include "DebugLog.h"

#define QueryLogDir "./querylog"

//����¼������־�ļ�4M
#define MAX_LOGFILE_SIZE 4*1024*1024
//��־�ļ���ʽ: <4�ֽڱ�ʶ>,���ݿ�host���˿ڣ�dbname��username,userpwd,<2�ֽ�TDSЭ��汾>,
CDBServer :: CDBServer(): m_fpsave(NULL)
{
	m_hMutex = CreateMutex(NULL, false, NULL);
	Init(); //��ʼ����Ϣ
}
CDBServer :: ~CDBServer()
{
	CloseQueryLog();
	if(m_hMutex!=NULL) CloseHandle(m_hMutex);
}
void  CDBServer :: Init() //��ʼ����Ϣ
{
	CloseQueryLog();
	m_iValid=0; 
	m_svrhost=""; m_svrport=0;
	m_username=m_userpwd="";
	m_nSQLClients=m_nQueryCount=m_nWriteCount=m_nDBConnNums=0;
}

void CDBServer :: CloseQueryLog()
{
	if(m_fpsave!=NULL)
		::fclose(m_fpsave);
	m_fpsave=NULL;
}

//������¼�ļ����Ա��¼Query/RPC���(����ʹ��)
bool CDBServer :: OpenQueryLog(const char *fname)
{
	char szLogFile[64],szBuf[128];;
	if(fname==NULL || fname[0]==0)
	{
		if(_access(QueryLogDir,0)==-1)//Ŀ¼�����ڣ�����Ŀ¼
			CreateDirectory(QueryLogDir,NULL);
		time_t tNow=time(NULL);
		struct tm * ltime=localtime(&tNow);
		sprintf(szLogFile,"%s/q_%s_%02d%02d%02d%02d%02d%02d.dat",QueryLogDir,m_svrhost.c_str(),
							ltime->tm_year,ltime->tm_mon+1, ltime->tm_mday, ltime->tm_hour,ltime->tm_min, ltime->tm_sec);
		fname=szLogFile;
	}
	unsigned short usLen,tdsv=0;
	char *ptr_buf=szBuf; int buflen=0;
	::memcpy(ptr_buf,"sql",4);
	ptr_buf+=4; buflen+=4;
	//����д�����ݿ�host���˿ڣ�dbname��username,userpwd
	usLen=(m_svrhost.length()+1);
	*(unsigned short *)ptr_buf=usLen;
	::memcpy(ptr_buf+2,m_svrhost.c_str(),usLen);
	ptr_buf+=(2+usLen); buflen+=(2+usLen);
	*(unsigned short *)ptr_buf=m_svrport;//�˿�
	ptr_buf+=2; buflen+=2;
	usLen=(m_dbname.length()+1);		//dbname
	*(unsigned short *)ptr_buf=usLen;
	::memcpy(ptr_buf+2,m_dbname.c_str(),usLen);
	ptr_buf+=(2+usLen); buflen+=(2+usLen);
	usLen=(m_username.length()+1);		//username
	*(unsigned short *)ptr_buf=usLen;
	::memcpy(ptr_buf+2,m_username.c_str(),usLen);
	ptr_buf+=(2+usLen); buflen+=(2+usLen);
	usLen=(m_userpwd.length()+1);		//userpwd
	*(unsigned short *)ptr_buf=usLen;
	::memcpy(ptr_buf+2,m_userpwd.c_str(),usLen);
	ptr_buf+=(2+usLen); buflen+=(2+usLen);
	*(unsigned short *)ptr_buf=tdsv;//TDSЭ��汾
	ptr_buf+=2; buflen+=2;

	WaitForSingleObject(m_hMutex, INFINITE);
	if( (m_fpsave=::fopen(fname,"wb"))!=NULL )
		::fwrite((void *)szBuf,1,buflen,m_fpsave);
	ReleaseMutex(m_hMutex);
	return (m_fpsave!=NULL);
}

void CDBServer :: SaveQueryLog(unsigned char *ptrData,unsigned int nsize)
{
	if(ptrData==NULL || nsize==0) return;
	if(m_fpsave!=NULL){ //��ǰ�Ѵ���־��¼�ļ�
		if(::ftell(m_fpsave)>MAX_LOGFILE_SIZE)
			CloseQueryLog(); //��־���������µ���־�ļ���¼
	}
	if(m_fpsave==NULL && !OpenQueryLog("")) return;
	
	WaitForSingleObject(m_hMutex, INFINITE);
	::fwrite(ptrData,1,nsize,m_fpsave);
	ReleaseMutex(m_hMutex);
}
