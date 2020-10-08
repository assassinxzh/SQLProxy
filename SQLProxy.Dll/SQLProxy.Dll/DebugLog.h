/*
**	FILENAME			DebugLog.h
**
**	PURPOSE				������Ϣ������������ָ���Ĵ������DebugView�鿴
**						
*/

#ifndef __DEBUGLOG_H__
#define __DEBUGLOG_H__
#include <iostream>
#include "x64_86_def.h" //INTPTR����

#ifndef TCHAR
#define LPCTSTR const char *
#define LPTSTR char *
#define TCHAR char
#endif

typedef enum //���������Ϣ����
{
	LOGLEVEL_DEBUG=0,//������Ϣ
	LOGLEVEL_WARN,//���������Ϣ
	LOGLEVEL_INFO,//����û���¼��Ϣ
	LOGLEVEL_ERROR,//���������Ϣ
	LOGLEVEL_FATAL, //�������������Ϣ
	LOGLEVEL_NONE	//��ֹ���
}LOGLEVEL;
typedef enum //��־�������
{
	LOGTYPE_NONE,
	LOGTYPE_STDOUT,//�������׼����豸��
	LOGTYPE_FILE,//������ļ�
	LOGTYPE_HWND,//�����һ��windows�Ĵ�����
	LOGTYPE_DVIEW //�����DebugView
}LOGTYPE;

//����ڹ����ﶨ���˽�ֹDebugLog����������ʱ���Զ��������е�Debug���
#ifndef __NO_DEBUGLOG__
class DebugLogger
{
	HANDLE m_hMutex;
	LOGTYPE m_logtype;//��־�������
	LOGLEVEL m_loglevel;//��־����
	INTPTR m_hout;//������������LOGTYPE_FILE���Ͷ�ӦFILE *,LOGTYPE_HWND���Ͷ�ӦHWND
	TCHAR m_fileopenType[4]; //Ĭ��δ���Ǵ򿪷�ʽ"w"
	bool m_bOutputTime;//�Ƿ��ӡ���ʱ��
	
	static DebugLogger m_logger;//��̬��־��ʵ��
	DebugLogger(); //������������ʵ��
		
	void _dump(LPCTSTR buf,size_t len);
public:
	~DebugLogger();
	static DebugLogger & getInstance(){ return m_logger; }
	//�����Ƿ��ӡʱ��
	void setPrintTime(bool bPrint){ m_bOutputTime=bPrint; return; }
	LOGLEVEL setLogLevel(LOGLEVEL ll){ 
		LOGLEVEL lOld=m_loglevel; m_loglevel=ll; return lOld; }
	//������־�ļ��򿪷�ʽ
	void setOpenfileType(TCHAR c) { m_fileopenType[0]=c; return; }
	//�Ƿ��������ָ���������־
	bool ifOutPutLog(LOGLEVEL ll) { return ( (unsigned int)m_loglevel<=(unsigned int)ll ); }
	LOGTYPE LogType() { return m_logtype; }
	LOGTYPE setLogType(LOGTYPE lt,INTPTR lParam);
	void flush(){ 
		if(m_logtype==LOGTYPE_FILE && m_hout)
			::fflush((FILE *)m_hout); return;}
	void dump(LOGLEVEL ll,LPCTSTR fmt,...);
	void dump(LOGLEVEL ll,size_t len,LPCTSTR buf);
	//���DEBUG�������־
	void debug(LPCTSTR fmt,...);
	void debug(size_t len,LPCTSTR buf);
	void dumpBinary(LOGLEVEL ll,const char *buf,size_t len);
	void printTime(); //��ӡ��ǰʱ��
	void printTime(std::ostream &os);
	void dump(std::ostream &os,LPCTSTR fmt,...);
	void dump(std::ostream &os,size_t len,LPCTSTR buf);
	void dumpBinary(std::ostream &os,const char *buf,size_t len);
};

//�����ڴ�ӡ��Ϣǰ�ȴ�ӡʱ��
#define RW_LOG_SETPRINTTIME(b) \
{ \
	DebugLogger::getInstance().setPrintTime(b); \
}
//������־�������
#define RW_LOG_SETLOGLEVEL(ll) DebugLogger::getInstance().setLogLevel(ll);

//������־�����ָ�����ļ�
#define RW_LOG_SETFILE(filename) \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_FILE,filename); \
}
//�����ļ���־Ϊ׷�ӷ�ʽ�������Ǹ��Ƿ�ʽ
#define RW_LOG_OPENFILE_APPEND() \
{ \
	DebugLogger::getInstance().setOpenfileType('a'); \
}
//������־�����ָ���Ĵ���
#define RW_LOG_SETHWND(hWnd) \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_HWND,hWnd); \
}
//������־�����DebugView
#define RW_LOG_SETDVIEW() \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_DVIEW,0); \
}
//������־�������׼����豸stdout
#define RW_LOG_SETSTDOUT() \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_STDOUT,0); \
}
//�������־
#define RW_LOG_SETNONE() \
{ \
	DebugLogger::getInstance().setLogType(LOGTYPE_NONE,0); \
}

#define RW_LOG_FFLUSH() \
{ \
	DebugLogger::getInstance().flush(); \
}

#define RW_LOG_LOGTYPE DebugLogger::getInstance().LogType
//����Ƿ��������ָ���������־
#define RW_LOG_CHECK DebugLogger::getInstance().ifOutPutLog
//��־���
#define RW_LOG_PRINT DebugLogger::getInstance().dump
#define RW_LOG_PRINTBINARY DebugLogger::getInstance().dumpBinary
//��ӡ��ǰʱ��
#define RW_LOG_PRINTTIME DebugLogger::getInstance().printTime

#define RW_LOG_DEBUG if(DebugLogger::getInstance().ifOutPutLog(LOGLEVEL_DEBUG)) DebugLogger::getInstance().debug

//����ڹ����ﶨ���˽�ֹDebugLog����������ʱ���Զ��������е�Debug���
#else //#ifndef __NO_DEBUGLOG__

#define NULL_OP		__noop			//NULL
#define RW_LOG_SETPRINTTIME(b)		NULL_OP
#define RW_LOG_SETLOGLEVEL(ll)		NULL_OP
#define RW_LOG_SETFILE(filename)	NULL_OP
#define RW_LOG_OPENFILE_APPEND()	NULL_OP
#define RW_LOG_SETHWND(hWnd)		NULL_OP
#define RW_LOG_SETDVIEW()			NULL_OP
#define RW_LOG_SETSTDOUT()			NULL_OP
#define RW_LOG_SETNONE()			NULL_OP
#define RW_LOG_FFLUSH()				NULL_OP
#define RW_LOG_LOGTYPE				NULL_OP
#define RW_LOG_PRINT				NULL_OP
#define RW_LOG_CHECK				NULL_OP
#define RW_LOG_PRINTBINARY			NULL_OP
#define RW_LOG_PRINTTIME			NULL_OP
#define RW_LOG_DEBUG				NULL_OP

#endif //#ifndef __NO_DEBUGLOG__ 

#endif

