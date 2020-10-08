/*******************************************************************
   *	DebugLog.cpp
   *    DESCRIPTION:��־��¼ʵ�֣����ڳ��������ӡ������Ϣ
   *			�������������̨���ļ������ڻ�DebugView
   *    AUTHOR:yyc
   *
   *    ����ڹ����ﶨ���˽�ֹDebugLog����������ʱ���Զ��������е�Debug���
   *
   *******************************************************************/


#include "stdafx.h"
#include <windows.h> //����windows��ͷ�ļ�
//���ʹ��Ԥ����ͷ���� stdafx.h������ڵ�һ�У�����"fatal error C1020: ����� #endif"
//���ʹ����Ԥ����ͷ...��ôVC��������CPP�ļ���Ѱ�� #include   "stdafx.h " 
//���ҵ����д���֮ǰ VC�������д���... 

#ifndef __NO_DEBUGLOG__

#pragma warning(disable:4996)
#pragma warning(disable:4786)
#include "DebugLog.h"

#include <ctime>
#include <cstdarg>
#include <string>
#include <map>

#ifdef UNICODE
#define strlowcase _wcslwr
#define strcasecmp _wcsicmp
#define strncasecmp _wcsnicmp
#define vsnprintf _vsnwprintf
#define stringlen wcslen
#define strprintf swprintf
#define fileopen _wfopen
#else
#define strlowcase _strlwr
#define strcasecmp _stricmp
#define strncasecmp _strnicmp	
#define vsnprintf  _vsnprintf	//_vsnprintf_s
#define stringlen strlen
#define strprintf sprintf	//sprintf_s
#define fileopen fopen
#endif
#define getloctime localtime

extern const char * _PROJECTNAME_;
int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps);
DebugLogger DebugLogger::m_logger;//��̬��־��ʵ����

//��ȡ������Ϣ���ã����ȴ������в�����ȡ�������ϵͳ����������ȡ
//[M=<ģ��> L=<����> [T=<view|file>] [FILE=<�����ָ�����ļ�>] [PATH=<ָ����־Ĭ�����·��>]
bool GetDebugLogSetting(char *szBuf,int buflen)
{
	const char * szCmd=::GetCommandLine();
	if(szCmd!=NULL){
		const char *ptr=strstr(szCmd," -DEBUGLOG");
		if(ptr!=NULL){
			const char *ptrb=NULL,*ptre=NULL;
			ptr+=10;
			while(true){
				char c=*ptr++;
				if(c==0) break;
				if(c!='\'') continue;
				ptrb=ptr; break;
			}
			ptr=ptrb;
			while(ptrb!=NULL){
				char c=*ptr++;
				if(c==0) break;
				if(c!='\'') continue;
				ptre=ptr-1; break;
			}
			if(ptrb && ptre && ptre>ptrb)
			{
				strncpy(szBuf,ptrb,ptre-ptrb);
				return true;
			}
			return false;
		}
	}//?if(szCmd!=NULL)
	
	//�����ȡ�����������õĵ�����Ϣ�������
	//����ϵͳ��������DEBUGLOG�����Ƿ���е������
	buflen=::GetEnvironmentVariable("DEBUGLOG",szBuf,buflen-1);
	if(buflen<=0) return false;
	szBuf[buflen]=0; return true;
}
DebugLogger::DebugLogger()
{
	const char *szModName=_PROJECTNAME_;
	m_hout=(INTPTR)stdout;
	m_logtype=LOGTYPE_NONE;
	m_loglevel=LOGLEVEL_WARN;
	m_bOutputTime=false;
	m_fileopenType[0]='w';
	m_fileopenType[1]='b';
	m_fileopenType[2]=m_fileopenType[3]=0;
	
	char buf[256]; int buflen=256;
	if( !GetDebugLogSetting(buf,256))
		return; //����ϵͳ��������DEBUGLOG�����Ƿ���е������
	std::map<std::string,std::string> maps;
	if(SplitString(buf,' ',maps)<=0) return;
	std::map<std::string,std::string>::iterator it;
	
	//[M=<ģ��> L=<����> [T=<view|file>] [FILE=<�����ָ�����ļ�>] [PATH=<ָ����־Ĭ�����·��>]
	if( (it=maps.find("m"))!=maps.end()) //M=��־���ģ��
	{//�Ƿ�ָ�����������ģ�����־��Ϣ
		if( (*it).second!="" && 
			strcasecmp((*it).second.c_str(),szModName)!=0)
			return; //��ģ�鲻���������־
	}

	if( (it=maps.find("l"))!=maps.end())  //L=��־�������
	{//������־�������
		m_logtype=LOGTYPE_DVIEW;
		int l=atoi((*it).second.c_str());
		if(l>=LOGLEVEL_DEBUG && l<=LOGLEVEL_NONE)
			m_loglevel=(LOGLEVEL)l;
	}
	if( (it=maps.find("t"))!=maps.end())  //T=��־�������
	{//������־�����ʽ
		if(strcasecmp((*it).second.c_str(),"view")==0)
			m_logtype=LOGTYPE_DVIEW;
		else if(strcasecmp((*it).second.c_str(),"file")==0)
			m_logtype=LOGTYPE_FILE;
	}
	std::string strFileName,strPath;
	if( (it=maps.find("path"))!=maps.end() )  //PATH=��־�����ָ����Ŀ¼
	{
		strPath=(*it).second;
		if(strPath!="" && strPath[strPath.length()-1]!='\\') 
			strPath.append("\\");
	}
	if( (it=maps.find("file"))!=maps.end() )  //FILE=��־�����ָ�����ļ�
		strFileName=(*it).second;
	if( m_logtype==LOGTYPE_FILE || strFileName!="")	
	{//������־������ļ�
		m_logtype=LOGTYPE_FILE;
		if(strFileName==""){ //����ļ���
			time_t tNow=time(NULL);
			struct tm * ltime=getloctime(&tNow);
			strprintf(buf,"%sDLog_%04d%02d%02d%02d%02d%02d_%s.txt",
				strPath.c_str(),
				(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, 
				ltime->tm_hour, ltime->tm_min, ltime->tm_sec,
				szModName); //�����ʼ��������ӣ�����û����(����ͬһֵ) rand()
			strFileName.assign(buf);						
		}else if(strstr(strFileName.c_str(),":\\")==NULL) //ָ���������ļ���
			strFileName=strPath+strFileName;	
		m_hout=(INTPTR)fileopen(strFileName.c_str(),m_fileopenType);
	}
	
	m_hMutex = CreateMutex(NULL, false, NULL);
}

DebugLogger::~DebugLogger()
{
	if(m_logtype==LOGTYPE_FILE && m_hout) ::fclose((FILE *)m_hout);
	if(m_hMutex!=NULL) CloseHandle(m_hMutex);

}

LOGTYPE DebugLogger::setLogType(LOGTYPE lt,INTPTR lParam)
{
	if(m_logtype==LOGTYPE_FILE && m_hout) ::fclose((FILE *)m_hout);
	m_hout=0; m_logtype=LOGTYPE_NONE;
	switch(lt)
	{
		case LOGTYPE_STDOUT:
			m_hout=(INTPTR)stdout;
			m_logtype=LOGTYPE_STDOUT;
			break;
		case LOGTYPE_FILE:
			if( (m_hout=(INTPTR)fileopen((LPCTSTR)lParam,m_fileopenType)) )
				m_logtype=LOGTYPE_FILE;
			break;
		case LOGTYPE_HWND:
			m_hout=lParam;
			m_logtype=LOGTYPE_HWND;
			break;
		case LOGTYPE_DVIEW:
			m_hout=1;
			m_logtype=LOGTYPE_DVIEW;
			break;
	}//?switch
	return m_logtype;
}

void DebugLogger::printTime(std::ostream &os)
{
	time_t tNow=time(NULL);
	struct tm * ltime=localtime(&tNow);
	TCHAR buf[64];
	size_t len=strprintf(buf,TEXT("[%04d-%02d-%02d %02d:%02d:%02d] - \r\n"),
		(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, ltime->tm_hour,
		ltime->tm_min, ltime->tm_sec);
	buf[len]=0;
	os<<buf; return;
}
void DebugLogger::dump(std::ostream &os,size_t len,LPCTSTR buf)
{
	if(buf==NULL) return;
	if(len<=0) len=stringlen(buf);
	if(m_bOutputTime) printTime(os);//��ӡʱ��
	if(len>0) os<<buf;
}
void DebugLogger::dump(std::ostream &os,LPCTSTR fmt,...)
{
	TCHAR buffer[1024]; buffer[0]=0;
	va_list args;
	va_start(args,fmt);
	int len=vsnprintf(buffer,1024,fmt,args);
	va_end(args);
	if(len==-1){//д�����ݳ����˸����Ļ������ռ��С
		buffer[1018]=buffer[1019]=buffer[1020]='.';
		buffer[1021]='.'; buffer[1022]='\r';
		buffer[1023]='\n';
		len=1024;
	}
	if(m_bOutputTime) printTime(os);//��ӡʱ��
	if(len>0) os<<buffer;
}

void DebugLogger::dumpBinary(std::ostream &os,const char *buf,size_t len)
{
	if(buf==NULL || len==0) return;
	if(m_bOutputTime) printTime(os);//��ӡʱ��
	
	char szBuffer[128]; int lBufLen=0;
	int i,j,lines=(len+15)/16; //���㹲������
	size_t count=0;//��ӡ�ַ�����
	sprintf(szBuffer,"Output Binary data,size=%d, lines=%d\r\n",len,lines);
	os<<szBuffer;  lBufLen=0;
	for(i=0;i<lines;i++)
	{
		count=i*16;
		for(j=0;j<16;j++)
		{
			if((count+j)<len)
				lBufLen+=sprintf(szBuffer+lBufLen,"%02X ",(unsigned char)buf[count+j]);
			else
				lBufLen+=sprintf(szBuffer+lBufLen,"   ");
		}
		lBufLen+=sprintf(szBuffer+lBufLen,"; ");
		for(j=0;j<16 && (count+j)<len;j++)
		{
			char c=(char)buf[count+j];
			if(c<32 || c>=127) c='.';
			lBufLen+=sprintf(szBuffer+lBufLen,"%c",c);
		}
		lBufLen+=sprintf(szBuffer+lBufLen,"\r\n");
		os<<szBuffer;  lBufLen=0;
	}//?for(...
	return;
}
void DebugLogger::printTime()
{
	if(m_logtype==LOGTYPE_NONE) return; 
	time_t tNow=time(NULL);
	struct tm * ltime=localtime(&tNow);
	TCHAR buf[64];
	size_t len=strprintf(buf,TEXT("[%04d-%02d-%02d %02d:%02d:%02d] - \r\n"),
		(1900+ltime->tm_year), ltime->tm_mon+1, ltime->tm_mday, 
		ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
	buf[len]=0;
	_dump(buf,len); return;
}

void DebugLogger::dump(LOGLEVEL ll,size_t len,LPCTSTR buf)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(ll<m_loglevel) return;//�˼������־�����
	if(m_hout==0 || buf==NULL) return;
	if(len<=0) len=stringlen(buf);
	if(m_bOutputTime) printTime();//��ӡʱ��
	if(len>0) _dump(buf,len);
}

void DebugLogger::dump(LOGLEVEL ll,LPCTSTR fmt,...)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(ll<m_loglevel) return;//�˼������־�����
	TCHAR buffer[1024]; buffer[0]=0;
	va_list args;
	va_start(args,fmt);
	int len=vsnprintf(buffer,1024,fmt,args);
	//int len=_vsnprintf_s(buffer,1024,_TRUNCATE,fmt,args);
	va_end(args);
	if(len==-1){//д�����ݳ����˸����Ļ������ռ��С
		buffer[1018]=buffer[1019]=buffer[1020]='.';
		buffer[1021]='.'; buffer[1022]='\r';
		buffer[1023]='\n';
		len=1024;
	}
	if(m_bOutputTime) printTime();//��ӡʱ��
	if(len>0) _dump(buffer,len);
	return;
}

void DebugLogger::debug(size_t len,LPCTSTR buf)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(LOGLEVEL_DEBUG<m_loglevel) return;//�˼������־�����
	if(m_hout==0 || buf==NULL) return;
	if(len<=0) len=stringlen(buf);
	if(m_bOutputTime) printTime();//��ӡʱ��
	if(len>0) _dump(buf,len);
	return;
}
void DebugLogger::debug(LPCTSTR fmt,...)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(LOGLEVEL_DEBUG<m_loglevel) return;//�˼������־�����
	if(m_hout==0) return;
	TCHAR buffer[1024]; buffer[0]=0;
	va_list args;
	va_start(args,fmt);
	int len=vsnprintf(buffer,1024,fmt,args);
	//int len=_vsnprintf_s(buffer,1024,_TRUNCATE,fmt,args);
	va_end(args);
	if(len==-1){//д�����ݳ����˸����Ļ������ռ��С
		buffer[1018]=buffer[1019]=buffer[1020]='.';
		buffer[1021]='.'; buffer[1022]='\r';
		buffer[1023]='\n';
		len=1024;
	}
	if(m_bOutputTime) printTime();//��ӡʱ��
	if(len>0) _dump(buffer,len);
	return;
}

inline void DebugLogger::_dump(LPCTSTR buf,size_t len)
{
	if(m_logtype==LOGTYPE_NONE) return;
	WaitForSingleObject(m_hMutex, INFINITE);
	switch(m_logtype)
	{
		case LOGTYPE_STDOUT:
#ifdef _DEBUG
			printf("%s",buf);
#else
			fwrite((const void *)buf,sizeof(TCHAR),len,stdout);
#endif
			break;
		case LOGTYPE_FILE:
			fwrite((const void *)buf,sizeof(TCHAR),len,(FILE *)m_hout);
			break;
		case LOGTYPE_HWND:
#ifdef WIN32
		{
			int end = ::GetWindowTextLength((HWND)m_hout);
			::SendMessage((HWND)m_hout, EM_SETSEL, (WPARAM)end, (LPARAM)end);
			TCHAR c=*(buf+len); 
			if(c!=0) *(LPTSTR)(buf+len)=0;
			::SendMessage((HWND)m_hout, EM_REPLACESEL, 0, (LPARAM)buf);
			if(c!=0) *(LPTSTR)(buf+len)=c;
		}
#endif
			break;
		case LOGTYPE_DVIEW:
			OutputDebugString(buf);
			break;
	}//?switch
	ReleaseMutex(m_hMutex);
}

void DebugLogger::dumpBinary(LOGLEVEL ll,const char *buf,size_t len)
{
	if(m_logtype==LOGTYPE_NONE) return;
	if(ll<m_loglevel) return;//�˼������־�����
	if(buf==NULL || len==0) return;

	if(m_bOutputTime) printTime();//��ӡʱ��
	
	int i,j,lines=(len+15)/16; //���㹲������
	size_t count=0;//��ӡ�ַ�����
	printf("Output Binary data,size=%d, lines=%d\r\n",len,lines);
	for(i=0;i<lines;i++)
	{
		count=i*16;
		for(j=0;j<16;j++)
		{
			if((count+j)<len)
				printf("%02X ",(unsigned char)buf[count+j]);
			else
				printf("   ");
		}
		printf("; ");
		for(j=0;j<16 && (count+j)<len;j++)
		{
			char c=(char)buf[count+j];
			if(c<32 || c>=127) c='.';
			printf("%c",c);
		}
		printf("\r\n");
	}//?for(...
	return;
}

int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps)
{
	if(str==NULL) return 0;
	while(*str==' ') str++;//ɾ��ǰ���ո�
	const char *ptr,*ptrStart,*ptrEnd;
	while( (ptr=strchr(str,'=')) )
	{
		char dm=delm; ptrStart=ptr+1;
		if(*ptrStart=='"') {dm='"'; ptrStart++; }
		ptrEnd=ptrStart;
		while(*ptrEnd && *ptrEnd!=dm) ptrEnd++;

		*(char *)ptr=0;
		strlowcase((char *)str);
		maps[str]=std::string(ptrStart,ptrEnd-ptrStart);
		*(char *)ptr='=';

		if(*ptrEnd==0) break;
		str=ptrEnd+1;
		while(*str==' ') str++;//ɾ��ǰ���ո�
	}//?while(ptr)
	
	return maps.size();
}
#endif