#include "tjsCommHead.h"
#include "Platform.h"
#include "PlatformFile.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "TVPMsg.h"
#include "casepath.h"

#include <vector>

//---------------------------------------------------------------------------
// tTVPFileMedia
//---------------------------------------------------------------------------
class tTVPFileMedia : public iTVPStorageMedia
{
    tjs_uint RefCount;

public:
    tTVPFileMedia() { RefCount = 1; }
    ~tTVPFileMedia() { ; }

    void AddRef() { RefCount++; }
    void Release()
    {
        if (RefCount == 1)
            delete this;
        else
            RefCount--;
    }

    void GetName(ttstr& name) { name = TJS_N("file"); }

    void NormalizeDomainName(ttstr& name);
    void NormalizePathName(ttstr& name);
    bool CheckExistentStorage(const ttstr& name);
    tTJSBinaryStream* Open(const ttstr& name, tjs_uint32 flags);
    void GetListAt(const ttstr& name, iTVPStorageLister* lister);
    void GetLocallyAccessibleName(ttstr& name);

public:
    void GetLocalName(ttstr& name);
};
//---------------------------------------------------------------------------
void tTVPFileMedia::NormalizeDomainName(ttstr& name)
{
    // normalize domain name
    // make all characters small
    tjs_char* p = name.Independ();
    while (*p)
    {
        if (*p >= TJS_N('A') && *p <= TJS_N('Z'))
            *p += TJS_N('a') - TJS_N('A');
        p++;
    }
}
//---------------------------------------------------------------------------
void tTVPFileMedia::NormalizePathName(ttstr& name)
{
    // do nothing
}
//---------------------------------------------------------------------------
bool tTVPFileMedia::CheckExistentStorage(const ttstr& name)
{
    if (name.IsEmpty())
        return false;

    ttstr _name(name);
    GetLocalName(_name);

    return TVPCheckExistentLocalFile(_name);
}
//---------------------------------------------------------------------------
tTJSBinaryStream* tTVPFileMedia::Open(const ttstr& name, tjs_uint32 flags)
{
    // open storage named "name".
    // currently only local/network(by OS) storage systems are supported.
    if (name.IsEmpty())
        TVPThrowExceptionMessage(TVPCannotOpenStorage, TJS_N("\"\""));

    ttstr origname = name;
    ttstr _name(name);
    GetLocalName(_name);

    return TVPCreateLocalFileStream(origname, _name, flags);
}

//---------------------------------------------------------------------------
void tTVPFileMedia::GetListAt(const ttstr& _name, iTVPStorageLister* lister)
{
    ttstr name(_name);
    GetLocalName(name);
    TVPGetLocalFileListAt(name,
                          [lister](const ttstr& name, tTVPLocalFileInfo* s)
                          {
                              if (s->Mode & (S_IFREG))
                              {
                                  lister->Add(name);
                              }
                          });
}

//---------------------------------------------------------------------------
void tTVPFileMedia::GetLocallyAccessibleName(ttstr& name)
{
    const tjs_char* ptr = name.c_str();

    if (!TJS_strncmp(ptr, TJS_N("./"), 2))
        ptr += 2; // skip "./"

    // As upstream, nothing after "./", or a name that starts with a slash, has no local name.
    if (!*ptr || *ptr == TJS_N('/'))
    {
        name.Clear();
        return;
    }

    // Windows games name files in any case, and iOS and case-sensitive Mac volumes keep it. Only
    // folders below the deepest one that exists as written are listed, since the iOS sandbox
    // refuses to list those above the app's own. The first name that matches nothing, and the rest
    // after it, stay as written, so a new file goes into the existing folder of another case.
    std::string path = std::string("/") + ptr;
    std::vector<char> real(path.size() + 1);
    casepath_resolve(path.c_str(), real.data(), real.size());
    name = ttstr(real.data());
}
//---------------------------------------------------------------------------
void tTVPFileMedia::GetLocalName(ttstr& name)
{
    ttstr tmp = name;
    GetLocallyAccessibleName(tmp);
    if (tmp.IsEmpty())
        TVPThrowExceptionMessage(TVPCannotGetLocalName, name);
    name = tmp;
}
//---------------------------------------------------------------------------
iTVPStorageMedia* TVPCreateFileMedia()
{
    return new tTVPFileMedia;
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
bool TVP_utime(const char* name, time_t modtime)
{
    timeval mt[2];
    mt[0].tv_sec = modtime;
    mt[0].tv_usec = 0;
    mt[1].tv_sec = modtime;
    mt[1].tv_usec = 0;
    return utimes(name, mt) == 0;
}
//---------------------------------------------------------------------------
void TVPRelinquishCPU()
{
    sched_yield();
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void TVPPreNormalizeStorageName(ttstr& name)
{
    // if the name is an OS's native expression, change it according
    // with the TVP storage system naming rule.
    tjs_int namelen = name.GetLen();
    if (namelen == 0)
        return;
    if (namelen >= 1)
    {
        if (name[0] == TJS_N('/'))
        {
            name = ttstr(TJS_N("file://.")) + name;
            return;
        }
    }
}

bool TVPTruncateFile(const std::string& path, size_t size)
{
    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0)
        return false;
    bool ok = (ftruncate(fd, (off_t)size) == 0);
    close(fd);
    return ok;
}

uint16_t TVPGetFileAttributes(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return 0xFFFF;
    uint16_t attr = 0;
    if (S_ISDIR(st.st_mode))
        attr |= 0x10;
    if (access(path.c_str(), W_OK) != 0)
        attr |= 0x01;
    return attr;
}

bool TVPSetFileAttributes(const std::string& path, uint16_t attr, uint16_t mask)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    mode_t mode = st.st_mode;
    if (mask & 0x01)
    {
        if (attr & 0x01)
            mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
        else
            mode |= (S_IWUSR | S_IWGRP | S_IWOTH);
    }
    return chmod(path.c_str(), mode) == 0;
}
//---------------------------------------------------------------------------