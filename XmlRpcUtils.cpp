#define _CRT_SECURE_NO_DEPRECATE

#ifdef _MSC_VER
//# define USE_WIN32API
# pragma warning(disable : 4530 4018 4786)
#endif

#ifdef USE_WIN32API
#pragma comment(lib, "wininet.lib")
#include <windows.h>
#include <wininet.h>
#include <mlang.h>
#include <atlbase.h>
#import "msxml4.dll" named_guids
#else
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <curl/curl.h>
#include <errno.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "XmlRpcUtils.h"
#include <sstream>

namespace XmlRpc {

const std::string base64_chars = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";
#define is_base64(c) ( \
		isalnum((unsigned char)c) || \
		((unsigned char)c == '+') || \
		((unsigned char)c == '/'))

struct interval {
  unsigned short first;
  unsigned short last;
};

static char utf8len_tab[256] =
{
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /*bogus*/
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1,
};


int utf_bytes2char(unsigned char* p)
{
	int		len;

	if (p[0] < 0x80)	/* be quick for ASCII */
		return p[0];

	len = utf8len_tab[p[0]];
	if ((p[1] & 0xc0) == 0x80) {
		if (len == 2)
			return ((p[0] & 0x1f) << 6) + (p[1] & 0x3f);
		if ((p[2] & 0xc0) == 0x80) {
			if (len == 3)
				return ((p[0] & 0x0f) << 12) + ((p[1] & 0x3f) << 6)
					+ (p[2] & 0x3f);
			if ((p[3] & 0xc0) == 0x80) {
				if (len == 4)
					return ((p[0] & 0x07) << 18) + ((p[1] & 0x3f) << 12)
						+ ((p[2] & 0x3f) << 6) + (p[3] & 0x3f);
				if ((p[4] & 0xc0) == 0x80) {
					if (len == 5)
						return ((p[0] & 0x03) << 24) + ((p[1] & 0x3f) << 18)
							+ ((p[2] & 0x3f) << 12) + ((p[3] & 0x3f) << 6)
							+ (p[4] & 0x3f);
					if ((p[5] & 0xc0) == 0x80 && len == 6)
						return ((p[0] & 0x01) << 30) + ((p[1] & 0x3f) << 24)
							+ ((p[2] & 0x3f) << 18) + ((p[3] & 0x3f) << 12)
							+ ((p[4] & 0x3f) << 6) + (p[5] & 0x3f);
				}
			}
		}
	}
	/* Illegal value, just return the first byte */
	return p[0];
}

int utf_char2bytes(int c, unsigned char* buf)
{
	if (c < 0x80) {			/* 7 bits */
		buf[0] = c;
		return 1;
	}
	if (c < 0x800) {		/* 11 bits */
		buf[0] = 0xc0 + ((unsigned)c >> 6);
		buf[1] = 0x80 + (c & 0x3f);
		return 2;
	}
	if (c < 0x10000) {		/* 16 bits */
		buf[0] = 0xe0 + ((unsigned)c >> 12);
		buf[1] = 0x80 + (((unsigned)c >> 6) & 0x3f);
		buf[2] = 0x80 + (c & 0x3f);
		return 3;
	}
	if (c < 0x200000) {		/* 21 bits */
		buf[0] = 0xf0 + ((unsigned)c >> 18);
		buf[1] = 0x80 + (((unsigned)c >> 12) & 0x3f);
		buf[2] = 0x80 + (((unsigned)c >> 6) & 0x3f);
		buf[3] = 0x80 + (c & 0x3f);
		return 4;
	}
	if (c < 0x4000000) {	/* 26 bits */
		buf[0] = 0xf8 + ((unsigned)c >> 24);
		buf[1] = 0x80 + (((unsigned)c >> 18) & 0x3f);
		buf[2] = 0x80 + (((unsigned)c >> 12) & 0x3f);
		buf[3] = 0x80 + (((unsigned)c >> 6) & 0x3f);
		buf[4] = 0x80 + (c & 0x3f);
		return 5;
	}

	/* 31 bits */
	buf[0] = 0xfc + ((unsigned)c >> 30);
	buf[1] = 0x80 + (((unsigned)c >> 24) & 0x3f);
	buf[2] = 0x80 + (((unsigned)c >> 18) & 0x3f);
	buf[3] = 0x80 + (((unsigned)c >> 12) & 0x3f);
	buf[4] = 0x80 + (((unsigned)c >> 6) & 0x3f);
	buf[5] = 0x80 + (c & 0x3f);
	return 6;
}

static int bisearch(wchar_t ucs, const struct interval *table, int max) {
  int min = 0;
  int mid;

  if (ucs < table[0].first || ucs > table[max].last)
    return 0;
  while (max >= min) {
    mid = (min + max) / 2;
    if (ucs > table[mid].last)
      min = mid + 1;
    else if (ucs < table[mid].first)
      max = mid - 1;
    else
      return 1;
  }

  return 0;
}

int wcwidth(wchar_t ucs)
{
  /* sorted list of non-overlapping intervals of non-spacing characters */
  static const struct interval combining[] = {
    { 0x0300, 0x034E }, { 0x0360, 0x0362 }, { 0x0483, 0x0486 },
    { 0x0488, 0x0489 }, { 0x0591, 0x05A1 }, { 0x05A3, 0x05B9 },
    { 0x05BB, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
    { 0x05C4, 0x05C4 }, { 0x064B, 0x0655 }, { 0x0670, 0x0670 },
    { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
    { 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
    { 0x07A6, 0x07B0 }, { 0x0901, 0x0902 }, { 0x093C, 0x093C },
    { 0x0941, 0x0948 }, { 0x094D, 0x094D }, { 0x0951, 0x0954 },
    { 0x0962, 0x0963 }, { 0x0981, 0x0981 }, { 0x09BC, 0x09BC },
    { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 },
    { 0x0A02, 0x0A02 }, { 0x0A3C, 0x0A3C }, { 0x0A41, 0x0A42 },
    { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D }, { 0x0A70, 0x0A71 },
    { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 },
    { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD }, { 0x0B01, 0x0B01 },
    { 0x0B3C, 0x0B3C }, { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 },
    { 0x0B4D, 0x0B4D }, { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 },
    { 0x0BC0, 0x0BC0 }, { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 },
    { 0x0C46, 0x0C48 }, { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 },
    { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
    { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D }, { 0x0DCA, 0x0DCA },
    { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 }, { 0x0E31, 0x0E31 },
    { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 },
    { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC }, { 0x0EC8, 0x0ECD },
    { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 },
    { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E }, { 0x0F80, 0x0F84 },
    { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 }, { 0x0F99, 0x0FBC },
    { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 }, { 0x1032, 0x1032 },
    { 0x1036, 0x1037 }, { 0x1039, 0x1039 }, { 0x1058, 0x1059 },
    { 0x1160, 0x11FF }, { 0x17B7, 0x17BD }, { 0x17C6, 0x17C6 },
    { 0x17C9, 0x17D3 }, { 0x180B, 0x180E }, { 0x18A9, 0x18A9 },
    { 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x206A, 0x206F },
    { 0x20D0, 0x20E3 }, { 0x302A, 0x302F }, { 0x3099, 0x309A },
    { 0xFB1E, 0xFB1E }, { 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF },
    { 0xFFF9, 0xFFFB }
  };

  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
    return -1;

  /* binary search in table of non-spacing characters */
  if (bisearch(ucs, combining,
	       sizeof(combining) / sizeof(struct interval) - 1))
    return 0;

  /* if we arrive here, ucs is not a combining or C0/C1 control character */

  return 1 + 
    (ucs >= 0x1100 &&
     (ucs <= 0x115f ||                    /* Hangul Jamo init. consonants */
      (ucs >= 0x2e80 && ucs <= 0xa4cf && (ucs & ~0x0011) != 0x300a &&
       ucs != 0x303f) ||                  /* CJK ... Yi */
      (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
      (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
      (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
      (ucs >= 0xff00 && ucs <= 0xff5f) || /* Fullwidth Forms */
      (ucs >= 0xffe0 && ucs <= 0xffe6) ||
      (ucs >= 0x20000 && ucs <= 0x2ffff)));
}

int wcswidth(const wchar_t *pwcs, size_t n)
{
  int w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = wcwidth(*pwcs)) < 0)
      return -1;
    else
      width += w;

  return width;
}

static int wcwidth_cjk(wchar_t ucs)
{
  /* sorted list of non-overlapping intervals of East Asian Ambiguous
   * characters */
  static const struct interval ambiguous[] = {
    { 0x00A1, 0x00A1 }, { 0x00A4, 0x00A4 }, { 0x00A7, 0x00A8 },
    { 0x00AA, 0x00AA }, { 0x00AD, 0x00AE }, { 0x00B0, 0x00B4 },
    { 0x00B6, 0x00BA }, { 0x00BC, 0x00BF }, { 0x00C6, 0x00C6 },
    { 0x00D0, 0x00D0 }, { 0x00D7, 0x00D8 }, { 0x00DE, 0x00E1 },
    { 0x00E6, 0x00E6 }, { 0x00E8, 0x00EA }, { 0x00EC, 0x00ED },
    { 0x00F0, 0x00F0 }, { 0x00F2, 0x00F3 }, { 0x00F7, 0x00FA },
    { 0x00FC, 0x00FC }, { 0x00FE, 0x00FE }, { 0x0101, 0x0101 },
    { 0x0111, 0x0111 }, { 0x0113, 0x0113 }, { 0x011B, 0x011B },
    { 0x0126, 0x0127 }, { 0x012B, 0x012B }, { 0x0131, 0x0133 },
    { 0x0138, 0x0138 }, { 0x013F, 0x0142 }, { 0x0144, 0x0144 },
    { 0x0148, 0x014B }, { 0x014D, 0x014D }, { 0x0152, 0x0153 },
    { 0x0166, 0x0167 }, { 0x016B, 0x016B }, { 0x01CE, 0x01CE },
    { 0x01D0, 0x01D0 }, { 0x01D2, 0x01D2 }, { 0x01D4, 0x01D4 },
    { 0x01D6, 0x01D6 }, { 0x01D8, 0x01D8 }, { 0x01DA, 0x01DA },
    { 0x01DC, 0x01DC }, { 0x0251, 0x0251 }, { 0x0261, 0x0261 },
    { 0x02C4, 0x02C4 }, { 0x02C7, 0x02C7 }, { 0x02C9, 0x02CB },
    { 0x02CD, 0x02CD }, { 0x02D0, 0x02D0 }, { 0x02D8, 0x02DB },
    { 0x02DD, 0x02DD }, { 0x02DF, 0x02DF }, { 0x0300, 0x034E },
    { 0x0360, 0x0362 }, { 0x0391, 0x03A1 }, { 0x03A3, 0x03A9 },
    { 0x03B1, 0x03C1 }, { 0x03C3, 0x03C9 }, { 0x0401, 0x0401 },
    { 0x0410, 0x044F }, { 0x0451, 0x0451 }, { 0x2010, 0x2010 },
    { 0x2013, 0x2016 }, { 0x2018, 0x2019 }, { 0x201C, 0x201D },
    { 0x2020, 0x2022 }, { 0x2024, 0x2027 }, { 0x2030, 0x2030 },
    { 0x2032, 0x2033 }, { 0x2035, 0x2035 }, { 0x203B, 0x203B },
    { 0x203E, 0x203E }, { 0x2074, 0x2074 }, { 0x207F, 0x207F },
    { 0x2081, 0x2084 }, { 0x20AC, 0x20AC }, { 0x2103, 0x2103 },
    { 0x2105, 0x2105 }, { 0x2109, 0x2109 }, { 0x2113, 0x2113 },
    { 0x2116, 0x2116 }, { 0x2121, 0x2122 }, { 0x2126, 0x2126 },
    { 0x212B, 0x212B }, { 0x2153, 0x2155 }, { 0x215B, 0x215E },
    { 0x2160, 0x216B }, { 0x2170, 0x2179 }, { 0x2190, 0x2199 },
    { 0x21B8, 0x21B9 }, { 0x21D2, 0x21D2 }, { 0x21D4, 0x21D4 },
    { 0x21E7, 0x21E7 }, { 0x2200, 0x2200 }, { 0x2202, 0x2203 },
    { 0x2207, 0x2208 }, { 0x220B, 0x220B }, { 0x220F, 0x220F },
    { 0x2211, 0x2211 }, { 0x2215, 0x2215 }, { 0x221A, 0x221A },
    { 0x221D, 0x2220 }, { 0x2223, 0x2223 }, { 0x2225, 0x2225 },
    { 0x2227, 0x222C }, { 0x222E, 0x222E }, { 0x2234, 0x2237 },
    { 0x223C, 0x223D }, { 0x2248, 0x2248 }, { 0x224C, 0x224C },
    { 0x2252, 0x2252 }, { 0x2260, 0x2261 }, { 0x2264, 0x2267 },
    { 0x226A, 0x226B }, { 0x226E, 0x226F }, { 0x2282, 0x2283 },
    { 0x2286, 0x2287 }, { 0x2295, 0x2295 }, { 0x2299, 0x2299 },
    { 0x22A5, 0x22A5 }, { 0x22BF, 0x22BF }, { 0x2312, 0x2312 },
    { 0x2329, 0x232A }, { 0x2460, 0x24BF }, { 0x24D0, 0x24E9 },
    { 0x2500, 0x254B }, { 0x2550, 0x2574 }, { 0x2580, 0x258F },
    { 0x2592, 0x2595 }, { 0x25A0, 0x25A1 }, { 0x25A3, 0x25A9 },
    { 0x25B2, 0x25B3 }, { 0x25B6, 0x25B7 }, { 0x25BC, 0x25BD },
    { 0x25C0, 0x25C1 }, { 0x25C6, 0x25C8 }, { 0x25CB, 0x25CB },
    { 0x25CE, 0x25D1 }, { 0x25E2, 0x25E5 }, { 0x25EF, 0x25EF },
    { 0x2605, 0x2606 }, { 0x2609, 0x2609 }, { 0x260E, 0x260F },
    { 0x261C, 0x261C }, { 0x261E, 0x261E }, { 0x2640, 0x2640 },
    { 0x2642, 0x2642 }, { 0x2660, 0x2661 }, { 0x2663, 0x2665 },
    { 0x2667, 0x266A }, { 0x266C, 0x266D }, { 0x266F, 0x266F },
    { 0x273D, 0x273D }, { 0x3008, 0x300B }, { 0x3014, 0x3015 },
    { 0x3018, 0x301B }, { 0xFFFD, 0xFFFD }
  };

  /* binary search in table of non-spacing characters */
  if (bisearch(ucs, ambiguous,
	       sizeof(ambiguous) / sizeof(struct interval) - 1))
    return 2;

  return wcwidth(ucs);
}


int wcswidth_cjk(const wchar_t *pwcs, size_t n)
{
  int w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = wcwidth_cjk(*pwcs)) < 0)
      return -1;
    else
      width += w;

  return width;
}

tstring getTimeString(struct tm* _cur) {
	struct tm* cur;
	if (_cur)
		cur = _cur;
	else {
		time_t tim;
		time(&tim);
		cur = localtime(&tim);
	}

	tchar buf[256];
	_stprintf(buf, _T("%04d/%02d/%02d %02d:%02d:%02d\n"),
		cur->tm_year+1900,
		cur->tm_mon,
		cur->tm_mday,
		cur->tm_hour,
		cur->tm_min,
		cur->tm_sec);
	return buf;
}

std::string tstring2string(tstring strSrc) {
	std::string strRet;
#ifdef _UNICODE
	UINT codePage = GetACP();
	size_t mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, NULL, 0, NULL, NULL);
	char* pszStr = new char[mbssize+1];
	mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, pszStr, mbssize, NULL, NULL);
	pszStr[mbssize] = '\0';
	strRet = pszStr;
	delete [] pszStr;
#else
	strRet = strSrc;
#endif
	return strRet;
}

tstring string2tstring(std::string strSrc) {
	tstring strRet;
#ifdef _UNICODE
	size_t in_len = strlen(strSrc.c_str());
	UINT codePage = GetACP();
	size_t wcssize = MultiByteToWideChar(codePage, 0, strSrc.c_str(),in_len,  NULL, 0);
	wchar_t* pszStr = new wchar_t[wcssize + 1];
	MultiByteToWideChar(codePage, 0, strSrc.c_str(), in_len, pszStr, wcssize + 1);
	pszStr[wcssize] = '\0';
	strRet = pszStr;
	delete [] pszStr;
#else
	strRet = strSrc;
#endif
	return strRet;
}

std::string string_to_utf8(std::string str) {
	std::string strRet;
	std::wstring strSrc;

#ifdef _MSC_VER
	UINT codePage;
	size_t in_len = str.size();
	codePage = GetACP();
	size_t wcssize = MultiByteToWideChar(codePage, 0, str.c_str(),in_len,  NULL, 0);
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	wcssize = MultiByteToWideChar(codePage, 0, str.c_str(), in_len, pszStrWC, wcssize + 1);
	pszStrWC[wcssize] = '\0';
	strSrc = pszStrWC;
	delete [] pszStrWC;

	codePage = CP_UTF8;
	size_t mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, NULL, 0, NULL, NULL);
	char* pszStr = new char[mbssize + 1];
	mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)strSrc.c_str(), -1, pszStr, mbssize, NULL, NULL);
	pszStr[mbssize] = '\0';
	strRet = pszStr;
	delete [] pszStr;
#else
	char* ptr = (char*)str.c_str();
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	int n = 0, clen = 0, len = 0;
	mblen(NULL, 0);
	while(len < mbssize) {
		clen = mblen(ptr, MB_CUR_MAX);
		if (clen <= 0) {
			mblen(NULL, 0);
			clen = 1;
		}
		clen = mbtowc(pszStrWC+n++, ptr,  clen);
		if (clen <= 0) {
			mblen(NULL, 0);
			clen = 1;
		}
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
	wcssize = n;
	for(n = 0; n < wcssize; n++) {
		unsigned char bytes[8] = {0};
		utf_char2bytes(pszStrWC[n], bytes);
		strRet += (char*)bytes;
	}
	delete[] pszStrWC;
#endif

	return strRet;
}

#ifdef _UNICODE
tstring string_to_utf8(tstring str) {
	return str;
}
#endif

std::string utf8_to_string(std::string str) {
	std::string strRet;

#ifdef _MSC_VER1
	UINT codePage = CP_UTF8;
	const char* ptr = str.c_str();
	if (str[0] == (char)0xef && str[1] == (char)0xbb && str[2] == (char)0xbf)
		ptr += 3;
	size_t wcssize = MultiByteToWideChar(codePage, 0, ptr, -1,  NULL, 0);
	wchar_t* pszStr = new wchar_t[wcssize + 1];
	wcssize = MultiByteToWideChar(codePage, 0, ptr, -1, pszStr, wcssize + 1);
	pszStr[wcssize] = '\0';

	codePage = GetACP();
	size_t mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)pszStr,-1,NULL,0,NULL,NULL);
	char* pszStrMB = new char[mbssize+1];
	mbssize = WideCharToMultiByte(codePage, 0, (LPCWSTR)pszStr, -1, pszStrMB, mbssize, NULL, NULL);
	pszStrMB[mbssize] = '\0';
	strRet = pszStrMB;
	delete [] pszStrMB;

	delete [] pszStr;
#else
	char* ptr = (char*)str.c_str();
	if (ptr[0] == (char)0xef && ptr[1] == (char)0xbb && ptr[2] == (char)0xbf)
		ptr += 3;
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	int n = 0, clen = 0, len = 0;
	while(len < mbssize) {
		int c = utf_bytes2char((unsigned char*)ptr);
		if (c == 0x301c) c = 0xff5e;
		if (c == 0x2016) c = 0x2225;
		if (c == 0x2212) c = 0xff0d;
		if (c == 0x00a2) c = 0xffe0;
		if (c == 0x00a3) c = 0xffe1;
		if (c == 0x00ac) c = 0xffe2;
		pszStrWC[n++] = c;
		clen = utf8len_tab[(unsigned char)*ptr];
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
	wcssize = n;
	mbssize = wcslen(pszStrWC)*8;
	char* pszStrMB = new char[mbssize + 1];
	clen = 0;
	len = 0;
	for(n = 0; n < wcssize; n++) {
		clen = wctomb(pszStrMB+len, pszStrWC[n]);
		len += clen <= 0 ? 1 : clen;
	}
	*(pszStrMB+len) = 0;
	delete[] pszStrWC;
	strRet = pszStrMB;
	delete[] pszStrMB;
#endif

	return strRet;
}

#ifdef _UNICODE
tstring utf8_to_string(tstring str) {
	return str;
}
#endif

#ifdef USE_WIN32API
tstring convert_string(tstring str, const tstring from_codeset, const tstring to_codeset) {
	USES_CONVERSION;
	tstring ret = str;
	IMultiLanguage *pml = NULL;
	HRESULT hr = CoCreateInstance(CLSID_CMultiLanguage, NULL, CLSCTX_ALL, IID_IMultiLanguage, (void**)&pml);
	unsigned char *ptr = NULL;
	if (SUCCEEDED(hr)) {
		DWORD dwMode=0;
		MIMECSETINFO charsetInfo;
		UINT uiFrom, uiTo;
		if (from_codeset == _T("char"))
			uiFrom = GetACP();
		else {
			ZeroMemory(&charsetInfo, sizeof(charsetInfo));
			pml->GetCharsetInfo(T2BSTR(from_codeset.c_str()), &charsetInfo);
			uiFrom = charsetInfo.uiInternetEncoding;
		}
		if (to_codeset == _T("char"))
			uiTo = GetACP();
		else {
			ZeroMemory(&charsetInfo, sizeof(charsetInfo));
			pml->GetCharsetInfo(T2BSTR(to_codeset.c_str()), &charsetInfo);
			uiTo = charsetInfo.uiInternetEncoding;
		}
		ptr = new unsigned char[str.size()*MB_LEN_MAX+1];
		unsigned int nin = str.size();
		unsigned int nout = str.size()*MB_LEN_MAX+1;
		hr = pml->ConvertString(&dwMode, uiFrom, uiTo, (unsigned char*)str.c_str(), &nin, ptr, &nout);
		if (FAILED(hr) || (str.size() && nout == 0)) {
			delete[] ptr;
			ptr = NULL;
		} else
			ptr[nout] = 0;
	}
	if (ptr) {
		ret = A2T((char*)ptr);
		delete[] ptr;
	}
	if (pml) pml->Release();
	return ret;
}
#else
std::string convert_string(const std::string str, const std::string from_codeset, const std::string to_codeset) {
	char *dest = NULL;
	iconv_t cd;
	char *outp;
	char *p = (char *) str.c_str();
	size_t inbytes_remaining = strlen (p);
	size_t outbuf_size = inbytes_remaining + 1;
	size_t outbytes_remaining;
	size_t err;
	int have_error = 0;

	outbuf_size *= MB_LEN_MAX;
	outbytes_remaining = outbuf_size - 1;

	if (strcmp(to_codeset.c_str(), from_codeset.c_str()) == 0)
		return str.c_str();

	cd = iconv_open(to_codeset.c_str(), from_codeset.c_str());
	if (cd == (iconv_t) -1)
		return str.c_str();

	outp = dest = (char *) malloc (outbuf_size);
	if (dest == NULL)
		goto out;

again:
#if defined(_LIBICONV_VERSION) && _LIBICONV_VERSION <= 0x010A
	err = iconv(cd, (const char**)&p, &inbytes_remaining, &outp, &outbytes_remaining);
#else
	err = iconv(cd, (char**)&p, &inbytes_remaining, &outp, &outbytes_remaining);
#endif
	if (err == (size_t) - 1) {
		switch (errno) {
			case EINVAL:
				break;
			case E2BIG:
				{
					size_t used = outp - dest;
					size_t newsize = outbuf_size * 2;
					char *newdest;

					if (newsize <= outbuf_size) {
						errno = ENOMEM;
						have_error = 1;
						goto out;
					}
					newdest = (char *) realloc (dest, newsize);
					if (newdest == NULL) {
						have_error = 1;
						goto out;
					}
					dest = newdest;
					outbuf_size = newsize;

					outp = dest + used;
					outbytes_remaining = outbuf_size - used - 1;        /* -1 for NUL */

					goto again;
				}
				break;
			case EILSEQ:
				have_error = 1;
				break;
			default:
				have_error = 1;
				break;
		}
	}

	*outp = '\0';

out:
	{
		int save_errno = errno;

		if (iconv_close (cd) < 0 && !have_error) {
			/* If we didn't have a real error before, make sure we restore
			   the iconv_close error below. */
			save_errno = errno;
			have_error = 1;
		}

		if (have_error && dest) {
			free (dest);
			dest = (char*)str.c_str();
			errno = save_errno;
		}
	}

	return dest;
}
#endif

std::string cut_string(std::string str, int cells, std::string padding) {
	char* ptr = (char*)str.c_str();
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	int n = 0;
#ifdef _WIN32
	n = MultiByteToWideChar(GetACP(), 0, ptr, mbssize, pszStrWC, wcssize + 1);
	pszStrWC[n] = 0;
#else
	int clen = 0, len = 0;
	mblen(NULL, 0);
	while(len < mbssize) {
		clen = mblen(ptr, MB_CUR_MAX);
		if (clen <= 0) {
			mblen(NULL, 0);
			clen = 1;
		}
		clen = mbtowc(pszStrWC+n++, ptr,  clen);
		if (clen <= 0) {
			mblen(NULL, 0);
			clen = 1;
		}
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
#endif
	wcssize = n;
	int width = 0;
	int padpos = 0;
	for(n = 0; n < wcssize; n++) {
		int c = wcwidth_cjk(pszStrWC[n]);
		if (width + c > cells) {
			pszStrWC[n] = 0;
			break;
		}
		width += c;
	}
	char* pszStrMB;
#ifdef _WIN32
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, NULL, 0, NULL, NULL);
	pszStrMB = new char[mbssize + 1];
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, pszStrMB, mbssize, NULL, NULL);
	pszStrMB[mbssize] = 0;
#else
	wcssize = wcslen(pszStrWC);
	mbssize = wcssize*8;
	pszStrMB = new char[mbssize + 1];
	clen = 0;
	len = 0;
	for(n = 0; n < wcssize; n++) {
		clen = wctomb(pszStrMB+len, pszStrWC[n]);
		len += clen <= 0 ? 1 : clen;
	}
	*(pszStrMB+len) = 0;
#endif
	delete[] pszStrWC;
	std::string ret = pszStrMB;
	delete[] pszStrMB;
	if (ret.size() < str.size())
		ret += padding;
	return ret;
}

std::string cut_string_r(std::string str, int cells, std::string padding) {
	char* ptr = (char*)str.c_str();
	size_t mbssize = strlen(ptr);
	size_t wcssize = mbssize;
	wchar_t* pszStrWC = new wchar_t[wcssize + 1];
	int n = 0;
#ifdef _WIN32
	n = MultiByteToWideChar(GetACP(), 0, ptr, mbssize, pszStrWC, wcssize + 1);
	pszStrWC[n] = 0;
#else
	int clen = 0, len = 0;
	mblen(NULL, 0);
	while(len < mbssize) {
		clen = mblen(ptr, MB_CUR_MAX);
		if (clen <= 0) {
			mblen(NULL, 0);
			clen = 1;
		}
		clen = mbtowc(pszStrWC+n++, ptr,  clen);
		if (clen <= 0) {
			mblen(NULL, 0);
			clen = 1;
		}
		len += clen;
		ptr += clen;
	}
	pszStrWC[n] = 0;
#endif
	wcssize = n;
	int width = 0;
	int padpos = 0;
	for(n = wcssize-1; n >= 0; n--) {
		int c = wcwidth_cjk(pszStrWC[n]);
		if (width + c > cells) {
			pszStrWC[n] = 0;
			wcscpy(pszStrWC, pszStrWC+n+1);
			break;
		}
		width += c;
	}
	char* pszStrMB;
#ifdef _WIN32
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, NULL, 0, NULL, NULL);
	pszStrMB = new char[mbssize + 1];
	mbssize = WideCharToMultiByte(GetACP(), 0, pszStrWC, -1, pszStrMB, mbssize, NULL, NULL);
	pszStrMB[mbssize] = 0;
#else
	wcssize = wcslen(pszStrWC);
	mbssize = wcssize*8;
	pszStrMB = new char[mbssize + 1];
	clen = 0;
	len = 0;
	for(n = 0; n < wcssize; n++) {
		clen = wctomb(pszStrMB+len, pszStrWC[n]);
		len += clen <= 0 ? 1 : clen;
	}
	*(pszStrMB+len) = 0;
#endif
	delete[] pszStrWC;
	std::string ret = pszStrMB;
	delete[] pszStrMB;
	if (ret.size() < str.size())
		ret = padding + ret;
	return ret;
}

std::vector<std::string> split_string(std::string strSrc, std::string strKey) {
	std::vector<std::string> vecLines;
	std::string strTmp = strSrc;

	int iIndex = 0;
	while (iIndex < (int)strTmp.length()) {
		int iOldIndex = iIndex;
		iIndex = strTmp.find(strKey, iIndex);
		if(iIndex != std::string::npos) {
			std::string item = strTmp.substr(iOldIndex, iIndex - iOldIndex);
			vecLines.push_back(item);
		} else {
			std::string item = strTmp.substr(iOldIndex);
			vecLines.push_back(item);
			break;
		}
		iIndex += strKey.length();
	}
	return vecLines;
}

#ifdef _UNICODE
std::vector<tstring> split_string(tstring strSrc, tstring strKey) {
	std::vector<tstring> vecLines;
	tstring strTmp = strSrc;

	int iIndex = 0;
	while (iIndex < (int)strTmp.length()) {
		int iOldIndex = iIndex;
		iIndex = strTmp.find(strKey, iIndex);
		if(iIndex != tstring::npos) {
			tstring item = strTmp.substr(iOldIndex, iIndex - iOldIndex);
			vecLines.push_back(item);
		} else {
			tstring item = strTmp.substr(iOldIndex);
			vecLines.push_back(item);
			break;
		}
		iIndex += strKey.length();
	}
	return vecLines;
}
#endif

std::string trim_string(const std::string strSrc) {
	std::string ret = strSrc;
	int end_pos = ret.find_last_not_of(" \n");
	if(end_pos != -1) {
		ret.erase(end_pos + 1);
		end_pos = ret.find_first_not_of(" \n");
		if(end_pos != -1) ret.erase(0, end_pos);
	}
	else ret.erase(ret.begin(), ret.end());
	return ret;
}
#ifdef _UNICODE
tstring trim_string(const tstring strSrc) {
	tstring ret = strSrc;
	int end_pos = ret.find_last_not_of(' ');
	if(end_pos != -1) {
		ret.erase(end_pos + 1);
		end_pos = ret.find_first_not_of(' ');
		if(end_pos != -1) ret.erase(0, end_pos);
	}
	else ret.erase(ret.begin(), ret.end());
	return ret;
}
#endif

std::string& replace_string(std::string& strSrc, const std::string strFrom, const std::string strTo) {
	std::string::size_type n, nb = 0;

	while((n = strSrc.find(strFrom, nb)) != std::string::npos) {
		strSrc.replace(n, strFrom.size(), strTo);
		nb = n + strTo.size();
	}
	return strSrc;
}
#ifdef _UNICODE
tstring& replace_string(tstring& strSrc, const tstring strFrom, const tstring strTo) {
	tstring::size_type n, nb = 0;

	while((n = strSrc.find(strFrom, nb)) != tstring::npos) {
		strSrc.replace(n, strFrom.size(), strTo);
		nb = n + strTo.size();
	}
	return strSrc;
}
#endif

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3] = {0};
	unsigned char char_array_4[4] = {0};

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for(i = 0; (i <4) ; i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i) {
		for(j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while((i++ < 3))
			ret += '=';
	}

	return ret;
}

std::string base64_decode(std::string const& encoded_string) {
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4] = {0};
	unsigned char char_array_3[3] = {0};
	std::string ret;

	while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i ==4) {
			for (i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			ret += char_array_3[j];
	}

	return ret;
}

std::vector<char> base64_decode_binary(std::string const& encoded_string) {
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4] = {0};
	unsigned char char_array_3[3] = {0};
	std::vector<char> ret;

	while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i ==4) {
			for (i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret.push_back(char_array_3[i]);
			i = 0;
		}
	}

	if (i) {
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			ret.push_back(char_array_3[j]);
	}

	return ret;
}

tstream& operator<<(tstream& os, XmlRpcValue& v) {
	switch (v.type) {
	default:           break;
	case XmlRpcValue::TypeBoolean:  os << v.value.asBool; break;
	case XmlRpcValue::TypeInt:      os << v.value.asInt; break;
	case XmlRpcValue::TypeDouble:   os << v.value.asDouble; break;
	case XmlRpcValue::TypeString:   os << *v.value.asString; break;
	case XmlRpcValue::TypeTime:
		{
			tchar buf[20];
			_sntprintf(buf, sizeof(buf)-1, _T("%4d%02d%02dT%02d:%02d:%02d"),
				v.value.asTime->tm_year+1900,
				v.value.asTime->tm_mon,
				v.value.asTime->tm_mday,
				v.value.asTime->tm_hour,
				v.value.asTime->tm_min,
				v.value.asTime->tm_sec);
			buf[sizeof(buf)-1] = 0;
			os << buf;
			break;
		}
	case XmlRpcValue::TypeBinary:
		{
			XmlRpcValue::ValueBinary::const_iterator itbinary;
			unsigned char *ptr = new unsigned char[v.value.asBinary->size()];
			int n;
			for(n = 0, itbinary = v.value.asBinary->begin(); itbinary != v.value.asBinary->end(); n++, itbinary++)
				ptr[n] = *itbinary;
			os << base64_encode((const unsigned char*)ptr, v.value.asBinary->size()).c_str();
			delete ptr;
			break;
		}
	case XmlRpcValue::TypeArray:
		{
			int s = int(v.value.asArray->size());
			os << '{';
			for (int i=0; i < s; i++) {
				if (i > 0) os << ',';
					os << v.value.asArray->at(i);
			}
			os << '}';
			break;
		}
	case XmlRpcValue::TypeStruct:
		{
			os << _T("[");
			XmlRpcValue::ValueStruct::const_iterator it;
			for (it = v.value.asStruct->begin(); it != v.value.asStruct->end(); it++)
			{
				if (it != v.value.asStruct->begin())
					os << _T(",");
				os << it->first << _T(":");
				XmlRpcValue s = it->second;
				os << s;
			}
			os << _T("]");
			break;
		}
	}

	return os;
}

#ifdef USE_WIN32API
tstring getErrMessage(DWORD dwError) {
	tstring err;
	void*	pMsgBuf;

	if (dwError == ERROR_INTERNET_CANNOT_CONNECT || dwError == ERROR_INTERNET_NAME_NOT_RESOLVED)
		dwError = ERROR_NOT_CONNECTED;
	FormatMessage(
		 FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL,
		 dwError,
		 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		 (LPTSTR) &pMsgBuf,
		 0,
		 NULL
	);

	if (pMsgBuf) {
		err = (LPTSTR)pMsgBuf;
		LocalFree(pMsgBuf);
	} else {
		TCHAR szErr[BUFSIZ] = {0};
		DWORD dwErrCd;
		DWORD dwErrLen = sizeof(szErr);
		InternetGetLastResponseInfo(&dwErrCd, szErr, &dwErrLen);
		err = szErr;
	}
	err.erase(std::remove(err.begin(), err.end(), '\r'), err.end());
	err.erase(std::remove(err.begin(), err.end(), '\n'), err.end());
	return err;
}
#endif

#ifdef USE_WIN32API
XmlRpcValue getXmlRpcValueFromDom(MSXML2::IXMLDOMNodeListPtr pList) {
	USES_CONVERSION;
	XmlRpcValue retVal;
	MSXML2::IXMLDOMNodePtr pNode;
	pNode = pList->nextNode();
	while(pNode) {
		XmlRpcValue ret;
		tstring strName = OLE2T(pNode->nodeName);
		if (strName == _T("xml")) {
			pNode = pList->nextNode();
			continue;
		}
		else
		if (strName == _T("methodCall"))
			return getXmlRpcValueFromDom(pNode->childNodes);
		else
		if (strName == _T("methodResponse"))
			return getXmlRpcValueFromDom(pNode->childNodes);
		else
		if (strName == _T("fault"))
			return getXmlRpcValueFromDom(pNode->childNodes);
		else
		if (strName == _T("params"))
			return getXmlRpcValueFromDom(pNode->childNodes);
		else
		if (strName == _T("param"))
			return getXmlRpcValueFromDom(pNode->childNodes);
		else
		if (strName == _T("struct")) {
			MSXML2::IXMLDOMNodeListPtr pMembers;
			MSXML2::IXMLDOMNodePtr pMember;
			pMembers = pNode->selectNodes(_T("member"));
			pMember = pMembers->nextNode();
			XmlRpcValue::ValueStruct valuestruct;
			while(pMember) {
				MSXML2::IXMLDOMNodePtr pName;
				tstring member_name;
				pName = pMember->selectSingleNode(_T("name"));
				member_name = string_to_utf8((tchar*)pName->text);
				valuestruct[member_name] = getXmlRpcValueFromDom(pMember->selectNodes(_T("value")));
				pMember = pMembers->nextNode();
			}
			ret = valuestruct;
		}
		else
		if (strName == _T("array")) {
			MSXML2::IXMLDOMNodePtr pData;
			pData = pNode->selectSingleNode(_T("data"));
			XmlRpcValue::ValueArray valuearray;
			ret = valuearray;
			if (pData != NULL && pData->childNodes != NULL) {
				XmlRpcValue datas = getXmlRpcValueFromDom(pData->childNodes);
				if (datas.getType() != XmlRpcValue::TypeInvalid)
					ret = datas;
			}
		}
		else
		if (strName == _T("value")) {
			if (pNode->hasChildNodes()) {
				MSXML2::DOMNodeType nodeType;
				pNode->firstChild->get_nodeType(&nodeType);
				if (MSXML2::NODE_TEXT == nodeType)
					ret = string_to_utf8((tchar*)pNode->text);
				else
					ret = getXmlRpcValueFromDom(pNode->childNodes);
			} else {
				ret = string_to_utf8((tchar*)pNode->text);
			}
		}
		else
		if (strName == _T("i4") || strName == _T("int"))
			ret = _ttol((tchar*)pNode->text);
		else
		if (strName == _T("boolean")) {
			tstring bool_value = tstring((tchar*)pNode->text);
			if (_ttol(bool_value.c_str()))
				ret = true;
			else
				ret = (bool)(bool_value == _T("true"));
		}
		else
		if (strName == _T("double"))
			ret = (double)_tcstod((tchar*)pNode->text, NULL);
		else
		if (strName == _T("string"))
			ret = string_to_utf8((tchar*)pNode->text);
		else
		if (strName == _T("dateTime.iso8601")) {
			struct tm tmTime = {0};
			tchar num[64];
			_tcscpy(num, (tchar*)pNode->text);
			_stscanf(num, _T("%4d-%2d-%2dT%2d:%2d:%2d"),
				&tmTime.tm_year,
				&tmTime.tm_mon,
				&tmTime.tm_mday,
				&tmTime.tm_hour,
				&tmTime.tm_min,
				&tmTime.tm_sec);
			tmTime.tm_isdst = -1;
			ret = tmTime;
		} else
		if (strName == _T("base64")) {
			XmlRpcValue::ValueBinary valuebinary;
			valuebinary = base64_decode_binary((char*)pNode->text);
			ret = valuebinary;
		}

		if (ret.getType() != XmlRpcValue::TypeInvalid) {
			if (retVal.getType() != XmlRpcValue::TypeInvalid) {
				if (retVal.getType() == XmlRpcValue::TypeArray)
					retVal[(int)retVal.value.asArray->size()] = ret;
				else {
					XmlRpcValue::ValueArray valuearray;
					valuearray.push_back(retVal);
					valuearray.push_back(ret);
					retVal = valuearray;
				}
			} else
				retVal = ret;
		}
		pNode = pList->nextNode();
	}
	return retVal;
}
#else
XmlRpcValue getXmlRpcValueFromDom(xmlNodePtr pList) {
	XmlRpcValue retVal;
	xmlNodePtr pNode;
	pNode = pList;
	while(pNode) {
		XmlRpcValue ret;
		tstring strName = (char*)pNode->name;
		if (strName == _T("xml")) {
			pNode = pNode->next;
			continue;
		}
		else
		if (strName == _T("methodCall"))
			return getXmlRpcValueFromDom(pNode->children);
		else
		if (strName == _T("methodResponse"))
			return getXmlRpcValueFromDom(pNode->children);
		else
		if (strName == _T("fault"))
			return getXmlRpcValueFromDom(pNode->children);
		else
		if (strName == _T("params"))
			return getXmlRpcValueFromDom(pNode->children);
		else
		if (strName == _T("param"))
			return getXmlRpcValueFromDom(pNode->children);
		else
		if (strName == _T("struct")) {
			xmlNodePtr pMembers;
			pMembers = pNode->children;
			XmlRpcValue::ValueStruct valuestruct;
			while(pMembers) {
				if ("member" == (std::string)(char*)pMembers->name) {
					tstring member_name;
					XmlRpcValue valuedata;
					xmlNodePtr pMember = pMembers->children;
					while(pMember) {
						if ("name" == (std::string)(char*)pMember->name) {
							member_name = (char*)pMember->children->content;
							valuestruct[member_name] = getXmlRpcValueFromDom(pMembers->children);
						}
						pMember = pMember->next;
					}
				}
				pMembers = pMembers->next;
			}
			ret = valuestruct;
		}
		else
		if (strName == _T("array")) {
			xmlNodePtr pDatas;
			pDatas = pNode->children;
			XmlRpcValue::ValueArray valuearray;
			ret = valuearray;
			while(pDatas) {
				if ("data" == (std::string)(char*)pDatas->name) {
					XmlRpcValue datas = getXmlRpcValueFromDom(pDatas->children);
					if (datas.getType() != XmlRpcValue::TypeInvalid)
						ret = datas;
				}
				pDatas = pDatas->next;
			}
		}
		else
		if (strName == _T("value"))
			ret = getXmlRpcValueFromDom(pNode->children);
		else
		if (strName == _T("i4") || strName == _T("int"))
			ret = (int)atol((char*)pNode->children->content);
		else
		if (strName == _T("boolean")) {
			tstring bool_value = tstring((char*)pNode->children->content);
			if (_ttol(bool_value.c_str()))
				ret = true;
			else
				ret = (bool)(bool_value == _T("true"));
		} else
		if (strName == _T("double"))
			ret = (double)atof((char*)pNode->children->content);
		else
		if (strName == _T("string"))
			ret = pNode->children ? (char*)pNode->children->content : _T("");
		else
		if (strName == _T("dateTime.iso8601")) {
			struct tm tmTime = {0};
			tchar num[64];
			_tcscpy(num, (tchar*)pNode->children->content);
			_stscanf(num, _T("%4d-%2d-%2dT%2d:%2d:%2d"),
				&tmTime.tm_year,
				&tmTime.tm_mon,
				&tmTime.tm_mday,
				&tmTime.tm_hour,
				&tmTime.tm_min,
				&tmTime.tm_sec);
			tmTime.tm_isdst = -1;
			ret = tmTime;
		} else
		if (strName == _T("base64")) {
			XmlRpcValue::ValueBinary valuebinary;
			valuebinary = base64_decode_binary((char*)pNode->children->content);
			ret = valuebinary;
		} else
		if (pNode->type == XML_TEXT_NODE && !pNode->next && !pNode->prev) {
			ret = pNode->content ? (char*)pNode->content : "";
		}

		if (ret.getType() != XmlRpcValue::TypeInvalid) {
			if (retVal.getType() != XmlRpcValue::TypeInvalid) {
				if (retVal.getType() == XmlRpcValue::TypeArray)
					retVal[(int)retVal.value.asArray->size()] = ret;
				else {
					XmlRpcValue::ValueArray valuearray;
					valuearray.push_back(retVal);
					valuearray.push_back(ret);
					retVal = valuearray;
				}
			} else
				retVal = ret;
		}
		pNode = pNode->next;
	}
	return retVal;
}
#endif

#ifdef USE_WIN32API
void convertXmlRpcValueToXml(MSXML2::IXMLDOMElementPtr pValue, const XmlRpcValue& param) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc = pValue->ownerDocument;
	MSXML2::IXMLDOMElementPtr pTypeValue;
	MSXML2::IXMLDOMElementPtr pArray, pData;
	MSXML2::IXMLDOMElementPtr pStruct;

	XmlRpcValue::ValueBinary valuebinary;
	XmlRpcValue::ValueBinary::const_iterator itbinary;

	XmlRpcValue::ValueArray valuearray;
	XmlRpcValue::ValueArray::const_iterator itarray;

	XmlRpcValue::ValueStruct valuestruct;
	XmlRpcValue::ValueStruct::const_iterator itstruct;

	tchar buf[64];
	unsigned char *ptr;
	struct tm* tmTime;
	int n;
	switch(param.getType()) {
	case XmlRpcValue::TypeString:
		pTypeValue = pDoc->createElement(_T("string"));
		pTypeValue->text = T2OLE((tchar*)utf8_to_string(param.getString()).c_str());
		pValue->appendChild(pTypeValue);
		pTypeValue.Release();
		break;
	case XmlRpcValue::TypeTime:
		pTypeValue = pDoc->createElement(_T("dateTime.iso8601"));
		tmTime = param.getTime();
		_stprintf(buf, _T("%04d/%02d/%02d %02d:%02d:%02d"),
			tmTime->tm_year+1900,
			tmTime->tm_mon,
			tmTime->tm_mday,
			tmTime->tm_hour,
			tmTime->tm_min,
			tmTime->tm_sec);
		pTypeValue->text = T2OLE(buf);
		pValue->appendChild(pTypeValue);
		break;
	case XmlRpcValue::TypeInt:
		pTypeValue = pDoc->createElement(_T("i4"));
		_stprintf(buf, _T("%d"), param.getInt());
		pTypeValue->text = T2OLE(buf);
		pValue->appendChild(pTypeValue);
		break;
	case XmlRpcValue::TypeDouble:
		pTypeValue = pDoc->createElement(_T("double"));
		_stprintf(buf, _T("%f"), param.getDouble());
		pTypeValue->text = T2OLE(buf);
		pValue->appendChild(pTypeValue);
		break;
	case XmlRpcValue::TypeBoolean:
		pTypeValue = pDoc->createElement(_T("boolean"));
		//pTypeValue->text = param.getBoolean() ? T2OLE(_T("true")) : T2OLE(_T("false"));
		pTypeValue->text = param.getBoolean() ? T2OLE(_T("1")) : T2OLE(_T("0"));
		pValue->appendChild(pTypeValue);
		break;
	case XmlRpcValue::TypeBinary:
		pTypeValue = pDoc->createElement(_T("base64"));
		valuebinary = param.getBinary();
		ptr = new unsigned char[valuebinary.size()];
		for(n = 0, itbinary = valuebinary.begin(); itbinary != valuebinary.end(); n++, itbinary++)
			ptr[n] = *itbinary;
		pTypeValue->text = A2OLE(base64_encode((const unsigned char*)ptr, valuebinary.size()).c_str());
		delete ptr;
		pValue->appendChild(pTypeValue);
		break;
	case XmlRpcValue::TypeArray:
		pArray = pDoc->createElement(_T("array"));
		pData = pDoc->createElement(_T("data"));

		for(itarray = param.value.asArray->begin(); itarray != param.value.asArray->end(); itarray++) {
			MSXML2::IXMLDOMElementPtr pParam, pSubValue;

			pSubValue = pDoc->createElement(_T("value"));
			convertXmlRpcValueToXml(pSubValue, *itarray);
			pData->appendChild(pSubValue);
			pSubValue.Release();
		}

		pArray->appendChild(pData);
		pValue->appendChild(pArray);
		pData.Release();
		pArray.Release();
		break;
	case XmlRpcValue::TypeStruct:
		pStruct = pDoc->createElement(_T("struct"));

		for(itstruct = param.value.asStruct->begin(); itstruct != param.value.asStruct->end(); itstruct++) {
			MSXML2::IXMLDOMElementPtr pMember, pName, pParam, pSubValue;

			pMember = pDoc->createElement(_T("member"));

			pName = pDoc->createElement(_T("name"));
			pName->text = T2OLE((tchar*)itstruct->first.c_str());
			pMember->appendChild(pName);
			pName.Release();

			pSubValue = pDoc->createElement(_T("value"));
			convertXmlRpcValueToXml(pSubValue, itstruct->second);
			pMember->appendChild(pSubValue);
			pSubValue.Release();

			pStruct->appendChild(pMember);
			pMember.Release();
		}

		pValue->appendChild(pStruct);
		pStruct.Release();
		break;
	}
}
#else
void convertXmlRpcValueToXml(xmlNodePtr pValue, const XmlRpcValue& param) {
	xmlDocPtr pDoc = pValue->doc;
	xmlNodePtr pArray, pData;
	xmlNodePtr pStruct;
	xmlNodePtr pSubValue;

	XmlRpcValue::ValueBinary valuebinary;
	XmlRpcValue::ValueBinary::const_iterator itbinary;

	XmlRpcValue::ValueArray valuearray;
	XmlRpcValue::ValueArray::const_iterator itarray;

	XmlRpcValue::ValueStruct valuestruct;
	XmlRpcValue::ValueStruct::const_iterator itstruct;

	tchar buf[64];
	unsigned char *ptr;
	struct tm *tmTime;
	int n;
	switch(param.getType()) {
	case XmlRpcValue::TypeString:
		xmlNewTextChild(pValue, NULL, (xmlChar*)"string", (xmlChar*)param.getString().c_str());
		break;
	case XmlRpcValue::TypeTime:
		tmTime = param.getTime();
		_stprintf(buf, _T("%04d/%02d/%02d %02d:%02d:%02d"),
			tmTime->tm_year+1900,
			tmTime->tm_mon,
			tmTime->tm_mday,
			tmTime->tm_hour,
			tmTime->tm_min,
			tmTime->tm_sec);
		xmlNewTextChild(pValue, NULL, (xmlChar*)"dateTime.iso8601", (xmlChar*)buf);
		break;
	case XmlRpcValue::TypeInt:
		_stprintf(buf, _T("%d"), param.getInt());
		xmlNewTextChild(pValue, NULL, (xmlChar*)"i4", (xmlChar*)buf);
		break;
	case XmlRpcValue::TypeDouble:
		_stprintf(buf, _T("%f"), param.getDouble());
		xmlNewTextChild(pValue, NULL, (xmlChar*)"double", (xmlChar*)buf);
		break;
	case XmlRpcValue::TypeBoolean:
		xmlNewTextChild(pValue, NULL, (xmlChar*)"boolean", param.getBoolean() ? (xmlChar*)"true" : (xmlChar*)"false");
		break;
	case XmlRpcValue::TypeBinary:
		valuebinary = param.getBinary();
		ptr = new unsigned char[valuebinary.size()];
		for(n = 0, itbinary = valuebinary.begin(); itbinary != valuebinary.end(); n++, itbinary++)
			ptr[n] = *itbinary;
		xmlNewTextChild(pValue, NULL, (xmlChar*)"base64", (xmlChar*)base64_encode((const unsigned char*)ptr, valuebinary.size()).c_str());
		delete ptr;
		break;
	case XmlRpcValue::TypeArray:
		pArray = xmlNewChild(pValue, NULL, (xmlChar*)"array", NULL);
		pData = xmlNewChild(pArray, NULL, (xmlChar*)"data", NULL);
		for(itarray = param.value.asArray->begin(); itarray != param.value.asArray->end(); itarray++) {
			pSubValue = xmlNewChild(pData, NULL, (xmlChar*)"value", NULL);
			convertXmlRpcValueToXml(pSubValue, *itarray);
		}
		break;
	case XmlRpcValue::TypeStruct:
		pStruct = xmlNewChild(pValue, NULL, (xmlChar*)"struct", NULL);
		for(itstruct = param.value.asStruct->begin(); itstruct != param.value.asStruct->end(); itstruct++) {
			xmlNodePtr pMember, pSubValue;
			pMember = xmlNewChild(pStruct, NULL, (xmlChar*)"member", NULL);
			xmlNewTextChild(pMember, NULL, (xmlChar*)"name", (xmlChar*)itstruct->first.c_str());
			pSubValue = xmlNewChild(pMember, NULL, (xmlChar*)"value", NULL);
			convertXmlRpcValueToXml(pSubValue, itstruct->second);
		}
		break;
	}
}
#endif

#ifdef USE_WIN32API
bool isFaultResult(tstring& strXml) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMNodeListPtr pList;
	bool ret = false;
	try {
		pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
		pDoc->loadXML(T2OLE((tchar*)strXml.c_str()));
		pList = pDoc->selectNodes(_T("methodResponse/fault"));
		ret = pList->nextNode() != NULL;
	} catch(...) {
	}
	if (pList) pList.Release();
	if (pDoc) pDoc.Release();
	return ret;
}
#else
bool isFaultResult(tstring& strXml) {
	xmlDocPtr pDoc;
	xmlNodePtr pMethodResponse, pFault;
	bool ret = false;
	pDoc = xmlParseDoc((xmlChar*)strXml.c_str());
	pMethodResponse = pDoc->children;
	while(pMethodResponse) {
		if ("methodResponse" == (std::string)(char*)pMethodResponse->name) {
			pFault = pMethodResponse->children;
			while(pFault) {
				if ("fault" == (std::string)(char*)pFault->name) {
					ret = true;
					break;
				}
				pFault = pFault->next;
			}
			if (ret) break;
		}
		pMethodResponse = pMethodResponse->next;
	}
	xmlFreeDoc(pDoc);
	return ret;
}
#endif
bool isFaultResult(XmlRpcValue& res) {
	if (res.getType() == XmlRpcValue::TypeStruct) {
		if (res.hasMember(_T("faultString")) && res.hasMember(_T("faultCode")))
			return true;
	} else
	if (res.getType() == XmlRpcValue::TypeArray) {
		if (res.size() > 1 && res[0].getType() == XmlRpcValue::TypeStruct) {
			if (res[0].hasMember(_T("faultString")) && res.hasMember(_T("faultCode")))
				return true;
		}
	}
	return false;
}

#ifdef USE_WIN32API
tstring getMethodFromXml(std::string& strXml) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMNodePtr pNode;
	tstring ret = _T("");
	try {
		pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
		pDoc->loadXML(A2OLE((char*)utf8_to_string(strXml).c_str()));
		pNode = pDoc->selectSingleNode(_T("methodCall/methodName"));
		ret = OLE2T(pNode->text);
	} catch(_com_error& /*e*/) {
		//std::wcerr << e.ErrorMessage() << std::endl;
	} catch(...) {
	}
	if (pNode) pNode.Release();
	if (pDoc) pDoc.Release();
	return ret;
}
#else
tstring getMethodFromXml(std::string& strXml) {
	xmlDocPtr pDoc;
	xmlNodePtr pMethodCall, pMethodName;
	tstring ret = _T("");
	pDoc = xmlParseDoc((xmlChar*)strXml.c_str());
	pMethodCall = pDoc->children;
	while(pMethodCall) {
		if ("methodCall" == (std::string)(char*)pMethodCall->name) {
			pMethodName = pMethodCall->children;
			while(pMethodName) {
				if ("methodName" == (std::string)(char*)pMethodName->name) {
					ret = (char*)pMethodName->children->content;
					break;
				}
				pMethodName = pMethodName->next;
			}
			if (ret != "") break;
		}
		pMethodCall = pMethodCall->next;
	}
	xmlFreeDoc(pDoc);
	return ret;
}
#endif
#ifdef USE_WIN32API
XmlRpcValue getXmlRpcValueFromXml(std::string& strXml) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	XmlRpcValue res;
	std::string faultString;
	try {
		pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
		pDoc->async = FALSE;
		pDoc->validateOnParse = TRUE;
#if 1
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s\n", utf8_to_string(strXml).c_str());
		fclose(fp);
#endif
		BOOL ret = pDoc->loadXML(A2OLE(utf8_to_string(strXml).c_str()));
#if 0
		if (ret == FALSE) {
			MSXML2::IXMLDOMParseErrorPtr pParseError;
			pParseError = pDoc->parseError;
			std::string responseXml = getXmlFromException(XmlRpcException(pParseError->errorCode, OLE2T(pParseError->reason)));
			res = getXmlRpcValueFromXml(responseXml);
			return res;
		}
#endif
		res = getXmlRpcValueFromDom(pDoc->childNodes);
	} catch(_com_error &e) {
		e;
#if 0
		std::string responseXml = getXmlFromException(XmlRpcException(e.WCode(), e.ErrorMessage()));
		res = getXmlRpcValueFromXml(responseXml);
#endif
	} catch(...) {
	}
	if (res.getType() == XmlRpcValue::TypeInvalid) {
		std::string responseXml = getXmlFromException(XmlRpcException(4, _T("Invalid Server Response")));
		res = getXmlRpcValueFromXml(responseXml);
	}
	if (pDoc) pDoc.Release();
	return res;
}
#else
XmlRpcValue getXmlRpcValueFromXml(std::string& strXml) {
	xmlDocPtr pDoc;
	XmlRpcValue res;
	pDoc = xmlParseDoc((xmlChar*)strXml.c_str());
	if (pDoc) {
		res = getXmlRpcValueFromDom(pDoc->children);
		xmlFreeDoc(pDoc);
	}
	return res;
}
#endif

#ifdef USE_WIN32API
std::string getXmlFromRequests(tstring method, std::vector<XmlRpcValue>& requests, tstring encoding) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMNodePtr pNode;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pMethodCall, pMethodName, pParams;
	MSXML2::IXMLDOMElementPtr pParam, pValue;
	BSTR bstrXml;

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));

	tstring xmlenc;
	xmlenc += TEXT("version=\"1.0\" encoding=\"");
	xmlenc += encoding;
	xmlenc += TEXT("\"");
	pInst = pDoc->createProcessingInstruction(_T("xml"), xmlenc.c_str());
	pDoc->appendChild(pInst);

	pMethodCall = pDoc->createElement(_T("methodCall"));
	pDoc->appendChild(pMethodCall);

	pMethodName = pDoc->createElement(_T("methodName"));
	pMethodName->text = T2OLE((tchar*)method.c_str());
	pMethodCall->appendChild(pMethodName);

	pParams = pDoc->createElement(_T("params"));
	pMethodCall->appendChild(pParams);

	std::vector<XmlRpcValue>::const_iterator it;
	for(it = requests.begin(); it != requests.end(); it++) {
		pParam = pDoc->createElement(_T("param"));
		pValue = pDoc->createElement(_T("value"));
		convertXmlRpcValueToXml(pValue, *it);
		pParam->appendChild(pValue);
		pParams->appendChild(pParam);
		pValue.Release();
		pParam.Release();
	}

	pParams.Release();
	pMethodName.Release();
	pMethodCall.Release();
	pInst.Release();

	pDoc->get_xml(&bstrXml);
	pDoc.Release();

	std::string strXml = OLE2A(bstrXml);
	SysFreeString(bstrXml);
	if (encoding == _T("utf-8")) {
		int nPos = strXml.find_first_of(">");
		std::string strSjisXml;
		strSjisXml = strXml.substr(0, nPos-1);
		strSjisXml += " encoding=\"utf-8\"";
		strSjisXml += strXml.substr(nPos-1);
		strXml = string_to_utf8(strSjisXml);
	}
#if 1
	FILE *fp = fopen("req.xml", "wb");
	fprintf(fp, "%s\n", strXml.c_str());
	fclose(fp);
#endif

	return strXml;
}
#else
std::string getXmlFromRequests(tstring method, std::vector<XmlRpcValue>& requests, tstring encoding) {
	xmlDocPtr pDoc;
	xmlNodePtr pMethodCall, pParams;
	xmlNodePtr pParam, pValue;

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pMethodCall = xmlNewNode(NULL, (xmlChar*)"methodCall");
	xmlDocSetRootElement(pDoc, pMethodCall);
	xmlNewTextChild(pMethodCall, NULL, (xmlChar*)"methodName", (xmlChar*)method.c_str());
	pParams = xmlNewChild(pMethodCall, NULL, (xmlChar*)"params", NULL);

	std::vector<XmlRpcValue>::const_iterator it;
	for(it = requests.begin(); it != requests.end(); it++) {
		pParam = xmlNewChild(pParams, NULL, (xmlChar*)"param", NULL);
		pValue = xmlNewChild(pParam, NULL, (xmlChar*)"value", NULL);
		convertXmlRpcValueToXml(pValue, *it);
	}

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, encoding.c_str(), 0);
	xmlFreeDoc(pDoc);
	tstring strXml = (char*)pstrXml;
	xmlFree(pstrXml);
	return strXml;
}
#endif

#ifdef USE_WIN32API
std::string getXmlFromResponse(XmlRpcValue& response, tstring encoding) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pMethodResponse, pParams;
	MSXML2::IXMLDOMElementPtr pParam, pValue;
	BSTR bstrXml;

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));

	tstring xmlenc;
	xmlenc += _T("version=\"1.0\" encoding=\"");
	xmlenc += encoding;
	xmlenc += _T("\"");
	pInst = pDoc->createProcessingInstruction(_T("xml"), xmlenc.c_str());
	pDoc->appendChild(pInst);

	pMethodResponse = pDoc->createElement(_T("methodResponse"));
	pDoc->appendChild(pMethodResponse);

	pParams = pDoc->createElement(_T("params"));
	pMethodResponse->appendChild(pParams);

	pParam = pDoc->createElement(_T("param"));
	pValue = pDoc->createElement(_T("value"));
	convertXmlRpcValueToXml(pValue, response);
	pParam->appendChild(pValue);
	pParams->appendChild(pParam);
	pValue.Release();
	pParam.Release();

	pParams.Release();
	pMethodResponse.Release();
	pInst.Release();

	pDoc->get_xml(&bstrXml);
	pDoc->save(T2OLE(_T("req.xml")));
	pDoc.Release();

	std::string strXml = OLE2A(bstrXml);
	SysFreeString(bstrXml);
	if (encoding == _T("utf-8")) {
		int nPos = strXml.find_first_of(">");
		std::string strSjisXml;
		strSjisXml = strXml.substr(0, nPos-1);
		strSjisXml += " encoding=\"utf-8\"";
		strSjisXml += strXml.substr(nPos-1);
		strXml = string_to_utf8(strSjisXml);
	}

	return strXml;
}
#else
std::string getXmlFromResponse(XmlRpcValue& response, tstring encoding) {
	xmlDocPtr pDoc;
	xmlNodePtr pMethodResponse, pParams;
	xmlNodePtr pParam, pValue;

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pMethodResponse = xmlNewNode(NULL, (xmlChar*)"methodResponse");
	xmlDocSetRootElement(pDoc, pMethodResponse);
	pParams = xmlNewChild(pMethodResponse, NULL, (xmlChar*)"params", NULL);

	pParam = xmlNewChild(pParams, NULL, (xmlChar*)"param", NULL);
	pValue = xmlNewChild(pParam, NULL, (xmlChar*)"value", NULL);
	convertXmlRpcValueToXml(pValue, response);

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, encoding.c_str(), 0);
	xmlFreeDoc(pDoc);
	tstring strXml = (char*)pstrXml;
	xmlFree(pstrXml);
	return strXml;
}
#endif

#ifdef USE_WIN32API
std::string getXmlFromXmlRpcValue(XmlRpcValue& response, tstring encoding) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pValue;
	BSTR bstrXml;

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));

	tstring xmlenc;
	xmlenc += _T("version=\"1.0\" encoding=\"");
	xmlenc += encoding;
	xmlenc += _T("\"");
	pInst = pDoc->createProcessingInstruction(_T("xml"), xmlenc.c_str());
	pDoc->appendChild(pInst);

	pValue = pDoc->createElement(_T("value"));
	convertXmlRpcValueToXml(pValue, response);
	pDoc->appendChild(pValue);
	pValue.Release();

	pInst.Release();

	pDoc->get_xml(&bstrXml);
	pDoc->save(T2OLE(_T("req.xml")));
	pDoc.Release();

	std::string strXml = OLE2A(bstrXml);
	SysFreeString(bstrXml);
	if (encoding == _T("utf-8")) {
		int nPos = strXml.find_first_of(">");
		std::string strSjisXml;
		strSjisXml = strXml.substr(0, nPos-1);
		strSjisXml += " encoding=\"utf-8\"";
		strSjisXml += strXml.substr(nPos-1);
		strXml = string_to_utf8(strSjisXml);
	}

	return strXml;
}
#else
std::string getXmlFromXmlRpcValue(XmlRpcValue& response, tstring encoding) {
	xmlDocPtr pDoc;
	xmlNodePtr pValue;

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pValue = xmlNewNode(NULL, (xmlChar*)"value");
	xmlDocSetRootElement(pDoc, pValue);
	convertXmlRpcValueToXml(pValue, response);

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, encoding.c_str(), 0);
	xmlFreeDoc(pDoc);
	tstring strXml = (char*)pstrXml;
	xmlFree(pstrXml);
	return strXml;
}
#endif

#ifdef USE_WIN32API
std::string getXmlFromException(XmlRpcException e) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pMethodResponse, pFault, pStruct, pMember;
	MSXML2::IXMLDOMElementPtr pName, pValue, pSubValue;
	BSTR bstrXml;

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));

	pInst = pDoc->createProcessingInstruction(_T("xml"), _T("version=\"1.0\" encoding=\"utf-8\""));
	pDoc->appendChild(pInst);

	pMethodResponse = pDoc->createElement(_T("methodResponse"));
	pDoc->appendChild(pMethodResponse);

	pFault = pDoc->createElement(_T("fault"));
	pMethodResponse->appendChild(pFault);

	pValue = pDoc->createElement(_T("value"));
	pFault->appendChild(pValue);

	pStruct = pDoc->createElement(_T("struct"));
	pValue->appendChild(pStruct);

	pMember = pDoc->createElement(_T("member"));
	pStruct->appendChild(pMember);

	pName = pDoc->createElement(_T("name"));
	pName->text = _T("faultString");
	pMember->appendChild(pName);
	pName.Release();

	pSubValue = pDoc->createElement(_T("value"));
	pSubValue->text = T2OLE((tchar*)e.message.c_str());
	pMember->appendChild(pSubValue);
	pSubValue.Release();

	pMember.Release();
	pMember = pDoc->createElement(_T("member"));
	pStruct->appendChild(pMember);

	pName = pDoc->createElement(_T("name"));
	pName->text = _T("faultCode");
	pMember->appendChild(pName);
	pName.Release();

	pSubValue = pDoc->createElement(_T("value"));
	tchar buf[16];
	_stprintf(buf, _T("%d"), e.code);
	pSubValue->text = T2OLE(buf);
	pMember->appendChild(pSubValue);
	pSubValue.Release();

	pMember.Release();
	pStruct.Release();
	pValue.Release();
	pFault.Release();
	pMethodResponse.Release();
	pInst.Release();

	pDoc->get_xml(&bstrXml);
	pDoc.Release();

	std::string strXml = OLE2A(bstrXml);
	SysFreeString(bstrXml);

	int nPos = strXml.find_first_of(">");
	if (nPos != std::string::npos) {
		std::string strSjisXml;
		strSjisXml = strXml.substr(0, nPos-1);
		strSjisXml += " encoding=\"utf-8\"";
		strSjisXml += strXml.substr(nPos-1);
		strXml = string_to_utf8(strSjisXml);
	}

	return strXml;
}
#else
std::string getXmlFromException(XmlRpcException e) {
	xmlDocPtr pDoc;
	xmlNodePtr pMethodResponse, pFault, pValue, pStruct, pMember;

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pMethodResponse = xmlNewNode(NULL, (xmlChar*)"methodResponse");
	xmlDocSetRootElement(pDoc, pMethodResponse);
	pFault = xmlNewChild(pMethodResponse, NULL, (xmlChar*)"fault", NULL);
	pValue = xmlNewChild(pFault, NULL, (xmlChar*)"value", NULL);
	pStruct = xmlNewChild(pValue, NULL, (xmlChar*)"struct", NULL);

	pMember = xmlNewChild(pStruct, NULL, (xmlChar*)"member", NULL);
	xmlNewTextChild(pMember, NULL, (xmlChar*)"name", (xmlChar*)"faultString");
	xmlNewTextChild(pMember, NULL, (xmlChar*)"value", (xmlChar*)e.message.c_str());

	pMember = xmlNewChild(pStruct, NULL, (xmlChar*)"member", NULL);
	xmlNewTextChild(pMember, NULL, (xmlChar*)"name", (xmlChar*)"faultCode");
	tchar buf[16];
	_stprintf(buf, _T("%d"), e.code);
	xmlNewTextChild(pMember, NULL, (xmlChar*)"value", (xmlChar*)buf);

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, "utf-8", 0);
	xmlFreeDoc(pDoc);
	tstring strXml = (char*)pstrXml;
	xmlFree(pstrXml);
	return strXml;
}
#endif

#ifdef USE_WIN32API
int postXmlRpc(tstring url, tstring userid, tstring passwd, tstring method, std::string requestXml, std::string& responseXml, tstring user_agent) {
	HINTERNET hSession = 0;
	HINTERNET hConnect = 0;
	HINTERNET hRequest = 0;
	tchar szHeader[] = _T("Content-Type: text/xml\n");
	tchar szUrl[2048], *pszUrl;
	tchar szBuf[1024];
	CHAR szData[1024];
	DWORD dwSize = 0;
	std::string strData;
	DWORD dwIndex = 0;
	DWORD dwContentLength = -1;
	DWORD dwStatusCode = 200;
	DWORD dwFlags = 0;

	short port = 80;
	tstring path;
	_tcscpy(szUrl, url.c_str());
	if (!_tcsncmp(szUrl, _T("https:"), 6)) port = 443;
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) return -1;
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) return -1;
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = _ttol(pszUrl+1);
	}
	tstring host = szUrl;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		DWORD dwError = GetLastError();
		responseXml = getXmlFromException(XmlRpcException(dwError, getErrMessage(dwError)));
		return -2;
	}

	DWORD dwTimeout = 30 * 1000;
	BOOL bRet;

	bRet = InternetSetOption(
			hSession,
			INTERNET_OPTION_RECEIVE_TIMEOUT,
			&dwTimeout,
			sizeof(dwTimeout));

	hConnect = InternetConnect(
			hSession,
			host.c_str(),
			port,
			userid.size() ? userid.c_str() : NULL,
			passwd.size() ? passwd.c_str() : NULL,
			INTERNET_SERVICE_HTTP,
			0,
			0);
	if (hConnect == NULL) {
		DWORD dwError = GetLastError();
		responseXml = getXmlFromException(XmlRpcException(dwError, getErrMessage(dwError)));
		InternetCloseHandle(hSession);
		return -2;
	}

	/*
	bRet = InternetSetOption(
		hConnect,
		INTERNET_OPTION_USERNAME,
		(LPVOID)(LPCTSTR)userid.c_str(),
		userid.size());

	bRet = InternetSetOption(
		hConnect,
		INTERNET_OPTION_PASSWORD,
		(LPVOID)(LPCTSTR)passwd.c_str(),
		passwd.size());
	*/

	if (port == 443) {
		hRequest = HttpOpenRequest(
				hConnect,
				_T("POST"),
				path.c_str(),
				NULL,
				NULL,
				NULL,
				INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE,
				0);
	} else {
		hRequest = HttpOpenRequest(
				hConnect,
				_T("POST"),
				path.c_str(),
				NULL,
				NULL,
				NULL,
				INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD,
				0);
	}
	if (hRequest == NULL) {
		DWORD dwError = GetLastError();
		responseXml = getXmlFromException(XmlRpcException(dwError, getErrMessage(dwError)));
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hSession);
		return -2;
	}

	if (HttpSendRequest(
				hRequest,
				szHeader,
				_tcslen(szHeader),
				(void*)requestXml.c_str(),
				requestXml.size()) == FALSE) {
		DWORD dwError = GetLastError();
		responseXml = getXmlFromException(XmlRpcException(dwError, getErrMessage(dwError)));
		InternetCloseHandle(hRequest);
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hSession);
		return -2;
	}

	szBuf[0] = 0;
	dwSize = sizeof(szBuf)-1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, &szBuf, &dwSize, &dwIndex) == TRUE) {
		dwStatusCode = _ttol(szBuf);
	}

	szBuf[0] = 0;
	dwSize = sizeof(szBuf)-1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, &szBuf, &dwSize, &dwIndex) == TRUE) {
		dwContentLength = _ttol(szBuf);
	}

	strData = "";
	if (dwStatusCode == 200 || dwStatusCode == 201) {
		for (;;) {
			dwSize = sizeof(szData)-1;
			if (InternetReadFile(
					hRequest,
					szData,
					dwSize,
					&dwSize) == FALSE) break;
			if (dwSize <= 0) break;
			szData[dwSize] = 0;
			strData += szData;
			if (strData.size() > dwContentLength)
				break;
		}
		responseXml = strData;
	}
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "w");
		fprintf(fp, "%s\n", responseXml.c_str());
		fclose(fp);
	}
#endif

	InternetCloseHandle(hRequest);
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hSession);
	return 0;
}
#else
static std::string responseBuf;
static int handle_returned_data(char *ptr, size_t size, size_t nmemb, void *stream) {
	char* pbuf = new char[size*nmemb+1];
	memcpy(pbuf, ptr, size*nmemb);
	pbuf[size*nmemb] = 0;
	responseBuf += pbuf;
	delete[] pbuf;
	return size*nmemb;
}
int postXmlRpc(tstring url, tstring userid, tstring passwd, tstring method, std::string requestXml, std::string& responseXml, tstring user_agent) {
	CURL *curl;
	CURLcode res;
	char szHeader[] = _T("Content-Type: text/xml");
	struct curl_slist *headerlist=NULL;

	curl = curl_easy_init();
	headerlist = curl_slist_append(headerlist, szHeader);
	responseBuf = "";
	responseXml = "";
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestXml.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestXml.size());
		curl_easy_setopt(curl, CURLOPT_TRANSFERTEXT, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		curl_slist_free_all (headerlist);
		if (res == CURLE_OK)
			responseXml = responseBuf;
		else 
			responseXml = getXmlFromException(XmlRpcException(res, curl_easy_strerror(res)));
#if 1
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s\n", responseXml.c_str());
		fclose(fp);
#endif
	}
	responseBuf = "";
	return 0;
}
#endif

tstring getFaultStringFromXml(tstring& strXml) {
	tstring ret;
	ret = strXml;
	ret.erase(std::remove(ret.begin(), ret.end(), '\r'), ret.end());
	std::replace(ret.begin(), ret.end(), '\t', ' ');
	while (true) {
		int iPos1 = ret.find_first_of('<');
		int iPos2 = ret.find_first_of('>');
		if (iPos1 == tstring::npos || iPos2 == tstring::npos) break;
		ret.erase(iPos1, iPos2-iPos1+1);
		if (ret[iPos1] == '\n' || ret[iPos1] == ' ') {
			iPos2 = ret.substr(iPos1).find_first_not_of(_T("\t \n"));
			ret.erase(iPos1, iPos2);
		} else
		if (iPos1 > 0 && ret[iPos1-1] != '\n' && ret[iPos1-1] != ' ')
			ret.insert(iPos1, _T(" "));
	}
	return ret;
}

tstring getFaultResult(XmlRpcValue& res) {
	if (res.getType() == XmlRpcValue::TypeStruct) {
		if (res.hasMember(_T("faultString")) && res.hasMember(_T("faultCode"))) {
			tstring msg;
			msg += res[_T("faultCode")].valueString();
			msg += _T(" : ");
			msg += res[_T("faultString")].valueString();
			return msg;
		}
	} else
	if (res.getType() == XmlRpcValue::TypeArray) {
		if (res.size() > 1 && res[0].getType() == XmlRpcValue::TypeStruct) {
			if (res[0].hasMember(_T("faultString")) && res.hasMember(_T("faultCode"))) {
				tstring msg = res[0][_T("faultCode")];
				msg += _T(" : ");
				msg += res[0][_T("faultString")].valueString();
				return msg;
			}
		}
	}
	return _T("Unknown Error");
}

#ifdef USE_WIN32API
std::string queryXml(std::string& strXml, std::string query, std::string defaultNs) {
	USES_CONVERSION;
	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMNodePtr pNode;
	tstring ret = _T("");
	try {
		pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
		pDoc->loadXML(A2OLE((char*)utf8_to_string(strXml).c_str()));
		pNode = pDoc->selectSingleNode(_T(query.c_str()));
		ret = OLE2T(pNode->text);
	} catch(_com_error& /*e*/) {
		//std::wcerr << e.ErrorMessage() << std::endl;
	} catch(...) {
	}
	if (pNode) pNode.Release();
	if (pDoc) pDoc.Release();
	return ret;
}
#else
std::string queryXml(std::string& strXml, std::string query, std::string defaultNs) {
	xmlDocPtr pDoc = xmlParseDoc((xmlChar*)strXml.c_str());
	xmlAttrPtr pAttr = NULL;
	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr xpathObj=NULL;
	xmlNodeSetPtr nodes;
	std::string ret;

	int n;
	if (!pDoc) goto leave;

	xpathCtx = xmlXPathNewContext(pDoc);
	if(!xpathCtx) goto leave;

	if (defaultNs.size())
		xmlXPathRegisterNs(xpathCtx, (const xmlChar*)"default", (xmlChar*)defaultNs.c_str());

	xpathObj = xmlXPathEvalExpression((xmlChar*)query.c_str(), xpathCtx);
	if(!xpathObj) {
		goto leave;
	}

	nodes = xpathObj->nodesetval;
	for(n = 0; n < xmlXPathNodeSetGetLength(nodes); n++) {
		xmlNodePtr node = nodes->nodeTab[n];
		if(node->type != XML_ATTRIBUTE_NODE && node->type != XML_ELEMENT_NODE && node->type != XML_CDATA_SECTION_NODE) continue;
		if (node->type == XML_CDATA_SECTION_NODE)
			ret = (char*)node->content;
		else
		if (node->children)
			ret = (char*)node->children->content;
		break;
	}

leave:
	if (xpathObj) xmlXPathFreeObject(xpathObj);
    if (xpathCtx) xmlXPathFreeContext(xpathCtx);
	if (pDoc) xmlFreeDoc(pDoc);
	return ret;
}
#endif

XmlRpcValue::ValueBinary loadBinaryFromFile(tstring filename) {
#ifdef _MSC_VER
	struct _stat statbuf = {0};
#else
	struct stat statbuf = {0};
#endif
	if (_tstat(filename.c_str(), &statbuf) != -1) {
		XmlRpc::XmlRpcValue::ValueBinary valuebinary;
		FILE *fp = _tfopen(filename.c_str(), _T("rb"));
		char *ptr = new char[statbuf.st_size];
		fread(ptr, statbuf.st_size, 1, fp);
		fclose(fp);
		valuebinary.reserve(statbuf.st_size);
		for(int n = 0; n < statbuf.st_size; n++)
			valuebinary.push_back(ptr[n]);
		delete[] ptr;
		return valuebinary;
	}
	XmlRpcValue empty;
	return empty;
}

bool saveBinaryToFile(tstring filename, XmlRpcValue::ValueBinary valuebinary) {
	FILE *fp = _tfopen(filename.c_str(), _T("wb"));
	if (!fp) return false;
	XmlRpc::XmlRpcValue::ValueBinary::const_iterator it;
	for(it = valuebinary.begin(); it != valuebinary.end(); it++) {
		char c = *it;
		fwrite(&c, 1, 1, fp);
	}
	fclose(fp);
	return true;
}

int execXmlRpc(tstring url, tstring method, std::vector<XmlRpcValue>& requests, XmlRpcValue& response, tstring userid, tstring passwd, tstring encoding, tstring user_agent) {
	response.clear();

	int result = 0;
	std::string responseXml;
	result = postXmlRpc(url, userid, passwd, method, getXmlFromRequests(method, requests), responseXml, user_agent);

	if (responseXml.size()) {
		response = getXmlRpcValueFromXml(responseXml);
		if (isFaultResult(response)) {
			result = -2;
		}
	} else
		result = -3;

	return result;
}

std::string urldecode(const std::string& url) {
	std::ostringstream rets;
	std::string hexstr;
	for(int n = 0; n < url.size(); n++) {
		switch(url[n]) {
		case '+':
			rets << ' ';
			break;
		case '%':
			hexstr = url.substr(n+1, 2);
			n += 2;
			if (hexstr == "26" || hexstr == "3D")
				rets << '%' << hexstr;
			else {
				std::istringstream hexstream(hexstr);
				int hexint;
				hexstream >> std::hex >> hexint;
				rets << static_cast<char>(hexint);
			}
			break;
		default:
			rets << url[n];
			break;
		}
	}
	return rets.str();
}

std::string urlencode(const std::string& url) {
	std::ostringstream rets;
	for(int n = 0; n < url.size(); n++) { 
		unsigned char c = (unsigned char)url[n];
		if (isalnum(c) || c == '_' || c == '.' || c == '/' )
			rets << c;
		else {
			char buf[8];
			sprintf(buf, "%02x", (int)c);
			rets << '%' << buf[0] << buf[1];
		}
	}
	return rets.str();
}

const std::string html_encode(const std::string& html) {
	std::string ret = html;
	replace_string(ret, "&", "&amp;");
	replace_string(ret, "<", "&lt;");
	replace_string(ret, ">", "&gt;");
	return ret;
}

const std::string html_decode(const std::string& html) {
	std::string ret = html;
	replace_string(ret, "&gt;", ">");
	replace_string(ret, "&lt;", "<");
	replace_string(ret, "&nbsp;", " ");
	replace_string(ret, "&amp;", "&");
	return ret;
}
}
