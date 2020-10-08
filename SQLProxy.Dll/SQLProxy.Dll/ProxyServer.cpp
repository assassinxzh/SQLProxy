
#include "stdafx.h"
#pragma warning(disable:4996)

#include "ServerSocket.h"
#include "ProxyServer.h"
#include "CDBServer.h"
#include "DebugLog.h"
extern //defined in DebugLog.cpp
int SplitString(const char *str,char delm,std::map<std::string,std::string> &maps);
static const char *szLoadBalanceMode[]={"���Ӹ���","���⸺��","","","","���ֱ���",""};
CProxyServer :: CProxyServer(void *dbservers)
{
	m_iLoadMode=0;
	m_idxSelectedMainDB=0;
	m_iSynDBTimeOut=0;
	m_bRollback=false;
	m_dbservers=dbservers;
	m_hSynMutex=NULL;
	m_nValidDBServer=0;
}

CProxyServer :: ~CProxyServer()
{	
	if(m_hSynMutex!=NULL) CloseHandle(m_hSynMutex);
}

const char * CProxyServer ::GetLoadMode()
{
	return szLoadBalanceMode[m_iLoadMode];
}
//������ЧDB����������
unsigned int CProxyServer ::GetDBNums()
{
	unsigned int iNumsSocketSvrs=0; //SQL��Ⱥ�������
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	for(;iNumsSocketSvrs<MAX_NUM_SQLSERVER;iNumsSocketSvrs++)
	{
		CDBServer &sqlsvr=dbservers[iNumsSocketSvrs];
		if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") 
			break; 
	}
	return iNumsSocketSvrs;
}

int CProxyServer ::GetDBStatus(int idx,int proid)
{
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	if(idx==-1) return GetDBNums(); //������ЧSQL��Ⱥ�������
	if(idx<0 || idx>=MAX_NUM_SQLSERVER)
		return 0;
	
	if(proid==0) //����SQL�����Ƿ�����
		return dbservers[idx].SetValid(-1);
	if(proid==1){ //����SQL����ǰ�����ͻ�������
		if(m_iLoadMode==0 && dbservers[idx].IsValid() ) //���Ӹ���ģʽ,�ӷ�����������ʼ��δ0
			return dbservers[m_idxSelectedMainDB].m_nSQLClients;
		return dbservers[idx].m_nSQLClients;
	}
	if(proid==2){ //���غ�̨ͬ��ִ�����ݿ����Ӹ���������Ӧ����m_nSQLClients*(��Ч���ݿ����������-1)
		if(m_iLoadMode==0 && dbservers[idx].IsValid() ) //���Ӹ���ģʽ
			return dbservers[m_idxSelectedMainDB].m_nDBConnNums;
		return dbservers[idx].m_nDBConnNums;
	}
	if(proid==3) //����SQL����ǰ���ش�����
		return dbservers[idx].m_nQueryCount;
	if(proid==4) //����д��������
		return dbservers[idx].m_nWriteCount;
	return 0;
}
static std::string splitstring(const char *szString,char c,int idx)
{
	if(szString==NULL) return std::string();
	int nCount=0;
	const char *ptrBegin=NULL,*ptrEnd=NULL;
	while(true){
		ptrEnd=strchr(szString,c);
		if(nCount==idx){ 
			ptrBegin=szString; break; 
		}else if(nCount>idx) break;
		else nCount++;
		if(ptrEnd==NULL) break;
		szString=ptrEnd+1; //����
	}
	if(ptrBegin==NULL) return std::string();
	return (ptrEnd==NULL)?std::string(ptrBegin):std::string(ptrBegin,(ptrEnd-ptrBegin));
}
//���ú�̨��SQL���ݿ�,szParam��ʽname=val name=val...
//���������ò��� mode=<rand|order> dbname=<���ݿ���> user=<�ʺ�> pswd=<����> dbhost=<ip:port,ip:port,...>
void CProxyServer ::SetBindServer(const char *szParam)
{
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	std::map<std::string,std::string> maps;
	std::map<std::string,std::string>::iterator it;
	if(SplitString(szParam,' ',maps)>0)
	{
		if((it=maps.find("rollback"))!=maps.end() )
		{
			m_bRollback=( (*it).second=="true" );
		}
		if((it=maps.find("waitreply"))!=maps.end() )
		{
			m_iSynDBTimeOut=atoi((*it).second.c_str());
		}
		//����DB����ѡ��ģʽ rand����� order��˳������
		if( (it=maps.find("mode"))!=maps.end() )
		{
			if((*it).second=="rand" || (*it).second=="order")
				m_iLoadMode=1; //���⸺��ģʽ
			else if((*it).second=="master")
				m_iLoadMode=0;			//���Ӹ���ģʽ
			else if((*it).second=="backup")
				m_iLoadMode=5;			//���ֱ���ģʽ
			m_idxSelectedMainDB=0; //ָ��Ĭ�ϵ�������������
		}
		
		if( (it=maps.find("dbhost"))!=maps.end())
		{
			int nCount=0; //��ЧDB��Ⱥ����������
			const char *szdbsvr=(*it).second.c_str();
			while(true){
				while(*szdbsvr==' ') szdbsvr++; //ȥ��ǰ���ո�
				const char *ptr_e=strchr(szdbsvr,',');
				if(ptr_e) *(char *)ptr_e=0;

				dbservers[nCount].Init();
				dbservers[nCount].m_svrhost=splitstring(szdbsvr,':',0);
				std::string strtmp=splitstring(szdbsvr,':',1);
				if(strtmp!=""){
					dbservers[nCount].m_svrport=atoi(strtmp.c_str());
					dbservers[nCount].m_username=splitstring(szdbsvr,':',2);
					dbservers[nCount].m_userpwd=splitstring(szdbsvr,':',3);
				}else dbservers[nCount].m_svrport=1433;

				if(dbservers[nCount].m_svrport>0 && dbservers[nCount].m_svrhost!="")
					++nCount;
				if(ptr_e==NULL) break;
				*(char *)ptr_e=','; szdbsvr=ptr_e+1;
			}//?while(true)
			dbservers[nCount].m_svrhost="";
			dbservers[nCount].m_svrport=0;
		}//?if( (it=maps.find("dbhost"))!=maps.end()
		if( (it=maps.find("dbname"))!=maps.end() && (*it).second!="" )
		{
			for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
				CDBServer &dbsvr=dbservers[idx];
				if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
				dbsvr.m_dbname =(*it).second;
			}
		}
		if( (it=maps.find("dbuser"))!=maps.end() && (*it).second!="" )
		{
			for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
				CDBServer &dbsvr=dbservers[idx];
				if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
				dbsvr.m_username =(*it).second;
			}
		}
		if( (it=maps.find("dbpswd"))!=maps.end() )
		{
			for(int idx=0;idx<MAX_NUM_SQLSERVER;idx++){
				CDBServer &dbsvr=dbservers[idx];
				if(dbsvr.m_svrhost=="" || dbsvr.m_svrport<=0 ) break; 
				dbsvr.m_userpwd =(*it).second;
			}
		}
		if( (it=maps.find("logquerys"))!=maps.end() && 
			( (*it).second=="true" || (*it).second=="all") )
		{
			if((*it).second=="all") //��¼����������ӡ�����Query Data(������)
				m_psqlAnalyse->m_iLogQuerys=-1; 
			else if((*it).second=="true") 
				m_psqlAnalyse->m_iLogQuerys=1; //��¼����ָ�Query Data
			else m_psqlAnalyse->m_iLogQuerys=0;
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] %s���ݿ�δͬ����¼��־\r\n", (m_psqlAnalyse->m_iLogQuerys==0)?"�ر�":"����");
		}
		if((it=maps.find("accesslist"))!=maps.end() )
		{//��ʽ <true|false|1:0>|ip,ip,ip....
			bool bAccess_default=false; //Ĭ�Ͻ�ֹ
			const char *ptrB=strchr((*it).second.c_str(),'|');
			if(ptrB!=NULL){
				*(char *)ptrB=0;
				if(_stricmp((*it).second.c_str(),"false")==0 || _stricmp((*it).second.c_str(),"0")==0 )
					bAccess_default=true; //��ֹ�б��е�ip���ʣ�Ĭ����������
				*(char *)ptrB=':'; ptrB+=1; //����:
			}else ptrB=(*it).second.c_str();
			m_vecAcessList.clear();
			for(int idx=0;;idx++){
				std::string strip=splitstring(ptrB,',',idx);
				if(strip=="") break;
				unsigned long ipAddr=CSocket::Host2IP(strip.c_str());
				if(ipAddr==INADDR_NONE) continue;
				TAccessAuth aa; aa.bAcess=!bAccess_default;
				aa.ipAddr =ipAddr; aa.ipMask =0xffffffff; //255.255.255.255
				m_vecAcessList.push_back(aa);
			}
			if(m_vecAcessList.size()!=0){//���Ĭ�Ϸ��ʹ���
				TAccessAuth aa; aa.bAcess=bAccess_default;
				aa.ipAddr =0; aa.ipMask =0;
				m_vecAcessList.push_back(aa);
				RW_LOG_PRINT(LOGLEVEL_DEBUG,"%s�б���IP���ʱ�����, IP��:%d\r\n",(bAccess_default)?"��ֹ":"����",m_vecAcessList.size()-1);
			}
			
		}//?if((it=maps.find("accesslist"))!=maps.end() )
		
		if((it=maps.find("syncond"))!=maps.end() )
		{
			if((*it).second=="false")
			{
				HANDLE hMutex=m_hSynMutex;
				if(hMutex!=NULL){
					WaitForSingleObject(hMutex, INFINITE);
					m_hSynMutex=NULL; CloseHandle(hMutex);
				}
			}else if((*it).second=="true")
			{
				if(m_hSynMutex==NULL) m_hSynMutex=CreateMutex(NULL, false, NULL);
			}
			/*(m_hSynMutex)?RW_LOG_PRINT(LOGLEVEL_ERROR,0,"[SQLProxy] �������л�ͬ��ִ�У�\r\n"):
						  RW_LOG_PRINT(LOGLEVEL_ERROR,0,"[SQLProxy] �رմ��л�ͬ��ִ�У�\r\n");*/
		}
	}//?if(SplitString(szParam,' ',maps)>0)
	//���⸺��ģʽ�������ͬ�����޷�����SQL�ķ������Ļ���ͬ��
	if(m_iLoadMode==1 && m_iSynDBTimeOut<0) m_iSynDBTimeOut=0;
	if(m_iSynDBTimeOut<0) m_bRollback=false; //��������˽�ֹͬ������ͬ��ʧ�ܻع���Ч
}

//��һ���ͻ������ӹر�
void CProxyServer :: SocketClosed(CSocket *pSocket)
{
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	CSQLClient *pclnt=(CSQLClient *)pSocket->GetUserTag();
	pSocket->SetUserTag(0);
	if(pclnt!=NULL){
		int idxSelectedDBSvr=pclnt->GetSelectedDBSvr();
		RW_LOG_PRINT(LOGLEVEL_INFO,"[SQLProxy] one client(%s:%d) closed!\r\n\t DB server: %d - %s:%d\r\n",
						pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),idxSelectedDBSvr+1,
						dbservers[idxSelectedDBSvr].m_svrhost.c_str(),dbservers[idxSelectedDBSvr].m_svrport);
		delete pclnt;
	}
}
//��һ���ͻ������ӽ���
bool CProxyServer :: SocketAccepted(CSocket *pSocket)
{
	bool bAccess=true; //�ж��Ƿ��������
	unsigned long clntip=pSocket->GetRemoteAddr();
	for(unsigned int idx=0;idx<m_vecAcessList.size();idx++)
	{
		if( (clntip & m_vecAcessList[idx].ipMask)!=m_vecAcessList[idx].ipAddr )
			continue;
		bAccess=m_vecAcessList[idx].bAcess; break;
	}
	if(!bAccess){
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] one client(%s:%d) connected - �ܾ�����!\r\n",pSocket->GetRemoteAddress(),pSocket->GetRemotePort());
		return false; //�ܾ����ʣ��ر�����
	}
	
	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	int idxLoadDBsvr,idxSelectedDBSvr=-1; //��ǰ�ɹ����ӵ�������������
	CSQLClient *pclnt=new CSQLClient(this);
	if(pclnt==NULL) return false;
	CSocket *pMainDBSocket=ConnectMainDB(&idxSelectedDBSvr,pclnt);
	if(pMainDBSocket==NULL){ delete pclnt; return false; }
	RW_LOG_PRINT(LOGLEVEL_INFO,"[SQLProxy] one client(%s:%d) connected!\r\n\t DB server: %d - %s:%d\r\n",
							pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),idxSelectedDBSvr+1,
							dbservers[idxSelectedDBSvr].m_svrhost.c_str(),dbservers[idxSelectedDBSvr].m_svrport);
	idxLoadDBsvr=(rand() % m_nValidDBServer); //���ѡ��һ̨���Ӳ�ѯ������������������ÿ���ͻ����ӵ�һ����ʼ
	pclnt->m_tds_dbname=dbservers[idxSelectedDBSvr].m_dbname;
	pclnt->SetMainDBSocket(pSocket,pMainDBSocket,idxSelectedDBSvr,idxLoadDBsvr);
	pSocket->SetUserTag((int)pclnt);
	return true;
}

bool CProxyServer :: SocketReceived(CSocket *pSocket)
{
	CSQLClient *pclnt=(CSQLClient *)pSocket->GetUserTag();
	if(pclnt==NULL) return false;
	IOutputStream *pOutStream=pSocket->GetOutputStream();
	CSocketStream *pInStream=(CSocketStream *) pSocket->GetInputStream();
	QueryPacket * plastquery=NULL;

	while(true) //ѭ�����������ѽ��յ�����
	{
		unsigned int nSize=pInStream->GetInputSize(); //����δ�������ݴ�С
		unsigned int offset=pInStream->GetReadPos();
		unsigned char *pInbuffer=(unsigned char *)pInStream->GetInput()+offset; //δ����������ʼָ��
		if(nSize==0) break; //�����Ѵ������
		if(nSize<sizeof(tds_header)) break; //����δ�������,��������
		
		tds_header tdsh;
		unsigned char *pinstream=pInbuffer;
		pinstream+=parse_tds_header(pinstream,tdsh);
		if(tdsh.type<0x01 || tdsh.type >0x1f){	
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] ���ش��� - ��Ч��Э�����ݣ�T=0x%02X S=%d - %d\r\n",tdsh.type,tdsh.size,nSize);
			pInStream->Skip(nSize); return false; //�ر�socket
		}
		if(!pclnt->m_bLoginDB){ //δ�ɹ���¼��������
			if(tdsh.type ==0x17 || tdsh.type ==TDS7_LOGIN)
			{//0x17ʵ��Ϊssl���ܵĵ�¼���ݰ� (TDSЭ����ֻ�е�¼������ssl���ܣ��������ݰ�������)
			 //�������ssl���ܷ�ʽ��һ��Ԥ��¼������2��ssl����Э�齻����(typeҲ��0x12)��Ȼ���Ǽ��ܵĵ�¼��(type=0x17)
				if(tdsh.type ==TDS7_LOGIN) nSize=tdsh.size; //�Ѵ����ֽ�
				if(!Parse_LoginPacket(pSocket,pInbuffer,nSize))
					return false;	
			}else{
				if(tdsh.type ==TDS71_PRELOGIN && *pinstream==0) //Ԥ��¼�� 0x12
				{//����ssl���ܵ�¼����ssl�ĵ�2���������� TDS Heaer��typeҲ��0x12,���Ҫע�Ᵽ��
					unsigned char *lpEncry=parse_prelogin_packet(pInbuffer,1);
					if(lpEncry!=NULL) *lpEncry=0x02; //����Ϊ�����ܵ�¼��Ϣ
					pclnt->m_tdsver=GetNetLib_PreLogin(pInbuffer);
				}
				nSize=tdsh.size; //��¼ǰ���������ݶ�ԭ�ⲻ��ת��
				pclnt->SendWaitRelpy(pInbuffer,nSize,0); //ת������,���ȴ�����
			}
			//�ƶ����ݴ���ָ�룬�����Ѵ������ݴ�С
			pInStream->Skip(nSize);  continue;
		}//?if(!pclnt->m_bLoginDB){ //δ�ɹ���¼��������

		//����ɹ���¼��������
		if(tdsh.size>nSize) break; //����δ�������,��������
		//���յ���һ������packet���������� begin====================================
		RW_LOG_DEBUG("[SQLProxy] Received %d request data from %s:%d\r\n\t   type=0x%x status=%d size=%d\r\n",
						nSize,pSocket->GetRemoteAddress(),pSocket->GetRemotePort(),tdsh.type ,tdsh.status ,tdsh.size);
		
		//�Ż�: ���ֻ��һ����Ч�����ݿ⣬���ûع�false�����ٲ���Ҫ�ĵ��úʹ���
		bool bRollback=(m_nValidDBServer>1)?m_bRollback:false; 
		if(tdsh.type ==TDS_QUERY || tdsh.type ==TDS_RPC)
		{//����SQL������Ϣ Query��RPC��RPCΪ������SQL
			if(plastquery==NULL)
			{
				if( (plastquery=new QueryPacket(pclnt->GetSelectedDBSvr(),pSocket->GetRemoteAddr()) )!=NULL )
				{
					plastquery->SetQueryData(pInbuffer,tdsh.size);
					plastquery->IsNotSelectSQL(true); //�ж��Ƿ�Ϊ��select���
				}
			}else plastquery->AddQueryData(pInbuffer,tdsh.size);
			if( !IS_LASTPACKET(tdsh.status) || plastquery==NULL) goto PACKET_DONE;//û�н��յ�һ������SQL�������

			if(0!=m_iLoadMode)//���౸�ݻ���⸺��ģʽ�����в���������������ִ�У�д�����ִ�гɹ���ͬ�����������ط�������
				pclnt->SendQuery_mode1(plastquery,bRollback);
			else //==0 ���Ӹ���ģʽ,��ѯ�������ѡ���ط�����ִ�У�д�����ִ�гɹ���ͬ�����������ط�������
				pclnt->SendQuery_mode0(plastquery,bRollback);
			plastquery=NULL; //�Ѵ��������ͷ�
		}//?if(tdsh.type ==TDS_QUERY || tdsh.type ==TDS_RPC)
		else if(tdsh.type ==TDS7_TRANS) //��������Ϣ
			pclnt->TransactPacket(pInbuffer,tdsh.size,bRollback);
		else //������Ϣ������ת������������
			pclnt->SendWaitRelpy(pInbuffer,tdsh.size,0); //ת������,���ȴ�����

PACKET_DONE:
		//���յ���һ������packet����������  end ====================================
		pInStream->Skip(tdsh.size); //�ƶ����ݴ���ָ�룬�����Ѵ������ݴ�С
	}//?while(true)
	//if(plastquery!=NULL) delete plastquery;
	if(plastquery!=NULL){//yyc modify 2017-11-05 ����˶�����Ч��˵���˴ν��յ����ݲ���һ��������sql�������ݰ�
	//���Ҫ�ָ��Ѵ���Ľ������ݣ����ϲ�socket�������ն�����ֱ���ͷţ������ϲ��ٴν��յ����ݽ���˴������ʱ��ǰ���յ������Ѿ����ͷŵ���
		pInStream->UnSkip(plastquery->GetQueryDataSize()); delete plastquery;
	}
	return true;
}

//����������ת����¼���ݰ��������δ���ܵ�¼���ݰ��������¼��Ϣ����¼�ɹ������ӵ�¼���ط�����
bool CProxyServer :: Parse_LoginPacket(CSocket *pSocket,unsigned char *pInbuffer,unsigned int nSize)
{
	CDBServer *dbservers=(CDBServer *)m_dbservers;
	CSQLClient *pclnt=(CSQLClient *)pSocket->GetUserTag();
	if(pclnt==NULL) return false;
	int iLoginOK=0,idxSelectedDBSvr=pclnt->GetSelectedDBSvr();
	unsigned int lTimeout=MAX_WAITREPLY_TIMEOUT; //��¼���ݰ���Ӧ�ظ����ȴ��ӳ�(s)
	unsigned char type=*pInbuffer; //��¼���ݰ�����(TDS7_LOGIN δ���ܵ�)
	tds_login login, *ptr_login=NULL;
	if(type ==TDS7_LOGIN){
		parse_login_packet(pInbuffer,login); ptr_login=&login;
		pclnt->m_tdsver =GetUSVerByULVer(login.version);
		RW_LOG_PRINT(LOGLEVEL_DEBUG,"[SQLProxy] Access '%s' DB using %s:****** ;ver=0x%x block=%d\r\n",login.dbname.c_str(),
									login.username.c_str(),pclnt->m_tdsver,login.psz);//login.userpswd.c_str()
		
		CDBServer &sqlsvr=dbservers[idxSelectedDBSvr];
		std::string dbuser=sqlsvr.m_username,dbpswd=sqlsvr.m_userpwd,dbname=sqlsvr.m_dbname;
		if(dbname==""){ dbname=login.dbname; pclnt->m_tds_dbname=dbname; }
		if(dbuser==""){ dbuser=login.username; dbpswd=login.userpswd; }
		//���ָ�������ݿ��������жϵ�ǰ�������ݿ���趨�����ݿ��Ƿ�һ��
		if(_stricmp(dbname.c_str(),login.dbname.c_str())!=0)
		{
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] '%s' �ܾ����� - ���ʵ����ݿ���趨�Ĳ�һ��\r\n",login.dbname.c_str());
			return false;
		}
		if(_stricmp(dbuser.c_str(),login.username.c_str())!=0 || dbpswd!=login.userpswd)
		{//���ĵ�¼Packet�е�¼��ϢȻ���¼
			unsigned char NewLoginPacket[512]; 
			unsigned int NewSize=write_login_new(NewLoginPacket,512,login,NULL,dbuser.c_str(),dbpswd.c_str());
			iLoginOK=(NewSize==0)?0x0101:
					  pclnt->SendWaitRelpy(NewLoginPacket,NewSize,lTimeout);
		}else //����ת��ԭʼ��¼���ݰ���Ϣ���е�¼
			iLoginOK=pclnt->SendWaitRelpy(pInbuffer,nSize,lTimeout); //ת������,�ȴ�����
	}else //����ֱ��ת��ԭʼ�������ݰ����е�¼
		iLoginOK=pclnt->SendWaitRelpy(pInbuffer,nSize,lTimeout); //ת������,�ȴ�����
	if(iLoginOK!=0 && iLoginOK!=-1){ //�������ݿ�ʧ��
		unsigned short tds_token=(iLoginOK & 0x0000ffff),bit_flags=((iLoginOK & 0xffff0000)>>16);
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] �������ݿ�ʧ�� - Token=%02X %02X\r\n",tds_token, bit_flags);
		return false;
	}
	//�����δ���ܵĵ�¼���������Ȳ���ԭʼ��¼�����и��ط������ĵ�¼��������֤���������͸��ط������ĵ�¼����һ��
	unsigned char *login_packet=(type ==TDS7_LOGIN)?pInbuffer:NULL;
	pclnt->InitDBConn(login_packet,nSize,ptr_login); //��ʱ�ſ�ʼ��ʼ����̨���ݿ����Ӳ����������߳�
	return (pclnt->m_bLoginDB=true);
}

//������������
CSocket * CProxyServer :: ConnectMainDB(int *pIdxMainDB,ISocketListener *pListener)
{
	CSocket *pSocket = new CSocket(SOCK_STREAM);
	if(pSocket==NULL) return NULL;

	CDBServer *dbservers=(CDBServer *)this->m_dbservers;
	int idxSelected=m_idxSelectedMainDB; //��ǰӦѡ���������������
	unsigned int i,iNumsSocketSvrs=0; //SQL���ݿ⼯Ⱥ�ĸ���
	unsigned int iNumsValidSocketSvrs=0; //��Ч���ݿ⼯Ⱥ����
	for(;iNumsSocketSvrs<MAX_NUM_SQLSERVER;iNumsSocketSvrs++)
	{
		CDBServer &sqlsvr=dbservers[iNumsSocketSvrs];
		if(sqlsvr.m_svrport<=0 || sqlsvr.m_svrhost=="") 
			break;
		if(sqlsvr.IsValid()) iNumsValidSocketSvrs++;
	}
	m_nValidDBServer=iNumsValidSocketSvrs;

	for(i=0;i<iNumsSocketSvrs;i++)
	{
		CDBServer &sqlsvr=dbservers[idxSelected];
		if( sqlsvr.IsValid() && 
			pSocket->Connect(sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport,pListener) )
		{ //��������Ч�����ӳɹ�
			sqlsvr.m_nSQLClients++;
			//����Ǿ��⸺��ģʽ����������������++,�´�����ѡ����һ��
			m_idxSelectedMainDB=(m_iLoadMode==1)?((idxSelected+1) % iNumsSocketSvrs):idxSelected;
			if(pIdxMainDB) *pIdxMainDB=idxSelected; //���ص�ǰѡ���������������
			return pSocket; 
		}
		//�����ǰ��������Ч���߷���������ʧ�ܣ���ѡ����һ������������
		idxSelected=(idxSelected+1) % iNumsSocketSvrs;
		if(!sqlsvr.IsValid()) continue; //�÷������ѱ���쳣
		sqlsvr.SetValid(1);//�������ô�SQL�������쳣
		RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] Failed to connect %d - %s:%d\r\n",idxSelected,sqlsvr.m_svrhost.c_str(),sqlsvr.m_svrport);
	}//?for(i=0;i<iNumsSocketSvrs;i++)
	delete pSocket; return NULL;
}
