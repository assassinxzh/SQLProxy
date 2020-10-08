#ifndef _RPCSERVER_H_
#define _RPCSERVER_H_

#include <map>
#include <vector>
#include <string>

#include "SocketListener.h"
#include "Socket.h"
#include "dbclient.h"
#include "mytds.h"
#include "SQLAnalyse.h"

#define MAX_WAITREPLY_TIMEOUT 30*1000	//30s���ȴ��ظ����ʱʱ��
//SQL���ʿͻ��˶���
class CProxyServer;
class CSQLClient : public ISocketListener
{
public:
	bool m_bLoginDB;		 //��ǰ�ͻ����Ƿ��ѳɹ���¼��������
	unsigned short m_tdsver; //��ǰ�ͻ���ʹ�õ�tds�汾
	std::string m_tds_dbname; //��ǰ���ʵ����ݿ� (dbserver�ɲ��������ݿ������������ݿ�ʵ����Ⱥ)
public:
	CSQLClient(CProxyServer *proxysvr);
	~CSQLClient();
	bool SocketReceived(CSocket *pSocket);
	void SocketClosed(CSocket *pSocket){ return; }
	int GetSelectedDBSvr() { return m_idxRandDBSvr; }
	void InitDBConn(unsigned char *login_packet,unsigned int nSize,tds_login *ptr_login);
	void SendQuery_mode1(QueryPacket *plastquery,bool bRollback);	 //iLoadMode!=0���⸺��ģʽ������յ�Query
	void SendQuery_mode0(QueryPacket *plastquery,bool bRollback);	//iLoadMode=0���Ӹ���ģʽ������յ�Query
	void TransactPacket(void *pData, unsigned int nSize,bool bRollback);

	//ֱ��ת�����ݵ���������;msTimeOut!=0��ȴ�������������;0ִ�гɹ� -1ִ�г�ʱ������Token+bit_flag
	int SendWaitRelpy(void *pData, unsigned int nSize,int msTimeOut);
	void SetMainDBSocket(CSocket *pSocketUser,CSocket *pRandDBSvr,int idxRandDBSvr,int idxLoadDBsvr);
private:
	//��ǰ������������д��SQL��䣬���ִ�гɹ���ͬ�����и��ػ򱸷ݷ�����
	bool SendQueryAndSynDB(QueryPacket *plastquery);
	//���ػ򱸷ݷ�����ͬ��ִ�У��ɹ�����0��������� -1��ʱ
	int SendQueryToAll(QueryPacket *plastquery,unsigned int msTimeout);
	int  GetInValidDBsvr(); //��ȡ��Чδͬ�������ݿ�����

	bool BeginTransaction(bool bIncludeMainDB,unsigned char sta);
	bool EndTransaction(bool bIncludeMainDB,bool bCommit);
	//��ǰ���з�����ͬ��д����²������κ�һ��������ִ��ʧ������������������
	bool SendQueryAndSynDB_rb(QueryPacket *plastquery);
	void SendCancelToAll_rb();
	int SendQueryToAll_rb(QueryPacket *plastquery,unsigned int msTimeout,bool bEndTrans);
private:
	CSocket *m_pSocketUser; //�ͻ��˵ķ���socket
	CSocket *m_pRandDBSvr;	//�������������ӵ�socket
	int		m_idxRandDBSvr;	//��ǰ���ӵ�������������
	int		m_idxLoadDBSvr;	//��ǰ���ѡ��ĸ��ط�����
	ReplyPacket m_lastreply; //�������һ�εȴ��Ļظ���Ϣ����
	unsigned __int64 m_uTransactionID;	//��ǰ���������ID��==0��ǰ��������
	std::deque<QueryPacket *> m_vecQueryData; //���������ѷ��͵ȴ������ݿ�ظ���Query/RPC����
	std::vector<QueryPacket *> m_vecTransData; //��������Ự�ڼ������Query/RPC����
	std::vector<DBConn *> m_vecDBConn; //��̨���ݿ�������Ӷ�����������ͬ��ִ��
	
	CProxyServer *m_proxysvr;
	//dbconn���ݿ����ӶϿ�֪ͨ�ص�
	static void dbconn_onStatus(int iEvent,CSQLClient *pclnt);
	//dbconn�յ�db������������Packet֪ͨ�ص�
	static void dbconn_onData(tds_header *ptdsh,unsigned char *pData,CSQLClient *pclnt);
};

typedef struct _TAccessList
{
	unsigned long ipAddr;	//����ip
	unsigned long ipMask;	//��������    if((clntip & ipMask)==ipAddr) �������=bAcess
	bool bAcess;			//true �������������ʽ�ֹ
}TAccessAuth;
class CProxyServer : public ISocketListener
{
public: //���Ʋ���
	int m_iLoadMode;	 //����ģʽ 0���Ӹ��� 1���⸺��ģʽ 5 ���౸��ģʽ��Ĭ��0
						//���Ӹ���ģʽ,��ѯ�������ѡ���ط�����ִ�У�д�����ִ�гɹ���ͬ�����������ط�������
						//���౸�ݻ���⸺��ģʽ�����в���������������ִ�У�д�����ִ�гɹ���ͬ�����������ط�������
	int m_iSynDBTimeOut;//ͬ��ִ�еȴ���ʱʱ��(s) <0������д�����ͬ�� =0�ȴ�ģʽʱ�� >0���ȴ�ָ��ʱ��s
						//���Ӹ��ػ����౸��ģʽ�ɲ�����д��ͬ��������������������SQL�����������ͬ�����������ط��������ö���
	bool m_bRollback;	//����ͬ��ʧ���Ƿ�ع���false-��
	void * m_dbservers;	//DB��������Ⱥ����ָ��(���ڻ�ȡ��Ⱥ���ݿ����Ϣ)
	SQLAnalyse *m_psqlAnalyse; //SQL��������ӡ����(�߳��첽����)
	HANDLE m_hSynMutex;	//���л�ͬ��ִ��	
public:
	CProxyServer(void *dbservers);
	virtual ~CProxyServer();
	bool SocketReceived(CSocket *pSocket);
	void SocketClosed(CSocket *pSocket);
	bool SocketAccepted(CSocket *pSocket);
	//���ú�̨��SQL���ݿ�,szParam��ʽname=val name=val...
	//���������ò��� dbname=<���ݿ���> user=<�ʺ�> pswd=<����> dbhost=<ip:port,ip:port,...>
	void SetBindServer(const char *szParam);
	unsigned int GetDBNums(); //������ЧDB����������
	int GetDBStatus(int idx,int proid);
	const char *GetLoadMode();
protected:
	CSocket *ConnectMainDB(int *pIdxMainDB,ISocketListener *pListener); //������������
	bool Parse_LoginPacket(CSocket *pSocket,unsigned char *pInbuffer,unsigned int nSize);
	
	unsigned int  m_nValidDBServer;	//��ǰ��Ч�ļ�Ⱥ���ݿ����
	int  m_idxSelectedMainDB;		//��һ������Ӧѡ���������������
	std::vector<TAccessAuth> m_vecAcessList; //������ɿ����б�
};

#endif

