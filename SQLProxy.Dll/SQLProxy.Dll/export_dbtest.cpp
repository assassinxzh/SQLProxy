#include "stdafx.h"
#pragma warning(disable:4996)

#include "export.h"
#include "dbclient.h"
#include "DebugLog.h"

//���ݿ���ͨ�Բ���,����0 �ɹ� 1 ��Ч��IP������ 2���������ɴ� 3 ���񲻿��� 4�ʺ�������� 5���ݿ�������
extern int PingHost(const char *szHost,unsigned long ipAddr);
extern "C"
SQLPROXY_API int _stdcall DBTest(const char  *szHost,int iPort,const char *szUser,const char *szPswd,const char *dbname)
{
	if(iPort<=0) iPort=1433;
	RW_LOG_PRINT(LOGLEVEL_ERROR,"���������ݿ� %s:%d ����ȴ�...\r\n",szHost,iPort);
	//1��IP����������Ч�Լ��
	if(szHost==NULL || szHost[0]==0 || CSocket::Host2IP(szHost)==INADDR_NONE)
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"���ݿ� %s:%d �쳣����Ч��IP��������\r\n\r\n",szHost,iPort);
		return 1;
	}else
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"��������飺��Ч��IP��������\r\n");
	//2��������Ч��ping ��飬�ж������Ƿ�ɵ���(��Щ�����ر���Ping�����Ping��ͨ������������)
	if(PingHost(szHost,CSocket::Host2IP(szHost))==-1)
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"���ݿ� %s:%d �쳣���������ɴ��ping����ֹ\r\n\r\n",szHost,iPort);
		return 2;
	}else
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"Ping��飺�����ɷ���\r\n");
	//3����������ָ���ķ���Ͷ˿�
	CSocket sock(SOCK_STREAM);
	if( !sock.Connect(szHost,iPort,NULL))
	{
		RW_LOG_PRINT(LOGLEVEL_ERROR,"���ݿ� %s:%d �쳣������δ������˿ڴ���\r\n\r\n",szHost,iPort);
		return 3;
	}else
		RW_LOG_PRINT(LOGLEVEL_ERROR,0,"�����飺���ݿ����������\r\n");
	sock.Close();
	
	//4������TDSЭ�� ���Ե�¼���ݿ�
	//Socket closeʱ�����WaitForSingleObject�ȴ�socket�Ĺ����߳̽�����
	//��socket�Ĺ����߳��ڽ���ǰ�����ISocketListener�����SocketClosed����
	//SocketClosed�л���ǰUI�̵߳Ĵ���ͨ��SendMessage����ر���Ϣ��SendMessage��һ������ʽ����
	//��UI�̴߳�ʱ����WaitForSingleObject����������2���߳����������޷���Ӧ;���̴߳�ӡ�������������������
	//�����Ҫ�ر���Ϣ�������(���������������Ϣʱ�Żᵼ�´�����)
	LOGLEVEL ll=RW_LOG_SETLOGLEVEL(LOGLEVEL_NONE);
	DBConn dbconn;
	dbconn.Setdbinfo(szHost,iPort,dbname,szUser,szPswd);
	int iresult=dbconn.TestConnect();
	dbconn.Close();
	RW_LOG_SETLOGLEVEL(ll); //�ָ�
	if(iresult==1)
	{ 
		RW_LOG_PRINT(LOGLEVEL_ERROR,"���ݿ� %s:%d �쳣�������ʺ�����\r\n\r\n",szHost,iPort);
		return 4; 
    }
	RW_LOG_PRINT(LOGLEVEL_ERROR,0,"�ʺż�飺���ݿ��ʺ�������Ч\r\n");
	if(iresult==2){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"���ݿ� %s:%d �쳣���������ݿ���\r\n\r\n",szHost,iPort);
		return 5;
	}
	RW_LOG_PRINT(LOGLEVEL_ERROR,"���ݿ� %s:%d ***����***\r\n\r\n",szHost,iPort); 
	return 0;
}
