
#pragma once 

#include <string>

#define MAX_NUM_SQLSERVER 10 //�������Ⱥ�ĺ��SQL����������
class CDBServer //����SQL��Ⱥ��������Ϣ
{
	bool OpenQueryLog(const char *szLogFile); //������¼�ļ����Ա��¼Query/RPC���
public:
	CDBServer();
	~CDBServer();
	void Init(); //��ʼ����Ϣ
	int SetValid(int iflag){//����/���ط�����״̬
		if(iflag>=0) m_iValid=iflag;
		return m_iValid;
	}
	bool IsValid() { return (m_iValid!=1); } //�������Ƿ�����ӷ���
	void CloseQueryLog();
	void SaveQueryLog(unsigned char *ptrData,unsigned int nsize);
	void QueryCountAdd(bool bNotSelect){
		//WaitForSingleObject(m_hMutex, INFINITE);
		++m_nQueryCount;
		if(bNotSelect) ++m_nWriteCount;
		//ReleaseMutex(m_hMutex);
	}
	std::string m_svrhost;
	int m_svrport;
	std::string m_dbname;
	std::string m_username;
	std::string m_userpwd;
	
	unsigned int m_nSQLClients; //��ǰ�����������ӿͻ�����
	unsigned int m_nQueryCount; //��ǰ��������ִ����query����
	unsigned int m_nWriteCount; //����д��������
	unsigned int m_nDBConnNums; //��̨ͬ��ִ�����ݿ����Ӹ���������Ӧ����m_nSQLClients*(��Ч���ݿ����������-1)
private:
	int m_iValid; //�Ƿ���Ч 0��Ч 1���ݿ����Ӵ��� 2���ݿ�ͬ���쳣
	FILE * m_fpsave;
	HANDLE m_hMutex;
};
