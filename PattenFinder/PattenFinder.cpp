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
#include<thread>
#include<future>
#include<mutex>
#include"BoyerMoore.h"
using namespace std;
#define BENCHMARKTIMES 100
DWORD64 ForceFindPatternEx(BYTE *pData,int datasize,BYTE * bMask, char * szMask) {//暴力匹配
	static const auto bDataCompare=[&] (BYTE * pData, BYTE * bMask, char * szMask)->bool {
		for (; *szMask; ++szMask, ++pData, ++bMask)
			if (*szMask == 'x' && *pData != *bMask)
				return false;
		return (*szMask == NULL);
	};
	for (unsigned int i=0; i < datasize; ++i) {//遍历进程内存
		if (bDataCompare((BYTE *)(pData + i), bMask, szMask))
			return i;
	}
	return 0;
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
	//for (int i=0; i < pattenlength; i++) {
	//	cout <<dec<< (int)next[i] << " ";
	//}
	//cout << endl;
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
	
	auto PattenLen=strlen(szmask);
	auto next=new int[PattenLen];
	memset(next, 0, sizeof(int) * PattenLen);
	GetNext(patten, PattenLen, next, szmask);
	int parts=6;
	int segment=datasize / parts;
#pragma omp parallel for num_threads(parts)
	for (int i=0; i < parts; i++) {
		FindPatten(pData, i * segment, segment * (i + 1), patten, PattenLen, next, szmask, result);
	}
#pragma omp parallel for num_threads(parts-1)
	for (int i=0; i < parts - 1; i++) {
		FindPatten(pData, segment * (i + 1) - PattenLen, segment * (i + 1) + PattenLen, patten,PattenLen, next, szmask, result);
	}
	return result.size() > 0;
}
//\x66\x6a\x42\x1\x0\x0\x0\x98


struct Pattern {
	BYTE * m_patten;
	int m_pattenlength;
	int m_masklength;
	LPSTR m_szmask;
	bool m_Bfind=false;
	std::vector<int> m_result;
	Pattern(BYTE* patten, int pattenlength,int masklenfth,LPSTR szmask) {
		m_patten=patten;
		m_pattenlength=pattenlength;
		m_masklength=masklenfth;
		m_szmask=szmask;
	}
};

class PattenFinder{
public:
	BYTE* m_pData;
	int m_DataSize;
	std::vector<Pattern> m_patterns;
	PattenFinder(BYTE* pData, int DataSize) {
		m_pData=pData;
		m_DataSize=DataSize;
	}
	void AddPattern(BYTE* patten, int pattenlength,int masklenfth,LPSTR szmask) {
		m_patterns.push_back(Pattern(patten, pattenlength,masklenfth,szmask));
	}
	void Find() {
		auto Pfind=[&](BYTE* data, int datasize, Pattern *p)->void {
			//cout <<"ThreadID" << std::this_thread::get_id() << endl;
			FindPattenEx(data, p->m_patten, datasize, p->m_szmask, p->m_result);
			if(p->m_result.size()>0)
				p->m_Bfind=true;
		};
		std::vector<std::future<void>> futures;
		for (auto & p : m_patterns) {
			auto future=std::async(std::launch::async|std::launch::deferred,Pfind, m_pData, m_DataSize, &p);
			futures.push_back(std::move(future));
		}
	}
};

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
	PBYTE patten=(PBYTE)"\x66\x6A\x42\x01\x00\x00\x00\x98";
	//print patten
	cout << "模式串为：";
	for (int i=0; i < strlen((char*)patten); i++) {
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
	cout << "搜索" <<dec<< BENCHMARKTIMES << "次" << endl;
	PattenFinder pf(pData, size);
	auto begin=std::chrono::high_resolution_clock::now();
	for (int i=0; i < BENCHMARKTIMES; i++) {
		pf.AddPattern(patten, pattenSize, maskSize, szMask);
	}
	std::vector<std::future<bool>> rets;
	pf.Find();
	auto usetime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
	cout << "KMP搜索耗时：" << dec << usetime << "ms" << endl;
	//cout << "搜索结果：" << endl;
	//for (auto & p : pf.m_patterns) {
	//	if (p.m_Bfind) {
	//		cout << "模式串：";
	//		for (int i = 0; i < p.m_pattenlength; i++) {
	//			cout << "\\x" << hex << (int)p.m_patten[i];
	//		}
	//		cout << endl;
	//		cout << "掩码：";
	//		for (int i = 0; i < p.m_masklength; i++) {
	//			cout << p.m_szmask[i];
	//		}
	//		cout << endl;
	//		cout << "结果：";
	//		for (auto & r : p.m_result) {
	//			cout << "0x" << hex << r << " ";
	//		}
	//		cout << endl;
	//	}
	//}





	begin=std::chrono::high_resolution_clock::now();
	int result2;
	for (int i=0; i < BENCHMARKTIMES; i++) {
		result2=ForceFindPatternEx(pData, size, patten, szMask);
	}
	auto usetime2=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
	cout << "暴力搜索耗时：" << dec << usetime2 << "ms" << endl;
	cout << "暴力搜寻结果地址   =0x" << hex << result2 << endl;
	std::vector<int> result3;
	begin=std::chrono::high_resolution_clock::now();
	for (int i=0; i < BENCHMARKTIMES; i++) {
		BoyerMoore((PBYTE)"\x66\x6a\x42\x1\x0\x0\x0\x98", pattenSize, pData, size, result3);
	}
	auto usetime3=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
	cout << "Boyer-Moore搜索耗时：" << dec << usetime3 << "ms" << endl;
	cout << "Boyer-Moore搜寻结果地址   =0x" << hex << result3[0] << endl;
	
	cout << "真实数据：";
	for (auto i=result2; i < result2 + pattenSize; i++) {
		cout << "\\x" << hex << (int)pData[i];
	}
	delete[] pData;
	system("PAUSE");
	return 0;
}
