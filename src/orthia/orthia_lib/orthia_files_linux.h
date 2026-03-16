#ifdef DIANA_HAS_LINUX

#include "orthia_core.h"
#include "map"
namespace orthia
{

struct EnvironmentPaths
{
    std::string m_ApplicationDataPath;
    std::string m_TempPath;
};
void InitEnvironmentPaths(const EnvironmentPaths & paths);

//--
inline std::string GetFileNameOfFullPathName_Win(const std::string & str)
{
    for(long i = (long)str.size()-1; i>0; --i)
    {
        if (str[i] == '\\' || str[i] == '/' )
            return std::string(str.begin()+i+1, str.end());
    }
    return str;
}

inline bool IsFileNameSeparator(char ch)
{
    return ch == '/';
}

inline void Normalize(std::string & fullPath)
{
    size_t s = fullPath.size();
    if (!s)
        return;

    int delta = 0;
    for(size_t i = 1; i<s; ++i)
    {
        if ((fullPath[i]=='/') && (fullPath[i-1-delta]=='/'))
        {
            ++delta;
        }
        fullPath[i - delta] = fullPath[i];
    }
    fullPath.erase(fullPath.size() - delta);
}
inline void EraseLastSlash(std::string & str)
{
    for(;;)
    {
        if (str.empty())
            return;

        if (!orthia::IsFileNameSeparator(*str.rbegin()))
            break;
        
        str.resize(str.size()-1);
    }
}
inline void EnsureLastSlash(std::string & str,
                            char slash = '/')
{
    if (str.empty())
    {
        str += slash;
        return;
    }
    char & lastChar = str[str.size()-1];
    if (lastChar == '/')
    {
        lastChar = slash;
        return;
    }
    str += slash;
}
inline std::string EnsureLastSlash2(const std::string & str,
                                        char slash = '/'
                                        )
{
    std::string str2(str);
    EnsureLastSlash(str2, slash);
    return str2;
}
inline void EnsureLastSlash_Ansi(std::string & str,
                                    char slash = '/')
{
    if (str.empty())
    {
        str += slash;
        return;
    }
    char & lastChar = str[str.size()-1];
    if (lastChar == '/')
    {
        lastChar = slash;
        return;
    }
    str += slash;
}
inline std::string EnsureLastSlash2_Ansi(const std::string & str,
                                            char slash = '/')
{
    std::string str2(str);
    EnsureLastSlash_Ansi(str2, slash);
    return str2;
}

inline std::string GetPathNameOfFullPathName2(const std::string & str)
{
    for(long i = (long)str.size()-1; i>0; --i)
    {
        if (str[i] == '/')
            return std::string(str.begin(), str.begin()+i);
    }
    return "";
}

class CFile
{
    int m_descriptor;

    long long GetCurrentPointer() const;

    CFile(const CFile & );
    CFile & operator = (const CFile&);
public:
    CFile();
    ~CFile();

    void OpenExistingRead(const std::string & fullname);
    void CreateNewAlways(const std::string & fullname);
    bool OpenAsLock(const std::string & fullname);

    unsigned long long GetSize() const;
    void Close();

    void MoveToFirst(long long offset);
    void WriteToFile(const void * pBegin, size_t size);
    void WriteToFile(const void * pBegin, const void * pEnd);
    unsigned int Read(void * pData, unsigned int dwSize);
    void ExactRead(void * pData, unsigned int dwSize);
    void FlushBuffers();
    int FlushBuffers_Silent();

    void swap(CFile & file);
};

template<class VectorCharType>
void SaveVectorToFile(const std::string& fileName, const std::vector<VectorCharType>& data)
{
    CFile file;
    file.CreateNewAlways(fileName);

    if (data.empty())
        return;

    const VectorCharType * pData = &data[0];
    file.WriteToFile(pData, pData + data.size());
}
inline 
void SaveDataToFile(const std::string& fileName,
                    const void * pData,
                    size_t sizeInBytes)
{
    CFile file;
    file.CreateNewAlways(fileName);
    
    if (!sizeInBytes)
        return;

    file.WriteToFile(pData, sizeInBytes);
}

inline void LoadFileToVector(const std::string& fileName, std::vector<char>& data)
{
    CFile file;
    file.OpenExistingRead(fileName);        

    unsigned long long size = file.GetSize();
    const int sanityCheck = 256*1024*1024;
    if (size > (unsigned long long )sanityCheck)
        throw std::runtime_error("File too big");

    data.resize(size);

    if (!size)
        return;

    file.ExactRead(data.data(), size);
}

std::string GetApplicationDataPathWithSlash();

bool IsFileExists(const char * pFileName);
bool IsFileExists(const std::string & fileName);

void CreateDir(const std::string & path, const char * pData = "Can't access directory");
bool CreateDir_Silent(const std::string & path);
std::string GetTempPathWithSlash();
void DeleteFolder(const orthia::PlatformString_type & sPath,
                    bool bDeleteRoot);


}
#endif