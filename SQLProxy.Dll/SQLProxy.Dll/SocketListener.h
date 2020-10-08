#ifndef _SOCKETLISTENER_H_
#define _SOCKETLISTENER_H_

class CSocket;

class ISocketListener
{
public:
	//���ؼ٣����ݴ������
	virtual bool SocketReceived(CSocket *pSocket) = 0;
	virtual void SocketClosed(CSocket *pSocket)=0;
	//���سɹ�����ܿɴ�������ܾ����ܹر�socket (Only for Server)
	virtual bool SocketAccepted(CSocket *pSocket){ return true; }
};

#endif
