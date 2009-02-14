#define _CRT_SECURE_NO_DEPRECATE
#ifdef _MSC_VER1
#include <windows.h>
#include <wininet.h>
#else
#include <curl/curl.h>
#include <errno.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "Flickr.h"
#include "XmlRpcUtils.h"
#include "AtomPP.h"
#include <sstream>
#include <fstream>

namespace Flickr {

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

std::string getFlickrFrob(std::string appkey, std::string secret)
{
	std::string sig;
	std::vector<XmlRpc::XmlRpcValue> requests;
	XmlRpc::XmlRpcValue::ValueStruct valuestruct;
	XmlRpc::XmlRpcValue response;
	requests.clear();
	valuestruct["api_key"] = appkey;
	sig = "";
	sig += "api_key";
	sig += appkey;
	sig = AtomPP::StringToHex(AtomPP::md5(secret + sig));
	valuestruct["api_sig"] = sig;

	requests.push_back(valuestruct);
	if (XmlRpc::execXmlRpc("http://api.flickr.com/services/xmlrpc/", "flickr.auth.getFrob", requests, response) != 0) {
		return "";
	}
	std::string frob = response.valueString();
	frob = XmlRpc::queryXml(frob, "/frob");
	return frob;
}

std::string getFlickrAuthUrl(std::string appkey, std::string secret, std::string frob)
{
	std::string ret = "http://flickr.com/services/auth/";
	std::string data;

	ret += "?api_key=";
	data += "api_key";
	ret += appkey;
	data += appkey;
	ret += "&frob=";
	data += "frob";
	ret += frob;
	data += frob;
	ret += "&perms=";
	data += "perms";
	ret += "write";
	data += "write";

	std::string sig = secret;
	sig = AtomPP::StringToHex(AtomPP::md5(sig + data));

	ret += "&api_sig=";
	ret += sig;
	return ret;
}

std::string getFlickrToken(std::string appkey, std::string secret, std::string frob)
{
	std::string sig;
	std::vector<XmlRpc::XmlRpcValue> requests;
	XmlRpc::XmlRpcValue::ValueStruct valuestruct;
	XmlRpc::XmlRpcValue response;
	requests.clear();
	valuestruct["api_key"] = appkey;
	valuestruct["frob"] = frob;
	sig = "";
	sig += "api_key";
	sig += appkey;
	sig += "frob";
	sig += frob;
	sig = AtomPP::StringToHex(AtomPP::md5(secret + sig));
	valuestruct["api_sig"] = sig;

	requests.push_back(valuestruct);
	if (XmlRpc::execXmlRpc("http://api.flickr.com/services/xmlrpc/", "flickr.auth.getToken", requests, response) != 0) {
		return "";
	}
	std::string token = response.valueString();
	token = XmlRpc::queryXml(token, "/auth/token");
	return token;
}

int registerFlickrToken(std::string url)
{
	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;

	curl = curl_easy_init();
	if (!curl) {
		return -2;
	}

	std::string sig;

	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	std::map<std::string, std::string> formdata;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
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
	curl_easy_cleanup(curl);

	if(res != CURLE_OK) {
		responseBuf = "";
		return -3;
	}
	responseBuf = "";
	return 0;
}

int FlickrUpload(std::string appkey, std::string secret, std::string token, std::string filename)
{
	CURL* curl;
	CURLcode res;
	struct curl_slist *headerlist = NULL;

	curl = curl_easy_init();
	if (!curl) {
		return -2;
	}

	std::string sig;

	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	std::map<std::string, std::string> formdata;

	curl_easy_setopt(curl, CURLOPT_URL, "http://api.flickr.com/services/upload/");

	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME, "api_key",
		CURLFORM_COPYCONTENTS, appkey.c_str(),
		CURLFORM_END);
	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME, "auth_token",
		CURLFORM_COPYCONTENTS, token.c_str(),
		CURLFORM_END);
	sig = "";
	sig += "api_key";
	sig += appkey;
	sig += "auth_token";
	sig += token;
	sig = AtomPP::StringToHex(AtomPP::md5(secret + sig));
	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME, "api_sig",
		CURLFORM_COPYCONTENTS, sig.c_str(),
		CURLFORM_END);
	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME, "photo",
		CURLFORM_FILE, filename.c_str(),
		CURLFORM_CONTENTTYPE, "image/jpeg",
		CURLFORM_END);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);

	responseBuf = "";
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if(res != CURLE_OK) {
		responseBuf = "";
		return -3;
	}
	std::string photoid = XmlRpc::queryXml(responseBuf, "/rsp/photoid");
	return 0;
}

}
