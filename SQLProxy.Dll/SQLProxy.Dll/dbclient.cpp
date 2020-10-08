#include "stdafx.h"
#include <time.h>
#pragma warning(disable:4996)

#include "mytds.h"
#include "dbclient.h"
#include "DebugLog.h"
using namespace std;


static const TDS_UCHAR tds72_query_start[] = {
	/* total length */
	0x16, 0, 0, 0,
	/* length */
	0x12, 0, 0, 0,
	/* type */
	0x02, 0,
	/* transaction */
	0, 0, 0, 0, 0, 0, 0, 0,
	/* request count */
	1, 0, 0, 0
};
//��ʼ��������TDS��¼������Ϣ
static void Init_tds_login_params(tds_login_param &tdsparam)
{
	tdsparam.tdsversion=0x701;	//Ĭ�ϲ��õ�tdsЭ��汾
	tdsparam.tds_ssl_login=0;	//��¼ʱ�Ƿ����ssl���ܻ��� 0���� 1��
	tdsparam.tds_block_size=4096;		//TDS 7.0+ use a default of 4096 as block size.
}
//�����趨block size����packet�����packet��С����block size��ֳɶ��packet����
static bool SendOnePacket(CSocket *psocket,tds_header &tdsh,unsigned char *pinstream,int block_size)
{
	char tmpbuf[sizeof(tds_header)]; //��ʱ���ݻָ����ݻ���
	if(tdsh.size >block_size)
	{//�ֳɶ��packet����
		unsigned short packetLen=tdsh.size-sizeof(tds_header); //������TDSͷ�����ݳ���
		unsigned short usMaxDataLen=block_size-sizeof(tds_header);
		while(packetLen!=0)
		{
			tds_header tdsh_new; ::memcpy(&tdsh_new,&tdsh,sizeof(tds_header));
			unsigned short usDataLen=(packetLen>usMaxDataLen)?usMaxDataLen:packetLen;
			packetLen-=usDataLen;
			tdsh_new.size=usDataLen+sizeof(tds_header);
			if( IS_LASTPACKET(tdsh.status) )
			{//ԭ��Ϊ���һ��������ʱ���ֳɶ����
				tdsh_new.status=(packetLen!=0)?0:0x01; //tdsh.status;
			}else tdsh_new.status=tdsh.status; //����ԭ��status����

			::memcpy(tmpbuf,pinstream,sizeof(tds_header));  //����
			write_tds_header(pinstream,tdsh_new);			//д���µ�TDS��Э��ͷ
			int iSendRet=psocket->Send(pinstream, tdsh_new.size);
			::memcpy(pinstream,tmpbuf,sizeof(tds_header));  //�ָ�
			if(iSendRet<=0) return false; //����ʧ��
			pinstream=pinstream+tdsh_new.size-sizeof(tds_header);
		}//?while(packetLen!=0)
		return true;
	}
	return (psocket->Send(pinstream, tdsh.size)>0);
}
static bool SendOnePacket(CSocket *psocket,unsigned char *pinstream,int block_size)
{
	tds_header tdsh; parse_tds_header(pinstream,tdsh);
	return SendOnePacket(psocket,tdsh,pinstream,block_size);
}

DBConn :: DBConn()
{
	m_psocket=NULL;
	m_dbconn=NULL;
	m_uTransactionID=0;
	m_pfnOnStatus=NULL;
	m_pfnOnData=NULL;
	m_lUserParam=0;
	Init_tds_login_params(m_params);
}

DBConn :: ~DBConn()
{
	Close();	//�ر����ݿ�����
}

void DBConn::Setdbinfo(const char *szHost,int iport,const char *szDbname,const char *szUsername,const char *szUserpwd)
{
	if(szHost) m_hostname.assign(szHost);
	m_hostport=iport;
	if(szDbname) m_params.dbname.assign(szDbname);
	if(szUsername) m_params.dbuser.assign(szUsername);
	if(szUserpwd)  m_params.dbpswd.assign(szUserpwd);
}

//����SQL��䣬�ɹ�������;lTimeout:��ʱʱ�� s
bool DBConn::ExcuteSQL(const char *szsql,int lTimeout)
{
	std::string strsql(szsql);
	std::wstring wstrsql=MultibyteToWide(strsql);
	int iPacketlen=sizeof(tds_header)+strsql.length()*sizeof(wchar_t);
	if(IS_TDS72_PLUS(m_params.tdsversion)) iPacketlen+=TDS9_QUERY_START_LEN;

	unsigned char *szPacket=new unsigned char[iPacketlen];
	if(szPacket==NULL) return false;
	tds_header tdsh; tdsh.type =TDS_QUERY;
	tdsh.status =0x01; tdsh.size =iPacketlen; 
	tdsh.channel=0; tdsh.packet_number=tdsh.window=0;
	unsigned int buflen=write_tds_header(szPacket,tdsh);
	if(IS_TDS72_PLUS(m_params.tdsversion))
	{
		::memcpy(szPacket+buflen,tds72_query_start,TDS9_QUERY_START_LEN);
		buflen+=TDS9_QUERY_START_LEN;
	}
	::memcpy(szPacket+buflen,wstrsql.c_str(),strsql.length()*sizeof(wchar_t));
	buflen+=strsql.length()*sizeof(wchar_t);
	
	int iresult=WaitForResult(szPacket,tdsh.size,lTimeout); 
	delete[] szPacket; return (iresult==0);
}
//tdsv - ��ǰҪִ�з������ݵİ汾
bool DBConn::ExcuteData(unsigned short tdsv,unsigned char *ptrData,int iLen)
{
	if(tdsv==m_params.tdsversion){
		//query data�汾�͵�ǰ�������ݿ�ʹ�õ�TDS�汾һ�£�ֱ��ת��
		int nReadCount=0; tds_header tdsh;
		while(nReadCount<iLen)
		{
			unsigned char *pinstream=ptrData+nReadCount;
			pinstream+=parse_tds_header(pinstream,tdsh);
			if(IS_TDS9_QUERY_START(pinstream)){ //���������Transaction IDΪ��ǰTransaction ID
				//ʵ�ʲ��Է���(0x703�汾)���������Query��statusΪ0x01Ϊ��������־ ������Ϊ0x09.
				unsigned char *psta=pinstream-sizeof(tds_header)+1;
				if(m_uTransactionID!=0 && (tdsh.type==TDS_QUERY || tdsh.type==TDS_RPC) )
					*psta &=0xF7; //ȡ��RESETCONNECTIONλ(0x08)
				TDS_UINT8 *pTransID=(TDS_UINT8 *)( pinstream+4+4+2); 
				*pTransID=m_uTransactionID;
			} 
			m_psocket->SetUserTag(WAITREPLY_FLAG); //���õȴ���Ϣ�ظ���ʶ
			if(m_psocket->Send(pinstream-sizeof(tds_header), tdsh.size)<=0)
			{ m_psocket->SetUserTag(0);	return false; } //����ʧ��
			nReadCount+=tdsh.size;
		}
		return true;
	}//?if(tdsv==tdsparam.tdsversion)
	//������õ�ǰ�������ݵ�Э��汾����Query ����
	return IS_TDS72_PLUS(m_params.tdsversion)?ExcuteData_TDS72PLUS(ptrData,iLen):ExcuteData_TDS72SUB(ptrData,iLen);
}

//����0x702����Э�鷢��query data�����query data����START_QUERY��Ϣ��Ҫȥ��
bool DBConn::ExcuteData_TDS72SUB(unsigned char *ptrData,int iLen)
{
	int block_size=m_params.tds_block_size;
	char tmpbuf[sizeof(tds_header)]; //��ʱ���ݻָ����ݻ���
	tds_header tdsh_old,tdsh_new;
	int nReadCount=0;
	while(nReadCount<iLen)
	{
		unsigned char *pinstream=ptrData+nReadCount;
		pinstream+=parse_tds_header(pinstream,tdsh_old);
		//�ж��Ƿ����START_QUERY,�������������ȥ��
		int nSkipBytes=(IS_TDS9_QUERY_START(pinstream))?TDS9_QUERY_START_LEN:0;
		::memcpy(&tdsh_new,&tdsh_old,sizeof(tds_header));
		tdsh_new.status=(tdsh_old.status!=0x0)?0x01:0;
		tdsh_new.size=tdsh_old.size-nSkipBytes;
		
		m_psocket->SetUserTag(WAITREPLY_FLAG); //���õȴ���Ϣ�ظ���ʶ
		pinstream=pinstream+nSkipBytes-sizeof(tds_header); //�µ�packet��ʼλ��(����TDSͷ)
		::memcpy(tmpbuf,pinstream,sizeof(tds_header));  //����
		write_tds_header(pinstream,tdsh_new);			//д���µ�TDS7.1��Э��ͷ
		bool bret=SendOnePacket(m_psocket,tdsh_new,pinstream,block_size);
		::memcpy(pinstream,tmpbuf,sizeof(tds_header));  //�ָ�

		if(!bret){ m_psocket->SetUserTag(0); return false;} //����ʧ��
		nReadCount+=tdsh_old.size;
	}//?while(nReadCount<iLen)
	return true;
}
//����0x702������Э�鷢��query data�����query data������START_QUERY��Ϣ��Ҫ�����
bool DBConn::ExcuteData_TDS72PLUS(unsigned char *ptrData,int iLen)
{
	tds_header tdsh_old,tdsh_new;
	int nReadCount=0,block_size=m_params.tds_block_size;
	while(nReadCount<iLen)
	{
		unsigned char *ptrBuffer=NULL;
		unsigned char *pinstream=ptrData+nReadCount;
		pinstream+=parse_tds_header(pinstream,tdsh_old);
		if( !IS_TDS9_QUERY_START(pinstream) )
		{//����������STRAT QUERY��Ϣ
			ptrBuffer=(unsigned char *)::malloc(tdsh_old.size+TDS9_QUERY_START_LEN);
			if(ptrBuffer==NULL) return false;
			::memcpy(&tdsh_new,&tdsh_old,sizeof(tds_header));
			tdsh_new.size =tdsh_old.size+TDS9_QUERY_START_LEN;
			int buflen=write_tds_header(ptrBuffer,tdsh_new);
			::memcpy(ptrBuffer+buflen,tds72_query_start,TDS9_QUERY_START_LEN);
			TDS_UINT8 *pTransID=(TDS_UINT8 *)(ptrBuffer+buflen+4+4+2); 
			*pTransID=m_uTransactionID;//���������Transaction IDΪ��ǰTransaction ID
			buflen+=TDS9_QUERY_START_LEN;
			::memcpy(ptrBuffer+buflen,pinstream,tdsh_old.size-sizeof(tds_header));
		}else{ //����STRAT QUERY��Ϣ
			TDS_UINT8 *pTransID=(TDS_UINT8 *)( pinstream+4+4+2); 
			*pTransID=m_uTransactionID;//���������Transaction IDΪ��ǰTransaction ID;0������
			pinstream=pinstream-sizeof(tds_header);
		}
		unsigned char *psta=(ptrBuffer!=NULL)?(ptrBuffer+1):(pinstream+1);
		if(m_uTransactionID!=0 && (tdsh_old.type==TDS_QUERY || tdsh_old.type==TDS_RPC) )
					*psta &=0xF7; //ȡ��RESETCONNECTIONλ(0x08)

		m_psocket->SetUserTag(WAITREPLY_FLAG); //���õȴ���Ϣ�ظ���ʶ
		bool bret=(ptrBuffer!=NULL)?SendOnePacket(m_psocket,tdsh_new,ptrBuffer,block_size):
									SendOnePacket(m_psocket,tdsh_old,pinstream,block_size);
		if(ptrBuffer!=NULL) ::free(ptrBuffer);
		if(!bret){ m_psocket->SetUserTag(0); return false;} //����ʧ��
		nReadCount+=tdsh_old.size;
	}//?while(nReadCount<iLen)
	return true;
}

void DBConn :: SocketClosed(CSocket *pSocket)
{
	int iEvent=(m_psocket==NULL)?1:2; //���������رջ����쳣
	LOGLEVEL ll=(m_psocket==NULL)?LOGLEVEL_INFO:LOGLEVEL_ERROR; 
	RW_LOG_PRINT(ll,"[SQLProxy] Connection to SQL SERVER(%s:%d) has been closed\r\n",m_hostname.c_str(),m_hostport);
	if(m_pfnOnStatus) (*m_pfnOnStatus)(iEvent,m_lUserParam);
}
//���մ���SQL���������ص�����
bool DBConn :: SocketReceived(CSocket *pSocket)
{
	CSocketStream *pInStream= (CSocketStream *)pSocket->GetInputStream();
	bool bNewReply=true; //����Ƿ�ʼһ����Ϣ�Ļظ�packet��ÿ��reply���ܰ������packets
	long lUserTag=0;	//���==WAITREPLY_FLAG˵�����첽�ȴ��ظ��Ĵ���������Ϊcond��ͬ���ȴ������ַ
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
			RW_LOG_PRINT(LOGLEVEL_ERROR,"[SQLProxy] ���ش��� - ��Ч��Э�����ݣ�T=0x%02X s=%d - %d\r\n",tdsh.type,tdsh.size,nSize);
			pInStream->Skip(nSize); break;
		}
		if(tdsh.size>nSize) break; //����δ�������,��������
		//���յ���һ������packet���������� begin====================================
		RW_LOG_DEBUG("[SQLProxy] Received %d data from DB Server\r\n\t   type=0x%x status=%d size=%d\r\n",
								nSize,tdsh.type ,tdsh.status ,tdsh.size);
		
		if(m_pfnOnData) //���������ݵ���ص���ֱ��ת��
			(*m_pfnOnData)(&tdsh,pInbuffer,m_lUserParam);
		else if(tdsh.type ==TDS_REPLY)
		{//�ظ���Ϣ
			if( (lUserTag=pSocket->GetUserTag())!=0) //�����ȴ��ظ�����Ĵ���
			{
				(bNewReply)?m_lastreply.SetReplyData(pInbuffer,tdsh.size):
							m_lastreply.AddReplyData(pInbuffer,tdsh.size);
				if( IS_LASTPACKET(tdsh.status) )
				{//�ظ������һ��packet
					m_lastreply.m_iresult=(int)parse_reply(pInbuffer,m_params.tdsversion,&m_lastreply.m_nAffectRows);
					pSocket->SetUserTag(0); //�ѽ��յ��ظ���ȡ�������ȴ��ظ�����Ĵ����ʶ
					bNewReply=true; //��ʼһ���µĻظ����մ���
				}else bNewReply=false; //�������иûظ���packet������
			}//?if(pSocket->GetUserTag()!=0)
		}//�ظ���Ϣ����=====================
		
		//���յ���һ������packet����������  end ====================================
		pInStream->Skip(tdsh.size); //�ƶ����ݴ���ָ�룬�����Ѵ������ݴ�С
	}//while(true)
	return true;
}

//�������ݲ��ȴ�ִ�з��ؽ�� lTimeout(s):��ʱʱ�� =0���ȴ� <0���޵ȴ�����ȴ�ָ��s
//����0�ɹ� -2����ʧ�� -1��Ӧ��ʱ ������Ӧ������
int DBConn::WaitForResult(void *pdata,unsigned int nsize,int lTimeout)
{
	if(lTimeout==0){ //���ȴ�����
		m_psocket->SetUserTag(WAITREPLY_FLAG); //���õȴ���Ϣ�ظ���ʶ
		if(m_psocket->Send(pdata,nsize)>0) return 0;
		m_psocket->SetUserTag(0); return DBSTATUS_ERROR;
	}//����ȴ�����lTimeout<0 ���޵ȴ�
	
	int iresult=-1; //��ʱ
	DWORD msTimeOut=(lTimeout>0)?(lTimeout*1000):(300*1000);
	if(pdata!=NULL) m_psocket->SetUserTag(WAITREPLY_FLAG);
	if(pdata==NULL || m_psocket->Send(pdata,nsize)>0)
	{
		long tStartTime=clock();
		while(true)
		{
			iresult=-1; //��ʱ
			if( (clock()-tStartTime)>msTimeOut ) break;
			if( (iresult=this->GetStatus_Asyn()) !=DBSTATUS_PROCESS) 
				break;
			::Sleep(1); //��ʱ,����
		}
	}else iresult=DBSTATUS_ERROR; //�������Ӵ���
	m_psocket->SetUserTag(0);	return iresult;

/*	//���̣߳��п���cond.wait�ȴ�ǰ�����Ѿ���������޷������Ӧ
	clsCond cond; int iresult=DBSTATUS_ERROR;
	DWORD msTimeOut=(lTimeout>0)?(lTimeout*1000):0;
	m_psocket->SetUserTag((int)&cond);
	if( m_psocket->Send(pdata,nsize)>0)
		iresult=(cond.wait(msTimeOut))?(m_lastreply.m_iresult):-1;
	m_psocket->SetUserTag(0);	return iresult; */
}

