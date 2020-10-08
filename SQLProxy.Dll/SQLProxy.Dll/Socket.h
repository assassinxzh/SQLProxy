#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "SocketListener.h"
#include "SocketStream.h"
#include <winsock.h>
#pragma comment(lib,"ws2_32") //��̬����ws2_32.dll��̬���lib��


//�������������� Beign=================================
#define pthread_cond_t HANDLE
#define pthread_cond_init(pCond,p) \
{ \
	*(pCond)=CreateEvent(NULL,false,false,NULL); \
}
#define pthread_cond_destroy(pCond) CloseHandle(*(pCond))
#define pthread_cond_wait(pCond) WaitForSingleObject(*(pCond),INFINITE)
#define pthread_cond_timedwait(pCond,ms) WaitForSingleObject(*(pCond),ms)
#define pthread_cond_signal(pCond) SetEvent(*(pCond))
class clsCond
{
	pthread_cond_t m_cond;
	int m_status; //0���ڷǵȴ�״̬�������ڵȴ�״̬
public:
	long long m_ltag;
	clsCond():m_status(0)
	{
		m_ltag=0; //����ʱ���ⷵ�ز���
        pthread_cond_init(&m_cond,NULL);
	}
	~clsCond()
	{
		if(m_status!=0) pthread_cond_signal(&m_cond);
        pthread_cond_destroy(&m_cond);
	}
	bool wait(DWORD mseconds=0) //������--���������ʱ
	{
		if(m_status!=0) return false;
		m_status=1;//�ȴ�����״̬
		DWORD dwRet=(mseconds==0)?pthread_cond_wait(&m_cond):	//���޵ȴ���ֱ��������
					pthread_cond_timedwait(&m_cond,mseconds);	//�ȴ�ָ���ĺ���
		m_status=0; 
		return (dwRet==WAIT_OBJECT_0); //WAIT_TIMEOUT ��ʱ
	}
	void active(long long ltag)
	{
		m_ltag=ltag;
        if(m_status!=0) pthread_cond_signal(&m_cond);
	}
	//�Ƿ�Ϊ�ȴ�״̬
	bool status() { return (m_status!=0)?true:false;}
};
//�������������� End=================================

class CSocket
{
public:
	//����ָ�������� ,only for IPV4
	static unsigned long Host2IP(const char *host);
	static const char *IP2A(unsigned long ipAddr)
		{
			struct in_addr in;
			in.S_un.S_addr =ipAddr;
			return inet_ntoa(in);
		}
	void SetUserTag(long tag){ m_lUserTag=tag;}
	long GetUserTag(){ return m_lUserTag; }
private:
	long m_lUserTag;	//�����ϲ��û��Զ���˽������
public:
	CSocket(int nType);
	virtual ~CSocket();
	int GetType(void);
	bool Connect(const char *host,int port,ISocketListener *pListener);
	void Bind(SOCKET socket, ISocketListener *pListener, struct sockaddr_in *pRemoteAddr = NULL);
	bool Open(); //���������߳�
	//��socket��ISocketListener�����������߳�
	void Open(SOCKET socket, ISocketListener *pListener, struct sockaddr_in *pRemoteAddr = NULL);
	//bMandatory: ǿ�ƹرգ����ȴ������߳̽��������ã���
	void Close(bool bMandatory=false);
	void Send(void); //������������е�����
	int Send(void *pdata,unsigned int nsize){ //ֱ�ӷ�������
		return ::send(m_Socket, (const char *)pdata,nsize, 0);
	}
	bool Active(void){ return m_bActive; }  //thread is active or not
	char *GetRemoteAddress(void);
	unsigned long GetRemoteAddr(void);
	int GetRemotePort(void);
	IInputStream *GetInputStream(void);
	IOutputStream *GetOutputStream(void);
	void Run(void);

private:
	int m_nType;
	SOCKET m_Socket;
	struct sockaddr_in m_RemoteAddr;
	ISocketListener *m_pListener;
	CSocketStream m_SocketStream;
	bool m_bActive;
	HANDLE m_hThread;
};

#endif
