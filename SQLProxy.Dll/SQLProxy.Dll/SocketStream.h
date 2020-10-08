#ifndef _SOCKETSTREAM_H_
#define _SOCKETSTREAM_H_

#include "InputStream.h"
#include "OutputStream.h"

class CSocketStream : public IInputStream, public IOutputStream
{
public:
	CSocketStream();
	virtual ~CSocketStream();
	//��ȡ��Ч�Ŀ������/����ռ��С //yyc add
	unsigned int GetOutBufferSize(void);
	unsigned int GetInBufferSize(void);
	//��ȡ���/���뻺��������׵�ַ
	unsigned char *GetOutput(void){return m_pOutBuffer; }
	unsigned char *GetInput(void){return m_pInBuffer; }
	//��ȡ��ǰ����������ֽ���
	unsigned int GetOutputSize(void){ return m_nOutBufferSize; }
	//��ȡ��ǰδ����������ֽ��� number of bytes of rest data in the input buffer
	unsigned int GetInputSize(void){ return m_nInBufferSize - m_nInBufferIndex; }
	//��������������
	void OutputReset(void){m_nOutBufferIndex = m_nOutBufferSize = 0;} 
	void InputReset(void){m_nInBufferIndex = m_nInBufferSize = 0;} 
	
	void SetInputSize(unsigned int nSize);
	unsigned int Read(void *pData, unsigned int nSize);
	unsigned int Read(unsigned int *pnValue);
	unsigned int Read(unsigned short *pnValue);
	unsigned int Read8(unsigned __int64 *pnValue);
	unsigned int Skip(unsigned int nSize);
	unsigned int GetReadPos(void){ return m_nInBufferIndex; }
	//yyc add 2017-11-05 �ָ��Ѵ�������ָ��
	void UnSkip(unsigned int nSize){ m_nInBufferIndex=(nSize>=m_nInBufferIndex)?0:(m_nInBufferIndex-nSize); }

	void Write(void *pData, unsigned int nSize);
	void Write(unsigned int nValue);
	void Write8(unsigned __int64 nValue);
	void Seek(int nOffset, int nFrom);
	int GetWritePos(void){ return m_nOutBufferIndex; }

private:
	unsigned char *m_pInBuffer, *m_pOutBuffer;
	//m_nInBufferIndex���뻺��δ��������λ�ã�m_nInBufferSize���뻺�������ݴ�С
	//m_nOutBufferIndex�������д��λ��������m_nOutBufferSize��ǰ�ܵ�д�����ݴ�С
	unsigned int m_nInBufferIndex, m_nInBufferSize, m_nOutBufferIndex, m_nOutBufferSize;
};

#endif
