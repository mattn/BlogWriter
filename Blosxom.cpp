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

#include "Blosxom.h"
#include "XmlRpcUtils.h"
#include <sstream>
#include <fstream>

namespace Blosxom {

typedef struct {
	size_t current;
	size_t size;
	void* data;
} FtpSteram;

int upload_cb(char *ptr, size_t size, size_t nmemb, void* stream)
{
	FtpSteram* ftpstream = (FtpSteram*)stream;
	size_t writesize = size*nmemb;
	if (ftpstream->current+writesize > ftpstream->size)
		writesize = ftpstream->size-ftpstream->current;
	memcpy(ptr, (char*)ftpstream->data+ftpstream->current, writesize);
	ftpstream->current += writesize;
	return writesize;
}

static char* response_data = NULL;
static size_t response_size = 0;
int download_cb(char* ptr, size_t size, size_t nmemb, void* stream)
{
	if (!response_data)
		response_data = (char*)malloc(size*nmemb);
	else
		response_data = (char*)realloc(response_data, response_size+size*nmemb);
	if (response_data) {
		memcpy(response_data+response_size, ptr, (size*nmemb));
		response_size += (size*nmemb);
	}
	return size*nmemb;
}

FolderList FtpFolders(tstring server, tstring userid, tstring passwd, tstring root)
{
	FolderList ret;
	CURL* curl;
	CURLcode res;
	
	curl = curl_easy_init();
	if (!curl) {
		return ret;
	}

	tstring auth = userid + ":";
	auth += passwd;

	tstring command = "NLST -Ra ";
	command += root;
	curl_easy_setopt(curl, CURLOPT_URL, server.c_str());
	curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
	curl_easy_setopt(curl, CURLOPT_FTPLISTONLY, true);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, command.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_cb);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		if (response_data) free(response_data);
		response_data = NULL;
		response_size = 0;
		return ret;
	}

	if (response_data && response_size > 0) {
		tstring data = tstring(response_data, response_size);
		data = XmlRpc::replace_string(data, "\r", "");
		data = XmlRpc::replace_string(data, "\\", "/");
		std::vector<tstring> list = XmlRpc::split_string(data, "\n");
		std::vector<tstring>::iterator it;
		bool have_root = false;
		for(it = list.begin(); it != list.end(); it++) {
			if (strchr(it->c_str(), ':')) {
				tstring file = it->substr(0, it->size()-1) + "/";;
				file = file.substr(root.size()-1);
				if (file == "/")
					have_root = true;
				ret[file] = file;
			}
		}
		if (!have_root)
			ret["/"] = "/";
	}

	if (response_data) free(response_data);
	response_data = NULL;
	response_size = 0;

	return ret;
}

FileList FtpFiles(tstring server, tstring userid, tstring passwd, tstring root)
{
	FileList ret;
	CURL* curl;
	CURLcode res;
	
	curl = curl_easy_init();
	if (!curl) {
		return ret;
	}

	tstring auth = userid + ":";
	auth += passwd;

	response_data = NULL;
	response_size = 0;

	curl_easy_setopt(curl, CURLOPT_URL, server.c_str());
	curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
	curl_easy_setopt(curl, CURLOPT_FTPLISTONLY, true);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "NLST -Rl ");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_cb);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		if (response_data) free(response_data);
		response_data = NULL;
		response_size = 0;
		return ret;
	}

	if (response_data && response_size > 0) {
		tstring data = tstring(response_data, response_size);
		data = XmlRpc::replace_string(data, "\r", "");
		data = XmlRpc::replace_string(data, "\\", "/");
		std::vector<tstring> list = XmlRpc::split_string(data, "\n");
		std::vector<tstring>::iterator it;
		bool have_root = false;
		tstring current_folder = "/";
		for(it = list.begin(); it != list.end(); it++) {
			tstring file = it->c_str();
			if (file.size() == 0) continue;
			if (file[file.size()-1] != ':') {
				std::vector<tstring> cols = XmlRpc::split_string(file, " ");
				if (file[0] != 'd' && cols.size() > 3) {
					FileInfo info;
					info["id"] = current_folder + cols[cols.size()-1];
					info["title"] = cols[cols.size()-1];
					tstring date;
					if (!isalnum(cols[cols.size()-4][0])) {
						info["description"] = cols[cols.size()-4] + " bytes contents";
						date += cols[cols.size()-3] + _T(" ");
						date += cols[cols.size()-2];
					} else {
						info["description"] = cols[cols.size()-5] + " bytes contents";
						date += cols[cols.size()-4] + _T("/");
						date += cols[cols.size()-3] + _T(" ");
						date += cols[cols.size()-2];
					}
					info["updated"] = date;
					ret.push_back(info);
				}
			} else {
				if (file.substr(0, 2) == "./")
					file = file.substr(1);
				file = file.substr(0, file.size()-1) + "/";
				file = file.substr(root.size()-1);
				current_folder = file;
			}
		}
	}

	if (response_data) free(response_data);
	response_data = NULL;
	response_size = 0;

	return ret;
}

int FtpDownload(tstring server, tstring userid, tstring passwd, tstring remotefile, tstring& text)
{
	CURL* curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl) {
		return -2;
	}

	tstring url = server;
	if (url.size() && url[url.size()-1] == '/')
		url += remotefile.substr(1);
	else
		url += remotefile;

	tstring auth = userid + ":";
	auth += passwd;

	response_data = NULL;
	response_size = 0;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_cb);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if(res != CURLE_OK) {
		if (response_data) free(response_data);
		response_data = NULL;
		response_size = 0;
		return -3;
	}

	if (response_data && response_size > 0) {
		text = tstring(response_data, response_size);
	}

	if (response_data) free(response_data);
	response_data = NULL;
	response_size = 0;
	return 0;
}

int FtpUpload(tstring server, tstring userid, tstring passwd, tstring text, tstring remotefile)
{
	CURL* curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl) {
		return -2;
	}

	tstring url = server;
	if (url.size() && url[url.size()-1] == '/')
		url += remotefile.substr(1);
	else
		url += remotefile;

	tstring auth = userid + ":";
	auth += passwd;

	int size = text.size();
	FtpSteram ftpstream;
	ftpstream.current = 0;
	ftpstream.data = (void*)text.c_str();
	ftpstream.size = text.size();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
	curl_easy_setopt(curl, CURLOPT_UPLOAD, true) ;
	curl_easy_setopt(curl, CURLOPT_READDATA, &ftpstream);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, upload_cb);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if(res != CURLE_OK)
		return -3;
	return 0;
}

int FtpDelete(tstring server, tstring userid, tstring passwd, tstring remotefile)
{
	CURL* curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl) {
		return -2;
	}

	tstring url = server;
	if (url.size() && url[url.size()-1] == '/')
		url += remotefile.substr(1);
	else
		url += remotefile;

	tstring auth = userid + ":";
	auth += passwd;

	response_data = NULL;
	response_size = 0;

	tstring command = "DELE ";
	command += remotefile;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
	curl_easy_setopt(curl, CURLOPT_FTPLISTONLY, 1);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, command.c_str());
	//curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_cb);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if(res != CURLE_FTP_COULDNT_RETR_FILE) {
		if (response_data) free(response_data);
		response_data = NULL;
		response_size = 0;
		return -3;
	}

	if (response_data) free(response_data);
	response_data = NULL;
	response_size = 0;
	return 0;
}

}
