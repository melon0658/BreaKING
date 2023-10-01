#pragma once

// class Tを最大MAXSIZE個確保可能なPoolAllocatorを実装してください
template<class T, size_t MAXSIZE> class PoolAllocator
{
public:
	// コンストラクタ
	PoolAllocator() {
		// TODO: 必要に応じて実装してください
		for (size_t i = 0; i < MAXSIZE; ++i) {
			m_pool[i] = &m_memory[i];
		}
		m_freeIndex = MAXSIZE - 1;
	}

	// デストラクタ
	~PoolAllocator() {
		// TODO: 必要に応じて実装してください
	}

	// 確保できない場合はnullptrを返す事。
	T* Alloc() {
		// TODO: 実装してください
		if (m_freeIndex >= 0) {
			T* allocated = m_pool[m_freeIndex];
			--m_freeIndex;
			return allocated;
		}
		return nullptr;
	}

	// Free(nullptr)で誤動作しないようにする事。
	void Free(T* addr) {
		// TODO: 実装してください
		if (addr) {
			if (m_freeIndex < static_cast<int>(MAXSIZE) - 1) {
				++m_freeIndex;
				m_pool[m_freeIndex] = addr;
			}
		}
	}

private:
	// TODO: 実装してください
	// この状態だと余計にメモリを確保している。共用体を用いて使いまわすことで削減できる。
	T* m_pool[MAXSIZE];
	T m_memory[MAXSIZE];
	int m_freeIndex;
};
