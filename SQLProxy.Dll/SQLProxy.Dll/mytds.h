
#ifndef _SQLTDS_H_
#define _SQLTDS_H_
#include <iostream>
#include <string>
typedef std::basic_string<wchar_t> wstring;

/*TDS header*/
typedef struct tds_header{
	unsigned char type;
	unsigned char status;
	unsigned short size;
	unsigned short channel;
	unsigned char packet_number;
	unsigned char window;
}tds_header;
#define IS_LASTPACKET(s) ( ((s) & 0x01)!=0 )

/*Login Packet*/
typedef struct tds_login{
	unsigned int tlen;      //total packet size
	unsigned int version;   //TDS Version	0x00000070 for TDS7, 0x01000071 for TDS8
	unsigned int psz;       //packet size (default 4096)
	unsigned int client_version ;   //client program version
	unsigned int client_pid;        //PID of client
	unsigned int connection_id;   //connection id (usually 0)
	unsigned char opt_flag1;//option flags1
	unsigned char opt_flag2;//option flags2
	unsigned char sql_flag;//sql type flags 
	unsigned char res_flag;//reserved flags 
	unsigned int time_zone;//time zone 
	unsigned int collation;//collation information
	std::string hostname;
	std::string username;
	std::string userpswd;
	std::string appname;
	std::string servername;
	std::string remotepair; //00 00 00 00
	std::string libraryname;
	std::string language;
	std::string dbname;
	unsigned char clt_mac[6];//MAC address of client
	std::string authnt; 
}tds_login;

//�汾>=0x702ʱquery 0x01��rpc 0x03����sql����ʱ������ͷ
typedef struct tds9_query_start
{
	unsigned int totalLength;	//�̶���ֵ0x16 /*0x16, 0, 0, 0*/
	unsigned int length;		//�̶���ֵ0x12 /*0x12, 0, 0, 0*/
	unsigned short type;		//type��Ĭ��Ϊ /*0x02,0*/
	unsigned __int64 transaction; //��������������0x0E��db�������᷵����Ӧ��trancid.���������ֵĬ��Ϊ0
	unsigned int count;			//request count,Ĭ��Ϊ/* 1, 0, 0, 0 */
}TDS9_QUERY_START;
#define TDS9_QUERY_START_LEN	0x16
#define IS_TDS9_QUERY_START(ptr) (*(unsigned int *)(ptr)==0x00000016 && *(unsigned int *)(ptr+4)==0x00000012)
#define IS_TDS72_PLUS(x) ((x)>=0x702)
#define IS_TDS73_PLUS(x) ((x)>=0x703)

typedef enum tds_packet_type
{
	TDS_QUERY = 0x01,	//TDS 4.2 or 7.0 query
	TDS_LOGIN = 0x02,	//TDS 4.2 or 5.0 login packet
	TDS_RPC = 0x03,		//RPC
	TDS_REPLY = 0x04,	//responses from server
	TDS_CANCEL = 0x06,	//cancels
	TDS_BULK = 0x07,	//Used in Bulk Copy
	TDS7_TRANS=0x0E,	//TDS72 (0x702�汾֧�ֵ�����packet)
	TDS_NORMAL = 0x0F,	//TDS 5.0 query
	TDS7_LOGIN = 0x10,	//TDS 7.0 login packet
	TDS7_AUTH = 0x11,	//TDS 7.0authentication packet
	TDS71_PRELOGIN =0x12	//TDS7.1 prelogin packet
} TDS_PACKET_TYPE;

#define TDS_DONE_TOKEN            253	/* 0xFD    TDS_DONE                  */
#define TDS_DONEPROC_TOKEN        254	/* 0xFE    TDS_DONEPROC              */
#define TDS_DONEINPROC_TOKEN      255	/* 0xFF    TDS_DONEINPROC            */
#define is_end_token(x) (x==TDS_DONE_TOKEN||x==TDS_DONEPROC_TOKEN||x==TDS_DONEINPROC_TOKEN)
#define TDS_DONE_ERROR		0x02
#define TDS_DONE_CANCELLED	0x20

typedef char TDS_CHAR;					/*  8-bit char     */
typedef unsigned char TDS_UCHAR;			/*  8-bit uchar    */
typedef unsigned char TDS_TINYINT;			/*  8-bit unsigned */
typedef short TDS_SMALLINT;		/* 16-bit int      */
typedef unsigned short TDS_USMALLINT;	/* 16-bit unsigned */
typedef int TDS_INT;			/* 32-bit int      */
typedef unsigned int TDS_UINT;	/* 32-bit unsigned */
typedef float TDS_REAL;		/* 32-bit real     */
typedef double TDS_FLOAT;		/* 64-bit real     */
typedef __int64 TDS_INT8;			/* 64-bit integer  */
typedef unsigned __int64 TDS_UINT8;	/* 64-bit unsigned */


unsigned int GetULVerByUSVer(unsigned short tdsv);
unsigned short GetUSVerByULVer(unsigned int tdsversion);
unsigned char * parse_prelogin_packet(void * pinstream,unsigned char idx);
//����netlib��Ϣ�����ж�tdsЭ��汾 (����׼ȷ)
unsigned short GetNetLib_PreLogin(void * pinstream);
//�����ظ�packet��ȡִ�н��,����0ִ�гɹ�������ִ��ʧ�� (��2�ֽ�Token����2�ֽ�bit_falgs)
unsigned int parse_reply(void * pinstream,unsigned short tdsver,unsigned long *nAffectRows);
unsigned int parse_tds_header(void * pinstream,tds_header &tdsh);
unsigned int write_tds_header(void * pinstream,tds_header &tdsh);

//����/д��login packet��Ϣ������login packet size
unsigned int parse_login_packet(void * pinstream,tds_login &login);
unsigned int write_login_packet(void * pinstream,tds_login &login);
unsigned int write_login_new(unsigned char *pInBuffer,unsigned int buflen,tds_login &login,const char *szName,const char *szUser,const char *szPswd);

//��std::stringת��std::wstring
std::wstring MultibyteToWide(const std::string& multibyte);
#endif