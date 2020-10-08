#pragma once 

#include <map>
#include <deque>
#include <vector>
#include "SQLPacket.h"

class SQLAnalyse
{
public:
	int m_iLogQuerys;	//�Ƿ��¼����ָ���־ 0����¼ 1��¼ -1��¼��ӡ�����Query Data(������)
public:
	SQLAnalyse(void *dbservers);
	~SQLAnalyse();
	int Size() const { return m_QueryLists.size(); }
	void SetFilter(const char *szParam);
	bool Active(void){ return !m_bStopped; }  //thread is active or not
	bool Start();
	void Stop();
	bool AddQuery(QueryPacket *pquery,void * pinstream,unsigned short tdsver);
	
	void Run(void);
private:
	bool m_bStopped;
	HANDLE m_hThread;
	HANDLE m_hMutex;
	std::deque<QueryPacket *> m_QueryLists; //Ҫ������ӡ�Ĳ�ѯ�б�
	void * m_dbservers;	//DB��������Ⱥ����ָ��(���ڻ�ȡ��Ⱥ���ݿ����Ϣ)

private: //SQL��ӡ������˲���
	bool m_bPrintSQL;
	int m_iSQLType; //1��ѯ 2д�� 4 ����
	int m_msExec; //������ӡ������ڵ���ָ��ִ��ʱ��ĵ�SQL���(ms),Ĭ��0
	int m_staExec; //������ӡ���ָ��ִ��״̬����� 1�ɹ� 2ʧ�� 
	unsigned long m_clntip; //�������ָ���ͻ���SQL���
	void * m_pRegExpress; //������ʽ����
	bool MatchQuerys(std::vector<QueryPacket *> &vecQuery);
	void PrintQuerys(std::vector<QueryPacket *> &vecQuery,int LogLevel);
	void LogQuerys(std::vector<QueryPacket *> &vecQuery,bool bSynErr);
};
