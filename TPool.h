#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <config.h>
#include <SpinLock.h>

 
template <class T>
class TPool
{
public:
    
 TPool( int reserve = 1000, int expand = 100, int max = 10000)
{
    m_reserve = reserve;
    m_expand  = expand;
    m_max     = max;

    init();
};

 
virtual  ~TPool()
{
    m_lock_idle.Lock();
    
	typename list<T *>::iterator it_idle = _idle.begin();  //linux need typename
    while (it_idle != _idle.end())
    {
        if (*it_idle != NULL)
        {
            delete_z(*it_idle);
        }
        it_idle ++;
    }

    _idle.clear();

    m_lock_idle.UnLock();

    m_lock_occupy.Lock();

	typename list<T*>::iterator it_occupy = _occupy.begin();
    while (it_occupy != _occupy.end())
    {
        if (*it_occupy != NULL)
        {
            delete_z(*it_occupy);
        }
        it_occupy ++;
    }

    _occupy.clear();

    m_lock_occupy.UnLock();
};
T * GetIdle()  //then push_back , finally release
{
	// 出idle
	m_lock_idle.Lock();
	if (_idle.empty())
	{
		if (expand() != 0)
		{
			m_lock_idle.UnLock();
			return NULL;
		}
	}

	T * pt = _idle.front();
	_idle.pop_front();
	m_lock_idle.UnLock();
	//object->reset();
	return pt;
}
 
int push_back( T * pt )  //push_back just getT  is ok , no need other 
{
    // 进occupy
    m_lock_occupy.Lock();
    _occupy.push_back(pt);
    m_lock_occupy.UnLock();

    return 0;
};
 
// 请求对象小，对应的消息优先级别高 
bool CompareT(T  t1, const T  t2)
{
	//return t1->_header.Priority >= pRight->_header.Priority;
	return true;
}


int addT(T  * t)
{
	m_lock_occupy.Lock();
 
	typename list<T>::iterator it = lower_bound(_occupy.begin(), _occupy.end(), t, CompareT);
	if (it == _occupy.end())
		_occupy.push_back(t);
	else
		_occupy.insert(it, t);
		 

	_occupy.push_back(t);
	m_lock_occupy.UnLock();

	return 0;
};

bool getSize()
{
	 
	m_lock_occupy.Lock(); 		
	int size = _occupy.size();

	m_lock_occupy.UnLock();
	return size;
}
 
T *  getT()
{
	m_lock_occupy.Lock();
	if (_occupy.empty())
	{
		m_lock_occupy.UnLock();
		return NULL;
	}

	T * pt = _occupy.front();
	_occupy.pop_front();

	m_lock_occupy.UnLock();

	return pt;
};

 
void  release( T * pt )
{
     

    m_lock_idle.Lock();
    _idle.push_back(pt);
    m_lock_idle.UnLock();
};
 
void  init()
{
    int i = 0;
    for (; i < m_reserve; i++)
    {
        _idle.push_back(new T);
    }

    m_alloc = m_reserve;
};

 
int expand()
{
    if (m_alloc >= m_max)
    {
        return -1;
    }

    for (int i = 0; i < m_expand; i++)
    {
        _idle.push_back(new T);    
    }

    m_alloc += m_expand;

    return 0;
};




private:
    int m_reserve;
    int m_expand;
    int m_max;
    int m_alloc;

    list<T *> _idle;
    list<T *> _occupy;

    SpinLock m_lock_idle;
    SpinLock m_lock_occupy;
};


#endif