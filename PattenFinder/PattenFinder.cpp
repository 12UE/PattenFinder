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
#include"KMPPattenFinder.h"
using namespace std;
#define BENCHMARKTIMES 10
DWORD64 ForceFindPatternEx(BYTE *pData,int datasize,BYTE * bMask, char * szMask) {//暴力匹配
	static const auto bDataCompare=[&] (BYTE * pData, BYTE * bMask, char * szMask)->bool {
		for (; *szMask; ++szMask, ++pData, ++bMask)
			if (*szMask == 'x' && *pData != *bMask)
				return false;
		return (*szMask == NULL);
	};
	int result=0;
#pragma omp parallel for num_threads(4)
	for (int i=0; i < datasize; ++i) {//遍历进程内存
		if (bDataCompare((BYTE *)(pData + i), bMask, szMask))
			result=i;
	}
	return result;
}




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
	cout << "搜索" <<dec<< BENCHMARKTIMES << "次" << endl;
	KMPPattenFinder pf(pData, size);
	auto begin=std::chrono::high_resolution_clock::now();
	for (int i=0; i < BENCHMARKTIMES; i++) {
		pf.AddPattern(patten, pattenSize, maskSize, szMask);
	}
	pf.Find();
	auto usetime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count();
	cout << "KMP搜索耗时：" << dec << usetime << "ms" << endl;
	cout << "搜索结果：0x" <<hex<<pf.GetResult(0)[0] << endl;

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
