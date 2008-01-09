#ifndef _ATOMPP_H_
#define _ATOMPP_H_

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

namespace AtomPP {

class AtomException {
public:
	tstring message;
	int code;
	AtomException(int _code, tstring _message) {
		code = _code;
		message = _message;
	}
};

class AtomContent {
public:
	tstring type;
	tstring mode;
	tstring value;
};

class AtomLink {
public:
	tstring rel;
	tstring href;
	tstring type;
	tstring title;
};

class AtomEntry {
public:
	tstring id;
	struct tm published;
	struct tm updated;
	tstring edit_uri;
	tstring title;
	tstring summary;
	AtomContent content;
	std::vector<AtomLink> links;
};

class AtomEntries {
public:
	tstring error;
	std::vector<AtomEntry> entries;
};

class AtomFeed {
public:
	tstring error;
	tstring id;
	tstring title;
	std::vector<AtomLink> links;
};

AtomFeed getFeed(tstring url, tstring userid, tstring passwd, tstring user_agent = _T(""));
AtomEntries getEntries(tstring url, tstring userid, tstring passwd, tstring user_agent = _T(""));
int createEntry(tstring feed_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent = _T(""));
int updateEntry(tstring update_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent = _T(""));
int deleteEntry(tstring delete_url, tstring userid, tstring passwd, tstring user_agent = _T(""));

int createEntry_Blogger(tstring feed_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent = _T(""));
int updateEntry_Blogger(tstring update_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent = _T(""));
int deleteEntry_Blogger(tstring delete_url, tstring userid, tstring passwd, tstring user_agent = _T(""));

std::string StringToHex(const std::string& input);
std::string md5(const std::string& input);

}

#endif /* _ATOMPP_H_ */
