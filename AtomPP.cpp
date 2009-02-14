#define _CRT_SECURE_NO_DEPRECATE

#ifdef _MSC_VER
//# define USE_WIN32API
#pragma warning(disable : 4530 4018 4786)
#endif

#ifdef USE_WIN32API
#include <windows.h>
#include <wininet.h>
#include <mlang.h>
#include <atlbase.h>
#import "msxml4.dll" named_guids
#else
#include <libxml/parser.h>
#include <curl/curl.h>
#include <errno.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "AtomPP.h"
#include "XmlRpcUtils.h"
#include <sstream>
#include <fstream>

namespace AtomPP {

#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif

#ifndef uint64
# ifdef _WIN32
#  ifdef __GNUC__
#   define uint64 unsigned __int64
#  else
#   define uint64 unsigned _int64
#  endif
# else
#  define uint64 unsigned long long
# endif
#endif

#ifndef byte
# define byte unsigned char
#endif

typedef struct
{
	uint32 total[2];
	uint32 state[5];
	uint8 buffer[64];
} sha1_context;

#define GET_UINT32(n,b,i)                       \
{                                               \
	(n) = ( (uint32) (b)[(i)    ] << 24 )       \
	| ( (uint32) (b)[(i) + 1] << 16 )       \
	| ( (uint32) (b)[(i) + 2] <<  8 )       \
	| ( (uint32) (b)[(i) + 3]       );      \
}

#define PUT_UINT32(n,b,i)                       \
{                                               \
	(b)[(i)    ] = (uint8) ( (n) >> 24 );       \
	(b)[(i) + 1] = (uint8) ( (n) >> 16 );       \
	(b)[(i) + 2] = (uint8) ( (n) >>  8 );       \
	(b)[(i) + 3] = (uint8) ( (n)       );       \
}

void sha1_starts( sha1_context *ctx )
{
	ctx->total[0] = 0;
	ctx->total[1] = 0;

	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
}

void sha1_process( sha1_context *ctx, uint8 data[64] )
{
	uint32 temp, W[16], A, B, C, D, E;

	GET_UINT32( W[0],  data,  0 );
	GET_UINT32( W[1],  data,  4 );
	GET_UINT32( W[2],  data,  8 );
	GET_UINT32( W[3],  data, 12 );
	GET_UINT32( W[4],  data, 16 );
	GET_UINT32( W[5],  data, 20 );
	GET_UINT32( W[6],  data, 24 );
	GET_UINT32( W[7],  data, 28 );
	GET_UINT32( W[8],  data, 32 );
	GET_UINT32( W[9],  data, 36 );
	GET_UINT32( W[10], data, 40 );
	GET_UINT32( W[11], data, 44 );
	GET_UINT32( W[12], data, 48 );
	GET_UINT32( W[13], data, 52 );
	GET_UINT32( W[14], data, 56 );
	GET_UINT32( W[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define R(t)                                            \
	(                                                       \
															temp = W[(t -  3) & 0x0F] ^ W[(t - 8) & 0x0F] ^     \
															W[(t - 14) & 0x0F] ^ W[ t      & 0x0F],      \
															( W[t & 0x0F] = S(temp,1) )                         \
	)

#define P(a,b,c,d,e,x)                                  \
	{                                                       \
		e += S(a,5) + F(b,c,d) + K + x; b = S(b,30);        \
	}

	A = ctx->state[0];
	B = ctx->state[1];
	C = ctx->state[2];
	D = ctx->state[3];
	E = ctx->state[4];

#define F(x,y,z) (z ^ (x & (y ^ z)))
#define K 0x5A827999

	P( A, B, C, D, E, W[0]  );
	P( E, A, B, C, D, W[1]  );
	P( D, E, A, B, C, W[2]  );
	P( C, D, E, A, B, W[3]  );
	P( B, C, D, E, A, W[4]  );
	P( A, B, C, D, E, W[5]  );
	P( E, A, B, C, D, W[6]  );
	P( D, E, A, B, C, W[7]  );
	P( C, D, E, A, B, W[8]  );
	P( B, C, D, E, A, W[9]  );
	P( A, B, C, D, E, W[10] );
	P( E, A, B, C, D, W[11] );
	P( D, E, A, B, C, W[12] );
	P( C, D, E, A, B, W[13] );
	P( B, C, D, E, A, W[14] );
	P( A, B, C, D, E, W[15] );
	P( E, A, B, C, D, R(16) );
	P( D, E, A, B, C, R(17) );
	P( C, D, E, A, B, R(18) );
	P( B, C, D, E, A, R(19) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0x6ED9EBA1

	P( A, B, C, D, E, R(20) );
	P( E, A, B, C, D, R(21) );
	P( D, E, A, B, C, R(22) );
	P( C, D, E, A, B, R(23) );
	P( B, C, D, E, A, R(24) );
	P( A, B, C, D, E, R(25) );
	P( E, A, B, C, D, R(26) );
	P( D, E, A, B, C, R(27) );
	P( C, D, E, A, B, R(28) );
	P( B, C, D, E, A, R(29) );
	P( A, B, C, D, E, R(30) );
	P( E, A, B, C, D, R(31) );
	P( D, E, A, B, C, R(32) );
	P( C, D, E, A, B, R(33) );
	P( B, C, D, E, A, R(34) );
	P( A, B, C, D, E, R(35) );
	P( E, A, B, C, D, R(36) );
	P( D, E, A, B, C, R(37) );
	P( C, D, E, A, B, R(38) );
	P( B, C, D, E, A, R(39) );

#undef K
#undef F

#define F(x,y,z) ((x & y) | (z & (x | y)))
#define K 0x8F1BBCDC

	P( A, B, C, D, E, R(40) );
	P( E, A, B, C, D, R(41) );
	P( D, E, A, B, C, R(42) );
	P( C, D, E, A, B, R(43) );
	P( B, C, D, E, A, R(44) );
	P( A, B, C, D, E, R(45) );
	P( E, A, B, C, D, R(46) );
	P( D, E, A, B, C, R(47) );
	P( C, D, E, A, B, R(48) );
	P( B, C, D, E, A, R(49) );
	P( A, B, C, D, E, R(50) );
	P( E, A, B, C, D, R(51) );
	P( D, E, A, B, C, R(52) );
	P( C, D, E, A, B, R(53) );
	P( B, C, D, E, A, R(54) );
	P( A, B, C, D, E, R(55) );
	P( E, A, B, C, D, R(56) );
	P( D, E, A, B, C, R(57) );
	P( C, D, E, A, B, R(58) );
	P( B, C, D, E, A, R(59) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0xCA62C1D6

	P( A, B, C, D, E, R(60) );
	P( E, A, B, C, D, R(61) );
	P( D, E, A, B, C, R(62) );
	P( C, D, E, A, B, R(63) );
	P( B, C, D, E, A, R(64) );
	P( A, B, C, D, E, R(65) );
	P( E, A, B, C, D, R(66) );
	P( D, E, A, B, C, R(67) );
	P( C, D, E, A, B, R(68) );
	P( B, C, D, E, A, R(69) );
	P( A, B, C, D, E, R(70) );
	P( E, A, B, C, D, R(71) );
	P( D, E, A, B, C, R(72) );
	P( C, D, E, A, B, R(73) );
	P( B, C, D, E, A, R(74) );
	P( A, B, C, D, E, R(75) );
	P( E, A, B, C, D, R(76) );
	P( D, E, A, B, C, R(77) );
	P( C, D, E, A, B, R(78) );
	P( B, C, D, E, A, R(79) );

#undef K
#undef F

	ctx->state[0] += A;
	ctx->state[1] += B;
	ctx->state[2] += C;
	ctx->state[3] += D;
	ctx->state[4] += E;
}

void sha1_update( sha1_context *ctx, uint8 *input, uint32 length )
{
	uint32 left, fill;

	if( ! length ) return;

	left = ctx->total[0] & 0x3F;
	fill = 64 - left;

	ctx->total[0] += length;
	ctx->total[0] &= 0xFFFFFFFF;

	if( ctx->total[0] < length )
		ctx->total[1]++;

	if( left && length >= fill )
	{
		memcpy( (void *) (ctx->buffer + left),
				(void *) input, fill );
		sha1_process( ctx, ctx->buffer );
		length -= fill;
		input  += fill;
		left = 0;
	}

	while( length >= 64 )
	{
		sha1_process( ctx, input );
		length -= 64;
		input  += 64;
	}

	if( length )
	{
		memcpy( (void *) (ctx->buffer + left),
				(void *) input, length );
	}
}

static uint8 sha1_padding[64] =
{
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void sha1_finish( sha1_context *ctx, uint8 digest[20] )
{
	uint32 last, padn;
	uint32 high, low;
	uint8 msglen[8];

	high = ( ctx->total[0] >> 29 )
		| ( ctx->total[1] <<  3 );
	low  = ( ctx->total[0] <<  3 );

	PUT_UINT32( high, msglen, 0 );
	PUT_UINT32( low,  msglen, 4 );

	last = ctx->total[0] & 0x3F;
	padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

	sha1_update( ctx, sha1_padding, padn );
	sha1_update( ctx, msglen, 8 );

	PUT_UINT32( ctx->state[0], digest,  0 );
	PUT_UINT32( ctx->state[1], digest,  4 );
	PUT_UINT32( ctx->state[2], digest,  8 );
	PUT_UINT32( ctx->state[3], digest, 12 );
	PUT_UINT32( ctx->state[4], digest, 16 );
}

char HexTable[] = "0123456789abcdef";

std::string StringToHex(const std::string& input)
{
	std::string temp;
	temp.resize(input.size() * 2);
	std::string::size_type i;
	std::string::const_iterator itr = input.begin();
	for (i = 0; itr != input.end(); itr++, i++)
	{
		temp[i++] = HexTable[(*itr & 0xF0) >> 4];
		temp[i] = HexTable[*itr & 0x0F];
	}

	return temp;
}

std::string sha1(const std::string& input)
{
	std::string digest;

	sha1_context ctx;
	sha1_starts(&ctx);
	sha1_update(&ctx, (uint8*)&input[0], input.size());
	digest.resize(20);
	sha1_finish(&ctx, (uint8*)&digest[0]);

	return digest;
}

#ifndef _WIN32
#define _rotl(x, y) ((x<<y)|(x>>(32-y)))
#endif

#define F1(X, Y, Z) ((Z) ^ ((X) & ((Y) ^ (Z))))
#define F2(X, Y, Z) ((Y) ^ ((Z) & ((X) ^ (Y))))
#define F3(X, Y, Z) ((X) ^ (Y) ^ (Z))
#define F4(X, Y, Z) ((Y) ^ ((X) | ~(Z)))
#define ROUND(r, a, b, c, d, F, i, s) { a = b + _rotl(a + F(b, c, d) + reinterpret_cast<const uint32 *>(block)[i] + SIN##r, s); }
#define SIN1	3614090360ul
#define SIN2	3905402710ul
#define SIN3	606105819ul
#define SIN4	3250441966ul
#define SIN5	4118548399ul
#define SIN6	1200080426ul
#define SIN7	2821735955ul
#define SIN8	4249261313ul
#define SIN9	1770035416ul
#define SIN10	2336552879ul
#define SIN11	4294925233ul
#define SIN12	2304563134ul
#define SIN13	1804603682ul
#define SIN14	4254626195ul
#define SIN15	2792965006ul
#define SIN16	1236535329ul
#define SIN17	4129170786ul
#define SIN18	3225465664ul
#define SIN19	643717713ul
#define SIN20	3921069994ul
#define SIN21	3593408605ul
#define SIN22	38016083ul
#define SIN23	3634488961ul
#define SIN24	3889429448ul
#define SIN25	568446438ul
#define SIN26	3275163606ul
#define SIN27	4107603335ul
#define SIN28	1163531501ul
#define SIN29	2850285829ul
#define SIN30	4243563512ul
#define SIN31	1735328473ul
#define SIN32	2368359562ul
#define SIN33	4294588738ul
#define SIN34	2272392833ul
#define SIN35	1839030562ul
#define SIN36	4259657740ul
#define SIN37	2763975236ul
#define SIN38	1272893353ul
#define SIN39	4139469664ul
#define SIN40	3200236656ul
#define SIN41	681279174ul
#define SIN42	3936430074ul
#define SIN43	3572445317ul
#define SIN44	76029189ul
#define SIN45	3654602809ul
#define SIN46	3873151461ul
#define SIN47	530742520ul
#define SIN48	3299628645ul
#define SIN49	4096336452ul
#define SIN50	1126891415ul
#define SIN51	2878612391ul
#define SIN52	4237533241ul
#define SIN53	1700485571ul
#define SIN54	2399980690ul
#define SIN55	4293915773ul
#define SIN56	2240044497ul
#define SIN57	1873313359ul
#define SIN58	4264355552ul
#define SIN59	2734768916ul
#define SIN60	1309151649ul
#define SIN61	4149444226ul
#define SIN62	3174756917ul
#define SIN63	718787259ul
#define SIN64	3951481745ul

typedef struct
{
	size_t bufused;
	uint64 nbits;
	uint32 state[4];
	uint8 buffer[64];
} md5_context;

void md5_process(uint32 state[4], const byte block[64])
{
	uint32 A = state[0], B = state[1], C = state[2], D = state[3];
	ROUND(1, A, B, C, D, F1, 0, 7);		ROUND(2, D, A, B, C, F1, 1, 12);
	ROUND(3, C, D, A, B, F1, 2, 17);	ROUND(4, B, C, D, A, F1, 3, 22);
	ROUND(5, A, B, C, D, F1, 4, 7);		ROUND(6, D, A, B, C, F1, 5, 12);
	ROUND(7, C, D, A, B, F1, 6, 17);	ROUND(8, B, C, D, A, F1, 7, 22);
	ROUND(9, A, B, C, D, F1, 8, 7);		ROUND(10, D, A, B, C, F1, 9, 12);
	ROUND(11, C, D, A, B, F1, 10, 17);	ROUND(12, B, C, D, A, F1, 11, 22);
	ROUND(13, A, B, C, D, F1, 12, 7);	ROUND(14, D, A, B, C, F1, 13, 12);
	ROUND(15, C, D, A, B, F1, 14, 17);	ROUND(16, B, C, D, A, F1, 15, 22);
	ROUND(17, A, B, C, D, F2, 1, 5);	ROUND(18, D, A, B, C, F2, 6, 9);
	ROUND(19, C, D, A, B, F2, 11, 14);	ROUND(20, B, C, D, A, F2, 0, 20);
	ROUND(21, A, B, C, D, F2, 5, 5);	ROUND(22, D, A, B, C, F2, 10, 9);
	ROUND(23, C, D, A, B, F2, 15, 14);	ROUND(24, B, C, D, A, F2, 4, 20);
	ROUND(25, A, B, C, D, F2, 9, 5);	ROUND(26, D, A, B, C, F2, 14, 9);
	ROUND(27, C, D, A, B, F2, 3, 14);	ROUND(28, B, C, D, A, F2, 8, 20);
	ROUND(29, A, B, C, D, F2, 13, 5);	ROUND(30, D, A, B, C, F2, 2, 9);
	ROUND(31, C, D, A, B, F2, 7, 14);	ROUND(32, B, C, D, A, F2, 12, 20);
	ROUND(33, A, B, C, D, F3, 5, 4);	ROUND(34, D, A, B, C, F3, 8, 11);
	ROUND(35, C, D, A, B, F3, 11, 16);	ROUND(36, B, C, D, A, F3, 14, 23);
	ROUND(37, A, B, C, D, F3, 1, 4);	ROUND(38, D, A, B, C, F3, 4, 11);
	ROUND(39, C, D, A, B, F3, 7, 16);	ROUND(40, B, C, D, A, F3, 10, 23);
	ROUND(41, A, B, C, D, F3, 13, 4);	ROUND(42, D, A, B, C, F3, 0, 11);
	ROUND(43, C, D, A, B, F3, 3, 16);	ROUND(44, B, C, D, A, F3, 6, 23);
	ROUND(45, A, B, C, D, F3, 9, 4);	ROUND(46, D, A, B, C, F3, 12, 11);
	ROUND(47, C, D, A, B, F3, 15, 16);	ROUND(48, B, C, D, A, F3, 2, 23);
	ROUND(49, A, B, C, D, F4, 0, 6);	ROUND(50, D, A, B, C, F4, 7, 10);
	ROUND(51, C, D, A, B, F4, 14, 15);	ROUND(52, B, C, D, A, F4, 5, 21);
	ROUND(53, A, B, C, D, F4, 12, 6);	ROUND(54, D, A, B, C, F4, 3, 10);
	ROUND(55, C, D, A, B, F4, 10, 15);	ROUND(56, B, C, D, A, F4, 1, 21);
	ROUND(57, A, B, C, D, F4, 8, 6);	ROUND(58, D, A, B, C, F4, 15, 10);
	ROUND(59, C, D, A, B, F4, 6, 15);	ROUND(60, B, C, D, A, F4, 13, 21);
	ROUND(61, A, B, C, D, F4, 4, 6);	ROUND(62, D, A, B, C, F4, 11, 10);
	ROUND(63, C, D, A, B, F4, 2, 15);	ROUND(64, B, C, D, A, F4, 9, 21);
	state[0] += A; state[1] += B; state[2] += C; state[3] += D;
}

void md5_starts(md5_context *ctx)
{
	ctx->nbits = 0;
	ctx->bufused = 0;

	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xefcdab89;
	ctx->state[2] = 0x98badcfe;
	ctx->state[3] = 0x10325476;
}

void md5_update(md5_context *ctx, uint8 *input, uint32 length)
{
	if (ctx->bufused != 0) {
		size_t bufremain = 64 - ctx->bufused;
		if (length < bufremain) {
			memcpy(ctx->buffer + ctx->bufused, input, length);
			ctx->bufused += length;
			return;
		}
		else {
			memcpy(ctx->buffer + ctx->bufused, input, bufremain);
			md5_process(ctx->state, ctx->buffer);
			length -= bufremain;
			input = reinterpret_cast<uint8 *>(input) + bufremain;
		}
	}
	while (length >= 64) {
		md5_process(ctx->state, reinterpret_cast<uint8 *>(input));
		length -= 64;
		input = reinterpret_cast<uint8 *>(input) + 64;
		ctx->nbits += 512;
	}
	if ((ctx->bufused = length) != 0) {
		memcpy(ctx->buffer, input, length);
	}
}

void md5_finish(md5_context *ctx, byte digest[16])
{
	memcpy(digest, ctx->state, 16);
	memset(ctx->buffer + ctx->bufused, 0, 64 - ctx->bufused);
	ctx->buffer[ctx->bufused] = 0x80;
	if (ctx->bufused < 56) {
		reinterpret_cast<uint64 *>(ctx->buffer)[7] = ctx->nbits + ctx->bufused * 8;
		md5_process(reinterpret_cast<uint32 *>(digest), ctx->buffer);
	}
	else {
		byte extra[64];
		md5_process(reinterpret_cast<uint32 *>(digest), ctx->buffer);
		memset(extra, 0, 56);
		reinterpret_cast<uint64 *>(extra)[7] = ctx->nbits + ctx->bufused * 8;
		md5_process(reinterpret_cast<uint32 *>(digest), extra);
	}
}

std::string md5(const std::string& input)
{
	std::string digest;

	md5_context ctx;
	md5_starts(&ctx);
	md5_update(&ctx, (uint8*)&input[0], input.size());
	digest.resize(16);
	md5_finish(&ctx, (uint8*)&digest[0]);

	return digest;
}

static tstring time_to_string(struct tm t)
{
	tchar szTime[256];
	_tcsftime(szTime, sizeof(szTime), _T("%Y-%m-%dT%H:%M:%SZ"), &t);
	return szTime;
}

static struct tm string_to_time(tstring time)
{
	struct tm tmTime = {0};
	tchar num[64], *ptr;
	_tcscpy(num, time.c_str());
	ptr = num;

	tmTime.tm_year = _ttol(ptr)-1900;
	while(*ptr && isdigit(*ptr)) ptr++;
	while(*ptr && !isdigit(*ptr)) ptr++;

	tmTime.tm_mon= _ttol(ptr);
	while(*ptr && isdigit(*ptr)) ptr++;
	while(*ptr && !isdigit(*ptr)) ptr++;

	tmTime.tm_mday = _ttol(ptr);
	while(*ptr && isdigit(*ptr)) ptr++;
	while(*ptr && !isdigit(*ptr)) ptr++;

	tmTime.tm_hour = _ttol(ptr);
	while(*ptr && isdigit(*ptr)) ptr++;
	while(*ptr && !isdigit(*ptr)) ptr++;

	tmTime.tm_min = _ttol(ptr);
	while(*ptr && isdigit(*ptr)) ptr++;
	while(*ptr && !isdigit(*ptr)) ptr++;

	tmTime.tm_sec = _ttol(ptr);
	return tmTime;
}

static std::string get_time()
{
	time_t timer;
	struct tm *date;
	timer = time(NULL);
	date = localtime(&timer);
	char szTime[256];
	strftime(szTime, sizeof(szTime), "%Y-%m-%dT%H:%M:%SZ", date);
	return szTime;
}

static std::string get_nonce()
{
	static bool bInitialized = false;
	if (!bInitialized)
		srand((unsigned int)time(NULL));
	char szBuf[256];
	sprintf(szBuf, "%d %d", (int)time(NULL), (int)rand());
	return StringToHex(sha1(szBuf));
}

#ifdef USE_WIN32API
tstring getBloggerAuth(tstring userid, tstring passwd, tstring user_agent = _T(""))
{
	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwSize;
	DWORD dwLength = 0;
	tstring strRequest;
	std::string strData;
	tchar szUrl[2048], *pszUrl;
	DWORD dwTimeout = 30 * 1000;
	BOOL bRet;
	size_t pos;

	tstring post_url = _T("https://www.google.com/accounts/ClientLogin");
	short port = 80;
	tstring path;
	_tcscpy(szUrl, post_url.c_str());
	if (!_tcsncmp(szUrl, _T("https:"), 6)) port = 443;
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		return _T("");
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		return _T("");
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	hSession = InternetOpen(
			NULL,
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

	bRet = InternetSetOption(
			hSession,
			INTERNET_OPTION_RECEIVE_TIMEOUT,
			&dwTimeout,
			sizeof(dwTimeout));

	hConnect = InternetConnect(
			hSession,
			host.c_str(),
			port,
			NULL,
			NULL,
			INTERNET_SERVICE_HTTP,
			0,
			0);
	if (hConnect == NULL) {
		goto leave;
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
				INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_UI | INTERNET_FLAG_KEEP_CONNECTION,
				0);
	} else {
		hRequest = HttpOpenRequest(
				hConnect,
				_T("POST"),
				path.c_str(),
				NULL,
				NULL,
				NULL,
				INTERNET_FLAG_NO_CACHE_WRITE,
				0);
	}
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: application/x-www-form-urlencoded");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	strRequest = _T("");
	strRequest += _T("Email=");
	strRequest += userid;
	strRequest += _T("&Passwd=");
	strRequest += passwd;
	strRequest += _T("&service=blogger");
	strRequest += _T("&source=upbylunch-google-0");

	sHeader = _T("Content-Length: ");
	sprintf(szBuf, "%d", strRequest.size());
	sHeader += XmlRpc::string2tstring(szBuf);
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	if (HttpSendRequest(hRequest, NULL, 0, (LPVOID)strRequest.c_str(), strRequest.size()) == FALSE) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	dwLength = -1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "200")) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_TEXT, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	strData = "";
	for (;;) {
		dwSize = sizeof(szBuf)-1;
		szBuf[0] = 0;
		if (InternetReadFile(hRequest, szBuf, dwSize, &dwSize) == FALSE) break;
		if (dwSize <= 0) break;
		szBuf[dwSize] = 0;
		strData += szBuf;
		if (strData.size() >= dwLength) break;
	}
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", strData.c_str());
		fclose(fp);
	}
#endif

	pos = strData.find("Auth=");
	if (pos != std::string::npos) {
		strData = strData.substr(pos+5);
		pos = strData.find_first_of("&");
		if (pos != std::string::npos)
			strData = strData.substr(0, pos);
	}
	pos = strData.find("\r");
	if (pos != std::string::npos) strData = strData.substr(0, pos);
	pos = strData.find("\n");
	if (pos != std::string::npos) strData = strData.substr(0, pos);

leave:
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
#ifdef _UNICODE
	return XmlRpc::string2tstring(strData);
#else
	return strData;
#endif
}

AtomFeed getFeed(tstring url, tstring userid, tstring passwd, tstring user_agent)
{
	USES_CONVERSION;
	AtomFeed ret;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMNodePtr pNode;
	MSXML2::IXMLDOMNodeListPtr pNodeList;

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwIndex = 0;
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	INT n;
	INT f;
	tchar szUrl[2048], *pszUrl;

	short port = 80;
	tstring path;
	_tcscpy(szUrl, url.c_str());
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		ret.error = _T("Invalid Request URL");
		return ret;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		ret.error = _T("Invalid Request URL");
		return ret;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

	hConnect = InternetConnect(
			hSession,
			host.c_str(),
			port,
			NULL,
			NULL,
			INTERNET_SERVICE_HTTP,
			0,
			0);
	if (hConnect == NULL) {
		goto leave;
	}

	hRequest = HttpOpenRequest(
			hConnect,
			_T("GET"),
			path.c_str(),
			NULL,
			NULL,
			NULL,
			INTERNET_FLAG_NO_CACHE_WRITE,
			0);
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: application/x.atom+xml");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("X-WSSE: ");
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: WSSE profile=\"UsernameToken\"");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	if (HttpSendRequest(hRequest, NULL, 0, NULL, 0) == FALSE) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	dwLength = -1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, &szBuf, &dwSize, &dwIndex) == TRUE) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "200")) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_TEXT, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	strData = "";
	for (;;) {
		dwSize = sizeof(szBuf)-1;
		szBuf[0] = 0;
		if (InternetReadFile(hRequest, szBuf, dwSize, &dwSize) == FALSE) break;
		if (dwSize <= 0) break;
		szBuf[dwSize] = 0;
		strData += szBuf;
		if (strData.size() >= dwLength) break;
	}
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", strData.c_str());
		fclose(fp);
	}
#endif

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	try {
		pDoc->loadXML(A2OLE(XmlRpc::utf8_to_string(strData).c_str()));
		pNode = pDoc->selectSingleNode(_T("/feed/title"));
		if (pNode) {
			ret.title = XmlRpc::string_to_utf8((tchar*)pNode->text);
			pNode.Release();
		}
		pNode = pDoc->selectSingleNode(_T("/feed/id"));
		if (pNode) {
			ret.id = XmlRpc::string_to_utf8((tchar*)pNode->text);
			pNode.Release();
		}
		pNodeList = pDoc->selectNodes(_T("/feed/link"));
		for(n = 0; n < pNodeList->length; n++) {
			AtomLink link;
			for(f = 0; f < pNodeList->item[n]->attributes->length; f++) {
				if (tstring(_T("rel")) == OLE2T(pNodeList->item[n]->attributes->item[f]->nodeName))
					link.rel = XmlRpc::string_to_utf8((tchar*)pNodeList->item[n]->attributes->item[f]->text);
				if (tstring(_T("href")) == OLE2T(pNodeList->item[n]->attributes->item[f]->nodeName))
					link.href = XmlRpc::string_to_utf8((tchar*)pNodeList->item[n]->attributes->item[f]->text);
				if (tstring(_T("type")) == OLE2T(pNodeList->item[n]->attributes->item[f]->nodeName))
					link.type = XmlRpc::string_to_utf8((tchar*)pNodeList->item[n]->attributes->item[f]->text);
				if (tstring(_T("title")) == OLE2T(pNodeList->item[n]->attributes->item[f]->nodeName))
					link.title = XmlRpc::string_to_utf8((tchar*)pNodeList->item[n]->attributes->item[f]->text);
			}
			ret.links.push_back(link);
		}
	} catch(...) {
		ret.links.clear();
	}
leave:
	if (pNode != NULL) pNode.Release();
	if (pNodeList != NULL) pNodeList.Release();
	if (pDoc != NULL) pDoc.Release();
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

tstring GetNodeAttribute(MSXML2::IXMLDOMNodePtr pNode, tstring attrName)
{
	USES_CONVERSION;
	if (pNode == NULL) return _T("");
	for(int f = 0; f < pNode->attributes->length; f++) {
		tstring a = pNode->attributes->item[f]->nodeName;
		if (attrName == OLE2T(pNode->attributes->item[f]->nodeName)) {
			return (tchar*)pNode->attributes->item[f]->text;
		}
	}
	return _T("");
}

AtomEntries getEntries(tstring url, tstring userid, tstring passwd, tstring user_agent)
{
	USES_CONVERSION;
	AtomEntries ret;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMNodePtr pContent, pDate;
	MSXML2::IXMLDOMNodeListPtr pEntries;
	MSXML2::IXMLDOMNodeListPtr pLinks;

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwIndex = 0;
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	INT n;
	INT m;
	tchar szUrl[2048], *pszUrl;

	short port = 80;
	tstring path;
	_tcscpy(szUrl, url.c_str());
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		ret.error = _T("Invalid Request URL");
		return ret;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		ret.error = _T("Invalid Request URL");
		return ret;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

	hConnect = InternetConnect(
			hSession,
			host.c_str(),
			port,
			NULL,
			NULL,
			INTERNET_SERVICE_HTTP,
			0,
			0);
	if (hConnect == NULL) {
		goto leave;
	}

	hRequest = HttpOpenRequest(
			hConnect,
			_T("GET"),
			path.c_str(),
			NULL,
			NULL,
			NULL,
			INTERNET_FLAG_NO_CACHE_WRITE,
			0);
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: application/x.atom+xml");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("X-WSSE: ");
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: WSSE profile=\"UsernameToken\"");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	if (HttpSendRequest(hRequest, NULL, 0, NULL, 0) == FALSE) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	dwLength = -1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, &szBuf, &dwSize, &dwIndex) == TRUE) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "200")) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_TEXT, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	strData = "";
	for (;;) {
		dwSize = sizeof(szBuf)-1;
		szBuf[0] = 0;
		if (InternetReadFile(hRequest, szBuf, dwSize, &dwSize) == FALSE) break;
		if (dwSize <= 0) break;
		szBuf[dwSize] = 0;
		strData += szBuf;
		if (strData.size() >= dwLength) break;
	}
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", strData.c_str());
		fclose(fp);
	}
#endif

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	try {
		pDoc->loadXML(A2OLE(XmlRpc::utf8_to_string(strData).c_str()));
		pEntries = pDoc->selectNodes(_T("/feed/entry"));

		for(n = 0; n < pEntries->length; n++) {
			AtomEntry entry;
			entry.id = XmlRpc::string_to_utf8((tchar*)pEntries->item[n]->selectSingleNode(_T("id"))->text);
			entry.title = XmlRpc::string_to_utf8((tchar*)pEntries->item[n]->selectSingleNode(_T("title"))->text);

			time_t nowtime = time(NULL);
			localtime_r(&nowtime, &entry.updated);
			localtime_r(&nowtime, &entry.published);

			pDate = pEntries->item[n]->selectSingleNode(_T("published"));
			if (pDate == NULL) pDate = pEntries->item[n]->selectSingleNode(_T("issued"));
			if (pDate) entry.published = string_to_time((tchar*)pDate->text);

			pDate = pEntries->item[n]->selectSingleNode(_T("updated"));
			if (pDate == NULL) pDate = pEntries->item[n]->selectSingleNode(_T("modified"));
			if (pDate) entry.updated = string_to_time((tchar*)pDate->text);

			pContent = pEntries->item[n]->selectSingleNode(_T("content"));
			entry.content.type = GetNodeAttribute(pContent, _T("type"));
			entry.content.mode = GetNodeAttribute(pContent, _T("mode"));
			if (entry.content.mode == _T("xml"))
				entry.content.value = XmlRpc::string_to_utf8((tchar*)pEntries->item[n]->selectSingleNode(_T("content"))->xml);
			else
				entry.content.value = XmlRpc::string_to_utf8((tchar*)pEntries->item[n]->selectSingleNode(_T("content"))->text);
			pLinks = pEntries->item[n]->selectNodes(_T("link"));
			for(m = 0; m < pLinks->length; m++) {
				AtomLink link;
				link.rel = GetNodeAttribute(pLinks->item[m], _T("rel"));
				link.href = GetNodeAttribute(pLinks->item[m], _T("href"));
				link.type = GetNodeAttribute(pLinks->item[m], _T("type"));
				link.title = GetNodeAttribute(pLinks->item[m], _T("title"));
				if (link.rel == _T("self") || link.rel == _T("service.edit"))
					entry.edit_uri = link.href;
				entry.links.push_back(link);
			}
			ret.entries.push_back(entry);
		}
	} catch(_com_error &e) {
		ret.error = e.ErrorMessage();
	} catch(...) {
	}
leave:
	if (pEntries != NULL) pEntries.Release();
	if (pDoc != NULL) pDoc.Release();
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

int createEntry(tstring feed_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	USES_CONVERSION;

	AtomFeed feed = getFeed(feed_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_url;
	tstring post_mime = _T("application/x.atom+xml");
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == _T("service.post")) {
			post_url = it->href;
			post_mime =it->type;
		}
	}
	if (!post_url.size()) {
		for(it = feed.links.begin(); it != feed.links.end(); it++) {
			if (it->rel == _T("self")) {
				post_url = it->href;
				post_mime =it->type;
			}
		}
		if (!feed.links.size())
			post_url = feed_url;
		if (!post_url.size())
			return -1;
	}
	int ret = -3;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());

	std::string authorization_header;

	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pEntry;
	MSXML2::IXMLDOMElementPtr pElement;
	MSXML2::IXMLDOMNodeListPtr pLinks;

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	INT n;
	INT f;
	tchar szUrl[2048], *pszUrl;
	DWORD dwTimeout = 30 * 1000;
	BOOL bRet;

	short port = 80;
	tstring path;
	_tcscpy(szUrl, post_url.c_str());
	if (!_tcsncmp(szUrl, _T("https:"), 6)) port = 443;
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		return -1;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		return -2;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	std::string strXml;
	std::string strSjisXml;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

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
		goto leave;
	}

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
				INTERNET_FLAG_NO_CACHE_WRITE,
				0);
	}
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: ");
	sHeader += post_mime;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("X-WSSE: ");
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: WSSE profile=\"UsernameToken\"");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	pInst = pDoc->createProcessingInstruction( "xml", "version=\"1.0\" encoding=\"utf-8\"" );
	pDoc->appendChild(pInst);
	pEntry = pDoc->createElement(_T("entry"));
	pDoc->appendChild(pEntry);
	pEntry->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));

	pElement = pDoc->createElement(_T("title"));
	pElement->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));
	pElement->appendChild(pDoc->createTextNode(entry.title.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("summary"));
	pElement->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));
	pElement->appendChild(pDoc->createTextNode(entry.summary.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("content"));
	pElement->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));
	pElement->setAttribute(_T("mode"), entry.content.mode.c_str());
	pElement->setAttribute(_T("type"), entry.content.type.c_str());
	pElement->appendChild(pDoc->createTextNode(entry.content.value.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	strXml = OLE2A(pDoc->xml);
	n = strXml.find_first_of(">");
	strSjisXml = strXml.substr(0, n-1);
	strSjisXml += " encoding=\"utf-8\"";
	strSjisXml += strXml.substr(n-1);
	strXml = XmlRpc::string_to_utf8(strSjisXml);
	pEntry.Release();
	pInst.Release();
	pDoc.Release();
#if 1
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", strXml.c_str());
		fclose(fp);
	}
#endif

	if (HttpSendRequest(hRequest, NULL, 0, (LPVOID)strXml.c_str(), strXml.size()) == FALSE) {
		goto leave;
	}

	/*
	   if (HttpEndRequest(hRequest, NULL, 0, 0) == FALSE) {
	   goto leave;
	   }
	   */

	dwSize = sizeof(szBuf)-1;
	dwLength = -1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "201") && strcmp(szBuf, "200")) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_TEXT, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	strData = "";
	for (;;) {
		dwSize = sizeof(szBuf)-1;
		szBuf[0] = 0;
		if (InternetReadFile(hRequest, szBuf, dwSize, &dwSize) == FALSE) break;
		if (dwSize <= 0) break;
		szBuf[dwSize] = 0;
		strData += szBuf;
		if (strData.size() >= dwLength) break;
	}
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", strData.c_str());
		fclose(fp);
	}
#endif

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	try {
		pDoc->loadXML(A2OLE(XmlRpc::utf8_to_string(strData).c_str()));
		pLinks = pDoc->selectNodes(_T("/entry/link"));
		for(n = 0; n < pLinks->length; n++) {
			AtomLink link;
			for(f = 0; f < pLinks->item[n]->attributes->length; f++) {
				if (tstring(_T("rel")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.rel = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
				if (tstring(_T("href")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.href = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
				if (tstring(_T("type")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.type = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
				if (tstring(_T("title")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.title = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
			}
			if (link.rel == _T("service.edit")) {
				entry.edit_uri = link.href;
				break;
			}
		}
	} catch(...) {
		goto leave;
	}
	ret = 0;
leave:
	if (pLinks != NULL) pLinks.Release();
	if (pEntry != NULL) pEntry.Release();
	if (pElement != NULL) pElement.Release();
	if (pDoc != NULL) pDoc.Release();
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

int createEntry_Blogger(tstring feed_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	USES_CONVERSION;

	AtomFeed feed = getFeed(feed_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_url;
	tstring post_mime = _T("application/x.atom+xml");
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == _T("service.post")) {
			post_url = it->href;
			post_mime =it->type;
		}
	}
	if (!post_url.size()) {
		for(it = feed.links.begin(); it != feed.links.end(); it++) {
			if (it->rel == _T("self")) {
				post_url = it->href;
				post_mime =it->type;
			}
		}
		if (!post_url.size())
			return -1;
	}
	int ret = -3;

	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pEntry;
	MSXML2::IXMLDOMElementPtr pElement;
	MSXML2::IXMLDOMNodeListPtr pLinks;

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	INT n;
	INT f;
	tchar szUrl[2048], *pszUrl;
	DWORD dwTimeout = 30 * 1000;
	BOOL bRet;

	tstring bloggerAuth = getBloggerAuth(userid, passwd);

	short port = 80;
	tstring path;
	_tcscpy(szUrl, post_url.c_str());
	if (!_tcsncmp(szUrl, _T("https:"), 6)) port = 443;
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		return -1;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		return -2;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	std::string strXml;
	std::string strSjisXml;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

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
		goto leave;
	}

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
				INTERNET_FLAG_NO_CACHE_WRITE,
				0);
	}
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: ");
	sHeader += post_mime;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: GoogleLogin auth=");
	sHeader += bloggerAuth;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	pInst = pDoc->createProcessingInstruction( "xml", "version=\"1.0\" encoding=\"utf-8\"" );
	pDoc->appendChild(pInst);
	pEntry = pDoc->createElement(_T("entry"));
	pDoc->appendChild(pEntry);
	pEntry->setAttribute(_T("xmlns"), _T("http://www.w3.org/2005/Atom"));

	pElement = pDoc->createElement(_T("title"));
	pElement->setAttribute(_T("type"), _T("text"));
	pElement->appendChild(pDoc->createTextNode(entry.title.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("published"));
	pElement->appendChild(pDoc->createTextNode(get_time().c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("updated"));
	pElement->appendChild(pDoc->createTextNode(get_time().c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("content"));
	pElement->setAttribute(_T("mode"), entry.content.mode.c_str());
	pElement->setAttribute(_T("type"), entry.content.type.c_str());
	pElement->appendChild(pDoc->createTextNode(entry.content.value.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	strXml = OLE2A(pDoc->xml);
	n = strXml.find_first_of(">");
	strSjisXml = strXml.substr(0, n-1);
	strSjisXml += " encoding=\"utf-8\"";
	strSjisXml += strXml.substr(n-1);
	strXml = XmlRpc::string_to_utf8(strSjisXml);
	pEntry.Release();
	pInst.Release();
	pDoc.Release();
#if 1
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", strXml.c_str());
		fclose(fp);
	}
#endif

	if (HttpSendRequest(hRequest, NULL, 0, (LPVOID)strXml.c_str(), strXml.size()) == FALSE) {
		goto leave;
	}

	if (HttpEndRequest(hRequest, NULL, 0, 0) == FALSE) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	dwLength = -1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "201")) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_TEXT, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	strData = "";
	for (;;) {
		dwSize = sizeof(szBuf)-1;
		szBuf[0] = 0;
		if (InternetReadFile(hRequest, szBuf, dwSize, &dwSize) == FALSE) break;
		if (dwSize <= 0) break;
		szBuf[dwSize] = 0;
		strData += szBuf;
		if (strData.size() >= dwLength) break;
	}
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", strData.c_str());
		fclose(fp);
	}
#endif

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	try {
		pDoc->loadXML(A2OLE(XmlRpc::utf8_to_string(strData).c_str()));
		pLinks = pDoc->selectNodes(_T("/entry/link"));
		for(n = 0; n < pLinks->length; n++) {
			AtomLink link;
			for(f = 0; f < pLinks->item[n]->attributes->length; f++) {
				if (tstring(_T("rel")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.rel = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
				if (tstring(_T("href")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.href = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
				if (tstring(_T("type")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.type = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
				if (tstring(_T("title")) == OLE2T(pLinks->item[n]->attributes->item[f]->nodeName))
					link.title = XmlRpc::string_to_utf8((tchar*)pLinks->item[n]->attributes->item[f]->text);
			}
			if (link.rel == _T("service.edit")) {
				entry.edit_uri = link.href;
				break;
			}
		}
	} catch(...) {
		goto leave;
	}
	ret = 0;
leave:
	if (pLinks != NULL) pLinks.Release();
	if (pEntry != NULL) pEntry.Release();
	if (pElement != NULL) pElement.Release();
	if (pDoc != NULL) pDoc.Release();
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

int updateEntry(tstring update_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	USES_CONVERSION;

	AtomFeed feed = getFeed(update_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_mime = _T("application/x.atom+xml");
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == _T("self")) {
			post_mime =it->type;
		}
	}
	int ret = -3;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pEntry;
	MSXML2::IXMLDOMElementPtr pElement;
	MSXML2::IXMLDOMNodeListPtr pLinks;

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	INT n;
	tchar szUrl[2048], *pszUrl;

	short port = 80;
	tstring path;
	_tcscpy(szUrl, update_url.c_str());
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		return -1;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		return -2;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	std::string strXml;
	std::string strSjisXml;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

	hConnect = InternetConnect(
			hSession,
			host.c_str(),
			port,
			NULL,
			NULL,
			INTERNET_SERVICE_HTTP,
			0,
			0);
	if (hConnect == NULL) {
		goto leave;
	}

	hRequest = HttpOpenRequest(
			hConnect,
			_T("PUT"),
			path.c_str(),
			NULL,
			NULL,
			NULL,
			INTERNET_FLAG_NO_CACHE_WRITE,
			0);
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: ");
	sHeader += post_mime;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("X-WSSE: ");
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: WSSE profile=\"UsernameToken\"");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	pInst = pDoc->createProcessingInstruction( "xml", "version=\"1.0\" encoding=\"utf-8\"" );
	pDoc->appendChild(pInst);
	pEntry = pDoc->createElement(_T("entry"));
	pDoc->appendChild(pEntry);
	pEntry->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));

	pElement = pDoc->createElement(_T("title"));
	pElement->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));
	pElement->appendChild(pDoc->createTextNode(entry.title.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	if (entry.content.mode != "empty") {
		pElement = pDoc->createElement(_T("summary"));
		pElement->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));
		pElement->appendChild(pDoc->createTextNode(entry.summary.c_str()));
		pEntry->appendChild(pElement);
		pElement.Release();

		pElement = pDoc->createElement(_T("content"));
		pElement->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));
		pElement->setAttribute(_T("mode"), entry.content.mode.c_str());
		pElement->setAttribute(_T("type"), entry.content.type.c_str());
		pElement->appendChild(pDoc->createTextNode(entry.content.value.c_str()));
		pEntry->appendChild(pElement);
		pElement.Release();
	} else {
		std::vector<AtomLink>::iterator it;
		for(it = entry.links.begin(); it != entry.links.end(); it++) {
			pElement = pDoc->createElement(_T("link"));
			pElement->setAttribute(_T("xmlns"), _T("http://purl.org/atom/ns#"));
			pElement->setAttribute(_T("rel"), it->rel.c_str());
			pElement->setAttribute(_T("type"), it->type.c_str());
			pElement->setAttribute(_T("href"), it->href.c_str());
			pEntry->appendChild(pElement);
			pElement.Release();
		}
	}

	strXml = OLE2A(pDoc->xml);
	n = strXml.find_first_of(">");
	strSjisXml = strXml.substr(0, n-1);
	strSjisXml += " encoding=\"utf-8\"";
	strSjisXml += strXml.substr(n-1);
	strXml = XmlRpc::string_to_utf8(strSjisXml);
	pEntry.Release();
	pInst.Release();
	pDoc.Release();
#if 1
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", strXml.c_str());
		fclose(fp);
	}
#endif

	if (HttpSendRequest(hRequest, NULL, 0, (LPVOID)strXml.c_str(), strXml.size()) == FALSE) {
		goto leave;
	}

	if (HttpEndRequest(hRequest, NULL, 0, 0) == FALSE) {
		goto leave;
	}


	dwSize = sizeof(szBuf)-1;
	dwLength = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "201") && strcmp(szBuf, "200")) {
		goto leave;
	}

	ret = 0;
leave:
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

int updateEntry_Blogger(tstring update_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	USES_CONVERSION;

	int ret = -3;

	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pEntry;
	MSXML2::IXMLDOMElementPtr pElement;
	MSXML2::IXMLDOMNodeListPtr pLinks;

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	INT n;
	tchar szUrl[2048], *pszUrl;
	DWORD dwTimeout = 30 * 1000;
	BOOL bRet;

	tstring bloggerAuth = getBloggerAuth(userid, passwd);

	short port = 80;
	tstring path;
	_tcscpy(szUrl, update_url.c_str());
	if (!_tcsncmp(szUrl, _T("https:"), 6)) port = 443;
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		return -1;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		return -2;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	std::string strXml;
	std::string strSjisXml;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

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
		goto leave;
	}

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
				INTERNET_FLAG_NO_CACHE_WRITE,
				0);
	}
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: ");
	sHeader += _T("application/atom+xml;");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: GoogleLogin auth=");
	sHeader += bloggerAuth;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	pDoc.CreateInstance(__uuidof(MSXML2::DOMDocument));
	pInst = pDoc->createProcessingInstruction( "xml", "version=\"1.0\" encoding=\"utf-8\"" );
	pDoc->appendChild(pInst);
	pEntry = pDoc->createElement(_T("entry"));
	pDoc->appendChild(pEntry);
	pEntry->setAttribute(_T("xmlns"), _T("http://www.w3.org/2005/Atom"));

	pElement = pDoc->createElement(_T("title"));
	pElement->setAttribute(_T("type"), _T("text"));
	pElement->appendChild(pDoc->createTextNode(entry.title.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("published"));
	pElement->appendChild(pDoc->createTextNode(get_time().c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("updated"));
	pElement->appendChild(pDoc->createTextNode(get_time().c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	pElement = pDoc->createElement(_T("content"));
	pElement->setAttribute(_T("mode"), entry.content.mode.c_str());
	pElement->setAttribute(_T("type"), entry.content.type.c_str());
	pElement->appendChild(pDoc->createTextNode(entry.content.value.c_str()));
	pEntry->appendChild(pElement);
	pElement.Release();

	strXml = OLE2A(pDoc->xml);
	n = strXml.find_first_of(">");
	strSjisXml = strXml.substr(0, n-1);
	strSjisXml += " encoding=\"utf-8\"";
	strSjisXml += strXml.substr(n-1);
	strXml = XmlRpc::string_to_utf8(strSjisXml);
	pEntry.Release();
	pInst.Release();
	pDoc.Release();
#if 1
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", strXml.c_str());
		fclose(fp);
	}
#endif

	if (HttpSendRequest(hRequest, NULL, 0, (LPVOID)strXml.c_str(), strXml.size()) == FALSE) {
		goto leave;
	}

	if (HttpEndRequest(hRequest, NULL, 0, 0) == FALSE) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	dwLength = -1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "200") || strcmp(szBuf, "201")) {
		goto leave;
	}

	ret = 0;
leave:
	if (pLinks != NULL) pLinks.Release();
	if (pEntry != NULL) pEntry.Release();
	if (pElement != NULL) pElement.Release();
	if (pDoc != NULL) pDoc.Release();
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

int deleteEntry(tstring delete_url, tstring userid, tstring passwd, tstring user_agent)
{
	USES_CONVERSION;

	AtomFeed feed = getFeed(delete_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_mime = _T("application/x.atom+xml");
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == _T("self")) {
			post_mime =it->type;
		}
	}
	int ret = -3;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	tchar szUrl[2048], *pszUrl;

	short port = 80;
	tstring path;
	_tcscpy(szUrl, delete_url.c_str());
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		return -1;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		return -2;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	std::string strXml;
	std::string strSjisXml;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

	hConnect = InternetConnect(
			hSession,
			host.c_str(),
			port,
			NULL,
			NULL,
			INTERNET_SERVICE_HTTP,
			0,
			0);
	if (hConnect == NULL) {
		goto leave;
	}

	hRequest = HttpOpenRequest(
			hConnect,
			_T("DELETE"),
			path.c_str(),
			NULL,
			NULL,
			NULL,
			INTERNET_FLAG_NO_CACHE_WRITE,
			0);
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: ");
	sHeader += post_mime;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("X-WSSE: ");
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: WSSE profile=\"UsernameToken\"");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	if (HttpSendRequest(hRequest, NULL, 0, NULL, 0) == FALSE) {
		goto leave;
	}

	if (HttpEndRequest(hRequest, NULL, 0, 0) == FALSE) {
		goto leave;
	}


	dwSize = sizeof(szBuf)-1;
	dwLength = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "201")) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_TEXT, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	strData = "";
	for (;;) {
		dwSize = sizeof(szBuf)-1;
		szBuf[0] = 0;
		if (InternetReadFile(hRequest, szBuf, dwSize, &dwSize) == FALSE) break;
		if (dwSize <= 0) break;
		szBuf[dwSize] = 0;
		strData += szBuf;
		if (strData.size() >= dwLength) break;
	}
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", strData.c_str());
		fclose(fp);
	}
#endif

	ret = 0;
leave:
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

int deleteEntry_Blogger(tstring delete_url, tstring userid, tstring passwd, tstring user_agent)
{
	USES_CONVERSION;

	int ret = -3;

	MSXML2::IXMLDOMDocumentPtr pDoc;
	MSXML2::IXMLDOMProcessingInstructionPtr pInst;
	MSXML2::IXMLDOMElementPtr pEntry;
	MSXML2::IXMLDOMElementPtr pElement;
	MSXML2::IXMLDOMNodeListPtr pLinks;

	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	tstring sHeader;
	char szBuf[BUFSIZ];
	DWORD dwSize;
	DWORD dwLength = 0;
	std::string strData;
	tchar szUrl[2048], *pszUrl;
	DWORD dwTimeout = 30 * 1000;
	BOOL bRet;

	tstring bloggerAuth = getBloggerAuth(userid, passwd);

	short port = 80;
	tstring path;
	_tcscpy(szUrl, delete_url.c_str());
	if (!_tcsncmp(szUrl, _T("https:"), 6)) port = 443;
	pszUrl = _tcsstr(szUrl, _T("://"));
	if (!pszUrl) {
		return -1;
	}
	_tcscpy(szUrl, pszUrl+3);
	pszUrl = _tcschr(szUrl, '/');
	if (!pszUrl) {
		return -2;
	}
	path = pszUrl;
	*pszUrl = 0;
	pszUrl = _tcschr(szUrl, ':');
	if (pszUrl) {
		*pszUrl = 0;
		port = (short)_ttol(pszUrl+1);
	}
	tstring host = szUrl;

	std::string strXml;
	std::string strSjisXml;

	hSession = InternetOpen(
			user_agent.c_str(),
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL,
			NULL,
			0);
	if (hSession == NULL) {
		goto leave;
	}

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
		goto leave;
	}

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
				INTERNET_FLAG_NO_CACHE_WRITE,
				0);
	}
	if (hRequest == NULL) {
		goto leave;
	}

	sHeader = _T("Content-Type: ");
	sHeader += _T("application/atom+xml;");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);


	sHeader = _T("X-Http-Method-Override: DELETE");
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("Authorization: GoogleLogin auth=");
	sHeader += bloggerAuth;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	sHeader = _T("User-Agent: ");
	sHeader += user_agent;
	HttpAddRequestHeaders(hRequest, sHeader.c_str(), sHeader.size(), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);

	if (HttpSendRequest(hRequest, NULL, 0, NULL, 0) == FALSE) {
		goto leave;
	}

	dwSize = sizeof(szBuf)-1;
	dwLength = -1;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_LENGTH, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
		dwLength = atol(szBuf);
	}

	dwSize = sizeof(szBuf)-1;
	szBuf[0] = 0;
	if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE, szBuf, &dwSize, 0)) {
		szBuf[dwSize] = 0;
	}

	if (strcmp(szBuf, "200")) {
		goto leave;
	}

	ret = 0;
leave:
	if (hRequest) InternetCloseHandle(hRequest);
	if (hConnect) InternetCloseHandle(hConnect);
	if (hSession) InternetCloseHandle(hSession);
	return ret;
}

#else
static std::string responseBuf;
static int handle_returned_data(char* ptr, size_t size, size_t nmemb, void* stream)
{
	char* pbuf = new char[size*nmemb+1];
	memcpy(pbuf, ptr, size*nmemb);
	pbuf[size*nmemb] = 0;
	responseBuf += pbuf;
	delete[] pbuf;
	return size*nmemb;
}

std::string getBloggerAuth(tstring userid, tstring passwd, tstring user_agent = "")
{
	std::string post_url = "https://www.google.com/accounts/ClientLogin";
	size_t pos;

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return "";

	std::string sHeader;
	std::string strRequest;

	sHeader = "Content-Type: application/x-www-form-urlencoded";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	strRequest = "";
	strRequest += "Email=" + userid;
	strRequest += "&Passwd=" + passwd;
	strRequest += "&service=blogger";
	strRequest += "&source=upbylunch-google-0";

	curl_easy_setopt(curl, CURLOPT_URL, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strRequest.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strRequest.size());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || status != 200) goto leave;

	pos = responseBuf.find("Auth=");
	if (pos != std::string::npos) {
		responseBuf = responseBuf.substr(pos+5);
		pos = responseBuf.find_first_of("&");
		if (pos != std::string::npos)
			responseBuf = responseBuf.substr(0, pos);
	}
	pos = responseBuf.find("\r");
	if (pos != std::string::npos) responseBuf = responseBuf.substr(0, pos);
	pos = responseBuf.find("\n");
	if (pos != std::string::npos) responseBuf = responseBuf.substr(0, pos);

leave:
	std::string ret = responseBuf;
	responseBuf = "";
	curl_easy_cleanup(curl);
	return ret;
}

AtomFeed getFeed(tstring url, tstring userid, tstring passwd, tstring user_agent)
{
	AtomFeed ret;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	xmlDocPtr pDoc = NULL;
	xmlNodePtr pLinks;

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: application/x.atom+xml";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-WSSE: ";
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: WSSE profile=\"UsernameToken\"";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || status != 200) goto leave;

	try {
		const char *ptr = responseBuf.c_str();
		while(isspace(*ptr)) ptr++;
		responseBuf.erase(0, ptr-responseBuf.c_str());
		pDoc = xmlParseDoc((xmlChar*)responseBuf.c_str());
		if (pDoc) {
			pLinks = pDoc->children;
			while(pLinks) {
				if (tstring((char*)pLinks->name) == "feed") {
					pLinks = pLinks->children;
					break;
				}
				pLinks = pLinks->next;
			}
		} else
			pLinks = NULL;
		while(pLinks) {
			tstring strName = (char*)pLinks->name;
			if (strName == "title") {
				ret.title = (char*)pLinks->children->content;
			} else
				if (strName == "id") {
					ret.id = (char*)pLinks->children->content;
				} else
					if (strName == "link") {
						AtomLink link;
						char* ptr;
						ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"rel");
						if (ptr) link.rel = ptr;
						ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"href");
						if (ptr) link.href = ptr;
						ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"type");
						if (ptr) link.type = ptr;
						ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"title");
						if (ptr) link.title = ptr;
						ret.links.push_back(link);
					}
				pLinks = pLinks->next;
		}
	} catch(...) {
		ret.links.clear();
	}
leave:
	responseBuf = "";
	if (pDoc) xmlFreeDoc(pDoc);
	curl_easy_cleanup(curl);
	return ret;
}

int createEntry(tstring feed_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	AtomFeed feed = getFeed(feed_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_url;
	tstring post_mime = "application/x.atom+xml";
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == "service.post") {
			post_url = it->href;
			post_mime =it->type;
		}
	}
	if (!post_url.size()) {
		for(it = feed.links.begin(); it != feed.links.end(); it++) {
			if (it->rel == "self") {
				post_url = it->href;
				post_mime =it->type;
			}
		}
		if (!post_url.size())
			return -1;
	}
	int ret = -3;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	std::string requestXml;
	xmlDocPtr pDoc = NULL;
	xmlNodePtr pNode, pInfo, pLinks;

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: ";
	sHeader += post_mime;
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-WSSE: ";
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: WSSE profile=\"UsernameToken\"";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pNode = xmlNewNode(NULL, (xmlChar*)"entry");
	xmlDocSetRootElement(pDoc, pNode);
	xmlSetProp(pNode, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"title", (xmlChar*)entry.title.c_str());
	xmlSetProp(pInfo, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"summary", (xmlChar*)entry.summary.c_str());
	xmlSetProp(pInfo, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"content", (xmlChar*)entry.content.value.c_str());
	xmlSetProp(pInfo, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");
	xmlSetProp(pInfo, (xmlChar*)"mode", (xmlChar*)entry.content.mode.c_str());
	xmlSetProp(pInfo, (xmlChar*)"type", (xmlChar*)entry.content.type.c_str());

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, "utf-8", 0);
	xmlFreeDoc(pDoc);
	pDoc = NULL;
	requestXml = (char*)pstrXml;
	xmlFree(pstrXml);
#ifdef _DEBUG
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", requestXml.c_str());
		fclose(fp);
	}
#endif

	curl_easy_setopt(curl, CURLOPT_URL, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestXml.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestXml.size());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || status != 201) goto leave;

	try {
		const char *ptr = responseBuf.c_str();
		while(isspace(*ptr)) ptr++;
		responseBuf.erase(0, ptr-responseBuf.c_str());
		pDoc = xmlParseDoc((xmlChar*)responseBuf.c_str());
		if (!pDoc) {
			ret = 0;
			goto leave;
		}
		if (pDoc->children && tstring((char*)pDoc->children->name) == "entry")
			pLinks = pDoc->children->children;
		else
			pLinks = NULL;
		while(pLinks) {
			tstring strName = (char*)pLinks->name;
			if (strName != "link") {
				pLinks = pLinks->next;
				continue;
			}
			AtomLink link;
			char* ptr;
			if ((ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"rel"))) link.rel = ptr;
			if ((ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"href"))) link.href = ptr;
			if ((ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"type"))) link.type = ptr;
			if ((ptr = (char*)xmlGetProp(pLinks, (xmlChar*)"title"))) link.title = ptr;
			if (link.rel == "service.edit") {
				entry.edit_uri = link.href;
				break;
			}
			pLinks = pLinks->next;
		}
	} catch(...) {
		goto leave;
	}
	ret = 0;
leave:
	responseBuf = "";
	if (pDoc) xmlFreeDoc(pDoc);
	curl_easy_cleanup(curl);
	return ret;
}

int createEntry_Blogger(tstring feed_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	AtomFeed feed = getFeed(feed_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_url;
	tstring post_mime = _T("application/x.atom+xml");
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == _T("service.post")) {
			post_url = it->href;
			post_mime =it->type;
		}
	}
	if (!post_url.size()) {
		for(it = feed.links.begin(); it != feed.links.end(); it++) {
			if (it->rel == _T("self")) {
				post_url = it->href;
				post_mime =it->type;
			}
		}
		if (!post_url.size())
			return -1;
	}
	int ret = -3;

	std::string bloggerAuth = getBloggerAuth(userid, passwd);

	std::string requestXml;
	xmlDocPtr pDoc = NULL;
	xmlNodePtr pNode, pInfo, pEntries, pEntry;

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: ";
	sHeader += post_mime;
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: GoogleLogin auth=";
	sHeader += bloggerAuth;
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pNode = xmlNewNode(NULL, (xmlChar*)"entry");
	xmlDocSetRootElement(pDoc, pNode);
	xmlSetProp(pNode, (xmlChar*)"xmlns", (xmlChar*)"http://www.w3.org/2005/Atom");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"title", (xmlChar*)entry.title.c_str());
	xmlSetProp(pInfo, (xmlChar*)"type", (xmlChar*)"text");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"summary", (xmlChar*)entry.summary.c_str());

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"published", (xmlChar*)get_time().c_str());

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"updated", (xmlChar*)get_time().c_str());

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"content", (xmlChar*)entry.content.value.c_str());
	xmlSetProp(pInfo, (xmlChar*)"mode", (xmlChar*)entry.content.mode.c_str());
	xmlSetProp(pInfo, (xmlChar*)"type", (xmlChar*)entry.content.type.c_str());

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, "utf-8", 0);
	xmlFreeDoc(pDoc);
	pDoc = NULL;
	requestXml = (char*)pstrXml;
	xmlFree(pstrXml);
#ifdef _DEBUG
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", requestXml.c_str());
		fclose(fp);
	}
#endif

	curl_easy_setopt(curl, CURLOPT_URL, post_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestXml.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestXml.size());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || (status != 0 && status != 201)) goto leave;

	try {
		const char *ptr = responseBuf.c_str();
		while(isspace(*ptr)) ptr++;
		responseBuf.erase(0, ptr-responseBuf.c_str());
		pDoc = xmlParseDoc((xmlChar*)responseBuf.c_str());
		if (pDoc) {
			pEntries = pDoc->children;
			while(pEntries) {
				tstring strName = (char*)pEntries->name;
				if (strName == "entry")
					break;
				if (strName == "feed") {
					pEntries = pEntries->children;
					break;
				}
				pEntries = pEntries->next;
			}
		} else
			pEntries = NULL;
		while(pEntries) {
			tstring strName = (char*)pEntries->name;
			if (strName != "entry") {
				pEntries = pEntries->next;
				continue;
			}
			pEntry = pEntries->children;
			AtomEntry entry;
			AtomLink link;
			while(pEntry) {
				strName = (char*)pEntry->name;
				if (strName == "link") {
					link.rel = (char*)xmlGetProp(pEntry, (xmlChar*)"rel");
					link.href = (char*)xmlGetProp(pEntry, (xmlChar*)"href");
					if (link.rel == _T("service.edit")) {
						entry.edit_uri = link.href;
						break;
					}
				}
				pEntry = pEntry->next;
			}
			pEntries = pEntries->next;
		}
	} catch(...) {
		goto leave;
	}
	ret = 0;
leave:
	responseBuf = "";
	if (pDoc) xmlFreeDoc(pDoc);
	curl_easy_cleanup(curl);
	return ret;
}

int updateEntry(tstring update_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	AtomFeed feed = getFeed(update_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_mime = _T("application/x.atom+xml");
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == _T("self")) {
			post_mime =it->type;
		}
	}
	int ret = -3;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	std::string requestXml;
	xmlDocPtr pDoc = NULL;
	xmlNodePtr pNode, pInfo;

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: application/x.atom+xml";
	sHeader += post_mime;
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-WSSE: ";
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-Http-Method-Override: PUT";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: WSSE profile=\"UsernameToken\"";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pNode = xmlNewNode(NULL, (xmlChar*)"entry");
	xmlDocSetRootElement(pDoc, pNode);
	xmlSetProp(pNode, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"title", (xmlChar*)entry.title.c_str());
	xmlSetProp(pInfo, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"summary", (xmlChar*)entry.summary.c_str());
	xmlSetProp(pInfo, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");
	xmlSetProp(pInfo, (xmlChar*)"type", (xmlChar*)"plain/text");

	if (entry.content.mode != "empty") {
		pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"content", (xmlChar*)entry.content.value.c_str());
		xmlSetProp(pInfo, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");
		xmlSetProp(pInfo, (xmlChar*)"mode", (xmlChar*)entry.content.mode.c_str());
		xmlSetProp(pInfo, (xmlChar*)"type", (xmlChar*)entry.content.type.c_str());
	} else {
		std::vector<AtomLink>::iterator it;
		for(it = entry.links.begin(); it != entry.links.end(); it++) {
			pInfo = xmlNewChild(pNode, NULL, (xmlChar*)"link", NULL);
			xmlSetProp(pInfo, (xmlChar*)"xmlns", (xmlChar*)"http://purl.org/atom/ns#");
			xmlSetProp(pInfo, (xmlChar*)"rel", (xmlChar*)it->rel.c_str());
			xmlSetProp(pInfo, (xmlChar*)"type", (xmlChar*)it->type.c_str());
			xmlSetProp(pInfo, (xmlChar*)"href", (xmlChar*)it->href.c_str());
		}
	}

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, "utf-8", 0);
	xmlFreeDoc(pDoc);
	pDoc = NULL;
	requestXml = (char*)pstrXml;
	xmlFree(pstrXml);
#ifdef _DEBUG
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", requestXml.c_str());
		fclose(fp);
	}
#endif

	curl_easy_setopt(curl, CURLOPT_URL, update_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestXml.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestXml.size());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || (status != 200 && status != 201)) goto leave;

	ret = 0;
leave:
	responseBuf = "";
	if (pDoc) xmlFreeDoc(pDoc);
	curl_easy_cleanup(curl);
	return ret;
}

int updateEntry_Blogger(tstring update_url, tstring userid, tstring passwd, AtomEntry& entry, tstring user_agent)
{
	int ret = -3;

	std::string bloggerAuth = getBloggerAuth(userid, passwd);

	std::string requestXml;
	xmlDocPtr pDoc = NULL;
	xmlNodePtr pNode, pInfo;

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: ";
	sHeader += "application/atom+xml;";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-Http-Method-Override: PUT";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: GoogleLogin auth=";
	sHeader += bloggerAuth;
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	pDoc = xmlNewDoc((xmlChar*)"1.0");
	pNode = xmlNewNode(NULL, (xmlChar*)"entry");
	xmlDocSetRootElement(pDoc, pNode);
	xmlSetProp(pNode, (xmlChar*)"xmlns", (xmlChar*)"http://www.w3.org/2005/Atom");

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"title", (xmlChar*)entry.title.c_str());

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"summary", (xmlChar*)entry.summary.c_str());

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"published", (xmlChar*)get_time().c_str());

	pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"updated", (xmlChar*)get_time().c_str());

	//pInfo = xmlNewTextChild(pNode, NULL, (xmlChar*)"content", (xmlChar*)entry.content.value.c_str());
	pInfo = xmlNewChild(pNode, NULL, (xmlChar*)"content", (xmlChar*)entry.content.value.c_str());
	xmlSetProp(pInfo, (xmlChar*)"mode", (xmlChar*)entry.content.mode.c_str());
	xmlSetProp(pInfo, (xmlChar*)"type", (xmlChar*)entry.content.type.c_str());

	xmlChar* pstrXml;
	int bufsize;
	xmlDocDumpFormatMemoryEnc(pDoc, &pstrXml, &bufsize, "utf-8", 0);
	xmlFreeDoc(pDoc);
	pDoc = NULL;
	requestXml = (char*)pstrXml;
	xmlFree(pstrXml);
#if 0
	{
		FILE *fp = fopen("req.xml", "w");
		fprintf(fp, "%s\n", requestXml.c_str());
		fclose(fp);
	}
#endif

	curl_easy_setopt(curl, CURLOPT_URL, update_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestXml.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestXml.size());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || (status != 200 && status != 201)) goto leave;

	ret = 0;
leave:
	responseBuf = "";
	if (pDoc) xmlFreeDoc(pDoc);
	curl_easy_cleanup(curl);
	return ret;
}

int deleteEntry(tstring delete_url, tstring userid, tstring passwd, tstring user_agent)
{
	AtomFeed feed = getFeed(delete_url, userid, passwd, user_agent);
	std::vector<AtomLink>::iterator it;
	tstring post_mime = _T("application/x.atom+xml");
	for(it = feed.links.begin(); it != feed.links.end(); it++) {
		if (it->rel == _T("self")) {
			post_mime =it->type;
		}
	}
	int ret = -3;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: ";
	sHeader += post_mime;
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-WSSE: ";
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-Http-Method-Override: DELETE";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: WSSE profile=\"UsernameToken\"";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, delete_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || (status != 200 && status != 201)) goto leave;

	ret = 0;
leave:
	responseBuf = "";
	curl_easy_cleanup(curl);
	return ret;
}

int deleteEntry_Blogger(tstring delete_url, tstring userid, tstring passwd, tstring user_agent)
{
	int ret = -3;

	std::string bloggerAuth = getBloggerAuth(userid, passwd);

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: ";
	sHeader += "application/atom+xml;";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-Http-Method-Override: DELETE";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: GoogleLogin auth=";
	sHeader += bloggerAuth;
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, delete_url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || status != 200) goto leave;

	ret = 0;
leave:
	responseBuf = "";
	curl_easy_cleanup(curl);
	return ret;
}

AtomEntries getEntries(tstring url, tstring userid, tstring passwd, tstring user_agent)
{
	AtomEntries ret;

#ifdef _UNICODE
	std::string username = XmlRpc::tstring2string(userid);
	std::string password = XmlRpc::tstring2string(passwd);
#else
	std::string username = userid;
	std::string password = passwd;
#endif
	std::string nonce = get_nonce();
	std::string post_time = get_time();
	std::string base64nonce = XmlRpc::base64_encode(
			(unsigned char const*)nonce.c_str(), nonce.size());
	std::string pass = nonce;
	pass += post_time;
	pass += password;
	pass = sha1(pass);
	std::string base64pass = XmlRpc::base64_encode(
			(unsigned char const*)pass.c_str(), pass.size());
	std::string authorization_header;
	authorization_header += "UsernameToken ";
	authorization_header += "Username=\"";
	authorization_header += username + "\", ";
	authorization_header += "PasswordDigest=\"";
	authorization_header += base64pass + "\", ";
	authorization_header += "Created=\"";
	authorization_header += post_time + "\", ";
	authorization_header += "Nonce=\"";
	authorization_header += base64nonce + "\"";

	xmlDocPtr pDoc = NULL;
	xmlNodePtr pEntries, pEntry;

	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;
	long status = 0; 

	curl = curl_easy_init();
	if (!curl) return ret;

	std::string sHeader;

	sHeader = "Content-Type: application/x.atom+xml";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "X-WSSE: ";
	sHeader += XmlRpc::string2tstring(authorization_header.c_str());
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	sHeader = "Authorization: WSSE profile=\"UsernameToken\"";
	headerlist = curl_slist_append(headerlist, sHeader.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
	responseBuf = "";
	res = curl_easy_perform(curl);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.xml", "wb");
		fprintf(fp, "%s", responseBuf.c_str());
		fclose(fp);
	}
#endif
	if (res != CURLE_OK) goto leave;

	res = curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status);
	if (res != CURLE_OK || status != 200) goto leave;

	try {
		const char *ptr = responseBuf.c_str();
		while(isspace(*ptr)) ptr++;
		responseBuf.erase(0, ptr-responseBuf.c_str());
		pDoc = xmlParseDoc((xmlChar*)responseBuf.c_str());
		if (pDoc) {
			pEntries = pDoc->children;
			while(pEntries) {
				if (tstring((char*)pEntries->name) == "feed") {
					pEntries = pEntries->children;
					break;
				}
				pEntries = pEntries->next;
			}
		} else
			pEntries = NULL;
		while(pEntries) {
			tstring strName = (char*)pEntries->name;
			if (strName != "entry") {
				pEntries = pEntries->next;
				continue;
			}
			pEntry = pEntries->children;
			AtomEntry entry;
			time_t nowtime = time(NULL);
			localtime_r(&nowtime, &entry.updated);
			localtime_r(&nowtime, &entry.published);
			while(pEntry) {
				strName = (char*)pEntry->name;
				if (strName == "id") entry.id = (char*)xmlNodeListGetString(pDoc, pEntry->xmlChildrenNode, 1);
				if (strName == "title") entry.title = (char*)xmlNodeListGetString(pDoc, pEntry->xmlChildrenNode, 1);
				if (strName == "content") {
					char* ptr;
					if ((ptr = (char*)xmlGetProp(pEntry, (xmlChar*)"type"))) entry.content.type = ptr;
					if ((ptr = (char*)xmlGetProp(pEntry, (xmlChar*)"mode"))) entry.content.mode = ptr;

					if (entry.content.mode == "xml") {
						xmlDocPtr content;
						xmlNodePtr root;
						xmlChar *buffer;
						int size = 0;
						
						content = xmlNewDoc((xmlChar *)"1.0");
						root = xmlDocCopyNode(pEntry, content, 1);
						xmlDocSetRootElement(content, root);
						xmlDocDumpFormatMemoryEnc(content, &buffer, &size, "UTF-8", 0);
						entry.content.value = (char*)buffer;
					} else {
						entry.content.value = (char*)xmlNodeListGetString(pDoc, pEntry->xmlChildrenNode, 1);
					}
				}
				if (strName == "updated" || strName == "modified") {
					tstring updated = (char*)pEntry->children->content;
					entry.updated = string_to_time(updated);
				}
				if (strName == "published" || strName == "issued") {
					tstring published = (char*)pEntry->children->content;
					entry.published = string_to_time(published);
				}
				if (strName == "link") {
					AtomLink link;
					link.rel = (char*)xmlGetProp(pEntry, (xmlChar*)"rel");
					link.href = (char*)xmlGetProp(pEntry, (xmlChar*)"href");
					link.type = (char*)xmlGetProp(pEntry, (xmlChar*)"type");
					if (link.rel == "self" || link.rel == "service.edit")
						entry.edit_uri = link.href;
					entry.links.push_back(link);
				}
				pEntry = pEntry->next;
			}
			ret.entries.push_back(entry);
			pEntries = pEntries->next;
		}
	} catch(...) {
	}
leave:
	responseBuf = "";
	if (pDoc) xmlFreeDoc(pDoc);
	curl_easy_cleanup(curl);
	return ret;
}
#endif
}
