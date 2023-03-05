#pragma once

#include "orthia_utils.h"
#include "picojson.h"

namespace orthia
{
template <class A>
static A Query_Silent(const picojson::object * object,
                      const std::string & field,
                      A defaultValue)
{
    picojson::object::const_iterator it = object->find(field);
    if(it == object->end() || !(it->second.is<A>()))
    {
        return defaultValue;
    }
    return it->second.get<A>();
}
class JSONObject
{
    picojson::object m_obj;
public:
    JSONObject()
    {
    }
    JSONObject(const picojson::object & obj)
        :
            m_obj(obj)
    {
    }
    JSONObject(const picojson::value & obj)
    {
        if (!obj.is<picojson::object>())
            throw std::runtime_error("Incorrect json config");

        m_obj = obj.get<picojson::object>();
    }
    const picojson::object & GetObject() const { return m_obj; }
    picojson::object & GetObject() { return m_obj; }

    long long getInt64(const std::string & key) const
    {
        std::string str = get(key);
        if (str.empty())
        {
            return 0;
        }
        long long res = 0;
        StringToObject(str, &res);
        return res;
    }
    bool getSilent(const std::string & key, std::string * pResult) const
    {
        pResult->clear();
        picojson::object::const_iterator it = m_obj.find(key);
        if (it == m_obj.end())
        {
            return false;
        }
        *pResult = get(key);
        return true;
    }
    std::string get(const std::string & key) const
    {
        picojson::object::const_iterator it = m_obj.find(key);
        if (it == m_obj.end())
        {
            return std::string();
        }
        if (it->second.is<std::string>())
        {
            return it->second.get<std::string>();
        }

        if (it->second.is<double>())
        {
            return orthia::ObjectToString_Ansi(it->second.get<double>());
        }
        if (it->second.is<picojson::array>())
        {
            return it->second.serialize();
        }
        return std::string();
    }
    std::string getValue(const std::string & key) const
    {
        picojson::object::const_iterator it = m_obj.find(key);
        if (it == m_obj.end())
        {
            return std::string();
        }
        return it->second.serialize();
    }
    void set(const std::string & key, long long value)
    {
        m_obj[key] = picojson::value((double)value);
    }
    void set(const std::string & key, const std::string & value)
    {
        m_obj[key] = picojson::value(value);
    }
    void stringify(std::ostream & streamObject) const
    {
        streamObject << picojson::value(m_obj).serialize();
    }
    std::string serialize() const
    {
        return picojson::value(m_obj).serialize();
    }
    void deserialize(const std::string & data)
    {
        if (data.empty())
        {
            m_obj = picojson::object();
            return;
        }
        std::stringstream stream;
        stream<<data;
        picojson::value jsonConfig;
        stream>>jsonConfig;
        if (!jsonConfig.is<picojson::object>())
            throw std::runtime_error("Incorrect json config");

        m_obj = jsonConfig.get<picojson::object>();
    }
    typedef const picojson::array * ArrayPtr;
    const picojson::array * getArray(const std::string & key) const 
    {
        picojson::object::const_iterator it = m_obj.find(key);
        if (it == m_obj.end())
        {
            return 0;
        }
        if (!it->second.is<picojson::array>())
        {
            return 0;
        }
        return &it->second.get<picojson::array>();
    }
    void getNames(std::vector<std::string> & names) const
    {
        names.clear();
        names.reserve(m_obj.size());
        for(picojson::object::const_iterator it = m_obj.begin(), it_end = m_obj.end();
            it != it_end;
            ++it)
        {
            names.push_back(it->first);
        }
    }

};
}