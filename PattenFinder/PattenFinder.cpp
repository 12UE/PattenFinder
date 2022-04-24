// PattenFinder.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include<fstream>
#include<string>
#include<Windows.h>
#include<vector>
#include<algorithm>
#include<functional>
#include<chrono>
#include"BoyerMoore.h"
using namespace std;
bool bDataCompare(BYTE * pData, BYTE * bMask, char * szMask) {
	for (; *szMask; ++szMask, ++pData, ++bMask)
		if (*szMask == 'x' && *pData != *bMask)
			return FALSE;

	return (*szMask == NULL);
}
DWORD64 ForceFindPatternEx(BYTE *pData,int datasize,BYTE * bMask, char * szMask) {//暴力匹配
	int result=0x0;
	for (int i=0; i < datasize; i++) {//遍历进程内存
		if (bDataCompare((BYTE *)(pData + i), bMask, szMask))
			result=i;
	}
	return result;
}

void GetNext(PBYTE patten, int pattenlength, int * next, LPSTR szMask) {
	//位序号从0开始
	int i=0;
	int j=-1;
	next[0]=-1;
	while (i < pattenlength - 1) {
		if (j == -1 || szMask[i] == '?' || patten[i] == patten[j]) {
			++i;
			++j;
			if (patten[i] != patten[j] && szMask[i] != '?') {
				next[i]=j;
			} else {
				next[i]=next[j];
			}
		} else {
			j=next[j];
		}
	}
	//print next
	for (int i=0; i < pattenlength; i++) {
		cout <<dec<< (int)next[i] << " ";
	}
	cout << endl;
}	
//pData:数据指针//result 是搜索结果//end 是搜索的结束位置//begin 是搜索的起始位置//patten 是模式串//next 是模式串的next数组//mask 是模式串的掩码
bool FindPatten(PBYTE pData, int begin, int end, PBYTE Patten, int PattenLen, int * next, LPSTR szmask, std::vector<int> & result) {
	int i=begin;
	int j=-1;
	while (i < end && j < PattenLen) {
		if (j == -1 || szmask[j] != 'x' || pData[i] == Patten[j]) {
			++i;
			++j;
		}else {
			i=i+j-next[j];//i+j-next[j] 代表着i的下一个位置 这样的好处是如果模式串中出现了重复的字符，那么就可以跳过重复的字符
			j=next[j];
		}
	}
	if (j >= PattenLen) {
		result.push_back(i - PattenLen);
		return FindPatten(pData, i - PattenLen + 1, end, Patten, PattenLen, next, szmask, result);//递归
	} else {
		return false;
	}
}

bool FindPattenEx(PBYTE pData, PBYTE patten, int datasize, LPSTR szmask, std::vector<int> & result) {
	int PattenLen=strlen(szmask);
	int * next=new int[PattenLen];
	memset(next, 0, sizeof(int) * PattenLen);
	GetNext(patten, PattenLen, next, szmask);
	int begin=0;
	int end=datasize;
	//把数据分为4个部分
	int parts=4;
	int segment=datasize / parts;
	//利用openmp 并行搜索
#pragma omp parallel for num_threads(parts)
	for (int i=0; i < parts; i++) {
		{
			int nBegin=begin + i * segment;
			int nEnd=segment*(i + 1);
			FindPatten(pData,nBegin ,nEnd, patten, PattenLen, next, szmask, result);
		}
	}
	//搜索两个分块交界处的数据 交界处的大小为pattern的大小
#pragma omp parallel for num_threads(parts-1)
	for(int i=0;i<parts-1;i++){
		int nSegBegin=segment*(i+1)-PattenLen;
		int nSegEnd=begin+segment*(i+1)+PattenLen;
		cout<<hex<<"区间 "<<i<<" 的起始位置"<< nSegBegin <<"结束位置"<< nSegEnd <<endl;
		FindPatten(pData, nSegBegin, nSegEnd,patten,PattenLen,next,szmask,result);
	}
	delete[] next;
	return result.size() > 0;
}
//\x66\x6a\x42\x1\x0\x0\x0\x98
int main() {
	fstream file("memorydump.bin", ios::in | ios::out | ios::binary);
	if (!file) {
		cout << "打开文件失败" << endl;
		return -1;
	}
	int size=0;
	file.seekg(0, ios::end);
	size=file.tellg();
	file.seekg(0, ios::beg);
	PBYTE pData=new BYTE[size];
	file.read((LPSTR)pData, size);
	file.close();
	cout << "文件大小：" << (float)size / 1024 / 1024 << "MB" << endl;
	PBYTE patten=(PBYTE)"\x66\x6A\x00\x01\x00\x00\x00\x98";
	//print patten
	cout << "模式串为：";
	for (int i=0; i < 8; i++) {
		cout <<"\\x" << hex << (int)patten[i];
	}
	cout << endl;
	auto pattenSize=8;
	LPSTR szMask=(LPSTR)"xx?xxxxx";
	//print mask
	cout << "掩码为：";
	for (int i=0; i < 8; i++) {
		cout << szMask[i] << " ";
	}
	cout << endl;
	auto maskSize=8;
	SetThreadPriorityBoost(GetCurrentThread(), TRUE);
	cout << "特征码正确地址 =0x26a6699" << endl;
	
	auto begin=std::chrono::high_resolution_clock::now();
	std::vector<int> result;
	bool bRet=FindPattenEx(pData, patten, size, szMask, result);
	auto usetime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
	cout << "KMP搜索耗时：" << dec << usetime << "ms" << endl;
	if (bRet) {
		cout << "KMP搜寻结果地址   =0x" << hex << result[0] << endl;
	} else {
		cout << "KMP搜寻结果地址   =0x0"<< endl;
	}
		


	begin=std::chrono::high_resolution_clock::now();
	auto result2=ForceFindPatternEx(pData, size, patten, szMask);
	auto usetime2=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
	cout << "暴力搜索耗时：" << dec << usetime2 << "ms" << endl;
	cout << "暴力搜寻结果地址   =0x" << hex << result2 << endl;

	begin=std::chrono::high_resolution_clock::now();
	BoyerMoore((PBYTE)"\x66\x6a\x42\x1\x0\x0\x0\x98",pattenSize,pData,size);
	auto usetime3=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
	cout << "Boyer-Moore搜索耗时：" << dec << usetime3 << "ms" << endl;
	
	cout << "真实数据：";
	for (auto i=result2; i < result2 + pattenSize; i++) {
		cout << "\\x" << hex << (int)pData[i];
	}
	delete[] pData;
	return 0;
}
