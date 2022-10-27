#ifndef ORTHIA_POINTERS_H
#define ORTHIA_POINTERS_H

#include "windows.h"
#include "algorithm"

namespace orthia
{

struct IRefCountedBase
{
    virtual ~IRefCountedBase(){}
    virtual void OrthiaAddRef() = 0;
    virtual void OrthiaRelease() = 0;
};

class RefCountedBase:public IRefCountedBase
{
    long m_count;
public:
    RefCountedBase()
        :
            m_count(1)
    {
    }
    virtual ~RefCountedBase()
    {
    }
    virtual void OrthiaAddRef()
    {
        InterlockedIncrement(&m_count);
    }
    virtual void OrthiaRelease()
    {
        if (!InterlockedDecrement(&m_count))
        {
            delete this;
        }
    }
};

template<class Type>
class RefCountedBase_t:public Type
{
    long m_count;
public:
    RefCountedBase_t()
        :
            m_count(1)
    {
    }
    virtual ~RefCountedBase_t()
    {
    }
    virtual void OrthiaAddRef()
    {
        InterlockedIncrement(&m_count);
    }
    virtual void OrthiaRelease()
    {
        if (!InterlockedDecrement(&m_count))
        {
            delete this;
        }
    }
};

inline void Intrusive_AddRef(IRefCountedBase * pObject)
{
    pObject->OrthiaAddRef();
}
inline void Intrusive_Release(IRefCountedBase * pObject)
{
    pObject->OrthiaRelease();
}

#define ORTHIA_PREDECLARE(X)   X;  extern void Intrusive_AddRef(X * p);   extern void Intrusive_Release(X * p);

#define ORTHIA_DECLARE(X)   inline void Intrusive_AddRef(X * p) { p->OrthiaAddRef(); }  inline void Intrusive_Release(X * p) { p->OrthiaRelease(); }
template<class ObjectType>
class intrusive_ptr
{
    ObjectType * m_pObject;
    void AddRef()
    {
        if (m_pObject)
            Intrusive_AddRef(m_pObject);
    }
    void Release()
    {
        if (m_pObject)
           Intrusive_Release(m_pObject);
    }
public:
    intrusive_ptr()
        :
            m_pObject(0)
    {
    }
    intrusive_ptr(ObjectType * pObject, bool bAddRef = false)
        :
            m_pObject(pObject)
    {
        if (bAddRef)
        {
            AddRef();
        }
    }
    ~intrusive_ptr()
    {
        Release();
    }

    intrusive_ptr(const intrusive_ptr<ObjectType> & p)
        :
            m_pObject(p.m_pObject)
    {
        AddRef();
    }
    intrusive_ptr<ObjectType> & operator = (const intrusive_ptr<ObjectType> & p)
    {
        intrusive_ptr<ObjectType> temp(p);
        swap(temp);
        return *this;
    }

    template<class OtherType>
    intrusive_ptr(intrusive_ptr<OtherType> & p)
        :
            m_pObject(p.get())
    {
        AddRef();
    }
    template<class OtherType>
    intrusive_ptr(const intrusive_ptr<OtherType> & p)
        :
            m_pObject(p.get())
    {
        AddRef();
    }

    template<class OtherType>
    intrusive_ptr<ObjectType> & operator = (const intrusive_ptr<OtherType> & p)
    {
        intrusive_ptr<ObjectType> temp(p);
        swap(temp);
        return *this;
    }
    void swap(intrusive_ptr<ObjectType> & p)
    {
        std::swap(m_pObject, p.m_pObject);
    }
    ObjectType * operator -> ()
    {
        return m_pObject;
    }
    const ObjectType * operator -> () const 
    {
        return m_pObject;
    }
    ObjectType & operator * ()
    {
        return *m_pObject;
    }
    const ObjectType & operator *() const 
    {
        return *m_pObject;
    }

    bool operator ! () const
    {
        return !m_pObject;
    }
    ObjectType * get()
    {
        return m_pObject;
    }
    const ObjectType * get() const 
    {
        return m_pObject;
    }

};

template<class Type>
class Ptr:public intrusive_ptr<Type>
{
public:
    Ptr()
    {
    }
    Ptr(int)
        :
            intrusive_ptr(0, false)
    {
    }
    template<class ObjectType>
    Ptr(ObjectType * pObject, bool bAddRef = false)
        :
            intrusive_ptr(pObject, false)
    {
    }
    template<class OtherType>
    Ptr(intrusive_ptr<OtherType> & p)
        :
            intrusive_ptr(p)
    {
    }
    template<class OtherType>
    Ptr(const intrusive_ptr<OtherType> & p)
        :
            intrusive_ptr(p)
    {
    }
};


}
#endif