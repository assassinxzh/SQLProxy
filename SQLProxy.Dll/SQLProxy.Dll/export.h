// ���� ifdef ���Ǵ���ʹ�� DLL �������򵥵�
// ��ı�׼�������� DLL �е������ļ��������������϶���� DCMDIR_EXPORTS
// ���ű���ġ���ʹ�ô� DLL ��
// �κ�������Ŀ�ϲ�Ӧ����˷��š�������Դ�ļ��а������ļ����κ�������Ŀ���Ὣ
// DCMDIR_API ������Ϊ�Ǵ� DLL ����ģ����� DLL ���ô˺궨���
// ������Ϊ�Ǳ������ġ�
#ifdef SQLPROXYDLL_EXPORTS
#define SQLPROXY_API __declspec(dllexport)
#else
#define SQLPROXY_API __declspec(dllimport)
#endif
#include "x64_86_def.h"	//INTPTR����

extern "C"
SQLPROXY_API void _stdcall SetLogLevel(HWND hWnd,int LogLevel);
//���ݿ���ͨ�Բ���,����0 �ɹ� 1 ��Ч��IP������ 2���������ɴ� 3 ���񲻿��� 4�ʺ�������� 5���ݿ�������
extern "C"
SQLPROXY_API int _stdcall DBTest(const char  *szHost,int iPort,const char *szUser,const char *szPswd,const char *dbname);

//���ü�Ⱥ���ݿ���Ϣ���������в����Լ�SQL��ӡ������˲���
extern "C"
SQLPROXY_API void _stdcall SetParameters(int SetID,const char *szParam);
//��ȡ��̨ĳ��ȺSQL����״̬ idx==-1��ȡSQL��Ⱥ�������
//proid==0SQL�����Ƿ��������� ==1����SQL����ǰ�����ͻ������� ==3����SQL����ǰ���ش����� ==4д�봦���� 
extern "C"
SQLPROXY_API INTPTR _stdcall GetDBStatus(int idx,int proid);

//����SQLProxy�������
extern "C"
SQLPROXY_API bool _stdcall StartProxy(int iport);
extern "C"
SQLPROXY_API void _stdcall StopProxy();

extern "C"
SQLPROXY_API int _stdcall ProxyCall(int callID,INTPTR lParam);
