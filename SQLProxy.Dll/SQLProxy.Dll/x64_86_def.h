
#pragma once

//Ϊ�˷������64λ/32λdll������ǽ���������������ָ���ֿ�����Ϊ���ݵı����ú궨�����
//32λ����ʱ����Ϊint�� 64λ����ʱ����Ϊ__int64 
//�ص�ע��ص��������û��Զ���������Ͷ��壬dll���api�Ĳ����򷵻��п���ָ����ֵ��ָ���Ҫ�ú��滻ԭ���Ķ���
//vc����64λ����32λ���뻷��int��long����4�ֽڵģ�long longΪ64λ�ġ�

//#ifndef INTPTR
#ifdef WIN32
#define INTPTR  int
#else
#define INTPTR  __int64
#endif
//#endif
