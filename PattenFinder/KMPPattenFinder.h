#pragma once
class KMPPattenFinder {
private:
	struct Pattern {
		BYTE * m_patten;
		int m_pattenlength;
		int m_masklength;
		LPSTR m_szmask;
		bool m_Bfind=false;
		std::vector<int> m_result;
		Pattern(BYTE * patten, int pattenlength, int masklenfth, LPSTR szmask) {
			m_patten=patten;
			m_pattenlength=pattenlength;
			m_masklength=masklenfth;
			m_szmask=szmask;
		}
	};
	BYTE * m_pData;
	int m_DataSize;
	std::vector<Pattern> m_patterns;
	//pData:数据指针//result 是搜索结果//end 是搜索的结束位置//begin 是搜索的起始位置//patten 是模式串//next 是模式串的next数组//mask 是模式串的掩码
	bool FindPatten(PBYTE pData, int begin, int end, PBYTE Patten, int PattenLen, int * next, LPSTR szmask, std::vector<int> & result) {
		int i=begin;
		int j=-1;
		while (i < end && j < PattenLen) {
			if (j == -1 || szmask[j] != 'x' || pData[i] == Patten[j]) {
				++i; ++j;

			} else {
				i=i + j - next[j];//i+j-next[j] 代表着i的下一个位置 这样的好处是如果模式串中出现了重复的字符，那么就可以跳过重复的字符
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
		auto GetNext=[&] (PBYTE patten, int pattenlength, int * next, LPSTR szMask)->void {
			int i=0;
			int j=-1;
			next[0]=-1;
			while (i < pattenlength - 1) {
				if (j == -1 || szMask[i] == '?' || patten[i] == patten[j]) {
					++i; ++j;
					if (patten[i] != patten[j] && szMask[i] != '?') {
						next[i]=j;
					} else {
						next[i]=next[j];
					}
				} else {
					j=next[j];
				}
			}
		};
		auto PattenLen=strlen(szmask);
		auto next=std::make_unique<int[]>(PattenLen);
		memset(next.get(), 0, sizeof(int) * PattenLen);
		GetNext(patten, PattenLen, next.get(), szmask);
		int parts=6;
		int segment=datasize / parts;
#pragma omp parallel for num_threads(parts)
		for (int i=0; i < parts; i++) {
			FindPatten(pData, i * segment, segment * (i + 1), patten, PattenLen, next.get(), szmask, result);
		}
#pragma omp parallel for num_threads(parts-1)
		for (int i=0; i < parts - 1; i++) {
			FindPatten(pData, segment * (i + 1) - PattenLen, segment * (i + 1) + PattenLen, patten, PattenLen, next.get(), szmask, result);
		}
		return result.size() > 0;
	}
public:
	KMPPattenFinder(BYTE * pData, int DataSize) {
		m_pData=pData;
		m_DataSize=DataSize;
	}
	void AddPattern(BYTE * patten, int pattenlength, int masklenfth, LPSTR szmask) {
		m_patterns.push_back(Pattern(patten, pattenlength, masklenfth, szmask));
	}
	void Find() {
		auto Pfind=[&] (BYTE * data, int datasize, Pattern * p)->void {
			FindPattenEx(data, p->m_patten, datasize, p->m_szmask, p->m_result);
			if (p->m_result.size() > 0)
				p->m_Bfind=true;
		};
		std::vector<std::future<void>> futures;
		for (auto & p : m_patterns) {
			futures.push_back(std::move(std::async(std::launch::async | std::launch::deferred, Pfind, m_pData, m_DataSize, &p)));
		}
	}
	const std::vector<int>& GetResult(int index) {
		return m_patterns[index].m_result;
	}
	const std::vector<int>& GetResult(BYTE * patten) {
		for (auto & p : m_patterns) {
			if (memcmp(p.m_patten, patten, p.m_pattenlength) == 0) {
				return p.m_result;
			}
		}
	}
};