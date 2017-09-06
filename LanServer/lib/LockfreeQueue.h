#ifndef __LOCKFREEQUEUE__H__
#define __LOCKFREEQUEUE__H__

template <class DATA>
class CLockfreeQueue
{
public:
	struct st_NODE
	{
		DATA	Data;
		st_NODE *pNext;
	};

	struct st_END_NODE
	{
		st_NODE *pEndNode;
		__int64 iUniqueNum;
	};
public:
	/////////////////////////////////////////////////////////////////////////
	// ������
	//
	// Parameters: ����.
	// Return: ����.
	/////////////////////////////////////////////////////////////////////////
	CLockfreeQueue()
	{
		_pMemoryPool = new CMemoryPool<st_NODE>(0);

		_lUseSize = 0;

		_pHead = (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pHead->iUniqueNum = 0;

		_pTail = (st_END_NODE *)_aligned_malloc(sizeof(st_END_NODE), 16);
		_pTail->iUniqueNum = 0;

		/////////////////////////////////////////////////////////////////////
		// ���� ��� ���̱�
		/////////////////////////////////////////////////////////////////////
		st_NODE *stDummyNode = _pMemoryPool->Alloc();
		stDummyNode->pNext = NULL;

		_pHead->pEndNode = stDummyNode;
		_pTail->pEndNode = stDummyNode;

		_iUniqueNumHead = 0;
		_iUniqueNumTail = 0;
	}

	/////////////////////////////////////////////////////////////////////////
	// �Ҹ���
	//
	// Parameters: ����.
	// Return: ����.
	/////////////////////////////////////////////////////////////////////////
	virtual ~CLockfreeQueue()
	{
		st_NODE *pTemp;

		while (_pHead != NULL)
		{
			pTemp = _pHead->pEndNode;
			_pHead->pEndNode = _pHead->pEndNode->pNext;
			_pMemoryPool->Free(pTemp);
		}

		delete _pMemoryPool;
		delete _pHead;
		delete _pTail;
	}

	/////////////////////////////////////////////////////////////////////////
	// ���� ������� �뷮 ���.
	//
	// Parameters: ����.
	// Return: (int)������� �뷮.
	/////////////////////////////////////////////////////////////////////////
	long	GetUseSize(void);

	/////////////////////////////////////////////////////////////////////////
	// �����Ͱ� ����°� ?
	//
	// Parameters: ����.
	// Return: (bool)true, false
	/////////////////////////////////////////////////////////////////////////
	bool	isEmpty(void);


	/////////////////////////////////////////////////////////////////////////
	// CPacket ������ ����Ÿ ����.
	//
	// Parameters: (DATA)����Ÿ.
	// Return: (bool) true, false
	/////////////////////////////////////////////////////////////////////////
	bool	Put(DATA Data)
	{
		st_END_NODE pTail;
		st_NODE *pTailNext, *pNode = _pMemoryPool->Alloc();

		pNode->Data = Data;
		pNode->pNext = NULL;
		__int64 iUniqueNumTail = InterlockedIncrement64(&_iUniqueNumTail);

		while (1)
		{
			/////////////////////////////////////////////////////////////////
			// Tail �������� ����
			/////////////////////////////////////////////////////////////////
			pTail.iUniqueNum = _pTail->iUniqueNum;
			pTail.pEndNode = _pTail->pEndNode;
			
			/////////////////////////////////////////////////////////////////
			// Tail->Next ����
			/////////////////////////////////////////////////////////////////
			pTailNext = pTail.pEndNode->pNext;

			/////////////////////////////////////////////////////////////////
			// �ϴ� ���غ��� tail->next�� null�̸�
			// Unique���� ���ؼ� �ٲ�
			/////////////////////////////////////////////////////////////////
			if (pTailNext == NULL)
			{
				if (InterlockedCompareExchangePointer((PVOID *)&pTail.pEndNode->pNext, pNode, pTailNext) == nullptr)
				{
					InterlockedCompareExchange128((LONG64 *)_pTail, iUniqueNumTail, (LONG64)pNode, (LONG64 *)&pTail);
					break;
				}
			}

			/////////////////////////////////////////////////////////////////
			// �ϴ� ���غ��� tail->next�� null�ƴϸ�
			// �׳� �ϴ� Tail ��
			/////////////////////////////////////////////////////////////////
			else
			{
				InterlockedCompareExchange128((LONG64 *)_pTail, iUniqueNumTail, (LONG64)pTailNext, (LONG64 *)&pTail);
			}
		}

		InterlockedIncrement64((LONG64 *)&_lUseSize);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////
	// ����Ÿ ���� ������.
	//
	// Parameters: (DATA *) ���� ������ �־��� ������
	// Return: (bool) true, false
	/////////////////////////////////////////////////////////////////////////
	bool	Get(DATA *pOutData)
	{
		st_END_NODE pHead, pTail;
		st_NODE  *pHeadNext, *pNode;
		__int64 iUniqueNumHead = InterlockedIncrement64(&_iUniqueNumHead);

		if (_lUseSize == 0)
			return false;

		InterlockedDecrement64((LONG64 *)&_lUseSize);

		while (1)
		{
			/////////////////////////////////////////////////////////////////
			// Head �������� ����
			/////////////////////////////////////////////////////////////////
			pHead.iUniqueNum = _pHead->iUniqueNum;
			pHead.pEndNode = _pHead->pEndNode;
			
			/////////////////////////////////////////////////////////////////
			// Tail �������� ����
			/////////////////////////////////////////////////////////////////
			pTail.iUniqueNum = _pTail->iUniqueNum;
			pTail.pEndNode = _pTail->pEndNode;
			
			pHeadNext = pHead.pEndNode->pNext;

			/////////////////////////////////////////////////////////////////
			// Queue�� ����� ��
			/////////////////////////////////////////////////////////////////
			if (pHead.pEndNode == pTail.pEndNode)
			{
				if (pHeadNext == NULL)
					return false;
			}

			/////////////////////////////////////////////////////////////////
			// �ϴ� ���غ��� tail->next�� null�ƴϸ�
			// �׳� �ϴ� Tail ��
			/////////////////////////////////////////////////////////////////
			if (pTail.pEndNode->pNext != NULL)
			{
				__int64 iUniqueNumTail = InterlockedIncrement64(&_iUniqueNumTail);
				InterlockedCompareExchange128((LONG64 *)_pTail, iUniqueNumTail, (LONG64)pTail.pEndNode->pNext, (LONG64 *)&pTail);
			}

			else
			{
				if (pHeadNext != NULL)
				{
					*pOutData = pHeadNext->Data;
					if (InterlockedCompareExchange128((LONG64 *)_pHead, iUniqueNumHead, (LONG64)pHead.pEndNode->pNext, (LONG64 *)&pHead))
					{
						_pMemoryPool->Free(pHead.pEndNode);
						break;
					}
				}
			}
		}

		return true;
	}

	int GetBlockCount()	{ return _pMemoryPool->GetBlockCount(); }

private:
	CMemoryPool<st_NODE>	*_pMemoryPool;

	long					_lUseSize;

	st_END_NODE			*_pHead;
	st_END_NODE			*_pTail;

	__int64				_iUniqueNumHead;
	__int64				_iUniqueNumTail;
};

#endif