
#pragma once 
#include <iostream>

//TDS Query��������ӡ��
class QueryPacket
{
public:
	int m_idxDBSvr;		//���ѡ��ִ�д�Query�ķ���������
	unsigned long m_clntip; //���ʿͻ���ip��ַ
	time_t m_atExec;	//��ʼִ��ʱ��
	long m_msExec;		//ִ��ʱ��ms
	unsigned long m_nAffectRows; //��Ӱ�������򷵻ؼ�¼��
	int m_staExec;		//ִ��״̬ 0 �ɹ�,����ʧ��״̬�롣
	int m_staSynExec;	//ͬ��ִ��״̬ 0 �ɹ������������ (-1ͬ����ʱ...)
						//ͬ��ִ��״̬�������ִ��ͬ����DB�ķ���״̬�����ĳ��db��־Ϊ����״̬��������ͬ��
	long m_msSynExec;	//ͬ��ʱ��ms
	unsigned int m_idxSynDB; //ͬ��ʧ�ܻ�δ����ͬ�������ݿ�����
	unsigned int m_nSynDB; //��2�ֽ���Ҫͬ����DB����2�ֽ�ͬ���ɹ���DB
	bool m_bTransaction; //�Ƿ�Ϊ����packet
	int m_nMorePackets; //ָ����������N��packet��һ���������������
public:
	QueryPacket(int idxDBSvr,unsigned long ipAddr,unsigned int nAlloc=0);
	~QueryPacket();
	unsigned int GetQueryDataSize() { return m_datasize; }
	unsigned char *GetQueyDataPtr(){ return m_pquerydata; }
	void SetQueryData(unsigned char *ptrData,unsigned int nsize);
	void AddQueryData(unsigned char *ptrData,unsigned int nsize);
	bool IsNotSelectSQL(bool bParseQuery);
	std::wstring GetQuerySQL(void *pvecsql_params); //����sql����ַ����Լ�������Ϣ(��ѡ)
	void PrintQueryInfo(std::ostream &os);

private:
	//һ��������Query�����ɶ��packet��ɣ����һ��packetͷ��status��־Ϊ��0(һ��Ϊ1)
	unsigned char *m_pquerydata;	//����һ��������packets���ݣ����ܰ������packet
	unsigned int m_datasize;		//m_pquerydataָ������ݴ�С
	unsigned int m_nAllocated;		//m_pquerydataָ��Ļ���ռ��С
	bool	m_bNotSelectSQL;	//�����Ƿ�ǲ�ѯ���ķ������
};

//�첽�ȴ��ظ���ʶ
#define WAITREPLY_FLAG			1	//�ȴ�ִ�л�Ӧ�������ػ��Ӧ����Ӧ�������͵��ͻ���
#define WAITREPLY_NOSEND_FLAG	2	//�ȴ�ִ�л�Ӧ���ػ��Ӧ�����͸��ͻ���(only forͬ��ʧ�����ݻع�ģʽ)
//TDS��Ӧ�ظ�Packet
class ReplyPacket
{
public:
	unsigned short m_tdsver;	//��ǰʹ�õ�tds�汾
	int m_iresult; //�ظ���Ϣ���������0ִ�гɹ� ,���� Token+bit_flag
	unsigned long m_nAffectRows; //ִ�гɹ���������Ӱ�������
public:
	ReplyPacket();
	~ReplyPacket();
	unsigned int GetReplyDataSize() { return m_datasize; }
	unsigned char *GetReplyDataPtr(){ return m_preplydata; }
	//��ս�������
	void ClearData() { m_datasize=0; m_iresult=0; m_nAffectRows=0;}	
	void SetReplyData(unsigned char *ptrData,unsigned int nsize);
	void AddReplyData(unsigned char *ptrData,unsigned int nsize);
	
private:
	unsigned char *m_preplydata;	//����һ����������Ӧ�ظ���Ϣ
	unsigned int m_datasize;		//m_preplydataָ������ݴ�С
	unsigned int m_nAllocated;		//m_pquerydataָ��Ļ���ռ��С
};