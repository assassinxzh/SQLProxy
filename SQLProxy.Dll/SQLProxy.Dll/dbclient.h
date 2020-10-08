#pragma once 

#include <string>
#include "Socket.h"
#include "SQLPacket.h"
#include "x64_86_def.h" //INTPTR����

enum
{
	DBSTATUS_ERROR=-2,	//δ����
	DBSTATUS_PROCESS=-1, //���ڴ�����
	DBSTATUS_SUCCESS=0,	//ִ�гɹ�
};
//TDS��¼��Ҫ�Ĳ�����Ϣ
typedef struct tds_login_param
{
//PreLogin Packet���ò���
	unsigned short tdsversion;
	unsigned char tds_ssl_login;	//�ݲ�֧������ssl��ʽ
//Login Packet���ò���
	unsigned int tds_block_size;
	std::string dbname;		//���ݿ����Լ������ʺź�����
	std::string dbuser;
	std::string dbpswd;
}tds_login_param;

//���ݿ����ӶϿ�֪ͨ�¼�;iEvent==0������ ==1�����Ͽ� ==2�쳣�Ͽ�
typedef void (FUNC_DBSTATUS_CB)(int iEvent,INTPTR userParam);
typedef void (FUNC_DBDATA_CB)(void *ptdsh,unsigned char *pData,INTPTR userParam);
class DBConn : public ISocketListener
{
	bool SocketReceived(CSocket *pSocket);
	void SocketClosed(CSocket *pSocket);
	
public:
	DBConn();
	~DBConn();
	tds_login_param &LoginParams(){ return m_params; }
	void Setdbinfo(const char *szHost,int iport,const char *szDbname,const char *szUsername,const char *szUserpwd);
	void SetEventFuncCB(FUNC_DBSTATUS_CB *pfn,INTPTR userParam){	m_pfnOnStatus=pfn;m_lUserParam=userParam; }
	void SetEventFuncCB(FUNC_DBDATA_CB *pfn) { m_pfnOnData=pfn; }
	CSocket *GetSocket(){ return m_psocket; }
	ReplyPacket &GetLastReply() { return m_lastreply; }
	bool IsConnected(){ return (m_psocket!=NULL && m_psocket->Active()); }
	void Close(); //�ر����ݿ�����
	bool Connect(); //����SQL���ݿ������
	int TestConnect(); //���Ӳ��ԣ��ɹ�����0����ʧ��
	bool Connect_tds(unsigned char *login_packet,unsigned int nSize);
	//��ѯִ��״̬��DBSTATUS_PROCESS����ִ���з���ִ����Ϸ���ִ�н�� 0�ɹ�����Token+bit_flag
	int GetStatus_Asyn(){ //DBSTATUS_ERROR ���Ӵ��� 
		if(m_psocket && m_psocket->Active() )
		{
			if(m_psocket->GetUserTag()!=0) return DBSTATUS_PROCESS; //���ڴ����еȴ��ظ�
			//�����Ѿ����յ��ظ���Ϣ��m_lastreply���������Ļظ���Ϣ����
			return m_lastreply.m_iresult; //ֱ�ӷ��ػظ����
		}
		return DBSTATUS_ERROR;
	}
	bool ExcuteData(unsigned short tdsv,unsigned char *ptrData,int iLen);
	//ִ��ָ����sql���ȴ�ִ�н����������ִ�гɹ�
	bool ExcuteSQL(const char *szsql,int lTimeout);
	//�������ݲ��ȴ�ִ�з��ؽ�� lTimeout:��ʱʱ��s�� 0ִ�гɹ� -1ִ�г�ʱ������Token+bit_flag
	int WaitForResult(void *pdata,unsigned int nsize,int lTimeout);
	unsigned __int64 m_uTransactionID;	//��ǰ�����m_uTransactionID

private:
	//����0x702����Э�鷢��query data�����query data����START_QUERY��Ϣ��Ҫȥ��
	bool ExcuteData_TDS72SUB(unsigned char *ptrData,int iLen);
	//����0x702������Э�鷢��query data�����query data������START_QUERY��Ϣ��Ҫ�����
	bool ExcuteData_TDS72PLUS(unsigned char *ptrData,int iLen);

	std::string m_hostname;
	int m_hostport;
	tds_login_param m_params;
	ReplyPacket m_lastreply; //�������һ�εȴ��Ļظ���Ϣ����

	CSocket *m_psocket;
	void *m_dbconn;
	//���ݿ����ӶϿ�֪ͨ�¼�
	FUNC_DBSTATUS_CB *m_pfnOnStatus;
	FUNC_DBDATA_CB *m_pfnOnData;
	INTPTR m_lUserParam;
};

