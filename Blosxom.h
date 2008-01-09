#ifndef _BLOSXOM_H_
#define _BLOSXOM_H_

#define ATOMPP_VERSION 0x0100

#ifdef _MSC_VER
#include <tchar.h>
#pragma warning(disable : 4530 4018 4786)
#pragma comment(lib, "wininet.lib")
#endif

#include <vector>
#include <map>
#include <string>
#include <algorithm>

#ifndef _TSTRDEF
#ifndef _MSC_VER
#define _T(x) (x)
#define _stprintf sprintf
#define _sntprintf snprintf
#define _tprintf printf
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscmp strcmp
#define _tcsncmp strncmp
#define _tcschr strchr
#define _tcsstr strstr
#define _ttol atol
#define _tstat stat
#define _tfopen fopen
#define _stscanf sscanf
#define _tcsftime strftime
#define stricmp strcasecmp
#ifndef SD_BOTH
# define SD_BOTH SHUT_RDWR
#endif
#endif

#ifdef _UNICODE
typedef std::wstring tstring;
typedef std::wostream tstream;
typedef wchar_t tchar;
#define tcout std::wcout
#define tcerr std::wcerr
#else
typedef std::string tstring;
typedef std::ostream tstream;
typedef char tchar;
#define tcout std::cout
#define tcerr std::cerr
#endif
#define _TSTRDEF
#endif

namespace Blosxom {

typedef std::map<tstring, tstring> FolderList;
typedef std::map<tstring, tstring> FileInfo;
typedef std::vector<FileInfo> FileList;

int FtpUpload(tstring server, tstring userid, tstring passwd, tstring text, tstring remotefile);
int FtpDownload(tstring server, tstring userid, tstring passwd, tstring remotefile, tstring& text);
int FtpDelete(tstring server, tstring userid, tstring passwd, tstring remotefile);
FolderList FtpFolders(tstring server, tstring userid, tstring passwd, tstring root);
FileList FtpFiles(tstring server, tstring userid, tstring passwd, tstring root);

}

#endif /* _BLOSXOM_H_ */
