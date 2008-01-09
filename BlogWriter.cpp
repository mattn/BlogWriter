#define _CRT_SECURE_NO_DEPRECATE 1
#pragma warning (disable : 4503)
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>
#include <glib/gthread.h>
#include <curl/curl.h>
#include <libintl.h>
#include <locale.h>
#include <errno.h>
#include <string.h>
#include "XmlRpcValue.h"
#include "XmlRpcUtils.h"
#include "AtomPP.h"
#include "Blosxom.h"
#include "Flickr.h"

/*
#include "loading-icon.h"
*/

#include <boost/regex.hpp>

#ifndef _
# define _(x) (gettext(x))
#endif

#define APP_TITLE "BlogWriter"
#define URL_ACCEPT_LETTER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789;/?:@&=+$,-_.!~*'()%"

#define BLOGWRITER_FLICKR_APIKEY "3734d14ce1cd55222d21b3ba14af3513"
#define BLOGWRITER_FLICKR_SECRET "640e2d70cc85d251"

typedef enum {
	TARGET_URI_LIST,
	TARGET_UTF8_STRING,
	TARGET_TEXT,
	TARGET_COMPOUND_TEXT,
	TARGET_STRING,
	TARGET_COLOR,
	TARGET_BGIMAGE,
	TARGET_RESET_BG,
	TARGET_TEXT_PLAIN,
	TARGET_MOZ_URL
} DragDataType;

typedef enum {
	COLUMN_POSTID,
	COLUMN_DATE,
	COLUMN_TITLE,
	COLUMN_DESCRIPTION,
	NUM_COLUMNS
} PostListColumn;

typedef enum {
	PROTOCOL_UNKNOWN,
	PROTOCOL_ATOMPP,
	PROTOCOL_XMLRPC
} ProtocolType;

typedef enum {
	MAYBE_UNKNOWN,
	MAYBE_BLOGGER_ATOMPP,
	MAYBE_AMEBA_ATOMPP,
	MAYBE_MOVABLETYPE,
	MAYBE_HATENA_BOOKMARK,
	MAYBE_HATENA_FOTOLIFE
} GuessServer;

typedef enum {
	BUFFER_INSERT_TEXT,
	BUFFER_INSERT_PIXBUF,
	BUFFER_DELETE_TEXT,
	BUFFER_DELETE_PIXBUF,
	BUFFER_APPLY_TAG,
	BUFFER_REMOVE_TAG
} UndoAction;

typedef enum {
	GET_SOURCE_UPLOAD,
	GET_SOURCE_SAVE,
	GET_SOURCE_CHECK
} GetSourceAction;

typedef struct {
  UndoAction	action;
  GType			type;
  gpointer		data;
  gint			start;
  gint			end;
  gint			sequence;
} UndoInfo;

typedef struct {
	std::string name;
	gchar* url;
} UrlListInfo;

typedef enum {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT
} AlignDirect;

typedef std::map<std::string, std::string>	Config;
typedef std::map<std::string, std::string>	CategoryList;
typedef std::map<std::string, Config>		ConfigList;

std::string		user_profile;
ConfigList		user_configs;
CategoryList	current_category_list;
std::string		current_postid;
std::string		module_path;
GuessServer		current_server = MAYBE_UNKNOWN;

GtkWidget* toplevel = NULL;
GList* undo_data;
GList* undo_current = NULL;

static guchar* response_data = NULL;
static size_t response_size = 0;

GtkTextTag* bold_tag;
const gchar* bold_tag_opening_tag = "<b>";
const gchar* bold_tag_closing_tag = "</b>";
GtkTextTag* italic_tag;
const gchar* italic_tag_opening_tag = "<i>";
const gchar* italic_tag_closing_tag = "</i>";
GtkTextTag* strike_tag;
const gchar* strike_tag_opening_tag = "<i>";
const gchar* strike_tag_closing_tag = "</i>";
GtkTextTag* underline_tag;
const gchar* underline_tag_opening_tag = "<u>";
const gchar* underline_tag_closing_tag = "</u>";
const gchar* image_tag_opening_tag = "<img>";
const gchar* image_tag_closing_tag = "";
const gchar* link_tag_opening_tag = "<a>";
const gchar* link_tag_closing_tag = "</a>";
GtkTextTag* h1_tag;
const gchar* h1_tag_opening_tag = "<h1>";
const gchar* h1_tag_closing_tag = "</h1>";
GtkTextTag* h2_tag;
const gchar* h2_tag_opening_tag = "<h2>";
const gchar* h2_tag_closing_tag = "</h2>";
GtkTextTag* h3_tag;
const gchar* h3_tag_opening_tag = "<h3>";
const gchar* h3_tag_closing_tag = "</h3>";
GtkTextTag* pre_tag;
const gchar* pre_tag_opening_tag = "<pre>";
const gchar* pre_tag_closing_tag = "</pre>";
GtkTextTag* blockquote_tag;
const gchar* blockquote_tag_opening_tag = "<blockquote>";
const gchar* blockquote_tag_closing_tag = "</blockquote>";
GtkTextTag* align_center_tag;
const gchar* align_center_tag_opening_tag = "<p align=\"center\">";
const gchar* align_center_tag_closing_tag = "</p>";
GtkTextTag* align_right_tag;
const gchar* align_right_tag_opening_tag = "<p align=\"right\">";
const gchar* align_right_tag_closing_tag = "</p>";

gint image_num = 0;
gint align_num = 0;
gint link_num = 0;

typedef struct {
	int index;
	char* description;
	char* value;
} combo_entry_data;
combo_entry_data engine_entry[] = {
	{ 0, "XML Remote Procedure Call",	"xmlrpc" },
	{ 1, "Atom Publishing Protocol",	"atompp" },
	{ 2, "Blosxom Type FTP Upload",		"ftpup" },
	{ -1, NULL, NULL }
};
combo_entry_data format_entry[] = {
	{ 0, "HTML", "html" },
	{ 1, "Wiki", "wiki" },
	{ -1, NULL, NULL }
};

typedef enum {
	SETTING_UNKNOWN,
	SETTING_PATH,
	SETTING_FONT
} setting_check_type;

typedef struct {
	char* label;
	char* key;
	setting_check_type type;
	char* default_value;
	GtkWidget* widget;
} setting_table_def;

typedef struct {
	GThreadFunc func;
	bool processing;
	gpointer data;
	gpointer retval;
} ProcessThreadInfo;
GtkWidget* process_message_label = NULL;

gpointer process_thread(gpointer data)
{
	ProcessThreadInfo* info = (ProcessThreadInfo*)data;
#ifdef _MSC_VER
	CoInitialize(NULL);
#endif

	info->retval = info->func(info->data);
	info->processing = false;

#ifdef _MSC_VER
	CoUninitialize();
#endif
	return info->retval;
}

void process_message(std::string message)
{
	if (process_message_label) {
		gtk_label_set_text(GTK_LABEL(process_message_label), message.c_str());
	}
}

gpointer process_func(GThreadFunc func, gpointer data, GtkWidget* parent = NULL, std::string message = "")
{
	GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
	if (parent)
		parent = gtk_widget_get_toplevel(parent);

	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_widget_hide_on_delete(window);

	GtkWidget* vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(window), vbox);

#if 0
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_inline(-1, my_pixbuf, FALSE, NULL);
	GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
#else
	GtkWidget* image = gtk_image_new_from_file("loading.gif");
#endif

	gtk_container_add(GTK_CONTAINER(vbox), image);
	gtk_widget_show_all(vbox);

	if (message.size()) {
		GtkWidget* label = gtk_label_new(message.c_str());
		gtk_container_add(GTK_CONTAINER(vbox), label);
	}

	GdkColor color;
	gdk_color_parse("#F0F0F0", &color);
	gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &color);

	gtk_widget_queue_resize(window);

	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

	if (parent) {
		gtk_window_set_transient_for(
				GTK_WINDOW(window),
				GTK_WINDOW(parent));
	}

	gtk_widget_show_all(window);

    GdkCursor* cursor = gdk_cursor_new(GDK_WATCH);
	if (parent) {
		gtk_widget_set_sensitive(parent, false);
		gdk_window_set_cursor(parent->window, cursor);
	}
    gdk_window_set_cursor(window->window, cursor);
	gdk_flush();
    gdk_cursor_destroy(cursor);

	gdk_threads_leave();

	ProcessThreadInfo info;
	info.func = func;
	info.data = data;
	info.retval = NULL;
	info.processing = true;
	GError *error = NULL;
	GThread* thread = g_thread_create(
		process_thread,
		&info,
		true,
		&error);
	while(info.processing) {
		gdk_threads_enter();
		while(gtk_events_pending())
			gtk_main_iteration_do(TRUE);
		gdk_threads_leave();
		g_thread_yield();
	}
	g_thread_join(thread);

	gdk_threads_enter();
	gtk_widget_hide(window);

	if (parent) {
	    gdk_window_set_cursor(parent->window, NULL);
		gtk_widget_set_sensitive(parent, true);
	}
	return info.retval;
}

bool get_font_info(std::string fontstr, int& size, std::string& family)
{
	if (fontstr.size()) {
		gchar* tmp;
		gchar* ptr;
		gchar* old;
		old = ptr = g_strdup(fontstr.c_str());
		tmp = ptr = strrchr(old, ' ');
		while(ptr && *tmp) {
			tmp++;
			if (*tmp && !isdigit(*tmp))
				break;
		}
		if (ptr && *tmp == 0) *ptr++ = 0;
		else ptr = NULL;

		if (!ptr) size = 10;
		else size = atol(ptr);
		family = old;
		g_free(old);
		return true;
	}
	return false;
}

gboolean text_iter_has_link_tag(GtkTextIter* iter)
{
	GSList* tags = gtk_text_iter_get_tags(iter);
	while(tags) {
		gchar* name = (gchar*)g_object_get_data(G_OBJECT(tags->data), "name");
		if (name && !g_strcasecmp(name, "a")) return true;
		tags = g_slist_next(tags);
	}
	return false;
}

std::string sanitize_html(std::string& html, gboolean preview = FALSE)
{
	std::string ret = html;
	ret = XmlRpc::replace_string(ret, "\r", "");
	if (preview) {
		ret = XmlRpc::replace_string(ret, "\n", "");
		ret = XmlRpc::replace_string(ret, "\t", "");
		ret = XmlRpc::replace_string(ret, "\x02", "\t");
	}
	std::string plain_text;
	while(TRUE) {
		size_t nb = ret.find_first_of("<");
		size_t ne = ret.find_first_of(">");
		if (nb == std::string::npos ||
			ne == std::string::npos ||
			nb > ne) {
			if (preview) ret = XmlRpc::trim_string(ret);
			plain_text += ret;
			break;
		}
		std::string tag = ret.substr(nb, ne-nb+1);
		std::string text = ret.substr(0, nb);
		plain_text += text;
		if (tag.substr(0, 4) == "<!--" && !isalnum(tag[4])) {
			ne = ret.find("-->");
			if (ne != std::string::npos) {
				ret = ret.substr(ne+3);
				continue;
			}
		}
		ret = ret.substr(ne+1);
	}
	plain_text = XmlRpc::replace_string(plain_text, "&quot;", "\"");
	plain_text = XmlRpc::replace_string(plain_text, "&gt;", ">");
	plain_text = XmlRpc::replace_string(plain_text, "&lt;", "<");
	plain_text = XmlRpc::replace_string(plain_text, "&nbsp;", " ");
	plain_text = XmlRpc::replace_string(plain_text, "&amp;", "&");
	if (strstr(plain_text.c_str(), "&#")) {
		char* ptr = strdup(plain_text.c_str());
		char* org = ptr;
		ret = "";
		while(*ptr) {
			if (!memcmp(ptr, "&#", 2)) {
				char* fnd = ptr+2;
				while(isdigit(*fnd)) fnd++;
				if (*fnd == ';') {
					unsigned char bytes[9] = {0};
					int len = XmlRpc::utf_char2bytes(atol(ptr+2), bytes);
					bytes[len] = 0;
					ret += (char*)bytes;
					ptr = fnd + 1;
					continue;
				}
			}
			ret += *ptr++;
		}
		free(org);
	} else
		ret = plain_text;
	if (preview) {
		ret = XmlRpc::trim_string(ret);
		while(ret.find("  ") != std::string::npos)
			ret = XmlRpc::replace_string(ret, "  ", " ");
	}
	return ret;
}

void append_buffer_undo_info(UndoAction action, GType type, gint start, gint end, gpointer data, gint sequence)
{
	UndoInfo* undoInfo = (UndoInfo*)g_malloc(sizeof(UndoInfo));
	undoInfo->action = action;
	undoInfo->type = type;
	undoInfo->data = data;
	undoInfo->start = start;
	undoInfo->end = end;
	undoInfo->sequence = sequence;
	undo_data = g_list_append(undo_data, undoInfo);
	undo_current = g_list_last(undo_data);
}

void destroy_buffer_undo_info(GList* current)
{
	UndoInfo* undoInfo = (UndoInfo*)g_list_nth_data(g_list_last(current), 0);
	undo_data = g_list_delete_link(undo_data, current);
	if (undoInfo->type == G_TYPE_STRING) {
		g_free(undoInfo->data);
	} else
	if (undoInfo->type == GDK_TYPE_PIXBUF) {
		gdk_pixbuf_unref(GDK_PIXBUF(undoInfo->data));
	} else
	if (undoInfo->type == GTK_TYPE_TEXT_TAG) {
		gchar* name = (gchar*)g_object_get_data(G_OBJECT(undoInfo->data), "name");
		if (name && !g_strcasecmp(name, "img")) {
			g_free(name);
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "title"));
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "url"));
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "type"));
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "emoji"));
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "movie"));
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "embed"));
			GdkPixbuf* real_pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(undoInfo->data), "real_pixbuf");
			if (real_pixbuf) gdk_pixbuf_unref(real_pixbuf);
		} else
		if (name && !g_strcasecmp(name, "a")) {
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "title"));
			g_free(g_object_get_data(G_OBJECT(undoInfo->data), "url"));
		}
		g_object_unref(G_OBJECT(undoInfo->data));
	}
	g_free(undoInfo);
}

void remove_buffer_undo_info(GList* current)
{
	UndoInfo* undoInfo = (UndoInfo*)g_list_nth_data(g_list_last(current), 0);
	undo_data = g_list_delete_link(undo_data, current);
	if (undoInfo->type == G_TYPE_STRING) {
		g_free(undoInfo->data);
	} else
	if (undoInfo->type == GDK_TYPE_PIXBUF) {
		gdk_pixbuf_unref(GDK_PIXBUF(undoInfo->data));
	} else
	if (undoInfo->type == GTK_TYPE_TEXT_TAG) {
		g_object_unref(G_OBJECT(undoInfo->data));
	}
	g_free(undoInfo);
}

void commit_undo_info()
{
	while(undo_current && undo_current != g_list_last(undo_data)) {
		remove_buffer_undo_info(g_list_last(undo_data));
	}
}

void clear_undo_info()
{
	undo_current = undo_data;
	commit_undo_info();
}

void destroy_undo_info()
{
	undo_current = undo_data;
	while(undo_current && undo_current != g_list_last(undo_data)) {
		destroy_buffer_undo_info(g_list_last(undo_data));
	}
	image_num = 0;
	link_num = 0;
}

bool has_undo_info()
{
	return g_list_length(undo_data) > 1;
}

void selected_file_update_preview_cb(GtkFileChooser *file_chooser, gpointer data)
{
	GtkWidget *preview;
	char *filename;
	GdkPixbuf *pixbuf = NULL;

	preview = GTK_WIDGET(data);
	filename = gtk_file_chooser_get_preview_filename(file_chooser);
	if (filename) {
		if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
			pixbuf = gdk_pixbuf_new_from_file_at_size(filename, 128, 128, NULL);
		g_free(filename);
	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(preview), pixbuf);
	if (pixbuf)
		gdk_pixbuf_unref(pixbuf);

	gtk_file_chooser_set_preview_widget_active(file_chooser, (pixbuf != NULL));
}

std::string get_selected_file(GtkWidget* widget, bool isOpen, std::string name, std::vector<std::string>& filters, std::vector<std::string>& mimes)
{
	GtkWidget* dialog;
	GtkFileFilter* filter;
	GtkWidget* preview;

	dialog = gtk_file_chooser_dialog_new(isOpen ? _("File Select...") : _("Save..."),
			GTK_WINDOW(gtk_widget_get_toplevel(widget)),
			isOpen ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			isOpen ? GTK_STOCK_OPEN : GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	preview = gtk_image_new();
	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog), preview);
	g_signal_connect(GTK_FILE_CHOOSER(dialog), "update-preview", G_CALLBACK (selected_file_update_preview_cb), preview);

	filter = gtk_file_filter_new();
	if (name.size())
		gtk_file_filter_set_name(filter, name.c_str());
	else
		gtk_file_filter_set_name(filter, _("Any File"));

	std::vector<std::string>::iterator it;
	for(it = filters.begin(); it != filters.end(); it++)
		gtk_file_filter_add_pattern(filter, it->c_str());
	for(it = mimes.begin(); it != mimes.end(); it++)
		gtk_file_filter_add_mime_type(filter, it->c_str());

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(toplevel));

	std::string ret;
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		const gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		ret = filename;
	}
	gtk_widget_destroy(dialog);
	return ret;
}

void error_dialog(GtkWidget* widget, std::string message, std::string iconfile = "") {
	gdk_threads_enter();

	GtkWidget* dialog;
	dialog = gtk_message_dialog_new(
			GTK_WINDOW(gtk_widget_get_toplevel(widget)),
			(GtkDialogFlags)(GTK_DIALOG_MODAL),
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			message.c_str());
	if (iconfile.size()) {
		GtkWidget* image = gtk_image_new_from_file(iconfile.c_str());
		if (image) {
#if GTK_CHECK_VERSION(2,10,0)
			gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog), image);
#else
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), image); 
#endif
			gtk_widget_show(image);
		}
	}
	gtk_window_set_title(GTK_WINDOW(dialog), _(APP_TITLE));
	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gdk_threads_leave();
}

bool confirm_dialog(GtkWidget* widget, std::string message, std::string iconfile = "") {
	gdk_threads_enter();

	GtkWidget* dialog;
	dialog = gtk_message_dialog_new(
			GTK_WINDOW(gtk_widget_get_toplevel(widget)),
			(GtkDialogFlags)(GTK_DIALOG_MODAL),
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO,
			message.c_str());
	if (iconfile.size()) {
		GtkWidget* image = gtk_image_new_from_file(iconfile.c_str());
		if (image) {
#if GTK_CHECK_VERSION(2,10,0)
			gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog), image);
#else
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), image); 
#endif
			gtk_widget_show(image);
		}
	}
	gtk_window_set_title(GTK_WINDOW(dialog), _(APP_TITLE));
	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	int res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gdk_threads_leave();
	return (res == GTK_RESPONSE_YES);
}

bool message_dialog(GtkWidget* widget, std::string message, std::string iconfile = "") {
	gdk_threads_enter();

	GtkWidget* dialog;
	dialog = gtk_message_dialog_new(
			GTK_WINDOW(gtk_widget_get_toplevel(widget)),
			(GtkDialogFlags)(GTK_DIALOG_MODAL),
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			message.c_str());
	if (iconfile.size()) {
		GtkWidget* image = gtk_image_new_from_file(iconfile.c_str());
		if (image) {
#if GTK_CHECK_VERSION(2,10,0)
			gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog), image);
#else
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), image); 
#endif
			gtk_widget_show(image);
		}
	}
	gtk_window_set_title(GTK_WINDOW(dialog), _(APP_TITLE));
	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	int res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gdk_threads_leave();
	return (res == GTK_RESPONSE_YES);
}

int handle_returned_data(char* ptr, size_t size, size_t nmemb, void* stream)
{
	if (!response_data)
		response_data = (guchar*)g_malloc(size*nmemb);
	else
		response_data = (guchar*)g_realloc(response_data, response_size+size*nmemb);
	if (response_data) {
		memcpy(response_data+response_size, ptr, size*nmemb);
		response_size += size*nmemb;
	}
	return size*nmemb;
}

std::string url2mime(const gchar* url, GError** error)
{
	CURLcode res;
	std::string ret;
	GError* _error = NULL;
	CURL* curl;

	response_data = NULL;
	response_size = 0;
	if (!strncmp(url, "file:///", 8) || g_file_test(url, G_FILE_TEST_EXISTS)) {
		if (!stricmp(url+strlen(url)-4, ".avi")) ret = "video/avi";
		if (!stricmp(url+strlen(url)-4, ".mp3")) ret = "audio/mp3";
		if (!stricmp(url+strlen(url)-4, ".txt")) ret = "text/plain";
		if (!stricmp(url+strlen(url)-4, ".htm")) ret = "text/html";
		if (!stricmp(url+strlen(url)-5, ".html")) ret = "text/html";
	} else {
		curl = curl_easy_init();
		if (!curl) return NULL;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, handle_returned_data);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res == CURLE_OK) {
			ret = (char*)response_data;
		} else
			_error = g_error_new_literal(G_FILE_ERROR, res, curl_easy_strerror(res));
	}

	if (response_data) g_free(response_data);
	response_data = NULL;
	response_size = 0;
	if (error && _error) *error = _error;
	return ret;
}

GdkPixbuf* url2pixbuf(const gchar* url, GError** error, const gchar* type = NULL)
{
	CURLcode res;
	GdkPixbuf* pixbuf = NULL;
	GdkPixbufLoader* loader = NULL;
	GdkPixbufFormat* format = NULL;
	GError* _error = NULL;
	CURL* curl;

	response_data = NULL;
	response_size = 0;
	if (!strncmp(url, "file:///", 8) || g_file_test(url, G_FILE_TEST_EXISTS)) {
		gchar* newurl = g_filename_from_uri(url, NULL, NULL);
		pixbuf = gdk_pixbuf_new_from_file(newurl ? newurl : url, &_error);
	} else {
		curl = curl_easy_init();
		if (!curl) return NULL;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res == CURLE_OK) {
			if (type)
				loader = gdk_pixbuf_loader_new_with_type(type, error);
			if (!loader)
				loader = gdk_pixbuf_loader_new();
			if (gdk_pixbuf_loader_write(loader, response_data, response_size, &_error)) {
				pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
				format = gdk_pixbuf_loader_get_format(loader);
			}
			gdk_pixbuf_loader_close(loader, NULL);
		} else
			_error = g_error_new_literal(G_FILE_ERROR, res, curl_easy_strerror(res));
	}

	if (response_data) g_free(response_data);
	response_data = NULL;
	response_size = 0;
	if (error && _error) *error = _error;
	return pixbuf;
}

std::string url2string(const gchar* url, GError** error)
{
	CURLcode res;
	GError* _error = NULL;
	CURL* curl;
	std::string ret;
	int status = 0;

	if (!strncmp(url, "file:///", 8) || g_file_test(url, G_FILE_TEST_EXISTS)) {
		gchar* newurl = g_filename_from_uri(url, NULL, NULL);
		response_data = NULL;
		response_size = 0;
		if (g_file_get_contents(newurl ? newurl : url, (char**)&response_data, &response_size, &_error))
			ret = std::string((char*)response_data, response_size);
	} else {
		curl = curl_easy_init();
		if (!curl) return NULL;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_returned_data);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		response_data = NULL;
		response_size = 0;
		res = curl_easy_perform(curl);
		res = res == CURLE_OK ? curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &status) : res;
		curl_easy_cleanup(curl);
		if (res == CURLE_OK && status == 200)
			ret = std::string((char*)response_data, response_size);
		else
			_error = g_error_new_literal(G_FILE_ERROR, res, curl_easy_strerror(res));
	}

	if (response_data) g_free(response_data);
	response_data = NULL;
	response_size = 0;
	if (error && _error) *error = _error;
	return ret;
}

std::string get_flv_from_movie(std::string movie)
{
	std::string command = user_configs["global"]["ffmpeg_command"];
	if (command.size() == 0) command = "ffmpeg";
	command += " -i ";
	command += "\"";
	command += movie.c_str();
	command += "\"";
	command += " -s 320x240 -ar 44100 -r 12 ";
	command += module_path;
	command += "/player.flv";
	gchar *pstdout = NULL, *pstderr = NULL;
	g_spawn_command_line_sync(command.c_str(), &pstdout, &pstderr, NULL, NULL);
	return module_path + "/player.flv";
}

std::string upload_file(std::string prefix, std::string name, std::string file, std::string& errmsg)
{
	if (user_configs[user_profile]["engine"] == "xmlrpc")
	{
		XmlRpc::XmlRpcValue::ValueArray valuearray;
		XmlRpc::XmlRpcValue::ValueStruct content;
		std::vector<XmlRpc::XmlRpcValue> requests;
		XmlRpc::XmlRpcValue response;

		std::string mime_type = "application/octet-stream";
		std::string file_name = prefix + std::string("_") + name;
		gchar* buff = NULL;
		gsize buffer_size = 0;
		g_file_get_contents(file.c_str(), &buff, &buffer_size, NULL);
		content["name"] = file_name;
		content["type"] = mime_type;
		content["bits"] = XmlRpc::XmlRpcValue(buff, buffer_size);
		g_free(buff);

		requests.clear();
		requests.push_back(user_configs[user_profile]["blogid"]);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		requests.push_back(content);
		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "metaWeblog.newMediaObject", requests, response) == 0) {
			if (response.getType() == XmlRpc::XmlRpcValue::TypeStruct)
				return response["url"];
			else
				return response.valueString();
		} else {
			std::string faultMessage = _("Can't upload file:");
			faultMessage += "\n";
			faultMessage += XmlRpc::getFaultResult(response);
			errmsg = faultMessage;
		}
	} else
	if (user_configs[user_profile]["engine"] == "atompp")
	{
		AtomPP::AtomEntry entry;
		entry.content.type = "application/octet-stream";

		gchar *buff = NULL;
		gsize buffer_size = 0;
		g_file_get_contents(file.c_str(), &buff, &buffer_size, NULL);

		entry.title = name;
		entry.summary = name;
		entry.content.value = XmlRpc::base64_encode((unsigned char*)buff, buffer_size);
		int ret = AtomPP::createEntry(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"], entry);
		if (ret == 0)
			return entry.edit_uri;
	}
	return "";
}

std::string upload_image(std::string prefix, std::string name, std::string type, std::string title, GdkPixbuf* pixbuf, std::string& errmsg)
{
	if (user_configs[user_profile]["engine"] == "xmlrpc")
	{
		XmlRpc::XmlRpcValue::ValueArray valuearray;
		XmlRpc::XmlRpcValue::ValueStruct content;
		std::vector<XmlRpc::XmlRpcValue> requests;
		XmlRpc::XmlRpcValue response;

		std::string mime_type = std::string("image/") + type;
		std::string file_name = prefix + std::string("_") + name;
		gchar* buff = NULL;
		gsize buffer_size = 0;
		gdk_pixbuf_save_to_buffer(pixbuf, &buff, &buffer_size, type.c_str(), NULL, NULL);
		content["name"] = file_name;
		content["type"] = mime_type;
		content["bits"] = XmlRpc::XmlRpcValue(buff, buffer_size);
		g_free(buff);

		requests.clear();
		requests.push_back(user_configs[user_profile]["blogid"]);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		requests.push_back(content);
		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "metaWeblog.newMediaObject", requests, response) == 0) {
			if (response.getType() == XmlRpc::XmlRpcValue::TypeStruct)
				return response["url"];
			else
				return response.valueString();
		} else {
			std::string faultMessage = _("Can't upload file:");
			faultMessage += "\n";
			faultMessage += XmlRpc::getFaultResult(response);
			errmsg = faultMessage;
		}
	} else
	if (user_configs[user_profile]["engine"] == "atompp")
	{
		AtomPP::AtomEntry entry;
		entry.content.type = "image/";
		entry.content.type += type;

		gchar* buff = NULL;
		gsize buffer_size = 0;
		gdk_pixbuf_save_to_buffer(pixbuf, &buff, &buffer_size, type.c_str(), NULL, NULL);

		entry.title = title;
		entry.summary = type;
		entry.content.value = XmlRpc::base64_encode((unsigned char*)buff, buffer_size);
		int ret = AtomPP::createEntry(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"], entry);
		if (ret == 0)
			return entry.edit_uri;
	}
	return "";
}

std::string upload_movie(std::string prefix, std::string url, std::string type, std::string movie, std::string& errmsg)
{
	std::string tempmovie = get_flv_from_movie(movie.c_str());
	std::string newmovie = upload_file(prefix, g_path_get_basename(tempmovie.c_str()), tempmovie, errmsg);
	if (newmovie.size() == 0) return "";

	gchar *buff = NULL;
	gsize buffer_size = 0;
	g_file_get_contents((module_path + "/flowplayer.html").c_str(), &buff, &buffer_size, NULL);
	std::string newplayer = buff;
	std::string tempplayer = tempmovie + ".html";
	newplayer = XmlRpc::replace_string(newplayer, "$(FLVFILE)", newmovie);
	newplayer = XmlRpc::replace_string(newplayer, "$(FLVWIDTH)", "320");
	newplayer = XmlRpc::replace_string(newplayer, "$(FLVHEIGHT)", "240");
	g_file_set_contents(tempplayer.c_str(), newplayer.c_str(), newplayer.size(), NULL);
	newplayer = upload_file(prefix, g_path_get_basename(tempplayer.c_str()), tempplayer, errmsg);
	g_unlink(tempplayer.c_str());
	g_unlink(tempmovie.c_str());
	return newplayer;
}

void set_html(GtkTextBuffer* buffer, std::string html)
{
	std::string ret = html;
	gboolean bold_prev = FALSE;
	gboolean italic_prev = FALSE;
	gboolean underline_prev = FALSE;
	gboolean strike_prev = FALSE;
	gboolean link_prev = FALSE;
	gboolean h1_prev = FALSE;
	gboolean h2_prev = FALSE;
	gboolean h3_prev = FALSE;
	gboolean p_prev = FALSE;
	gboolean pre_prev = FALSE;
	gboolean align_center_prev = FALSE;
	gboolean align_right_prev = FALSE;
	gboolean blockquote_prev = FALSE;
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* iter_orig = new GtkTextIter;
	std::string url_text;
	size_t nb;
	size_t ne;
	std::string tag;
	std::string text;
	AlignDirect align_direct = ALIGN_LEFT;

	gtk_text_buffer_set_text(buffer, "", 0);
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	while(TRUE) {
		nb = std::string::npos;
		ne = std::string::npos;

		size_t emoji_pos = ret.find("&x-");
		if (emoji_pos != std::string::npos) {
			char* ptr = (char*)ret.c_str() + emoji_pos;
			char* fnd = ptr+3;
			while(isalnum(*fnd) || *fnd == '_' || *fnd == '-') fnd++;
			if (*fnd == ';') {
				text = ret.substr(0, emoji_pos);
				nb = emoji_pos;
				ne = emoji_pos + (fnd-ptr);
				tag = ret.substr(nb, ne-nb+1);
			}
		}
		if (ne == std::string::npos) {
			nb = ret.find_first_of("<");
			ne = ret.find_first_of(">");
			if (nb == std::string::npos ||
				ne == std::string::npos ||
				nb > ne) {
				ret = sanitize_html(ret);
				gtk_text_buffer_insert(buffer, iter, ret.c_str(), ret.size());
				break;
			}
			tag = ret.substr(nb, ne-nb+1);
			text = ret.substr(0, nb);
		}
		text = sanitize_html(text, !pre_prev);

		gint offset = gtk_text_iter_get_offset(iter);
		gtk_text_buffer_insert(buffer, iter, text.c_str(), text.size());
		gtk_text_buffer_get_iter_at_offset(buffer, iter_orig, offset);

		if (align_center_prev) gtk_text_buffer_apply_tag(buffer, align_center_tag, iter_orig, iter);
		if (align_right_prev) gtk_text_buffer_apply_tag(buffer, align_right_tag, iter_orig, iter);
		if (pre_prev) gtk_text_buffer_apply_tag(buffer, pre_tag, iter_orig, iter);
		if (blockquote_prev) gtk_text_buffer_apply_tag(buffer, blockquote_tag, iter_orig, iter);
		if (h1_prev) gtk_text_buffer_apply_tag(buffer, h1_tag, iter_orig, iter);
		if (h2_prev) gtk_text_buffer_apply_tag(buffer, h2_tag, iter_orig, iter);
		if (h3_prev) gtk_text_buffer_apply_tag(buffer, h3_tag, iter_orig, iter);
		if (bold_prev) gtk_text_buffer_apply_tag(buffer, bold_tag, iter_orig, iter);
		if (italic_prev) gtk_text_buffer_apply_tag(buffer, italic_tag, iter_orig, iter);
		if (underline_prev) gtk_text_buffer_apply_tag(buffer, underline_tag, iter_orig, iter);
		if (strike_prev) gtk_text_buffer_apply_tag(buffer, strike_tag, iter_orig, iter);
		if (link_prev) {
			GtkTextTag* link_tag = link_tag = gtk_text_buffer_create_tag(
					buffer,
					NULL,
					"underline",
					PANGO_UNDERLINE_SINGLE,
					"foreground",
					"#0000FF",
					NULL);
			g_object_set_data(G_OBJECT(link_tag), "name", g_strdup("a"));
			g_object_set_data(G_OBJECT(link_tag), "title", g_strdup(text.c_str()));
			g_object_set_data(G_OBJECT(link_tag), "url", g_strdup(url_text.c_str()));
			gtk_text_buffer_apply_tag(buffer, link_tag, iter_orig, iter);
		}

		if (tag.substr(0, 4) == "<!--" && !isalnum(tag[4])) {
			ne = ret.find("-->");
			if (ne != std::string::npos) {
				std::string text = ret.substr(nb, ne-nb+3);
				GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file("embed.png", NULL);
				if (pixbuf) {
					g_object_set_data(G_OBJECT(pixbuf), "embed", g_strdup(text.c_str()));
					gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
				}
				ret = ret.substr(ne+3);
			}
			continue;
		}
		if (tag.substr(0, 7) == "<object" && !isalnum(tag[7])) {
			ne = ret.find("</object>");
			if (ne != std::string::npos) {
				std::string text = ret.substr(nb, ne-nb+9);
				GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file("embed.png", NULL);
				if (pixbuf) {
					g_object_set_data(G_OBJECT(pixbuf), "embed", g_strdup(text.c_str()));
					gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
				}
				ret = ret.substr(ne+9);
			}
			continue;
		}
		if (tag.substr(0, 6) == "<embed" && !isalnum(tag[6])) {
			ne = ret.find("</embed>");
			if (ne != std::string::npos) {
				std::string text = ret.substr(nb, ne-nb+8);
				GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file("embed.png", NULL);
				if (pixbuf) {
					g_object_set_data(G_OBJECT(pixbuf), "embed", g_strdup(text.c_str()));
					gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
				}
				ret = ret.substr(ne+9);
			}
			continue;
		}
		if (tag.substr(0, 3) == "&x-") {
			GdkPixbuf* pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(buffer), tag.c_str());
			if (pixbuf)
				gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
		}
		if (tag.substr(0, 7) == "<strike" && !isalnum(tag[7])) strike_prev = TRUE;
		if (tag.substr(0, 10) == "</strike" && !isalnum(tag[10])) strike_prev = FALSE;
		if (tag.substr(0, 3) == "<h1" && !isalnum(tag[3])) h1_prev = TRUE;
		if (tag.substr(0, 4) == "</h1" && !isalnum(tag[4])) {
			h1_prev = FALSE;
			gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 3) == "<h2" && !isalnum(tag[3])) h2_prev = TRUE;
		if (tag.substr(0, 4) == "</h2" && !isalnum(tag[4])) {
			h2_prev = FALSE;
			gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 3) == "<h3" && !isalnum(tag[3])) h3_prev = TRUE;
		if (tag.substr(0, 4) == "</h3" && !isalnum(tag[4])) {
			h3_prev = FALSE;
			gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 2) == "<u" && !isalnum(tag[2])) underline_prev = TRUE;
		if (tag.substr(0, 3) == "</u" && !isalnum(tag[3])) underline_prev = FALSE;
		if (tag.substr(0, 2) == "<i" && !isalnum(tag[2])) italic_prev = TRUE;
		if (tag.substr(0, 3) == "</i" && !isalnum(tag[3])) italic_prev = FALSE;
		if (tag.substr(0, 3) == "<br" && !isalnum(tag[3])) gtk_text_buffer_insert(buffer, iter, "\n", 1);
		if (tag.substr(0, 2) == "<b" && !isalnum(tag[2])) bold_prev = TRUE;
		if (tag.substr(0, 3) == "</b" && !isalnum(tag[3])) bold_prev = FALSE;

		if (tag.substr(0, 4) == "<pre" && !isalnum(tag[4]))
			pre_prev = TRUE;
		if (tag.substr(0, 5) == "</pre" && !isalnum(tag[5])) {
			pre_prev = FALSE;
			gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 11) == "<blockquote" && !isalnum(tag[11]))
			blockquote_prev = TRUE;
		if (tag.substr(0, 12) == "</blockquote" && !isalnum(tag[12])) {
			blockquote_prev = FALSE;
			gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 2) == "<p" && !isalnum(tag[2])) {
			if (text.size() > 0) gtk_text_buffer_insert(buffer, iter, "\n", 1);
			align_direct = ALIGN_LEFT;
			std::string align_str = tag;
			size_t sp = align_str.find("align=");
			if (sp != std::string::npos) {
				align_str = align_str.substr(sp+6);
				align_str = XmlRpc::trim_string(align_str);
				if (align_str[0] == '\"')
					align_str = align_str.substr(1);
				size_t ep = align_str.find_first_not_of(URL_ACCEPT_LETTER);
				if (ep != std::string::npos) {
					align_str = align_str.substr(0, ep);
					align_str = XmlRpc::trim_string(align_str);
					if (align_str == "right") align_right_prev = TRUE;
					if (align_str == "center") align_center_prev = TRUE;
				}
			}
		}
		if (tag.substr(0, 3) == "</p" && !isalnum(tag[3])) {
			gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 4) == "<div" && !isalnum(tag[4])) {
			if (text.size() > 0) gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 5) == "</div" && !isalnum(tag[5])) {
			gtk_text_buffer_insert(buffer, iter, "\n", 1);
		}
		if (tag.substr(0, 2) == "<a" && !isalnum(tag[2])) {
			std::string link_str = tag;
			size_t sp = link_str.find("href=");
			if (sp != std::string::npos) {
				link_str = link_str.substr(sp+5);
				link_str = XmlRpc::trim_string(link_str);
				if (link_str[0] == '\"')
					link_str = link_str.substr(1);
				link_str = XmlRpc::trim_string(link_str);
				size_t ep = link_str.find_first_not_of(URL_ACCEPT_LETTER);
				if (ep != std::string::npos) {
					url_text = link_str.substr(0, ep);
				}
			}
			link_prev = TRUE;
		}
		if (tag.substr(0, 3) == "</a" && !isalnum(tag[3]))
			link_prev = FALSE;
		if (tag.substr(0, 4) == "<img" && !isalnum(tag[4])) {
			std::string image_str = tag;
			size_t sp = image_str.find("src=");
			if (sp != std::string::npos) {
				image_str = image_str.substr(sp+4);
				image_str = XmlRpc::trim_string(image_str);
				if (image_str[0] == '\"')
					image_str = image_str.substr(1);
				size_t ep = image_str.find_first_not_of(URL_ACCEPT_LETTER);
				if (ep != std::string::npos) {
					image_str = image_str.substr(0, ep);
					image_str = XmlRpc::trim_string(image_str);
					GError* error = NULL;
					GdkPixbuf* pixbuf = url2pixbuf(image_str.c_str(), &error);
					if (pixbuf) {
						gint offset = gtk_text_iter_get_offset(iter);
						gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
						gtk_text_buffer_get_iter_at_offset(buffer, iter_orig, offset);
						if (link_prev) {
							GtkTextTag* link_tag = link_tag = gtk_text_buffer_create_tag(
									buffer,
									NULL,
									"underline",
									PANGO_UNDERLINE_SINGLE,
									"foreground",
									"#0000FF",
									NULL);
							g_object_set_data(G_OBJECT(link_tag), "name", g_strdup("a"));
							g_object_set_data(G_OBJECT(link_tag), "title", g_strdup(text.c_str()));
							g_object_set_data(G_OBJECT(link_tag), "url", g_strdup(url_text.c_str()));
							gtk_text_buffer_apply_tag(buffer, link_tag, iter_orig, iter);
						}
					}
				}
			}
		}
		ret = ret.substr(ne+1);
	}
	delete iter;
}

std::string get_html(GtkTextBuffer* buffer, GetSourceAction action, std::string prefix, std::string& errmsg)
{
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* iter_copy;
	gchar* str;
	std::string html;
	std::string text;
	gboolean bold_prev = FALSE;
	gboolean italic_prev = FALSE;
	gboolean underline_prev = FALSE;
	gboolean strike_prev = FALSE;
	gboolean link_prev = FALSE;
	gboolean h1_prev = FALSE;
	gboolean h2_prev = FALSE;
	gboolean h3_prev = FALSE;
	gboolean pre_prev = FALSE;
	gboolean blockquote_prev = FALSE;
	gboolean align_center_prev = FALSE;
	gboolean align_right_prev = FALSE;
	GSList* tags;

	gtk_text_buffer_get_start_iter(buffer, iter);
	while(!gtk_text_iter_is_end(iter)) {
		gboolean bold = gtk_text_iter_has_tag(iter, bold_tag);
		gboolean italic = gtk_text_iter_has_tag(iter, italic_tag);
		gboolean underline = gtk_text_iter_has_tag(iter, underline_tag);
		gboolean strike = gtk_text_iter_has_tag(iter, strike_tag);
		gboolean h1 = gtk_text_iter_has_tag(iter, h1_tag);
		gboolean h2 = gtk_text_iter_has_tag(iter, h2_tag);
		gboolean h3 = gtk_text_iter_has_tag(iter, h3_tag);
		gboolean pre = gtk_text_iter_has_tag(iter, pre_tag);
		gboolean blockquote = gtk_text_iter_has_tag(iter, blockquote_tag);
		gboolean align_center = gtk_text_iter_has_tag(iter, align_center_tag);
		gboolean align_right = gtk_text_iter_has_tag(iter, align_right_tag);
		gboolean link = text_iter_has_link_tag(iter);

		if (link && link != link_prev) {
			tags = gtk_text_iter_get_tags(iter);
			while(tags) {
				gchar* name = (gchar*)g_object_get_data(G_OBJECT(tags->data), "name");
				if (name && !g_strcasecmp(name, "a")) {
					gchar* title = (gchar*)g_object_get_data(G_OBJECT(tags->data), "title");
					gchar* url = (gchar*)g_object_get_data(G_OBJECT(tags->data), "url");
					str = g_strdup_printf("<a href=\"%s\">", url);
					html += str;
					g_free(str);
				}
				tags = g_slist_next(tags);
			}
		}
		if (h3 && h3 != h3_prev) html += h3_tag_opening_tag;
		if (h2 && h2 != h2_prev) html += h2_tag_opening_tag;
		if (h1 && h1 != h1_prev) html += h1_tag_opening_tag;
		if (pre && pre != pre_prev) html += pre_tag_opening_tag;
		if (blockquote && blockquote != blockquote_prev) html += blockquote_tag_opening_tag;
		if (align_center && align_center != align_center_prev) html += align_center_tag_opening_tag;
		if (align_right && align_right != align_right_prev) html += align_right_tag_opening_tag;
		if (strike && strike != strike_prev) html += strike_tag_opening_tag;
		if (underline && underline != underline_prev) html += underline_tag_opening_tag;
		if (italic && italic != italic_prev) html += italic_tag_opening_tag;
		if (bold && bold != bold_prev) html += bold_tag_opening_tag;
		tags = gtk_text_iter_get_tags(iter);
		while(tags) {
			gchar* name = (gchar*)g_object_get_data(G_OBJECT(tags->data), "name");
			if (name && !g_strcasecmp(name, "img")) {
				gchar* title = (gchar*)g_object_get_data(G_OBJECT(tags->data), "title");
				gchar* url = (gchar*)g_object_get_data(G_OBJECT(tags->data), "url");
				gchar* type = (gchar*)g_object_get_data(G_OBJECT(tags->data), "type");
				gchar* emoji = (gchar*)g_object_get_data(G_OBJECT(tags->data), "emoji");
				gchar* movie = (gchar*)g_object_get_data(G_OBJECT(tags->data), "movie");
				gchar* embed = (gchar*)g_object_get_data(G_OBJECT(tags->data), "embed");
				GdkPixbuf* pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(tags->data), "pixbuf");
				GdkPixbuf* real_pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(tags->data), "real_pixbuf");
				if (emoji) {
					html += emoji;
				} else
				if (embed) {
					html += embed;
				} else
				if (movie) {
					if (action == GET_SOURCE_SAVE || action == GET_SOURCE_CHECK) {
						str = g_strdup_printf("<a href=\"%s\" target=\"_blank\"><img src=\"%s\" title=\"%s\" border=0></a>", movie, url, title);
						html += str;
						g_free(str);
					} else {
						std::string newurl = upload_image(prefix, url, type, title, pixbuf, errmsg);
						if (newurl.size() == 0) return "";
						std::string newplayer = upload_movie(prefix, url, type, movie, errmsg);
						if (newplayer.size() == 0) return "";
						str = g_strdup_printf("<a href=\"%s\" target=\"_blank\"><img src=\"%s\" title=\"%s\" border=0></a>", newplayer.c_str(), newurl.c_str(), title);
						html += str;
						g_free(str);
					}
				} else {
					if (action == GET_SOURCE_SAVE) {
						std::string fname = url;
						fname = prefix + "_";
						fname += url;
						GError* error = NULL;
						if (!gdk_pixbuf_save(pixbuf, fname.c_str(), "png", &error, NULL)) {
							if (error) errmsg = error->message;
							return "";
						}
						str = g_strdup_printf("<img src=\"%s\" title=\"%s\">", fname.c_str(), title);
						html += str;
						g_free(str);
					} else
					if (prefix.size() && type && strlen(type)) {
						if (current_server == MAYBE_HATENA_FOTOLIFE)
							real_pixbuf = NULL;
						if (real_pixbuf) {
							std::vector<std::string> sep = XmlRpc::split_string(url, ".");
							std::string thumb_name;
							thumb_name = sep[0];
							thumb_name += "-thumb.";
							thumb_name += sep[1];
							std::string newurl1 = upload_image(prefix, thumb_name, type, title, pixbuf, errmsg);
							if (newurl1.size() == 0) return "";
							std::string newurl2 = upload_image(prefix, url, type, title, real_pixbuf, errmsg);
							if (newurl2.size() == 0) return "";
							str = g_strdup_printf("<a href=\"%s\"><img src=\"%s\" title=\"%s\"></a>", newurl2.c_str(), newurl1.c_str(), title);
							html += str;
							g_free(str);
						} else {
							std::string newurl = upload_image(prefix, url, type, title, pixbuf, errmsg);
							if (newurl.size() == 0) return "";
							str = g_strdup_printf("<img src=\"%s\" title=\"%s\">", newurl.c_str(), title);
							html += str;
							g_free(str);
						}
					} else {
						str = g_strdup_printf("<img src=\"%s\" title=\"%s\">", url, title);
						html += str;
						g_free(str);
					}
				}
			}
			tags = g_slist_next(tags);
		}
		if (!strike && strike != strike_prev) html += strike_tag_closing_tag;
		if (!underline && underline != underline_prev) html += underline_tag_closing_tag;
		if (!italic && italic != italic_prev) html += italic_tag_closing_tag;
		if (!bold && bold != bold_prev) html += bold_tag_closing_tag;
		if (!h1 && h1 != h1_prev) html += h1_tag_closing_tag;
		if (!h2 && h2 != h2_prev) html += h2_tag_closing_tag;
		if (!h3 && h3 != h3_prev) html += h3_tag_closing_tag;
		if (!link && link != link_prev) html += link_tag_closing_tag;
		h1_prev = h1;
		h2_prev = h2;
		h3_prev = h3;
		bold_prev = bold;
		italic_prev = italic;
		underline_prev = underline;
		strike_prev = strike;
		link_prev = link;

		iter_copy = gtk_text_iter_copy(iter);
		if (!gtk_text_iter_forward_char(iter))
			gtk_text_buffer_get_end_iter(buffer, iter);
		str = gtk_text_buffer_get_text(buffer, iter_copy, iter, TRUE);
		gtk_text_iter_free(iter_copy);
		text = str;
		g_free(str);

		if (!pre && pre != pre_prev) {
			if (!gtk_text_iter_has_tag(iter, pre_tag))
				html += pre_tag_closing_tag;
			else
				pre = TRUE;
		}
		pre_prev = pre;
		if (!blockquote && blockquote != blockquote_prev) {
			if (!gtk_text_iter_has_tag(iter, blockquote_tag))
				html += blockquote_tag_closing_tag;
			else
				blockquote = TRUE;
		}
		blockquote_prev = blockquote;
		if (!align_center && align_center != align_center_prev) {
			if (!gtk_text_iter_has_tag(iter, align_center_tag))
				html += align_center_tag_closing_tag;
			else
				align_center = TRUE;
		}
		align_center_prev = align_center;
		if (!align_right && align_right != align_right_prev) {
			if (!gtk_text_iter_has_tag(iter, align_right_tag))
				html += align_right_tag_closing_tag;
			else
				align_right = TRUE;
		}
		align_right_prev = align_right;

		XmlRpc::replace_string(text, "&", "&amp;");
		XmlRpc::replace_string(text, "\"", "&quote;");
		XmlRpc::replace_string(text, ">", "&gt;");
		XmlRpc::replace_string(text, "<", "&lt;");
		XmlRpc::replace_string(text, "\n", "<br/>");
		html += text;
	}
	if (strike_prev) html += strike_tag_closing_tag;
	if (underline_prev) html += underline_tag_closing_tag;
	if (italic_prev) html += italic_tag_closing_tag;
	if (bold_prev) html += bold_tag_closing_tag;
	if (blockquote_prev) html += blockquote_tag_closing_tag;
	if (pre_prev) html += pre_tag_closing_tag;
	if (align_center_prev) html += align_center_tag_closing_tag;
	if (align_right_prev) html += align_right_tag_closing_tag;
	if (h3_prev) html += h3_tag_closing_tag;
	if (h2_prev) html += h2_tag_closing_tag;
	if (h1_prev) html += h1_tag_closing_tag;
	if (link_prev) html += link_tag_closing_tag;
	delete iter;
	return html;
}

void set_wiki(GtkTextBuffer* buffer, std::string wiki)
{
	std::string ret = wiki;

	boost::regex re_namedlinks = boost::regex("([^:alnum:])(http|https|ftp|mail|gopher)(://[A-Za-z0-9:#@%/;$~_?+-=.&\\-]+)\\[([^\\]]+)\\]");
	ret = boost::regex_replace(ret, re_namedlinks, "$1<a href=\"$2$3\">$4</a>", boost::regex_constants::format_all);
	boost::regex re_imglinks = boost::regex("([^:alnum:]?)(http|https|ftp|mail|gopher)(://[A-Za-z0-9:#@%/;$~_?+-=.&\\-]+)(.jpg|.gif|.bmp)");
	ret = boost::regex_replace(ret, re_imglinks, "$1<img src=\"$2$3$4\">", boost::regex_constants::format_all);
	boost::regex re_nonamedlinks = boost::regex("([^\"])(http|https|ftp|mail|gopher)(://[A-Za-z0-9:#@%/;$~_?+-=.&\\-]+)");
	ret = boost::regex_replace(ret, re_nonamedlinks, "$1<a href=\"$2$3\">$2$3</a>", boost::regex_constants::format_all);

	boost::regex re_blockquote;
	re_blockquote = boost::regex("^>$");
	ret = boost::regex_replace(ret, re_blockquote, "<blockquote>", boost::regex_constants::format_all);
	re_blockquote = boost::regex("^<$");
	ret = boost::regex_replace(ret, re_blockquote, "</blockquote>", boost::regex_constants::format_all);

	boost::regex re_pre;
	re_pre = boost::regex("^>>$");
	ret = boost::regex_replace(ret, re_pre, "<pre>", boost::regex_constants::format_all);
	re_pre = boost::regex("^<<$");
	ret = boost::regex_replace(ret, re_pre, "</pre>", boost::regex_constants::format_all);

	boost::regex re_underline = boost::regex("%%%([^%]+)%%%");
	ret = boost::regex_replace(ret, re_underline, "<u>$1</u>", boost::regex_constants::format_all);

	boost::regex re_header;
	re_header = boost::regex("^\\*\\*\\*([^\\n]+)");
	ret = boost::regex_replace(ret, re_header, "<h3>$1</h3>", boost::regex_constants::format_all);
	re_header = boost::regex("^\\*\\*([^\\n]+)");
	ret = boost::regex_replace(ret, re_header, "<h2>$1</h2>", boost::regex_constants::format_all);
	re_header = boost::regex("^\\*([^\\n]+)");
	ret = boost::regex_replace(ret, re_header, "<h1>$1</h1>", boost::regex_constants::format_all);

	std::vector<std::string> lines = XmlRpc::split_string(ret, "\n");
	std::vector<std::string>::iterator it;
	ret = "";
	int number[3] = {0};
	char num[16];
	for(it = lines.begin(); it != lines.end(); it++) {
		std::string line = *it;
		if (line[0] == '-' && line[1] == '-' && line[2] == '-') {
			line = std::string("\x02\x02* ") + line.substr(3);
		} else
		if (line[0] == '-' && line[1] == '-') {
			line = std::string("\x02* ") + line.substr(2);
		} else
		if (line[0] == '-') {
			line = std::string("* ") + line.substr(1);
		} else
		if (line[0] == '+' && line[1] == '+' && line[2] == '+') {
			number[2]++;
			sprintf(num, "\x02\x02%d.", number[2]);
			line = std::string(num) + line.substr(3);
		} else
		if (line[0] == '+' && line[1] == '+') {
			number[2] = 0;
			number[1]++;
			sprintf(num, "\x02%d.", number[1]);
			line = std::string(num) + line.substr(2);
		} else
		if (line[0] == '+') {
			number[2] = 0;
			number[1] = 0;
			number[0]++;
			sprintf(num, "%d.", number[0]);
			line = std::string(num) + line.substr(1);
		} else {
			number[2] = 0;
			number[1] = 0;
			number[0] = 0;
		}
		ret += line;
		ret += "<br />";
	}

	ret = XmlRpc::replace_string(ret, "\n", "<br />");

	set_html(buffer, ret);
}

std::string get_wiki(GtkTextBuffer* buffer, GetSourceAction action, std::string prefix, std::string& errmsg)
{
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* iter_copy;
	gchar* str;
	std::string wiki;
	std::string text;
	gboolean bold_prev = FALSE;
	gboolean underline_prev = FALSE;
	gboolean strike_prev = FALSE;
	gboolean link_prev = FALSE;
	gboolean h1_prev = FALSE;
	gboolean h2_prev = FALSE;
	gboolean h3_prev = FALSE;
	gboolean pre_prev = FALSE;
	gboolean blockquote_prev = FALSE;
	GSList* tags;

	gtk_text_buffer_get_start_iter(buffer, iter);
	while(!gtk_text_iter_is_end(iter)) {
		gboolean bold = gtk_text_iter_has_tag(iter, bold_tag);
		gboolean underline = gtk_text_iter_has_tag(iter, underline_tag);
		gboolean strike = gtk_text_iter_has_tag(iter, strike_tag);
		gboolean h1 = gtk_text_iter_has_tag(iter, h1_tag);
		gboolean h2 = gtk_text_iter_has_tag(iter, h2_tag);
		gboolean h3 = gtk_text_iter_has_tag(iter, h3_tag);
		gboolean pre = gtk_text_iter_has_tag(iter, pre_tag);
		gboolean blockquote = gtk_text_iter_has_tag(iter, blockquote_tag);
		gboolean link = text_iter_has_link_tag(iter);

		if (link && link != link_prev) {
			tags = gtk_text_iter_get_tags(iter);
			while(tags) {
				gchar* name = (gchar*)g_object_get_data(G_OBJECT(tags->data), "name");
				if (name && !g_strcasecmp(name, "a")) {
					gchar* title = (gchar*)g_object_get_data(G_OBJECT(tags->data), "title");
					gchar* url = (gchar*)g_object_get_data(G_OBJECT(tags->data), "url");
					str = g_strdup_printf("%s[", url);
					wiki += str;
					g_free(str);
				}
				tags = g_slist_next(tags);
			}
		}
		if (h1 && h1 != h1_prev) wiki += "*";
		if (h2 && h2 != h2_prev) wiki += "**";
		if (h3 && h3 != h3_prev) wiki += "***";
		if (pre && pre != pre_prev) {
			if (wiki[wiki.size()-1] != '\n') wiki += "\n";
			wiki += ">|\n";
		}
		if (blockquote && blockquote != blockquote) {
			if (wiki[wiki.size()-1] != '\n') wiki += "\n";
			wiki += ">>\n";
		}
		if (bold && bold != bold_prev) wiki += "''";
		if (underline && strike != underline_prev) wiki += "%%%";
		if (strike && strike != strike_prev) wiki += "%%";
		tags = gtk_text_iter_get_tags(iter);
		while(tags) {
			gchar* name = (gchar*)g_object_get_data(G_OBJECT(tags->data), "name");
			if (name && !g_strcasecmp(name, "img")) {
				gchar* title = (gchar*)g_object_get_data(G_OBJECT(tags->data), "title");
				gchar* url = (gchar*)g_object_get_data(G_OBJECT(tags->data), "url");
				gchar* type = (gchar*)g_object_get_data(G_OBJECT(tags->data), "t	pe");
				gchar* emoji = (gchar*)g_object_get_data(G_OBJECT(tags->data), "emoji");
				gchar* movie = (gchar*)g_object_get_data(G_OBJECT(tags->data), "movie");
				GdkPixbuf* pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(tags->data), "pixbuf");

				if (emoji) {
					wiki += emoji;
				} else
				if (movie) {
					if (action == GET_SOURCE_SAVE || action == GET_SOURCE_CHECK) {
						str = g_strdup_printf("<a href=\"%s\" target=\"_blank\"><img src=\"%s\" title=\"%s\" border=0></a>", movie, url, title);
						wiki += str;
						g_free(str);
					} else {
						std::string newurl = upload_image(prefix, url, type, title, pixbuf, errmsg);
						if (newurl.size() == 0) return "";
						std::string newplayer = upload_movie(prefix, url, type, movie, errmsg);
						if (newurl.size() == 0) return "";
						str = g_strdup_printf("%s[%s]", newplayer.c_str(), newurl.c_str());
						wiki += str;
						g_free(str);
					}
				} else {
					if (action == GET_SOURCE_SAVE) {
						std::string fname = url;
						fname = prefix + "_";
						fname += url;
						gdk_pixbuf_save(pixbuf, fname.c_str(), "png", NULL, NULL);
						str = g_strdup_printf("%s[%s]", fname.c_str(), title);
						wiki += str;
						g_free(str);
					} else
					if (prefix.size() && type && strlen(type)) {
						std::string newurl = upload_image(prefix, url, type, title, pixbuf, errmsg);
						if (newurl.size() == 0) return "";
						str = g_strdup_printf("%s[%s]", newurl.c_str(), title);
						wiki += str;
						g_free(str);
					} else {
						if (link)
							str = g_strdup_printf("%s", url);
						else
							str = g_strdup_printf("%s[%s]", url, title);
						wiki += str;
						g_free(str);
					}
				}
			}
			tags = g_slist_next(tags);
		}
		if (!strike && strike != strike_prev) wiki += "%%";
		if (!underline && underline != underline_prev) wiki += "%%%";
		if (!bold && bold != bold_prev) wiki += "''";
		if (!blockquote && blockquote != blockquote_prev) wiki += "<<\n";
		if (!pre && pre != pre_prev) wiki += "|<\n";
		if ((!h1 && h1 != h1_prev) || (!h2 && h2 != h2_prev) || (!h3 && h3 != h3_prev)) {
			iter_copy = gtk_text_iter_copy(iter);
			gtk_text_iter_forward_char(iter_copy);
			if (gtk_text_iter_get_line_index(iter) == gtk_text_iter_get_line_index(iter_copy))
				wiki += "\n";
			gtk_text_iter_free(iter_copy);
		}
		if (!link && link != link_prev) wiki += "]";
		bold_prev = bold;
		strike_prev = strike;
		blockquote_prev = blockquote;
		pre_prev = pre;
		h1_prev = h1;
		h2_prev = h2;
		h3_prev = h3;
		link_prev = link;

		iter_copy = gtk_text_iter_copy(iter);
		if (!gtk_text_iter_forward_char(iter))
			gtk_text_buffer_get_end_iter(buffer, iter);
		str = gtk_text_buffer_get_text(buffer, iter_copy, iter, TRUE);
		gtk_text_iter_free(iter_copy);
		text = str;
		g_free(str);
		wiki += text;
	}
	if (strike_prev) wiki += "%%";
	if (bold_prev) wiki += "''";
	if (blockquote_prev) wiki += "<<\n";
	if (pre_prev) wiki += "|<\n";
	if (h1_prev || h2_prev || h3_prev) wiki += "\n";
	if (link_prev) wiki += "]";
	delete iter;
	return wiki;
}

void on_menu_undo(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	gulong buffer_insert_text_signal_id = (gulong)g_object_get_data(G_OBJECT(buffer), "buffer-insert-text-signal-id");
	gulong buffer_delete_range_signal_id = (gulong)g_object_get_data(G_OBJECT(buffer), "buffer-delete-range-signal-id");
	gulong buffer_insert_pixbuf_signal_id = (gulong)g_object_get_data(G_OBJECT(buffer), "buffer-insert-pixbuf-signal-id");

	g_signal_handler_block(buffer, buffer_insert_text_signal_id);
	g_signal_handler_block(buffer, buffer_delete_range_signal_id);
	g_signal_handler_block(buffer, buffer_insert_pixbuf_signal_id);

	if (!undo_current) undo_current = g_list_last(undo_data);
	while(true) {
		UndoInfo *undoInfo = (UndoInfo*)g_list_nth_data(undo_current, 0);
		int pos = g_list_position(undo_data, undo_current);
		if(!undoInfo) break;

		switch(undoInfo->action) {
		case BUFFER_INSERT_TEXT:
			{
				GtkTextIter* start = new GtkTextIter;
				GtkTextIter* end = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_get_iter_at_offset(buffer, end, undoInfo->end);
				gtk_text_buffer_delete(buffer, start, end);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
				delete end;
			}
			break;
		case BUFFER_INSERT_PIXBUF:
			{
				GtkTextIter* start = new GtkTextIter;
				GtkTextIter* end = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_get_iter_at_offset(buffer, end, undoInfo->end);
				gtk_text_buffer_delete(buffer, start, end);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
				delete end;
			}
			break;
		case BUFFER_DELETE_TEXT:
			{
				GtkTextIter* start = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_insert(buffer, start, (gchar*)undoInfo->data, -1);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
			}
			break;
		case BUFFER_DELETE_PIXBUF:
			{
				GtkTextIter* start = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_insert_pixbuf(buffer, start, (GdkPixbuf*)undoInfo->data);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
			}
			break;
		case BUFFER_REMOVE_TAG:
			{
				GtkTextIter* start = new GtkTextIter;
				GtkTextIter* end = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_get_iter_at_offset(buffer, end, undoInfo->end);
				gtk_text_buffer_apply_tag(buffer, (GtkTextTag*)undoInfo->data, start, end);
				gtk_text_buffer_place_cursor(buffer, end);
				delete start;
				delete end;
			}
			break;
		case BUFFER_APPLY_TAG:
			{
				GtkTextIter* start = new GtkTextIter;
				GtkTextIter* end = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_get_iter_at_offset(buffer, end, undoInfo->end);
				gtk_text_buffer_remove_tag(buffer, (GtkTextTag*)undoInfo->data, start, end);
				gtk_text_buffer_place_cursor(buffer, end);
				delete start;
				delete end;
			}
			break;
		}
		undo_current = g_list_previous(undo_current);
		if (undoInfo->sequence == 0)
			break;
	}

	g_signal_handler_unblock(buffer, buffer_insert_text_signal_id);
	g_signal_handler_unblock(buffer, buffer_delete_range_signal_id);
	g_signal_handler_unblock(buffer, buffer_insert_pixbuf_signal_id);
}

void on_menu_redo(GtkWidget* widget, gpointer user_data)
{
	if (!undo_current) return;

	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	gulong buffer_insert_text_signal_id = (gulong)g_object_get_data(G_OBJECT(buffer), "buffer-insert-text-signal-id");
	gulong buffer_delete_range_signal_id = (gulong)g_object_get_data(G_OBJECT(buffer), "buffer-delete-range-signal-id");
	gulong buffer_insert_pixbuf_signal_id = (gulong)g_object_get_data(G_OBJECT(buffer), "buffer-insert-pixbuf-signal-id");

	g_signal_handler_block(buffer, buffer_insert_text_signal_id);
	g_signal_handler_block(buffer, buffer_delete_range_signal_id);
	g_signal_handler_block(buffer, buffer_insert_pixbuf_signal_id);

	while(true) {
		undo_current = g_list_next(undo_current);
		if (undo_current == NULL) break;
		UndoInfo *undoInfo = (UndoInfo*)g_list_nth_data(undo_current, 0);
		if(!undoInfo) break;

		switch(undoInfo->action) {
		case BUFFER_INSERT_TEXT:
			{
				GtkTextIter* start = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_insert(buffer, start, (gchar*)undoInfo->data, -1);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
			}
			break;
		case BUFFER_INSERT_PIXBUF:
			{
				GtkTextIter* start = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_insert_pixbuf(buffer, start, (GdkPixbuf*)undoInfo->data);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
			}
			break;
		case BUFFER_DELETE_TEXT:
			{
				GtkTextIter* start = new GtkTextIter;
				GtkTextIter* end = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
				gtk_text_buffer_get_iter_at_offset(buffer, end, undoInfo->end);
				gtk_text_buffer_delete(buffer, start, end);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
				delete end;
			}
			break;
		case BUFFER_DELETE_PIXBUF:
			{
				GtkTextIter* start = new GtkTextIter;
				GtkTextIter* end = new GtkTextIter;
				gtk_text_buffer_get_iter_at_offset(buffer, start, undoInfo->start);
					gtk_text_buffer_get_iter_at_offset(buffer, end, undoInfo->end);
				gtk_text_buffer_delete(buffer, start, end);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;
				delete end;
			}
			break;
		}
		if (undoInfo->sequence == 0)
			break;
	}

	g_signal_handler_unblock(buffer, buffer_insert_text_signal_id);
	g_signal_handler_unblock(buffer, buffer_delete_range_signal_id);
	g_signal_handler_unblock(buffer, buffer_insert_pixbuf_signal_id);
}

void on_menu_debug(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	gchar* contents = NULL;
	g_file_get_contents("test.dat", &contents, NULL, NULL);
	std::string body_text = contents;
	g_free(contents);
	set_wiki(buffer, body_text);

	tcout << body_text << std::endl;
}

void on_menu_help(GtkWidget* widget, gpointer user_data)
{
	const gchar* authors[2] = {"mattn", NULL};
	gchar* contents = NULL;
	gchar* utf8 = NULL;
	GdkPixbuf* logo = NULL;
	GtkWidget* dialog;
	dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), _(APP_TITLE));
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
	if (g_file_get_contents("COPYING", &contents, NULL, NULL)) {
		utf8 = g_locale_to_utf8(contents, -1, NULL, NULL, NULL);
		g_free(contents);
		contents = NULL;
		gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(dialog), utf8);
		g_free(utf8);
		utf8 = NULL;
	}
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "http://www.ac.cyberhome.ne.jp/~mattn/");
	logo = gdk_pixbuf_new_from_file((module_path + "/logo.png").c_str(), NULL);
	gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG(dialog), logo);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(toplevel));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(toplevel));
}

void on_menu_bold(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	GtkTextIter* start = new GtkTextIter;
	GtkTextIter* end = new GtkTextIter;

	if (!gtk_text_buffer_get_selection_bounds(buffer, start, end)) return;
	GtkTextTag* tag = bold_tag;
	bool has_tag = gtk_text_iter_has_tag(start, tag);

	append_buffer_undo_info(
		has_tag ? BUFFER_REMOVE_TAG : BUFFER_APPLY_TAG,
		GTK_TYPE_TEXT_TAG,
		gtk_text_iter_get_offset(start),
		gtk_text_iter_get_offset(end),
		g_object_ref(tag),
		0);
	
	if (has_tag) {
		gtk_text_buffer_remove_tag(buffer, tag, start, end);
	} else {
		gtk_text_buffer_apply_tag(buffer, tag, start, end);
	}

	delete start;
	delete end;
}

void on_menu_italic(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	GtkTextIter* start = new GtkTextIter;
	GtkTextIter* end = new GtkTextIter;

	if (!gtk_text_buffer_get_selection_bounds(buffer, start, end)) return;
	GtkTextTag* tag = italic_tag;
	bool has_tag = gtk_text_iter_has_tag(start, tag);

	append_buffer_undo_info(
		has_tag ? BUFFER_REMOVE_TAG : BUFFER_APPLY_TAG,
		GTK_TYPE_TEXT_TAG,
		gtk_text_iter_get_offset(start),
		gtk_text_iter_get_offset(end),
		g_object_ref(tag),
		0);
	
	if (has_tag) {
		gtk_text_buffer_remove_tag(buffer, tag, start, end);
	} else {
		gtk_text_buffer_apply_tag(buffer, tag, start, end);
	}

	delete start;
	delete end;
}

void on_menu_underline(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	GtkTextIter* start = new GtkTextIter;
	GtkTextIter* end = new GtkTextIter;

	if (!gtk_text_buffer_get_selection_bounds(buffer, start, end)) return;
	GtkTextTag* tag = underline_tag;
	bool has_tag = gtk_text_iter_has_tag(start, tag);

	append_buffer_undo_info(
		has_tag ? BUFFER_REMOVE_TAG : BUFFER_APPLY_TAG,
		GTK_TYPE_TEXT_TAG,
		gtk_text_iter_get_offset(start),
		gtk_text_iter_get_offset(end),
		g_object_ref(tag),
		0);
	
	if (has_tag) {
		gtk_text_buffer_remove_tag(buffer, tag, start, end);
	} else {
		gtk_text_buffer_apply_tag(buffer, tag, start, end);
	}

	delete start;
	delete end;
}

void on_menu_strike(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	GtkTextIter* start = new GtkTextIter;
	GtkTextIter* end = new GtkTextIter;

	if (!gtk_text_buffer_get_selection_bounds(buffer, start, end)) return;
	GtkTextTag* tag = strike_tag;
	bool has_tag = gtk_text_iter_has_tag(start, tag);

	append_buffer_undo_info(
		has_tag ? BUFFER_REMOVE_TAG : BUFFER_APPLY_TAG,
		GTK_TYPE_TEXT_TAG,
		gtk_text_iter_get_offset(start),
		gtk_text_iter_get_offset(end),
		g_object_ref(tag),
		0);
	
	if (has_tag) {
		gtk_text_buffer_remove_tag(buffer, tag, start, end);
	} else {
		gtk_text_buffer_apply_tag(buffer, tag, start, end);
	}

	delete start;
	delete end;
}

bool post_blog_ftpup(
		std::string title,
		std::string category,
		std::string body,
		CategoryList catmap,
		std::string& errmsg)
{
	char fname[256];

	if (current_postid.size()) {
		strcpy(fname, current_postid.c_str());
	} else {
		time_t tim;
		time(&tim);
		sprintf(fname, "%d.txt", tim);
	}
	
	CategoryList::iterator it;
	std::string category_id = "/";
	for(it = catmap.begin(); it != catmap.end(); it++) {
		if (it->second == category)
			category_id = it->first;
	}

	std::string text = title + "\n";
	text += body;

	std::string file;
	file += user_configs[user_profile]["blogid"];
	if (category_id != "/")
		file += category_id;
	file += fname;

	if (Blosxom::FtpUpload(
		user_configs[user_profile]["server"],
		user_configs[user_profile]["userid"],
		user_configs[user_profile]["passwd"],
		text,
		file) != 0)
	{
		if (current_postid.size())
			errmsg = _("Can't update entry");
		else
			errmsg = _("Can't create entry");
		return false;
	}

	current_postid = fname;

	return true;
}

bool post_blog_atompp(
		std::string title,
		std::string category,
		std::string body,
		CategoryList catmap,
		std::string& errmsg)
{
	AtomPP::AtomEntry entry;

	entry.title = title;
	entry.summary = category;
	entry.content.value = body;
	entry.content.mode = "escaped";
	entry.content.type = "text/html";

	if (current_postid.size()) {
		int ret = 0;
		if (current_server == MAYBE_HATENA_BOOKMARK) {
			std::vector<AtomPP::AtomLink>::iterator it;
			for(it = entry.links.begin(); it != entry.links.end(); it++)
				if (it->rel == "related") break;
			if (it != entry.links.end())
				it->href = body;
			else {
				AtomPP::AtomLink link;
				link.rel = "related";
				link.title = entry.title;
				link.type = "text/html";
				link.href = entry.content.value;
				entry.links.push_back(link);
			}
			entry.content.mode = "empty";
			ret = AtomPP::updateEntry(current_postid, user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"], entry);
		} else
		if (current_server == MAYBE_BLOGGER_ATOMPP)
			ret = AtomPP::updateEntry_Blogger(current_postid, user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"], entry);
		else
			ret = AtomPP::updateEntry(current_postid, user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"], entry);
		if (ret != 0) {
			errmsg = _("Can't update entry");
			return false;
		}
	} else {
		int ret = 0;
		if (current_server == MAYBE_BLOGGER_ATOMPP)
			ret = AtomPP::createEntry_Blogger(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"], entry);
		else
		if (current_server == MAYBE_AMEBA_ATOMPP) {
			std::string passwd = AtomPP::StringToHex(AtomPP::md5(user_configs[user_profile]["passwd"]));
			ret = AtomPP::createEntry(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], passwd, entry);
		} else
			ret = AtomPP::createEntry(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"], entry);
		if (ret == 0)
			current_postid = entry.edit_uri;
		else {
			errmsg = _("Can't create entry");
			return false;
		}
		if (ret == 0 && current_postid.size() == 0) {
			GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
			GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
			GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
			gtk_entry_set_text(GTK_ENTRY(blog_title), "");
			gtk_combo_box_set_active(GTK_COMBO_BOX(blog_category), 0);
			GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));
    		gtk_text_buffer_set_text(buffer, "", 0);
		}
	}
	return true;
}

bool post_blog_xmlrpc(
		std::string title,
		std::string category,
		std::string body,
		CategoryList catmap,
		std::string& errmsg)
{
	XmlRpc::XmlRpcValue::ValueStruct content;
	XmlRpc::XmlRpcValue::ValueArray categorylist;
	XmlRpc::XmlRpcValue::ValueArray tbpings;
	std::vector<XmlRpc::XmlRpcValue> requests;
	XmlRpc::XmlRpcValue response;

	content.clear();
	content["title"] = title;
	content["description"] = body;
	content["mt_text_more"] = "";
	content["mt_excerpt"] = "";
	content["mt_keywords"] = "";
	content["mt_allow_comments"] = 1;
	content["mt_allow_pings"] = 1;
	content["mt_convert_breaks"] = 1;
	content["mt_tb_ping_urls"] = tbpings;

	std::string postmethod;
	std::string postid;

	requests.clear();
	if (current_postid.size()) {
		postmethod = "metaWeblog.editPost";
		requests.push_back(current_postid);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		requests.push_back(content);
		requests.push_back(TRUE);
	} else {
		postmethod = "metaWeblog.newPost";
		requests.push_back(user_configs[user_profile]["blogid"]);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		requests.push_back(content);
		requests.push_back(TRUE);
	}

	if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], postmethod, requests, response) == 0) {
		if (current_postid.size() == 0)
			postid = response.valueString();
	} else {
		std::string faultMessage = _("Can't post entry:");
		faultMessage += "\n";
		faultMessage += XmlRpc::getFaultResult(response);
		errmsg = faultMessage;
		return false;
	}

	if (postid.size())
		current_postid = postid;

	if (current_server == MAYBE_MOVABLETYPE) {
		requests.clear();
		requests.push_back(current_postid);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "mt.publishPost", requests, response) == 0) {
		} else {
			std::string faultMessage = _("Can't publish entry:");
			faultMessage += "\n";
			faultMessage += XmlRpc::getFaultResult(response);
			errmsg = faultMessage;
			return false;
		}
	}

	CategoryList::iterator it;
	content.clear();
	for(it = catmap.begin(); it != catmap.end(); it++) {
		if (it->second == category)
			content["categoryId"] = it->first;
	}
	categorylist.push_back(content);
	requests.clear();
	requests.push_back(current_postid);
	requests.push_back(user_configs[user_profile]["userid"]);
	requests.push_back(user_configs[user_profile]["passwd"]);
	requests.push_back(categorylist);
	if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "mt.setPostCategories", requests, response) == 0) {
		;
	} else {
		std::string faultMessage = _("Can't set categroy:");
		faultMessage += "\n";
		faultMessage += XmlRpc::getFaultResult(response);
		errmsg = faultMessage;
		return false;
	}
	return true;
}

gpointer publish_post_thread(gpointer data)
{
	GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	char fname[256];
	time_t tim;
	time(&tim);
	sprintf(fname, "%d", tim);

	GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
	GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
	GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");

	gchar* err = NULL;
	gchar* title_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(blog_title));
	if (!title_text || !strlen(title_text)) {
		return g_strdup(_("Title is empty"));
	}
	gchar* category_text = (gchar*)gtk_combo_box_get_active_text(GTK_COMBO_BOX(blog_category));
	if (!category_text && current_category_list.size() == 0)
		category_text = "";
	else if (!category_text || !strlen(category_text)) {
		return g_strdup(_("Category is not selected"));
	}
	std::string body_text;
	std::string errmsg;
	if (user_configs[user_profile]["format"] == "html")
		body_text = get_html(buffer, GET_SOURCE_CHECK, "", errmsg);
	else
	if (user_configs[user_profile]["format"] == "wiki")
		body_text = get_wiki(buffer, GET_SOURCE_CHECK, "", errmsg);

	if (!body_text.size()) {
		if (errmsg.size())
			return g_strdup(_(errmsg.c_str()));
		else
			return g_strdup(_("Body is empty"));
	}

	if (user_configs[user_profile]["format"] == "html")
		body_text = get_html(buffer, GET_SOURCE_UPLOAD, fname, errmsg);
	else
	if (user_configs[user_profile]["format"] == "wiki")
		body_text = get_wiki(buffer, GET_SOURCE_UPLOAD, fname, errmsg);

	if (current_server == MAYBE_HATENA_FOTOLIFE) {
		if (errmsg.size())
			return g_strdup(_(errmsg.c_str()));
		return NULL;
	}

	if (!body_text.size()) {
		if (errmsg.size())
			return g_strdup(_(errmsg.c_str()));
		else
			return g_strdup(_("Body is empty"));
	}
	
	bool ret = false;
	if (user_configs[user_profile]["engine"] == "xmlrpc")
		ret = post_blog_xmlrpc(title_text, category_text, body_text, current_category_list, errmsg);
	else
	if (user_configs[user_profile]["engine"] == "atompp")
		ret = post_blog_atompp(title_text, category_text, body_text, current_category_list, errmsg);
	else
	if (user_configs[user_profile]["engine"] == "ftpup")
		ret = post_blog_ftpup(title_text, category_text, body_text, current_category_list, errmsg);

	if (ret == false)
		return g_strdup(errmsg.c_str());
	return NULL;
}

void on_publish_clicked(GtkWidget* widget, gpointer user_data)
{
	gpointer retval = process_func(publish_post_thread, NULL, toplevel, _("Publishing Post..."));
	if (retval) {
		error_dialog(toplevel, (gchar*)retval);
		g_free(retval);
	}
}

void on_emoji_clicked(GtkWidget* widget, GdkEventButton* event, gpointer user_data)
{
	if (user_data) {
		GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
		GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
		GtkTextIter* iter = new GtkTextIter;
		GdkPixbuf* pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(widget), "pixbuf");
		gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
		gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
		delete iter;
	} else {
		GtkWidget* image = (GtkWidget*)g_object_get_data(G_OBJECT(widget), "image");
		GtkWidget* panel = (GtkWidget*)g_object_get_data(G_OBJECT(widget), "panel");
		GdkPixbuf* pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(widget), "pixbuf");
		GdkPixbuf* pixbuf_on = (GdkPixbuf*)g_object_get_data(G_OBJECT(widget), "pixbuf_on");
		if (!GTK_WIDGET_VISIBLE(panel)) {
			gtk_widget_show(panel);
			gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf_on);
		} else {
			gtk_widget_hide(panel);
			gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
		}
	}
}

void on_emoji_item_selection_changed(GtkIconView* icon_view, gpointer user_data)
{
	GList* list = gtk_icon_view_get_selected_items(icon_view);
	if (!list) return;
	GtkTreePath* tree_path = (GtkTreePath*)list->data;
	GtkListStore* store = (GtkListStore*)g_object_get_data(G_OBJECT(icon_view), "store");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GdkPixbuf* pixbuf = NULL;
	GtkTreeIter* item_iter = new GtkTreeIter;

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), item_iter, tree_path)) {
		gtk_tree_model_get(GTK_TREE_MODEL(store), item_iter, 0, &pixbuf, -1);
		if (pixbuf) {
			GtkTextIter* iter = new GtkTextIter;
			gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
			gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
			delete iter;
		}
	}

	delete item_iter;
	gtk_icon_view_unselect_path(icon_view, tree_path);
}

void on_popupmenu_selected(GtkWidget* widget, gpointer user_data)
{
	gchar* url = g_locale_from_utf8((gchar*)user_data, -1, NULL, NULL, NULL);
	std::string command = user_configs["global"]["browser_command"];

#ifdef _WIN32
	if (!command.size()) {
		ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOW);
		return;
	}
#endif
	if (command.size() == 0) command = "firefox";
	command += " \"";
	command += url;
	command += "\"";
	g_spawn_command_line_async(command.c_str(), NULL);
}

void on_popupmenu_delete(GtkWidget* widget, gpointer user_data)
{
	if (user_data) g_free(user_data);
}

void on_populate_popup(GtkTextView *textview, GtkMenu *menu, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start = new GtkTextIter;
	GtkTextIter* end = new GtkTextIter;
	if (!gtk_text_buffer_get_selection_bounds(buffer, start, end)) {
		gtk_text_buffer_get_iter_at_mark(buffer, start, gtk_text_buffer_get_insert(buffer));
		gtk_text_buffer_get_iter_at_mark(buffer, end, gtk_text_buffer_get_insert(buffer));
		gtk_text_iter_backward_to_tag_toggle(start, NULL);
		gtk_text_iter_forward_to_tag_toggle(end, NULL);
		gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	} else {
		gint offset = gtk_text_iter_get_offset(start);
		gtk_text_buffer_get_iter_at_offset(buffer, iter, offset);
	}

	std::vector<UrlListInfo> urllist;
	std::vector<UrlListInfo>::iterator it;
	while(true) {
		if (!gtk_text_iter_in_range(iter, start, end)) break;
		GSList* tags = gtk_text_iter_get_tags(iter);
		while(tags) {
			gchar* name_text;
			name_text = (gchar*)g_object_get_data(G_OBJECT(tags->data), "name");
			if (name_text && !g_strcasecmp(name_text, "img")) {
				gchar* url_text = (gchar*)g_object_get_data(G_OBJECT(tags->data), "movie");
				if (url_text) {
					for(it = urllist.begin(); it != urllist.end(); it++)
						if (!strcmp(it->url, url_text))
							break;
					if (it == urllist.end()) {
						UrlListInfo urlInfo;
						urlInfo.url = url_text;
						std::string name = XmlRpc::cut_string_r(u2l(url_text), 20, "... ");
						gchar* str = g_strdup_printf(_("Open Movie %s"), l2u(name).c_str());
						urlInfo.name = str;
						g_free(str);
						urllist.push_back(urlInfo);
					}
				}
			} else
			if (name_text && !g_strcasecmp(name_text, "a")) {
				gchar* url_text = (gchar*)g_object_get_data(G_OBJECT(tags->data), "url");
				if (url_text) {
					for(it = urllist.begin(); it != urllist.end(); it++)
						if (!strcmp(it->url, url_text))
							break;
					if (it == urllist.end()) {
						UrlListInfo urlInfo;
						urlInfo.url = url_text;
						std::string name = XmlRpc::cut_string(u2l(url_text), 20, " ...");
						gchar* str = g_strdup_printf(_("Open URL %s"), l2u(name).c_str());
						urlInfo.name = str;
						g_free(str);
						urllist.push_back(urlInfo);
					}
				}
			}
			tags = g_slist_next(tags);
		}
		if (!gtk_text_iter_forward_char(iter))
			gtk_text_buffer_get_end_iter(buffer, iter);
	}
	delete iter;
	delete start;
	delete end;

	GtkWidget* submenuitem;
	if (urllist.size() > 0) {
		submenuitem = gtk_separator_menu_item_new();
		gtk_menu_append(GTK_MENU(menu), submenuitem);
		gtk_widget_show(submenuitem);
	}
	for(it = urllist.begin(); it != urllist.end(); it++) {
		submenuitem = gtk_menu_item_new_with_label(it->name.c_str());
		g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_popupmenu_selected), it->url);
		gtk_menu_append(GTK_MENU(menu), submenuitem);
		gtk_widget_show(submenuitem);
	}
}

void on_drag_data_received(
		GtkWidget* widget,
		GdkDragContext* drag_context,
		gint x,
		gint y,
		GtkSelectionData* data,
		guint info,
		guint time,
		gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	switch(info) {
	case TARGET_URI_LIST:
		{
			gchar* uri_list = (gchar*)data->data;
			gchar** uris = g_strsplit(uri_list, "\r\n", 255);
			for(; *uris; uris++) {
				gchar* uri = g_strchomp(*uris);
				GdkPixbuf* pixbuf = NULL;

				if (!*uri) continue;
				GError* error = NULL;

				std::string mime = url2mime(uri, &error);
				bool isMovie = false;
				std::string movie;
				if (mime.substr(0, 6) == "video/") {
					gchar* newurl = g_filename_from_uri(uri, NULL, NULL);
					movie = newurl;
					movie = u2l(movie);
					movie = XmlRpc::replace_string(movie, "\\", "/");
					movie = l2u(movie);
				}

				if (movie.size()) {
					std::string command = user_configs["global"]["ffmpeg_command"];
					if (command.size() == 0) command = "ffmpeg";
					command += " -i ";
					command += "\"";
					command += movie.c_str();
					command += "\"";
					command += " -s 320x240 -vframes 1 -f image2 ";
					command += module_path;
					command += "/player.png";
					gchar *pstdout = NULL, *pstderr = NULL;
					g_spawn_command_line_sync(command.c_str(), &pstdout, &pstderr, NULL, &error);
					if (error)
						error_dialog(toplevel, error->message);
					else {
						std::string file = module_path + "/player.png";
						pixbuf = url2pixbuf(file.c_str(), &error);
						g_unlink(file.c_str());
					}
				} else {
					pixbuf = url2pixbuf(uri, &error);
					if (!pixbuf) {
						if (error) error_dialog(toplevel, error->message);
						continue;
					}
				}

				if (pixbuf) {
					GtkTextIter* iter = new GtkTextIter;
					g_object_set_data(G_OBJECT(pixbuf), "url", NULL);
					if (movie.size())
						g_object_set_data(G_OBJECT(pixbuf), "movie", (void*)g_strdup(movie.c_str()));
					gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
					gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
					delete iter;
				}
			}
		}
		break;
	case TARGET_STRING:
	case TARGET_UTF8_STRING:
	case TARGET_COMPOUND_TEXT:
	case TARGET_TEXT:
		{
			char *str = (char*)gtk_selection_data_get_text(data);
			if (str) {
				if (*str) {
					GtkTextIter* iter = new GtkTextIter;
					gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
					gtk_text_buffer_insert(buffer, iter, str, strlen(str));
					delete iter;
				}
				g_free(str);
			}
		}
		break;
	}
	gtk_drag_finish(drag_context, TRUE, FALSE, time);
}

void on_buffer_insert_pixbuf(
		GtkTextBuffer* buffer,
		GtkTextIter* iter,
		GdkPixbuf* pixbuf,
		gpointer user_data)
{
	GtkTextTag* tag;
	GtkTextIter* iter_orig = new GtkTextIter;
	gchar* src = NULL;
	gchar* title = NULL;
	gchar* type = NULL;
	gchar* emoji = NULL;
	gchar* movie = NULL;
	gchar* embed = NULL;
	gint offset;
	GdkPixbuf* real_pixbuf = NULL;
	gulong buffer_insert_pixbuf_signal_id = (gulong)g_object_get_data(G_OBJECT(buffer), "buffer-insert-pixbuf-signal-id");

	g_signal_stop_emission_by_name(G_OBJECT(buffer), "insert-pixbuf");

	title = g_strdup("");
	emoji = (gchar*)g_object_get_data(G_OBJECT(pixbuf), "emoji");
	if (emoji) {
		src = g_strdup("");
		type = g_strdup("");
	} else {
		src = (gchar*)g_object_get_data(G_OBJECT(pixbuf), "url");
		if (src) {
			g_object_set_data(G_OBJECT(pixbuf), "url", NULL);
			type = g_strdup("");
		} else {
			image_num++;
			if (gdk_pixbuf_get_has_alpha(pixbuf)) {
				src = g_strdup_printf("%03d.png", image_num);
				type = g_strdup("png");
			} else {
				src = g_strdup_printf("%03d.jpg", image_num);
				type = g_strdup("jpeg");
			}
		}
		movie = (gchar*)g_object_get_data(G_OBJECT(pixbuf), "movie");
		embed = (gchar*)g_object_get_data(G_OBJECT(pixbuf), "embed");
	}

	int w = gdk_pixbuf_get_width(pixbuf);
	int h = gdk_pixbuf_get_height(pixbuf);
	double rate, ratew, rateh;
	ratew = 320/(double)w;
	rateh = 240/(double)h;
	rate = ratew < rateh ? ratew : rateh;
	GdkPixbuf* scaled = NULL;
	if (rate < 1.0)
		scaled = gdk_pixbuf_scale_simple(pixbuf, (int)(w*rate), (int)(h*rate), GDK_INTERP_HYPER);

	tag = gtk_text_buffer_create_tag(buffer, NULL, NULL);
	g_object_set_data(G_OBJECT(tag), "name", g_strdup("img"));
	g_object_set_data(G_OBJECT(tag), "title", title);
	g_object_set_data(G_OBJECT(tag), "url", src);
	g_object_set_data(G_OBJECT(tag), "type", type);
	g_object_set_data(G_OBJECT(tag), "emoji", emoji);
	g_object_set_data(G_OBJECT(tag), "movie", movie);
	g_object_set_data(G_OBJECT(tag), "embed", embed);
	g_object_set_data(G_OBJECT(tag), "pixbuf", scaled ? scaled : pixbuf);
	g_object_set_data(G_OBJECT(tag), "real_pixbuf", scaled ? pixbuf : NULL);

	g_signal_handler_block(buffer, buffer_insert_pixbuf_signal_id);

	pixbuf = scaled ? scaled : pixbuf;

	offset = gtk_text_iter_get_offset(iter);

	int sequence = 0;

	commit_undo_info();
	append_buffer_undo_info(
		BUFFER_INSERT_PIXBUF,
		GDK_TYPE_PIXBUF,
		offset,
		offset+1,
		gdk_pixbuf_ref(pixbuf),
		sequence++);

	append_buffer_undo_info(
		BUFFER_APPLY_TAG,
		GTK_TYPE_TEXT_TAG,
		offset,
		offset+1,
		g_object_ref(tag),
		sequence++);

	gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
	gtk_text_buffer_get_iter_at_offset(buffer, iter_orig, offset);
	gtk_text_buffer_apply_tag(buffer, tag, iter_orig, iter);

	delete iter_orig;

	g_signal_handler_unblock(buffer, buffer_insert_pixbuf_signal_id);
}

void on_buffer_insert_text(
	GtkTextBuffer* buffer,
	GtkTextIter* iter,
	gchar *str,
	gint len)
{
	commit_undo_info();
	append_buffer_undo_info(
		BUFFER_INSERT_TEXT,
		G_TYPE_STRING,
		gtk_text_iter_get_offset(iter),
		gtk_text_iter_get_offset(iter)+len,
		g_strdup(str),
		0);
}

void on_buffer_delete_range(
		GtkTextBuffer* buffer,
		GtkTextIter* start,
		GtkTextIter* end,
		gpointer user_data)
{
	commit_undo_info();

	int sequence = 0;

	GtkTextIter* iter = gtk_text_iter_copy(end);
	while(true) {
		if (!gtk_text_iter_backward_char(iter)) break;
		if (!gtk_text_iter_in_range(iter, start, end)) break;
		GSList* tags = gtk_text_iter_get_tags(iter);
		if (!tags) continue;
		int length = g_slist_length(tags);
		for(int n = length - 1; n >= 0; n--) {
			GtkTextTag* tag = (GtkTextTag*)g_slist_nth_data(tags, n);
			append_buffer_undo_info(
				BUFFER_REMOVE_TAG,
				GTK_TYPE_TEXT_TAG,
				gtk_text_iter_get_offset(iter),
				gtk_text_iter_get_offset(iter)+1,
				g_object_ref(tag),
				sequence++);
			GdkPixbuf* pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(tag), "pixbuf");
			if (pixbuf) {
				append_buffer_undo_info(
					BUFFER_DELETE_PIXBUF,
					GDK_TYPE_PIXBUF,
					gtk_text_iter_get_offset(iter),
					gtk_text_iter_get_offset(iter)+1,
					g_object_ref(pixbuf),
					sequence++);
			}
		}
	}
	gtk_text_iter_free(iter);

	gchar* text = gtk_text_buffer_get_text(buffer, start, end, TRUE);
	if (text && strlen(text)) {
		append_buffer_undo_info(
			BUFFER_DELETE_TEXT,
			G_TYPE_STRING,
			gtk_text_iter_get_offset(start),
			gtk_text_iter_get_offset(end),
			g_strdup(text),
			sequence++);
	}
}

void on_imagefile_select(GtkWidget* widget, gpointer user_data)
{
	std::string name = _("Image File");
	std::vector<std::string> filters;
	filters.push_back("*.jpg");
	filters.push_back("*.png");
	filters.push_back("*.gif");
	std::vector<std::string> mimes;
	mimes.push_back("image/*");

	std::string filename = get_selected_file(widget, true, name, filters, mimes);
	if (filename.size()) {
		GtkWidget* url_entry = (GtkWidget*)user_data;
		gtk_entry_set_text(GTK_ENTRY(url_entry), filename.c_str());
	}
}

void insert_link(GtkWidget* widget, GtkTextBuffer* buffer, gboolean is_image)
{
	GtkWidget* dialog;
	GtkWidget* table;
	GtkWidget* label;
	GtkWidget* title;
	GtkWidget* url;
	GtkWidget* fileselect;

	dialog = gtk_dialog_new();
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			_("_Add"), GTK_RESPONSE_ACCEPT,
			NULL);

	if (is_image)
		gtk_window_set_title(GTK_WINDOW(dialog), _("Image"));
	else
		gtk_window_set_title(GTK_WINDOW(dialog), _("Link"));

	table = gtk_table_new(2, is_image ? 3 : 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 6);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table); 

	label = gtk_label_new(_("_Title:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   0, 1,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	title = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), title);
	gtk_table_attach(
			GTK_TABLE(table),
			title,
			1, 2,                   0, 1,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	label = gtk_label_new(_("_URL:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   1, 2,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	url = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), url);
	gtk_table_attach(
			GTK_TABLE(table),
			url,
			1, 2,                   1, 2,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	if (is_image) {
		fileselect = gtk_button_new_with_label(_("_Select"));
		gtk_button_set_use_underline(GTK_BUTTON(fileselect), TRUE);
		g_signal_connect(G_OBJECT(fileselect), "clicked", G_CALLBACK(on_imagefile_select), url);
		gtk_table_attach(
				GTK_TABLE(table),
				fileselect,
				2, 3,                   1, 2,
				(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
				0,                      0);
	}

	gboolean is_selected = false;
	GtkTextIter* start = new GtkTextIter;
	GtkTextIter* end = new GtkTextIter;
	if (gtk_text_buffer_get_selection_bounds(buffer, start, end))
	{
		gchar* selected_text = gtk_text_buffer_get_text(buffer, start, end, FALSE);
		gtk_entry_set_text(GTK_ENTRY(title), selected_text);
		GSList* tags = gtk_text_iter_get_tags(start);
		while(tags) {
			gchar* name_text = (gchar*)g_object_get_data(G_OBJECT(tags->data), "name");
			if (name_text && (!g_strcasecmp(name_text, "a") || !g_strcasecmp(name_text, "img"))) {
				gchar* title_text = (gchar*)g_object_get_data(G_OBJECT(tags->data), "title");
				gchar* url_text = (gchar*)g_object_get_data(G_OBJECT(tags->data), "url");
				gtk_entry_set_text(GTK_ENTRY(title), title_text);
				gtk_entry_set_text(GTK_ENTRY(url), url_text);
				break;
			}
			tags = g_slist_next(tags);
		}
		is_selected = TRUE;
	}

	gtk_widget_show_all(table);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(toplevel));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar* title_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(title));
		gchar* url_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(url));
		GtkTextIter* iter = new GtkTextIter;
		gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
		if (is_image) {
			GdkPixbuf* pixbuf = NULL;
			gchar* src = NULL;
			if (is_selected) {
				GSList* tags = gtk_text_iter_get_tags(start);
				while(tags) {
					pixbuf = (GdkPixbuf*)g_object_get_data(G_OBJECT(tags->data), "pixbuf");
					if (pixbuf) {
						g_free(g_object_get_data(G_OBJECT(tags->data), "title"));
						g_object_set_data(G_OBJECT(tags->data), "title", g_strdup(title_text));
						src = (gchar*)g_object_get_data(G_OBJECT(tags->data), "src");
						break;
					}
					tags = g_slist_next(tags);
				}
			} else
			if (url_text && strlen(url_text) && !src) {
				GError* error = NULL;
				pixbuf = url2pixbuf(url_text, &error);
				if (error) error_dialog(widget, error->message);
				if (pixbuf)
					g_object_set_data(G_OBJECT(pixbuf), "url", g_strdup(url_text));
				if (is_selected && gtk_text_buffer_get_selection_bounds(buffer, start, end))
					gtk_text_buffer_delete(buffer, start, end);
			}

			if (!is_selected)
				gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
		} else {
			GtkTextTag* link_tag = link_tag = gtk_text_buffer_create_tag(
					buffer,
					NULL,
					"underline",
					PANGO_UNDERLINE_SINGLE,
					"foreground",
					"#0000FF",
					NULL);
			g_object_set_data(G_OBJECT(link_tag), "name", g_strdup("a"));
			g_object_set_data(G_OBJECT(link_tag), "title", g_strdup(title_text));
			g_object_set_data(G_OBJECT(link_tag), "url", g_strdup(url_text));
			if (is_selected) {
				append_buffer_undo_info(
					BUFFER_APPLY_TAG,
					GTK_TYPE_TEXT_TAG,
					gtk_text_iter_get_offset(start),
					gtk_text_iter_get_offset(end),
					g_object_ref(link_tag),
					0);

				gtk_text_buffer_apply_tag(buffer, link_tag, start, end);
			} else {
				append_buffer_undo_info(
					BUFFER_APPLY_TAG,
					GTK_TYPE_TEXT_TAG,
					gtk_text_iter_get_offset(iter),
					gtk_text_iter_get_offset(iter)+strlen(title_text),
					g_object_ref(link_tag),
					1);

				gtk_text_buffer_insert_with_tags(buffer, iter, title_text, -1, link_tag, NULL);
			}
		}
		delete iter;
	}
	delete start;
	delete end;

	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(toplevel));
}

void on_menu_link(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	insert_link(widget, buffer, FALSE);
}

void on_menu_image(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	insert_link(widget, buffer, TRUE);
}

void on_menu_header1(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;

	gtk_text_buffer_begin_user_action(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	start = gtk_text_iter_copy(iter);
	end = gtk_text_iter_copy(iter);
	if (gtk_text_iter_has_tag(iter, h1_tag)) {
		gtk_text_iter_backward_to_tag_toggle(start, h1_tag);
		gtk_text_iter_forward_to_tag_toggle(end, h1_tag);
		gtk_text_buffer_remove_tag(buffer, h1_tag, start, end);
	} else {
		int lineno = gtk_text_iter_get_line(iter);
		gtk_text_buffer_get_iter_at_line_offset(buffer, start, lineno, 0);
		gtk_text_iter_forward_to_line_end(end);
		gtk_text_buffer_apply_tag(buffer, h1_tag, start, end);
		if (gtk_text_iter_equal(end, iter))
			gtk_text_buffer_insert_with_tags(buffer, end, "\n", -1, h1_tag, NULL);
	}
	gtk_text_iter_free(start);
	gtk_text_iter_free(end);
	delete iter;
	gtk_text_buffer_end_user_action(buffer);
}

void on_menu_header2(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;

	gtk_text_buffer_begin_user_action(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	start = gtk_text_iter_copy(iter);
	end = gtk_text_iter_copy(iter);
	if (gtk_text_iter_has_tag(iter, h2_tag)) {
		gtk_text_iter_backward_to_tag_toggle(start, h2_tag);
		gtk_text_iter_forward_to_tag_toggle(end, h2_tag);
		gtk_text_buffer_remove_tag(buffer, h2_tag, start, end);
	} else {
		int lineno = gtk_text_iter_get_line(iter);
		gtk_text_buffer_get_iter_at_line_offset(buffer, start, lineno, 0);
		gtk_text_iter_forward_to_line_end(end);
		gtk_text_buffer_apply_tag(buffer, h2_tag, start, end);
		if (gtk_text_iter_equal(end, iter))
			gtk_text_buffer_insert_with_tags(buffer, end, "\n", -1, h2_tag, NULL);
	}
	gtk_text_iter_free(start);
	gtk_text_iter_free(end);
	delete iter;
	gtk_text_buffer_end_user_action(buffer);
}

void on_menu_header3(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;

	gtk_text_buffer_begin_user_action(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	start = gtk_text_iter_copy(iter);
	end = gtk_text_iter_copy(iter);
	if (gtk_text_iter_has_tag(iter, h3_tag)) {
		gtk_text_iter_backward_to_tag_toggle(start, h3_tag);
		gtk_text_iter_forward_to_tag_toggle(end, h3_tag);
		gtk_text_buffer_remove_tag(buffer, h3_tag, start, end);
	} else {
		int lineno = gtk_text_iter_get_line(iter);
		gtk_text_buffer_get_iter_at_line_offset(buffer, start, lineno, 0);
		gtk_text_iter_forward_to_line_end(end);
		gtk_text_buffer_apply_tag(buffer, h3_tag, start, end);
		if (gtk_text_iter_equal(end, iter))
			gtk_text_buffer_insert_with_tags(buffer, end, "\n", -1, h3_tag, NULL);
	}
	gtk_text_iter_free(start);
	gtk_text_iter_free(end);
	delete iter;
	gtk_text_buffer_end_user_action(buffer);
}

void on_menu_pre(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	if (gtk_text_iter_has_tag(iter, pre_tag)) {
		start = gtk_text_iter_copy(iter);
		end = gtk_text_iter_copy(iter);
		gtk_text_iter_backward_to_tag_toggle(start, pre_tag);
		gtk_text_iter_forward_to_tag_toggle(end, pre_tag);
		gtk_text_buffer_remove_tag(buffer, pre_tag, start, end);

		append_buffer_undo_info(
			BUFFER_REMOVE_TAG,
			GTK_TYPE_TEXT_TAG,
			gtk_text_iter_get_offset(start),
			gtk_text_iter_get_offset(end),
			g_object_ref(pre_tag),
			0);

		gtk_text_iter_free(start);
		gtk_text_iter_free(end);
	} else {
		start = new GtkTextIter;
		end = new GtkTextIter;
		if (gtk_text_buffer_get_selection_bounds(buffer, start, end)) {
			gtk_text_buffer_apply_tag(buffer, pre_tag, start, end);

			append_buffer_undo_info(
				BUFFER_APPLY_TAG,
				GTK_TYPE_TEXT_TAG,
				gtk_text_iter_get_offset(start),
				gtk_text_iter_get_offset(end),
				g_object_ref(pre_tag),
				0);

			gtk_text_buffer_get_end_iter(buffer, iter);
			if (gtk_text_iter_equal(end, iter)) {
				gtk_text_buffer_insert_with_tags(buffer, end, "\n", -1, pre_tag, NULL);
			}
		}
		delete start;
		delete end;
	}
	delete iter;
}

void on_menu_blockquote(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	if (gtk_text_iter_has_tag(iter, blockquote_tag)) {
		start = gtk_text_iter_copy(iter);
		end = gtk_text_iter_copy(iter);
		gtk_text_iter_backward_to_tag_toggle(start, blockquote_tag);
		gtk_text_iter_forward_to_tag_toggle(end, blockquote_tag);

		gtk_text_buffer_remove_tag(buffer, blockquote_tag, start, end);

		append_buffer_undo_info(
			BUFFER_REMOVE_TAG,
			GTK_TYPE_TEXT_TAG,
			gtk_text_iter_get_offset(start),
			gtk_text_iter_get_offset(end),
			g_object_ref(blockquote_tag),
			0);

		gtk_text_iter_free(start);
		gtk_text_iter_free(end);
	} else {
		start = new GtkTextIter;
		end = new GtkTextIter;
		if (gtk_text_buffer_get_selection_bounds(buffer, start, end)) {
			gtk_text_buffer_apply_tag(buffer, blockquote_tag, start, end);

			append_buffer_undo_info(
				BUFFER_APPLY_TAG,
				GTK_TYPE_TEXT_TAG,
				gtk_text_iter_get_offset(start),
				gtk_text_iter_get_offset(end),
				g_object_ref(blockquote_tag),
				0);

			gtk_text_buffer_get_end_iter(buffer, iter);
			if (gtk_text_iter_equal(end, iter))
				gtk_text_buffer_insert_with_tags(buffer, end, "\n", -1, blockquote_tag, NULL);
		}
		delete start;
		delete end;
	}
	delete iter;
}

void on_menu_align_left(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	start = new GtkTextIter;
	end = new GtkTextIter;
	if (gtk_text_buffer_get_selection_bounds(buffer, start, end)) {
		int sequence = 0;
		if (gtk_text_iter_has_tag(start, align_center_tag)) {
			gtk_text_buffer_remove_tag(buffer, align_center_tag, start, end);
			append_buffer_undo_info(
				BUFFER_REMOVE_TAG,
				GTK_TYPE_TEXT_TAG,
				gtk_text_iter_get_offset(start),
				gtk_text_iter_get_offset(end),
				g_object_ref(align_center_tag),
				sequence++);
		}

		if (gtk_text_iter_has_tag(start, align_right_tag)) {
			gtk_text_buffer_remove_tag(buffer, align_right_tag, start, end);
			append_buffer_undo_info(
				BUFFER_REMOVE_TAG,
				GTK_TYPE_TEXT_TAG,
				gtk_text_iter_get_offset(start),
				gtk_text_iter_get_offset(end),
				g_object_ref(align_right_tag),
				sequence++);
		}
	}
	delete start;
	delete end;
	delete iter;
}

void on_menu_align_center(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	if (gtk_text_iter_has_tag(iter, align_center_tag)) {
		start = gtk_text_iter_copy(iter);
		end = gtk_text_iter_copy(iter);
		gtk_text_iter_backward_to_tag_toggle(start, align_center_tag);
		gtk_text_iter_forward_to_tag_toggle(end, align_center_tag);

		gtk_text_buffer_remove_tag(buffer, align_center_tag, start, end);

		append_buffer_undo_info(
			BUFFER_REMOVE_TAG,
			GTK_TYPE_TEXT_TAG,
			gtk_text_iter_get_offset(start),
			gtk_text_iter_get_offset(end),
			g_object_ref(align_center_tag),
			0);

		gtk_text_iter_free(start);
		gtk_text_iter_free(end);
	} else {
		start = new GtkTextIter;
		end = new GtkTextIter;
		if (gtk_text_buffer_get_selection_bounds(buffer, start, end)) {
			gtk_text_buffer_remove_tag(buffer, align_right_tag, start, end);

			gtk_text_buffer_apply_tag(buffer, align_center_tag, start, end);
			append_buffer_undo_info(
				BUFFER_APPLY_TAG,
				GTK_TYPE_TEXT_TAG,
				gtk_text_iter_get_offset(start),
				gtk_text_iter_get_offset(end),
				g_object_ref(align_center_tag),
				0);

			gtk_text_buffer_get_end_iter(buffer, iter);
			if (gtk_text_iter_equal(end, iter))
				gtk_text_buffer_insert_with_tags(buffer, end, "\n", -1, align_center_tag, NULL);
		}
		delete start;
		delete end;
	}
	delete iter;
}

void on_menu_align_right(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));
	GtkTextIter* iter = new GtkTextIter;
	GtkTextIter* start;
	GtkTextIter* end;
	gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
	if (gtk_text_iter_has_tag(iter, align_right_tag)) {
		start = gtk_text_iter_copy(iter);
		end = gtk_text_iter_copy(iter);
		gtk_text_iter_backward_to_tag_toggle(start, align_right_tag);
		gtk_text_iter_forward_to_tag_toggle(end, align_right_tag);

		gtk_text_buffer_remove_tag(buffer, align_right_tag, start, end);

		append_buffer_undo_info(
			BUFFER_REMOVE_TAG,
			GTK_TYPE_TEXT_TAG,
			gtk_text_iter_get_offset(start),
			gtk_text_iter_get_offset(end),
			g_object_ref(align_right_tag),
			0);

		gtk_text_iter_free(start);
		gtk_text_iter_free(end);
	} else {
		start = new GtkTextIter;
		end = new GtkTextIter;
		if (gtk_text_buffer_get_selection_bounds(buffer, start, end)) {
			gtk_text_buffer_remove_tag(buffer, align_center_tag, start, end);

			gtk_text_buffer_apply_tag(buffer, align_right_tag, start, end);

			append_buffer_undo_info(
				BUFFER_APPLY_TAG,
				GTK_TYPE_TEXT_TAG,
				gtk_text_iter_get_offset(start),
				gtk_text_iter_get_offset(end),
				g_object_ref(align_right_tag),
				0);

			gtk_text_buffer_get_end_iter(buffer, iter);
			if (gtk_text_iter_equal(end, iter))
				gtk_text_buffer_insert_with_tags(buffer, end, "\n", -1, align_right_tag, NULL);
		}
		delete start;
		delete end;
	}
	delete iter;
}

void on_menu_embedobj(GtkWidget* widget, gpointer user_data)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_data));

	GtkWidget* dialog;
	GtkWidget* source;
	GtkTextBuffer* input_buffer;

	dialog = gtk_dialog_new();
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			_("_Insert"), GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Embed Object"));
	source = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(source), GTK_WRAP_WORD);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), source); 

	input_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(source));

	gtk_widget_show(source);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(toplevel));
	gtk_widget_set_size_request(source, 400, 300);
	gtk_widget_queue_resize(source);

	gboolean is_selected = false;
	GtkTextIter* start = new GtkTextIter;
	GtkTextIter* end = new GtkTextIter;
	if (gtk_text_buffer_get_selection_bounds(buffer, start, end))
	{
		GSList* tags = gtk_text_iter_get_tags(start);
		while(tags) {
			gchar* source_text = (gchar*)g_object_get_data(G_OBJECT(tags->data), "embed");
			if (source_text) {
				if (is_selected) {
					g_free(g_object_get_data(G_OBJECT(tags->data), "embed"));
					g_object_set_data(G_OBJECT(tags->data), "embed", source_text);
				} else
					gtk_text_buffer_set_text(input_buffer, source_text, strlen(source_text));
				break;
			}
			tags = g_slist_next(tags);
		}
		is_selected = TRUE;
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gtk_text_buffer_get_start_iter(input_buffer, start);
		gtk_text_buffer_get_end_iter(input_buffer, end);
		gchar* source_text = gtk_text_buffer_get_text(input_buffer, start, end, TRUE);
		if (source_text) {
			GtkTextIter* iter = new GtkTextIter;
			gtk_text_buffer_get_iter_at_mark(buffer, iter, gtk_text_buffer_get_insert(buffer));
			GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file("embed.png", NULL);
			if (pixbuf) {
				g_object_set_data(G_OBJECT(pixbuf), "embed", source_text);
				gtk_text_buffer_insert_pixbuf(buffer, iter, pixbuf);
			}
			delete iter;
		}
	}
	delete start;
	delete end;

	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(toplevel));
}

bool fetch_profile_atompp(std::string& errmsg)
{
	current_category_list.clear();
	current_category_list["-"] = "-";

	AtomPP::AtomFeed feed;
	
	if (current_server == MAYBE_AMEBA_ATOMPP) {
		std::string passwd = AtomPP::StringToHex(AtomPP::md5(user_configs[user_profile]["passwd"]));
		feed = AtomPP::getFeed(
			user_configs[user_profile]["server"],
			user_configs[user_profile]["userid"],
			passwd);
	} else
		feed = AtomPP::getFeed(
			user_configs[user_profile]["server"],
			user_configs[user_profile]["userid"],
			user_configs[user_profile]["passwd"]);
	if (user_configs[user_profile]["url"].size() == 0) {
		std::vector<AtomPP::AtomLink>::iterator it;
		for(it = feed.links.begin(); it != feed.links.end(); it++) {
			if (it->rel == "alternate") {
				user_configs[user_profile]["url"] = it->href;
				break;
			}
		}
	}

	if (user_configs[user_profile]["blogid"].size() == 0)
		user_configs[user_profile]["blogid"] = feed.id;
	if (user_configs[user_profile]["name"].size() == 0)
		user_configs[user_profile]["name"] = feed.title;

	return true;
}

bool fetch_profile_xmlrpc(std::string& errmsg)
{
	XmlRpc::XmlRpcValue::ValueArray valuearray;
	std::vector<XmlRpc::XmlRpcValue> requests;
	XmlRpc::XmlRpcValue response;

	current_category_list.clear();

	requests.clear();
	requests.push_back(user_configs[user_profile]["appkey"]);
	requests.push_back(user_configs[user_profile]["userid"]);
	requests.push_back(user_configs[user_profile]["passwd"]);
	if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "blogger.getUsersBlogs", requests, response) == 0) {
		int index = 0;
		if (response.getType() == XmlRpc::XmlRpcValue::TypeArray) {
			for(int n = 0; n < response.size(); n++) {
				if (response[n]["blogid"].valueString() == user_configs[user_profile]["blogid"]) {
					index = n;
					break;
				}
			}
			XmlRpc::XmlRpcValue value = response[index];
			response = value;
		}
		if (response.getType() != XmlRpc::XmlRpcValue::TypeStruct) {
			std::string faultMessage = _("Can't fetch category list:");
			faultMessage += "\n";
			faultMessage += XmlRpc::getFaultResult(response);
			errmsg = faultMessage;
			return false;
		}
		std::string blogid = response["blogid"].valueString();
		std::string blogName = response["blogName"].valueString();

		user_configs[user_profile]["url"] = response["url"].valueString();
		user_configs[user_profile]["blogid"] = blogid;
		user_configs[user_profile]["name"] = blogName;

		XmlRpc::XmlRpcValue::ValueArray valuearray;
		std::vector<XmlRpc::XmlRpcValue> requests;
		XmlRpc::XmlRpcValue response;

		requests.clear();
		requests.push_back(user_configs[user_profile]["blogid"]);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "mt.getCategoryList", requests, response) == 0) {
			current_server = MAYBE_MOVABLETYPE;
			if (response.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
				std::string key = response["categoryId"];
				std::string val = response["categoryName"];
				current_category_list[key] = val;
			} else
			if (response.getType() == XmlRpc::XmlRpcValue::TypeArray) {
				int size = response.size();
				for(int n = 0; n < size; n++) {
					XmlRpc::XmlRpcValue::ValueStruct st = response[n];
					std::string key = st["categoryId"];
					std::string val = st["categoryName"];
					current_category_list[key] = val;
				}
			} else {
				std::string val = response.valueString();
			}
		} else
		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "metaWeblog.getCategories", requests, response) == 0) {
			current_server = MAYBE_UNKNOWN;
			for(int n = 0; n < response.size(); n++) {
				printf("%s\n", response[n].valueString().c_str());
				std::string key = response[n]["description"].valueString();
				std::string val = response[n]["title"].valueString();
				current_category_list[key] = val;
			}
		}
	} else {
		std::string faultMessage = _("Can't fetch category list:");
		faultMessage += "\n";
		faultMessage += XmlRpc::getFaultResult(response);
		errmsg = faultMessage;
		return false;
	}
	return true;
}

bool fetch_profile_ftpup(std::string& errmsg)
{
	current_category_list.clear();
	current_category_list["/"] = "/";

	current_category_list.clear();

	Blosxom::FolderList folders;
	folders = Blosxom::FtpFolders(
		user_configs[user_profile]["server"],
		user_configs[user_profile]["userid"],
		user_configs[user_profile]["passwd"],
		user_configs[user_profile]["blogid"]);
	Blosxom::FolderList::iterator it;
	for(it = folders.begin(); it != folders.end(); it++)
		current_category_list[it->first] = it->second;
	std::string server = user_configs[user_profile]["server"];
	std::string blogid = user_configs[user_profile]["blogid"];
	if (blogid[0] == '/')
		user_configs[user_profile]["name"] = server + blogid.substr(1);
	else
		user_configs[user_profile]["name"] = server + blogid;

	return true;
}

GdkPixbuf* load_blog_icon_cache()
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* cachefile = g_build_filename(confdir, "." APP_TITLE".icon", NULL);
	gchar* newurl = g_filename_from_uri(cachefile, NULL, NULL);
	GError* _error = NULL;
	GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(newurl ? newurl : cachefile, &_error);
	g_free(cachefile);
	return pixbuf;
}

void save_blog_icon_cache(GdkPixbuf* pixbuf)
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* cachefile = g_build_filename(confdir, "." APP_TITLE".icon", NULL);
	if (pixbuf)
		gdk_pixbuf_save(pixbuf, cachefile, "png", NULL, NULL);
	g_free(cachefile);
}

void revoke_blog_icon_cache()
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* cachefile = g_build_filename(confdir, "." APP_TITLE".icon", NULL);
	g_unlink(cachefile);
	g_free(cachefile);
}

void load_category_list_cache()
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* cachefile = g_build_filename(confdir, "." APP_TITLE".category", NULL);
	FILE* fp = fopen(cachefile, "r");
	g_free(cachefile);
	if (!fp) return;
	char buffer[BUFSIZ];

	current_category_list.clear();
	while(fp && fgets(buffer, sizeof(buffer), fp)) {
		gchar* line = g_strchomp(buffer);
		gchar* ptr;
		ptr = strchr(line, '=');
		if (ptr && *line != ';') {
			*ptr++ = 0;
			current_category_list[line] = ptr;
		}
	}
	if (fp) fclose(fp);
}

void save_category_list_cache()
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* cachefile = g_build_filename(confdir, "." APP_TITLE".category", NULL);
	FILE *fp;
	fp = fopen(cachefile, "w");
	g_free(cachefile);
	if (!fp) return;
	CategoryList::iterator it;
	for(it = current_category_list.begin(); it != current_category_list.end(); it++) {
		fprintf(fp, "%s=%s\n", it->first.c_str(), it->second.c_str());
	}
	fclose(fp);
}

void revoke_category_list_cache()
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* cachefile = g_build_filename(confdir, "." APP_TITLE".category", NULL);
	g_unlink(cachefile);
	g_free(cachefile);
	current_category_list.clear();
}

bool load_profile(std::string& errmsg)
{
	GtkWidget *category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");

	load_category_list_cache();

	bool ret = false;
	if (current_category_list.size() == 0) {
		if (user_configs[user_profile]["engine"] == "xmlrpc")
			ret = fetch_profile_xmlrpc(errmsg);
		else
		if (user_configs[user_profile]["engine"] == "atompp")
			ret = fetch_profile_atompp(errmsg);
		else
		if (user_configs[user_profile]["engine"] == "ftpup")
			ret = fetch_profile_ftpup(errmsg);
		if (!ret)
			return false;

		save_category_list_cache();
	}

	gdk_threads_enter();

	GtkListStore* category_model = (GtkListStore*)g_object_get_data(G_OBJECT(toplevel), "category_model");
	gtk_list_store_clear(category_model);
	GtkTreeIter* iter = new GtkTreeIter;
	CategoryList::iterator it;
	for(it = current_category_list.begin(); it != current_category_list.end(); it++) {
		gtk_list_store_append(category_model, iter);
		gtk_list_store_set(category_model, iter, 0, it->second.c_str(), -1);
		gtk_list_store_set(category_model, iter, 1, it->first.c_str(), -1);
	}
	delete iter;
	gtk_combo_box_set_active(GTK_COMBO_BOX(category), 0);

	GdkPixbuf* icon = load_blog_icon_cache();
	if (!icon) {
		char *ptr, *old;

		if (user_configs[user_profile]["url"].size())
			ptr = old = strdup(user_configs[user_profile]["url"].c_str());
		else
			ptr = old = strdup(user_configs[user_profile]["server"].c_str());
		ptr = strstr(ptr, "//");
		ptr = ptr ? strstr(ptr+2, "/~") : NULL;
		if (ptr) {
			char* sep = strchr(ptr+1, '/');
			if (sep) *sep = 0;
			std::string icon_url = old;
			icon_url += "/favicon.ico";
			icon = url2pixbuf(icon_url.c_str(), NULL, "ico");
		}
		free(old);

		if (!icon) {
			if (user_configs[user_profile]["url"].size())
				ptr = old = strdup(user_configs[user_profile]["url"].c_str());
			else
				ptr = old = strdup(user_configs[user_profile]["server"].c_str());
			ptr = strstr(ptr, "//");
			ptr = ptr ? strchr(ptr+2, '/') : NULL;
			if (ptr) {
				*ptr = 0;
				std::string icon_url = old;
				icon_url += "/favicon.ico";
				icon = url2pixbuf(icon_url.c_str(), NULL, "ico");
			}
			free(old);
		}

		if (icon) save_blog_icon_cache(icon);
	}
	if (!icon)
		icon = gdk_pixbuf_new_from_file((module_path + "/logo.png").c_str(), NULL);

	current_postid = "";

	gdk_threads_leave();

	GtkWidget *emoji = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "emoji");
	if (user_configs[user_profile]["emoji"] != "true")
		gtk_widget_hide(emoji);
	else
		gtk_widget_show(emoji);

	if (icon) {
#ifdef _WIN32
		GdkPixbuf* icon32 = gdk_pixbuf_scale_simple(icon, 32, 32, GDK_INTERP_HYPER);
		gtk_window_set_icon(GTK_WINDOW(toplevel), icon32);
		g_object_unref(icon32);
#else
		gtk_window_set_icon(GTK_WINDOW(toplevel), icon);
#endif
		g_object_unref(icon);
	}
	gchar* title = g_strconcat(_(APP_TITLE), " - ", user_configs[user_profile]["name"].c_str(), NULL);
	gtk_window_set_title(GTK_WINDOW(toplevel), title);
	g_free(title);

	std::string server = user_configs[user_profile]["server"];
	if (server.find(".blogger.com/") != std::string::npos)
		current_server = MAYBE_BLOGGER_ATOMPP;
	if (server.find("ameblo.jp/") != std::string::npos)
		current_server = MAYBE_AMEBA_ATOMPP;
	if (server.find("//b.hatena.ne.jp/") != std::string::npos)
		current_server = MAYBE_HATENA_BOOKMARK;
	if (server.find("//f.hatena.ne.jp/") != std::string::npos)
		current_server = MAYBE_HATENA_FOTOLIFE;

	return true;
}

void load_configs()
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* conffile = g_build_filename(confdir, "." APP_TITLE, NULL);
	gchar* confenv = getenv("BLOGWRITER_CONF");
	if (confenv)
		conffile = g_strdup(confenv);
	std::cout << conffile << std::endl;
	FILE* fp = fopen(conffile, "r");
	g_free(conffile);
	char buffer[BUFSIZ];

	user_configs.clear();
	Config config;
	std::string profile = "global";
	while(fp && fgets(buffer, sizeof(buffer), fp)) {
		gchar* line = g_strchomp(buffer);
		gchar* ptr;
		ptr = strchr(line, ']');
		if (*line == '[' && ptr) {
			*ptr = 0;
			if (config.size())
				user_configs[profile] = config;
			config.clear();
			profile = line+1;
			continue;
		}
		ptr = strchr(line, '=');
		if (ptr && *line != ';') {
			*ptr++ = 0;
			config[line] = ptr;
		}
	}
	if (fp) fclose(fp);
	user_configs[profile] = config;
	user_profile = user_configs["global"]["profile"];

	std::string font = user_configs["global"]["font"];
	int size;
	std::string family;
	if (get_font_info(font, size, family)) {
		GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");

		PangoFontDescription* pangoFont = pango_font_description_new();

		pango_font_description_set_family(pangoFont, family.c_str());
		if (size) pango_font_description_set_size(pangoFont, size * PANGO_SCALE);
		gtk_widget_modify_font(textview, pangoFont);

		pango_font_description_free(pangoFont);
	}
}

void save_configs()
{
	const gchar* confdir = g_get_user_config_dir();
	gchar* conffile = g_build_filename(confdir, "." APP_TITLE, NULL);
	gchar* backfile = g_build_filename(confdir, "." APP_TITLE".bak", NULL);
	std::cout << conffile << std::endl;
	FILE *fp, *fpback;
	char buffer[BUFSIZ];
	fp = fopen(conffile, "r");
	if (fp) {
		fpback = fopen(backfile, "w");
		while(fpback && fp && fgets(buffer, sizeof(buffer), fp)) {
			fputs(buffer, fpback);
		}
		if (fpback) fclose(fpback);
		fclose(fp);
	}
	fp = fopen(conffile, "w");
	g_free(conffile);
	g_free(backfile);
	if (!fp) {
		//error_dialog(toplevel, l2u(strerror(errno)));
		return;
	}
	ConfigList::iterator it_list;
	Config::iterator it;
	for(it_list = user_configs.begin(); it_list != user_configs.end(); it_list++) {
		if (it_list != user_configs.begin())
			fprintf(fp, "\n");
		std::string section = it_list->first;
		Config config = it_list->second;
		fprintf(fp, "[%s]\n", section.c_str());
		for(it = config.begin(); it != config.end(); it++) {
			std::string key = it->first;
			std::string val = it->second;
			fprintf(fp, "%s=%s\n", key.c_str(), val.c_str());
		}
	}
	fclose(fp);
}

void on_profile_new(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* parent = (GtkWidget*)user_data;
	GtkWidget* dialog;
	GtkWidget* table;
	GtkWidget* label;
	GtkWidget* name;
	GtkWidget* profile;
	GtkListStore* profile_model;

	profile = (GtkWidget*)g_object_get_data(G_OBJECT(parent), "profile");
	profile_model = (GtkListStore*)g_object_get_data(G_OBJECT(parent), "profile_model");

	dialog = gtk_dialog_new();
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			_("_New"), GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), _("New Profile"));

	table = gtk_table_new(1, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 6);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table); 

	label = gtk_label_new(_("_Profile:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   0, 1,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	name = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), profile);
	gtk_table_attach(
			GTK_TABLE(table),
			name,
			1, 2,                   0, 1,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	gtk_widget_show_all(table);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(parent));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar* name_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(name));
		if (strpbrk(name_text, "[]")) {
			error_dialog(dialog, _("Can't accept some letter for profile name."));
			name_text = NULL;
		}
		if (name_text) {
			GtkTreeIter* iter = new GtkTreeIter;
			ConfigList::iterator it_list;
			it_list = user_configs.find(name_text);
			if (it_list == user_configs.end()) {
				user_profile = name_text;
				gtk_list_store_append(profile_model, iter);
				gtk_list_store_set(profile_model, iter, 0, user_profile.c_str(), -1);
				gtk_combo_box_set_active(GTK_COMBO_BOX(profile), 0);
				if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(profile_model), iter)) {
					while(true) {
						GValue value = { 0, };
						gtk_tree_model_get_value(GTK_TREE_MODEL(profile_model), iter, 0, &value);
						if (user_profile == g_value_get_string(&value)) {
							gtk_combo_box_set_active_iter(GTK_COMBO_BOX(profile), iter);
							break;
						}
						if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(profile_model), iter))
							break;
					}
				}
			} else {
				error_dialog(dialog, _("This profile already used."));
			}

			delete iter;
		}
	}

	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(parent));
}

void on_profile_copy(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* parent = (GtkWidget*)user_data;
	GtkWidget* dialog;
	GtkWidget* table;
	GtkWidget* label;
	GtkWidget* name;
	GtkWidget* profile;
	GtkListStore* profile_model;

	profile = (GtkWidget*)g_object_get_data(G_OBJECT(parent), "profile");
	profile_model = (GtkListStore*)g_object_get_data(G_OBJECT(parent), "profile_model");

	dialog = gtk_dialog_new();
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			_("_New"), GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Copy Profile"));

	table = gtk_table_new(1, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 6);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table); 

	label = gtk_label_new(_("_Profile:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   0, 1,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	name = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), profile);
	gtk_table_attach(
			GTK_TABLE(table),
			name,
			1, 2,                   0, 1,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	gtk_widget_show_all(table);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(parent));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar* name_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(name));
		if (strpbrk(name_text, "[]")) {
			error_dialog(dialog, _("Can't accept some letter for profile name."));
			name_text = NULL;
		}
		if (name_text) {
			GtkTreeIter* iter = new GtkTreeIter;
			ConfigList::iterator it_list;
			it_list = user_configs.find(name_text);
			if (it_list == user_configs.end()) {
				char* profile_text = (char*)gtk_combo_box_get_active_text(GTK_COMBO_BOX(profile));
				if (profile_text) {
					user_configs[name_text] = user_configs[profile_text];
					user_profile = name_text;
				}
				gtk_list_store_append(profile_model, iter);
				gtk_list_store_set(profile_model, iter, 0, user_profile.c_str(), -1);
				gtk_combo_box_set_active(GTK_COMBO_BOX(profile), 0);
				if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(profile_model), iter)) {
					while(true) {
						GValue value = { 0, };
						gtk_tree_model_get_value(GTK_TREE_MODEL(profile_model), iter, 0, &value);
						if (user_profile == g_value_get_string(&value)) {
							gtk_combo_box_set_active_iter(GTK_COMBO_BOX(profile), iter);
							break;
						}
						if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(profile_model), iter))
							break;
					}
				}
			} else {
				error_dialog(dialog, _("This profile already used."));
			}

			delete iter;
		}
	}

	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(parent));
}

void on_profile_delete(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = (GtkWidget*)user_data;
	if (!confirm_dialog(dialog, _("Are you sure to delete this profile?")))
		return;

	GtkWidget* profile = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "profile");
	GtkListStore* profile_model = (GtkListStore*)g_object_get_data(G_OBJECT(dialog), "profile_model");

	GtkTreeIter* iter = new GtkTreeIter;
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(profile), iter)) {
		GValue value = { 0, };
		gtk_tree_model_get_value(GTK_TREE_MODEL(profile_model), iter, 0, &value);
		ConfigList::iterator it = user_configs.find(g_value_get_string(&value));
		if (it != user_configs.end())
			user_configs.erase(it);
		gtk_list_store_remove(profile_model, iter);
		gtk_combo_box_set_active(GTK_COMBO_BOX(profile), 0);
	}
	delete iter;
}

void on_profile_changed(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = (GtkWidget*)user_data;
	GtkWidget* profile;
	GtkWidget* blogurl;
	GtkWidget* engine;
	GtkWidget* server;
	GtkWidget* userid;
	GtkWidget* passwd;
	GtkWidget* blogid;
	GtkWidget* format;
	combo_entry_data *pdata;

	profile = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "profile");
	blogurl = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "url");
	engine = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "engine");
	server = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "server");
	userid = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "userid");
	passwd = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "passwd");
	blogid = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "blogid");
	format = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "format");

	char* profile_text = (char*)gtk_combo_box_get_active_text(GTK_COMBO_BOX(widget));

	if (profile_text) {
		pdata = engine_entry;
		gtk_combo_box_set_active(GTK_COMBO_BOX(engine), 0);
		while(pdata->index != -1) {
			if (user_configs[profile_text]["engine"] == pdata->value) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(engine), pdata->index);
				break;
			}
			pdata++;
		}
		gtk_entry_set_text(GTK_ENTRY(blogurl), user_configs[profile_text]["url"].c_str());
		gtk_entry_set_text(GTK_ENTRY(server), user_configs[profile_text]["server"].c_str());
		gtk_entry_set_text(GTK_ENTRY(userid), user_configs[profile_text]["userid"].c_str());
		gtk_entry_set_text(GTK_ENTRY(passwd), user_configs[profile_text]["passwd"].c_str());
		gtk_entry_set_text(GTK_ENTRY(blogid), user_configs[profile_text]["blogid"].c_str());
		pdata = format_entry;
		gtk_combo_box_set_active(GTK_COMBO_BOX(format), 0);
		while(pdata->index != -1) {
			if (user_configs[profile_text]["format"] == pdata->value) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(format), pdata->index);
				break;
			}
			pdata++;
		}
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(engine), 0);
		gtk_entry_set_text(GTK_ENTRY(blogurl), "");
		gtk_entry_set_text(GTK_ENTRY(server), "");
		gtk_entry_set_text(GTK_ENTRY(userid), "");
		gtk_entry_set_text(GTK_ENTRY(passwd), "");
		gtk_entry_set_text(GTK_ENTRY(blogid), "");
		gtk_combo_box_set_active(GTK_COMBO_BOX(format), 0);
	}
}

void on_menu_new_entry(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
	GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
	GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");

	if (has_undo_info()) {
		if (!confirm_dialog(toplevel, _("You are editing, are you sure?")))
			return;
	}
	destroy_undo_info();

	gtk_entry_set_text(GTK_ENTRY(blog_title), "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(blog_category), 0);
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));
    gtk_text_buffer_set_text(buffer, "", 0);
	current_postid = "";

	clear_undo_info();
}

gpointer update_profile_thread(gpointer data)
{
	revoke_category_list_cache();
	revoke_blog_icon_cache();

	std::string errmsg;
	if (!load_profile(errmsg)) {
		return g_strdup(errmsg.c_str());
	}
	return NULL;
}

gpointer profile_detect_from_site_thread(gpointer data)
{
	GtkWidget* dialog = (GtkWidget*)data;
	GtkWidget* blogurl;
	GtkWidget* engine;
	GtkWidget* server;
	GtkWidget* userid;
	GtkWidget* passwd;
	GtkWidget* blogid;

	blogurl = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "url");
	engine = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "engine");
	server = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "server");
	userid = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "userid");
	passwd = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "passwd");
	blogid = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "blogid");

	gdk_threads_enter();

	char* server_text = (char*)gtk_entry_get_text(GTK_ENTRY(blogurl));
	if (!server_text || !strlen(server_text)) {
		gdk_threads_leave();
		return NULL;
	}
	std::string html = url2string(server_text, NULL);
#ifdef _DEBUG
	{
		FILE *fp = fopen("res.html", "wb");
		fprintf(fp, "%s", html.c_str());
		fclose(fp);
	}
#endif

	std::string blog_url, api_url;
	ProtocolType protocol = PROTOCOL_UNKNOWN;

	boost::regex re_service_post = boost::regex(".*<link[^>]* rel=\"?service\\.post\"?[^>]* type=\"?application/atom\"?[^>]* href=\"([^\"]*)\"[^>]*>.*");
	boost::regex re_editurl = boost::regex(".*<link[^>]* rel=\"?[Ee]dit[Uu][Rr][Ii]\"?[^>]* type=\"?application/rsd\\+xml\"?[^>]* href=\"([^\"]*)\"[^>]*>.*");
	boost::regex re_alternate = boost::regex(".*<link[^>]* rel=\"?alternate\"?[^>]* type=\"?application/atom\"?[^>]* href=\"([^\"]*)\"[^>]*>.*");

	if (boost::regex_match(html, re_service_post)) {
		protocol = PROTOCOL_ATOMPP;
		api_url = boost::regex_replace(html, re_service_post, "$1", boost::regex_constants::format_all);
	} else
	if (boost::regex_match(html, re_editurl)) {
		html = boost::regex_replace(html, re_editurl, "$1", boost::regex_constants::format_all);
		std::string strXml = url2string(html.c_str(), NULL);
#ifdef _DEBUG
		{
			FILE *fp = fopen("res.xml", "wb");
			fprintf(fp, "%s", strXml.c_str());
			fclose(fp);
		}
#endif
		boost::regex re_namespace = boost::regex("<rsd[^>]*>");
		strXml = boost::regex_replace(strXml, re_namespace, "<rsd>", boost::regex_constants::format_all);

		blog_url = XmlRpc::queryXml(strXml, "/rsd/service/homePageLink");
		api_url = XmlRpc::queryXml(strXml, "/rsd/service/apis/api[@name='MetaWeblog']/@apiLink");
		if (!api_url.size())
			api_url = XmlRpc::queryXml(strXml, "/rsd/service/apis/api[@name='Blogger']/@apiLink");
		if (api_url.size())
			protocol = PROTOCOL_XMLRPC;
		else {
			api_url = XmlRpc::queryXml(strXml, "/rsd/service/apis/api[@name='Atom']/@apiLink");
			if (api_url.size())
				protocol = PROTOCOL_ATOMPP;
		}
	} else
	if (boost::regex_match(html, re_alternate)) {
		html = boost::regex_replace(html, re_alternate, "$1", boost::regex_constants::format_all);
		AtomPP::AtomFeed feed;
		feed = AtomPP::getFeed(html, "", "");
		std::vector<AtomPP::AtomLink>::iterator it;
		for(it = feed.links.begin(); it != feed.links.end(); it++) {
			if (it->rel == "service.post") {
				api_url = it->href;
				protocol = PROTOCOL_ATOMPP;
				break;
			}
		}
	}

	if (blog_url.size())
		gtk_entry_set_text(GTK_ENTRY(blogurl), blog_url.c_str());
	if (api_url.size())
		gtk_entry_set_text(GTK_ENTRY(server), api_url.c_str());

	if (protocol == PROTOCOL_XMLRPC)
		gtk_combo_box_set_active(GTK_COMBO_BOX(engine), 0);
	else
	if (protocol == PROTOCOL_ATOMPP)
		gtk_combo_box_set_active(GTK_COMBO_BOX(engine), 1);

	gdk_threads_leave();

	return NULL;
}

void on_profile_detect_from_site(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = gtk_widget_get_toplevel(widget);

	gtk_widget_set_sensitive(widget, false);

	gpointer retval = process_func(profile_detect_from_site_thread, dialog, dialog, _("Detecting server information..."));
	if (retval) {
		error_dialog(dialog, (gchar*)retval);
		g_free(retval);
	}

	gtk_widget_set_sensitive(widget, true);
}

void on_menu_profile_settings(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog;
	GtkWidget* table;
	GtkWidget* label;
	GtkWidget* button;
	GtkWidget* ebox;
	GtkWidget* profile;
	GtkWidget* blogurl;
	GtkWidget* engine;
	GtkWidget* server;
	GtkWidget* userid;
	GtkWidget* passwd;
	GtkWidget* blogid;
	GtkWidget* format;
	GtkWidget* hbox;
	combo_entry_data *pdata;
	GtkListStore* profile_model;
	GtkTreeIter* iter;
	ConfigList::iterator it_list;

	GtkTooltips* tooltips = (GtkTooltips*)g_object_get_data(G_OBJECT(toplevel), "tooltips");

	dialog = gtk_dialog_new();
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			_("_Update"), GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Profile Settings"));

	table = gtk_table_new(10, 3, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 6);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table); 

	// profile
	label = gtk_label_new(_("_Profile:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   0, 1,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	profile = gtk_combo_box_new_text();
	ebox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(ebox), profile);
	profile_model = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_combo_box_set_model(GTK_COMBO_BOX(profile), GTK_TREE_MODEL(profile_model));
	iter = new GtkTreeIter;
	for(it_list = user_configs.begin(); it_list != user_configs.end(); it_list++) {
		if (it_list->first != "global") {
			gtk_list_store_append(profile_model, iter);
			gtk_list_store_set(profile_model, iter, 0, it_list->first.c_str(), -1);
		}
	}
	delete iter;
	g_signal_connect(G_OBJECT(profile), "changed", G_CALLBACK(on_profile_changed), dialog);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), profile);
	gtk_table_attach(
			GTK_TABLE(table),
			ebox,
			1, 2,                   0, 1,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	// blogurl
	label = gtk_label_new(_("_URL:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   1, 2,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	blogurl = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), blogurl);
	gtk_table_attach(
			GTK_TABLE(table),
			blogurl,
			1, 2,                   1, 2,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	button = gtk_button_new_with_label(_("_Detect"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			button,
			_("Detect blog url and entry point from this server."),
			_("Detect blog url and entry point from this server."));
	gtk_button_set_use_underline(GTK_BUTTON(button), TRUE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_profile_detect_from_site), dialog);
	gtk_table_attach(
			GTK_TABLE(table),
			button,
			2, 3,                   1, 2,
			GTK_FILL,               GTK_FILL,
			0,                      0);

	label = gtk_label_new(_("_Engine:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   2, 3,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	engine = gtk_combo_box_new_text();
	pdata = engine_entry;
	while(pdata->index != -1) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(engine), pdata->description);
		pdata++;
	}
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), engine);
	gtk_table_attach(
			GTK_TABLE(table),
			engine,
			1, 2,                   2, 3,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	// server
	label = gtk_label_new(_("_Server:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   3, 4,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	server = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), server);
	gtk_table_attach(
			GTK_TABLE(table),
			server,
			1, 2,                   3, 4,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	// userid
	label = gtk_label_new(_("User _Id:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   4, 5,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	userid = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), userid);
	gtk_table_attach(
			GTK_TABLE(table),
			userid,
			1, 2,                   4, 5,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	// passwd
	label = gtk_label_new(_("_Password:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   5, 6,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	passwd = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), passwd);
	gtk_entry_set_visibility(GTK_ENTRY(passwd), false);
	gtk_table_attach(
			GTK_TABLE(table),
			passwd,
			1, 2,                   5, 6,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	// blogid
	label = gtk_label_new(_("_Blog ID:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   6, 7,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	blogid = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), blogid);
	gtk_table_attach(
			GTK_TABLE(table),
			blogid,
			1, 2,                   6, 7,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	// format
	label = gtk_label_new(_("_Format:"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   7, 8,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	format = gtk_combo_box_new_text();
	pdata = format_entry;
	while(pdata->index != -1) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(format), pdata->description);
		pdata++;
	}
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), format);
	gtk_table_attach(
			GTK_TABLE(table),
			format,
			1, 2,                   7, 8,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_table_attach(
			GTK_TABLE(table),
			hbox,
			1, 3,                   8, 9,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	button = gtk_button_new_with_label(_("_New"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			button,
			_("If you want to create new profile, click 'New' button."),
			_("If you want to create new profile, click 'New' button."));
	gtk_button_set_use_underline(GTK_BUTTON(button), TRUE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_profile_new), dialog);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);

	button = gtk_button_new_with_label(_("_Copy"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			button,
			_("If you want to copy this profile, click 'Copy' button."),
			_("If you want to copy this profile, click 'Copy' button."));
	gtk_button_set_use_underline(GTK_BUTTON(button), TRUE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_profile_copy), dialog);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);

	button = gtk_button_new_with_label(_("_Delete"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			button,
			_("If you want to delete this profile, click 'Delete' button."),
			_("If you want to delete this profile, click 'Delete' button."));
	gtk_button_set_use_underline(GTK_BUTTON(button), TRUE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_profile_delete), dialog);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);

	label = gtk_label_new(_("NOTE) If you selected another profile, editing value won't save."));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			1, 2,                   9, 10,
			GTK_FILL,               GTK_FILL,
			0,                      0);

	gtk_widget_show_all(table);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(toplevel));

	g_object_set_data(G_OBJECT(dialog), "profile", profile);
	g_object_set_data(G_OBJECT(dialog), "url", blogurl);
	g_object_set_data(G_OBJECT(dialog), "engine", engine);
	g_object_set_data(G_OBJECT(dialog), "server", server);
	g_object_set_data(G_OBJECT(dialog), "userid", userid);
	g_object_set_data(G_OBJECT(dialog), "passwd", passwd);
	g_object_set_data(G_OBJECT(dialog), "blogid", blogid);
	g_object_set_data(G_OBJECT(dialog), "format", format);
	g_object_set_data(G_OBJECT(dialog), "profile_model", profile_model);

	iter = new GtkTreeIter;
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(profile_model), iter)) {
		while(true) {
			GValue value = { 0, };
			gtk_tree_model_get_value(GTK_TREE_MODEL(profile_model), iter, 0, &value);
			if (user_profile == g_value_get_string(&value)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(profile), iter);
				on_profile_changed(profile, dialog);
				break;
			}
			if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(profile_model), iter))
				break;
		}
	}
	delete iter;

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar* profile_text = (gchar*)gtk_combo_box_get_active_text(GTK_COMBO_BOX(profile));
		gchar* blogurl_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(blogurl));
		gchar* engine_text = engine_entry[gtk_combo_box_get_active(GTK_COMBO_BOX(engine))].value;
		gchar* server_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(server));
		gchar* userid_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(userid));
		gchar* passwd_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(passwd));
		gchar* blogid_text = (gchar*)gtk_entry_get_text(GTK_ENTRY(blogid));
		gchar* format_text = format_entry[gtk_combo_box_get_active(GTK_COMBO_BOX(format))].value;
		if (user_profile != profile_text) {
			user_profile = profile_text;
			on_menu_new_entry(NULL, NULL);
		}
		user_configs["global"]["profile"] = user_profile;
		user_configs[user_profile]["engine"] = engine_text ? engine_text : "";
		user_configs[user_profile]["url"] = blogurl_text ? blogurl_text : "";
		user_configs[user_profile]["server"] = server_text ? server_text : "";
		user_configs[user_profile]["userid"] = userid_text ? userid_text : "";
		user_configs[user_profile]["passwd"] = passwd_text ? passwd_text : "";
		user_configs[user_profile]["blogid"] = blogid_text ? blogid_text : "";
		user_configs[user_profile]["format"] = format_text ? format_text : "";
		process_func(update_profile_thread, NULL, toplevel, _("Updating profile..."));
		save_configs();
	}

	gtk_widget_destroy(dialog);
	//gtk_window_present(GTK_WINDOW(toplevel));
}

void on_menu_update_profile(GtkWidget* widget, gpointer user_data)
{
	gpointer retval = process_func(update_profile_thread, NULL, toplevel, _("Updating profile..."));
	if (retval) {
		error_dialog(toplevel, (gchar*)retval);
		g_free(retval);
	}
}

void on_configuration_reset(GtkWidget* widget, gpointer user_data)
{
	setting_table_def *setting_tables = (setting_table_def*)user_data;
	while(setting_tables->label) {
		switch(setting_tables->type) {
		case SETTING_FONT:
			gtk_font_button_set_font_name(GTK_FONT_BUTTON(setting_tables->widget), setting_tables->default_value);
			break;
		case SETTING_PATH:
			gtk_entry_set_text(GTK_ENTRY(setting_tables->widget), setting_tables->default_value);
			break;
		default:
			gtk_entry_set_text(GTK_ENTRY(setting_tables->widget), setting_tables->default_value);
			break;
		}
		setting_tables++;
	}
}

void on_menu_configuration(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog;
	GtkWidget* table;
	GtkWidget* label;
	GtkWidget* entry;
	setting_table_def setting_tables[] = {
#ifdef _WIN32
		{ _("FFmpeg Command:"), "ffmpeg_command", SETTING_PATH, "ffmpeg.exe" },
		{ _("WWW Browser Command:"), "browser_command", SETTING_PATH, "C:\\Program Files\\Mozilla Firefox\\Firefox.exe" },
		{ _("Movie Player Command:"), "movie_command", SETTING_PATH, "C:\\Program Files\\Windows Media Player\\wmplayer.exe" },
		{ _("Editor Font:"), "font", SETTING_FONT, "" },
		{ NULL, NULL, SETTING_UNKNOWN }
#else
		{ _("FFmpeg Command:"), "ffmpeg_command", SETTING_PATH, "/usr/bin/ffmpeg" },
		{ _("WWW Browser Command:"), "browser_command", SETTING_PATH, "/usr/bin/firefox" },
		{ _("Movie Player Command:"), "movie_command", SETTING_PATH, "/usr/bin/mplayer" },
		{ _("Editor Font:"), "font", SETTING_FONT, "" },
		{ NULL, NULL, SETTING_UNKNOWN }
#endif
	};

	GtkTooltips* tooltips = (GtkTooltips*)g_object_get_data(G_OBJECT(toplevel), "tooltips");

	dialog = gtk_dialog_new();
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			_("_Update"), GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Configuration"));

	int row_size = sizeof(setting_tables) / sizeof(setting_tables[0]);
	table = gtk_table_new(row_size, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 6);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table); 

	int n;
	for(n = 0; n < row_size; n++) {
		if (!setting_tables[n].label) break;
		label = gtk_label_new(setting_tables[n].label);
		gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 1.0f, 0.5f);
		gtk_table_attach(
				GTK_TABLE(table),
				label,
				0, 1,                   n, n+1,
				GTK_FILL,               GTK_FILL,
				0,                      0);

		std::string config_value = user_configs["global"][setting_tables[n].key];
		switch(setting_tables[n].type) {
		case SETTING_FONT:
			{
				entry = gtk_font_button_new();
				gtk_font_button_set_show_style(GTK_FONT_BUTTON(entry), false);
				if (config_value.size()) {
					gtk_font_button_set_font_name(GTK_FONT_BUTTON(entry), config_value.c_str());
				} else {
					GtkWidget* textview = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
					GtkStyle* style = gtk_widget_get_style(textview);
					const char* font = style ? pango_font_description_get_family(style->font_desc) : NULL;
					int size = pango_font_description_get_size(style->font_desc);
					if (font) {
						char* family = g_strdup_printf("%s %d", font, size / PANGO_SCALE);
						gtk_font_button_set_font_name(GTK_FONT_BUTTON(entry), family);
						g_free(family);
					}
				}
			}
			break;
		case SETTING_PATH:
			entry = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(entry), config_value.c_str());
			break;
		default:
			entry = gtk_entry_new();
			gtk_entry_set_text(GTK_ENTRY(entry), config_value.c_str());
			break;
		}
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_table_attach(
				GTK_TABLE(table),
				entry,
				1, 2,                                    n, n+1,
				(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
				0,                                       0);
		setting_tables[n].widget = entry;
	}
	GtkWidget* reset = gtk_button_new_with_label(_("_Reset"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			reset,
			_("Reset all configuration."),
			_("Reset all configuration."));
	gtk_button_set_use_underline(GTK_BUTTON(reset), TRUE);
	g_signal_connect(G_OBJECT(reset), "clicked", G_CALLBACK(on_configuration_reset), setting_tables);
	gtk_table_attach(
			GTK_TABLE(table),
			reset,
			1, 2,                   n, n+1,
			GTK_FILL,               GTK_FILL,
			0,                      0);

	gtk_widget_set_size_request(dialog, 400, -1);
	gtk_widget_queue_resize(dialog);

	gtk_widget_show_all(table);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(toplevel));

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		for(int n = 0; n < row_size; n++) {
			if (!setting_tables[n].label) break;
			std::string config_value;
			const char* value;
			switch(setting_tables[n].type) {
			case SETTING_FONT:
				value = gtk_font_button_get_font_name(GTK_FONT_BUTTON(setting_tables[n].widget));
				if (value) config_value = value;
				break;
			case SETTING_PATH:
				value = gtk_entry_get_text(GTK_ENTRY(setting_tables[n].widget));
				if (value) config_value = value;
				break;
			default:
				value = gtk_entry_get_text(GTK_ENTRY(setting_tables[n].widget));
				if (value) config_value = value;
				break;
			}
			user_configs["global"][setting_tables[n].key] = config_value;
		}
		save_configs();
		load_configs();
	}
	gtk_widget_destroy(dialog);
}

void on_menu_viewblog(GtkWidget* widget, gpointer user_data)
{
	gchar* url = (gchar*)user_configs[user_profile]["url"].c_str();
	std::string command = user_configs["global"]["browser_command"];

#ifdef _WIN32
	if (!command.size()) {
		ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOW);
		return;
	}
#endif
	if (command.size() == 0) command = "firefox";
	command += " \"";
	command += url;
	command += "\"";
	g_spawn_command_line_async(command.c_str(), NULL);
}

void on_menu_loadblog(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
	GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
	GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));
	GtkListStore* category_model = (GtkListStore*)g_object_get_data(G_OBJECT(toplevel), "category_model");

	std::string name = _("Entry File");
	std::vector<std::string> filters;
	filters.push_back("*.xml");
	filters.push_back("*.entry");
	std::vector<std::string> mimes;
	mimes.push_back("text/xml");
	mimes.push_back("*/*");

	std::string filename = get_selected_file(widget, true, name, filters, mimes);
	if (!filename.size()) return;

	gchar* buff = NULL;
	gsize buffer_size = 0;
	g_file_get_contents(filename.c_str(), &buff, &buffer_size, NULL);
	std::string strXml = buff;
	g_free(buff);

	std::string title_text;
	std::string category_text;
	std::string body_text;

	title_text = XmlRpc::queryXml(strXml, "/entry/name");
	if (title_text.size()) {
		category_text = XmlRpc::queryXml(strXml, "/entry/categories");
		body_text = XmlRpc::queryXml(strXml, "/entry/content/text()");
	} else {
		try {
			XmlRpc::XmlRpcValue data = XmlRpc::getXmlRpcValueFromXml(strXml);
			title_text = data["title"].valueString();
			category_text = data["category"].valueString();
			body_text = data["content"].valueString();
		} catch(...) {
			error_dialog(widget, "Invalida File Format");
			return;
		}
	}

	destroy_undo_info();

	gtk_entry_set_text(GTK_ENTRY(blog_title), title_text.c_str());
	GtkTreeIter* iter = new GtkTreeIter;
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(category_model), iter)) {
		while(true) {
			GValue value = { 0, };
			gtk_tree_model_get_value(GTK_TREE_MODEL(category_model), iter, 1, &value);
			if (category_text == g_value_get_string(&value)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(blog_category), iter);
				break;
			}
			if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(category_model), iter))
				break;
		}
	}
	delete iter;
	set_html(buffer, body_text);

	GtkTextIter* start = new GtkTextIter;
	gtk_text_buffer_get_start_iter(buffer, start);
	gtk_text_buffer_place_cursor(buffer, start);
	delete start;

	clear_undo_info();

	current_postid = "";
}

void on_menu_saveblog(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
	GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
	GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));

	std::string name = _("XML File");
	std::vector<std::string> filters;
	filters.push_back("*.xml");
	std::vector<std::string> mimes;
	mimes.push_back("text/xml");

	std::string filename = get_selected_file(toplevel, false, name, filters, mimes);
	if (!filename.size()) return;

	char fname[256];
	time_t tim;
	gchar* ptr;

	time(&tim);
	sprintf(fname, "%d", tim);

	ptr = (gchar*)gtk_entry_get_text(GTK_ENTRY(blog_title));
	std::string title_text = ptr ? ptr : "";

	ptr = (gchar*)gtk_combo_box_get_active_text(GTK_COMBO_BOX(blog_category));
	std::string category_text = ptr ? ptr : "";

	std::string body_text;
	std::string errmsg;
	body_text = get_html(buffer, GET_SOURCE_SAVE, fname, errmsg);
	if (errmsg.size()) {
		error_dialog(toplevel, errmsg.c_str());
	}

	XmlRpc::XmlRpcValue::ValueStruct data_struct;

	data_struct["title"] = title_text;

	CategoryList::iterator it;
	for(it = current_category_list.begin(); it != current_category_list.end(); it++) {
		if (it->second == category_text)
			data_struct["category"] = it->first;
	}

	data_struct["content"] = body_text;

	XmlRpc::XmlRpcValue data = data_struct;
	std::string strXml = XmlRpc::getXmlFromXmlRpcValue(data, "utf-8");

	FILE *fp = fopen(filename.c_str(), "w");
	if (fp) {
		fwrite(strXml.c_str(), strXml.size(), 1, fp);
		fclose(fp);
	} else {
		error_dialog(toplevel, l2u(strerror(errno)));
	}
}

gpointer recentposts_update_thread(gpointer data)
{
	GtkWidget* dialog = (GtkWidget*)data;
	GtkListStore* store = (GtkListStore*)g_object_get_data(G_OBJECT(dialog), "model");
	GtkWidget* button_delete = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "delete");
	GtkWidget* button_get = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "get");
	bool is_enabled = false;

	gdk_threads_enter();
	gtk_list_store_clear(store);

	gtk_widget_set_sensitive(button_delete, false);
	gtk_widget_set_sensitive(button_get, false);
	gdk_threads_leave();

	if (user_configs[user_profile]["engine"] == "xmlrpc") {
		XmlRpc::XmlRpcValue response;
		XmlRpc::XmlRpcValue::ValueArray valuearray;
		std::vector<XmlRpc::XmlRpcValue> requests;
		requests.clear();
		requests.push_back(user_configs[user_profile]["blogid"]);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		requests.push_back(10);

		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "metaWeblog.getRecentPosts", requests, response) != 0) {

			requests.clear();
			requests.push_back(user_configs[user_profile]["appkey"]);
			requests.push_back(user_configs[user_profile]["blogid"]);
			requests.push_back(user_configs[user_profile]["userid"]);
			requests.push_back(user_configs[user_profile]["passwd"]);
			requests.push_back(10);
			if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "blogger.getRecentPosts", requests, response) != 0) {
				std::string faultMessage = _("Can't get recent posts:");
				faultMessage += "\n";
				faultMessage += XmlRpc::getFaultResult(response);
				return g_strdup(faultMessage.c_str());
			}
		}

		gdk_threads_enter();

		GtkTreeIter* iter = new GtkTreeIter;
		int size = response.getType() == XmlRpc::XmlRpcValue::TypeStruct ? 1 : response.size();
		for(int n = 0; n < size; n++) {
			XmlRpc::XmlRpcValue::ValueStruct st;
			if (response.getType() == XmlRpc::XmlRpcValue::TypeStruct)
				st = response;
			else
				st = response[n];

			gtk_list_store_append (store, iter);
			std::string title;
			std::string description;
			title = st["title"].valueString();
			description = st["description"].valueString();

			title = sanitize_html(title, TRUE);
			title = l2u(XmlRpc::cut_string(u2l(title), 10));
			description = sanitize_html(description, TRUE);
			description = l2u(XmlRpc::cut_string(u2l(description), 30));

			gtk_list_store_set(store, iter,
					COLUMN_POSTID, st["postid"].valueString().c_str(),
					COLUMN_DATE, st["dateCreated"].valueString().c_str(),
					COLUMN_TITLE, title.c_str(),
					COLUMN_DESCRIPTION, description.c_str(),
					-1);
		}
		delete iter;

		gdk_threads_leave();
	} else
	if (user_configs[user_profile]["engine"] == "atompp") {
		AtomPP::AtomEntries entries;

		if (current_server == MAYBE_AMEBA_ATOMPP) {
			std::string passwd = AtomPP::StringToHex(AtomPP::md5(user_configs[user_profile]["passwd"]));
			entries = AtomPP::getEntries(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], passwd);
		} else
			entries = AtomPP::getEntries(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"]);

		gdk_threads_enter();

		GtkTreeIter* iter = new GtkTreeIter;
		for(int n = 0; n < entries.entries.size(); n++) {
			gtk_list_store_append(store, iter);
			std::string title;
			std::string description;

			title = entries.entries[n].title;
			description = entries.entries[n].content.value;
			if (description.size() == 0 && current_server == MAYBE_HATENA_BOOKMARK) {
				std::vector<AtomPP::AtomLink>::iterator it;
				for(it = entries.entries[n].links.begin(); it != entries.entries[n].links.end(); it++) {
					if (it->rel == "related")
						description = it->href;
				}
			}

			title = sanitize_html(title, TRUE);
			title = l2u(XmlRpc::cut_string(u2l(title), 10));
			description = sanitize_html(description, TRUE);
			description = l2u(XmlRpc::cut_string(u2l(description), 30));

			char buf[256];
			sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d",
				entries.entries[n].updated.tm_year+1900,
				entries.entries[n].updated.tm_mon,
				entries.entries[n].updated.tm_mday,
				entries.entries[n].updated.tm_hour,
				entries.entries[n].updated.tm_min,
				entries.entries[n].updated.tm_sec);
			std::string updated = buf;

			gtk_list_store_set(store, iter,
					COLUMN_POSTID, entries.entries[n].edit_uri.c_str(),
					COLUMN_DATE, updated.c_str(),
					COLUMN_TITLE, title.c_str(),
					COLUMN_DESCRIPTION, description.c_str(),
					-1);
		}
		delete iter;

		gdk_threads_leave();
	} else
	if (user_configs[user_profile]["engine"] == "ftpup") {

		Blosxom::FileList::iterator it;
		Blosxom::FileList files;
		files = Blosxom::FtpFiles(
			user_configs[user_profile]["server"],
			user_configs[user_profile]["userid"],
			user_configs[user_profile]["passwd"],
			user_configs[user_profile]["blogid"]);

		gdk_threads_enter();

		GtkTreeIter* iter = new GtkTreeIter;
		for(it = files.begin(); it != files.end(); it++) {
			Blosxom::FileInfo info = *it;
			gtk_list_store_append(store, iter);
			gtk_list_store_set(store, iter,
					COLUMN_POSTID, info["id"].c_str(),
					COLUMN_DATE, info["updated"].c_str(),
					COLUMN_TITLE, info["title"].c_str(),
					COLUMN_DESCRIPTION, info["description"].c_str(),
					-1);
		}
		delete iter;

		gdk_threads_leave();
	}
	return NULL;
}

void on_recentposts_update(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = gtk_widget_get_toplevel(widget);

	gtk_widget_set_sensitive(widget, false);

	gpointer retval = process_func(recentposts_update_thread, dialog, dialog, _("Updating recent posts..."));
	if (retval) {
		error_dialog(dialog, (gchar*)retval);
		g_free(retval);
	}

	gtk_widget_set_sensitive(widget, true);
}

gpointer recentposts_delete_thread(gpointer data)
{
	GtkWidget* dialog = (GtkWidget*)data;
	GtkTreeSelection* selection = (GtkTreeSelection*)g_object_get_data(G_OBJECT(dialog), "selection");
	GtkListStore* store = (GtkListStore*)g_object_get_data(G_OBJECT(dialog), "model");

	gdk_threads_enter();
	std::string postid;
	GList* selected = gtk_tree_selection_get_selected_rows(selection, NULL);
	GtkTreePath *path = (GtkTreePath*)selected->data;
	if (selected) {
		GtkTreeIter* iter = new GtkTreeIter;
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), iter, path)) {
			GValue value = { 0, };
			gtk_tree_model_get_value(GTK_TREE_MODEL(store), iter, COLUMN_POSTID, &value);
			postid = g_value_get_string(&value);
		}
		delete iter;
	}
	gdk_threads_leave();
	if (!postid.size()) return NULL;

	if (user_configs[user_profile]["engine"] == "xmlrpc") {
		XmlRpc::XmlRpcValue response;
		XmlRpc::XmlRpcValue::ValueArray valuearray;
		std::vector<XmlRpc::XmlRpcValue> requests;
		requests.clear();
		requests.push_back(user_configs[user_profile]["appkey"]);
		requests.push_back(postid);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		requests.push_back(TRUE);
		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "blogger.deletePost", requests, response) == 0) {
			gdk_threads_enter();
			GtkTreeIter* iter = new GtkTreeIter;
			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), iter, path)) {
				gtk_list_store_remove(store, iter);
			}
			delete iter;
			gdk_threads_leave();
		} else {
			std::string faultMessage = _("Can't delete post:");
			faultMessage += "\n";
			faultMessage += XmlRpc::getFaultResult(response);
			return g_strdup(faultMessage.c_str());
		}

		if (current_server == MAYBE_MOVABLETYPE) {
			requests.clear();
			requests.push_back(postid);
			requests.push_back(user_configs[user_profile]["userid"]);
			requests.push_back(user_configs[user_profile]["passwd"]);
			if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "mt.publishPost", requests, response) == 0) {
				;
			} else {
				/*
				std::string faultMessage = "Can't rebuild blog:";
				faultMessage += "\n";
				faultMessage += XmlRpc::getFaultResult(response);
				return g_strdup(faultMessage.c_str());
				*/
			}
		}
	} else
	if (user_configs[user_profile]["engine"] == "atompp") {
		int ret = 0;
		if (current_server == MAYBE_BLOGGER_ATOMPP)
			ret = AtomPP::deleteEntry_Blogger(postid, user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"]);
		else
		if (current_server == MAYBE_AMEBA_ATOMPP) {
			std::string passwd = AtomPP::StringToHex(AtomPP::md5(user_configs[user_profile]["passwd"]));
			ret = AtomPP::deleteEntry(postid, user_configs[user_profile]["userid"], passwd);
		} else
			ret = AtomPP::deleteEntry(postid, user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"]);
		if (ret == 0) {
			gdk_threads_enter();
			GtkTreeIter* iter = new GtkTreeIter;
			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), iter, path)) {
				gtk_list_store_remove(store, iter);
			}
			delete iter;
			gdk_threads_leave();
		} else {
			return g_strdup(_("Can't delete post:"));
		}
	} else
	if (user_configs[user_profile]["engine"] == "ftpup") {
		std::string file;

		file = user_configs[user_profile]["blogid"];
		if (postid[0] == '/')
			file += postid.substr(1);
		else
			file += postid;
		if (Blosxom::FtpDelete(
			user_configs[user_profile]["server"],
			user_configs[user_profile]["userid"],
			user_configs[user_profile]["passwd"],
			file) == 0)
		{
			gdk_threads_enter();
			GtkTreeIter* iter = new GtkTreeIter;
			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), iter, path)) {
				gtk_list_store_remove(store, iter);
			}
			delete iter;
			gdk_threads_leave();
		} else {
			return g_strdup(_("Can't delete post:"));
		}
	}
	return NULL;
}

void on_recentposts_delete(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = gtk_widget_get_toplevel(widget);
	if (!confirm_dialog(dialog, _("Are you sure to delete this post?")))
		return;

	gpointer retval = process_func(recentposts_delete_thread, dialog, dialog, _("Deleting recent post..."));
	if (retval) {
		error_dialog(dialog, (gchar*)retval);
		g_free(retval);
	}
}

void on_recentposts_selected(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = (GtkWidget*)user_data;
	GtkTreeSelection* selection = (GtkTreeSelection*)g_object_get_data(G_OBJECT(dialog), "selection");
	GtkListStore* store = (GtkListStore*)g_object_get_data(G_OBJECT(dialog), "model");
	GtkWidget* button_delete = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "delete");
	GtkWidget* button_get = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "get");

	gtk_widget_set_sensitive(button_get, false);
	gtk_widget_set_sensitive(button_delete, false);

	GList* selected = gtk_tree_selection_get_selected_rows(selection, NULL);
	if (!selected) return;

	GtkTreePath *path = (GtkTreePath*)selected->data;
	GtkTreeIter* iter = new GtkTreeIter;
	std::string postid;
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), iter, path)) {
		GValue value = { 0, };
		gtk_tree_model_get_value(GTK_TREE_MODEL(store), iter, COLUMN_POSTID, &value);
		postid = g_value_get_string(&value);
	}
	if (postid.size()) {
		gtk_widget_set_sensitive(button_get, true);
		gtk_widget_set_sensitive(button_delete, true);
	}
	delete iter;
}

gpointer recentposts_get_thread(gpointer data)
{
	GtkWidget* dialog = (GtkWidget*)data;
	GtkTreeSelection* selection = (GtkTreeSelection*)g_object_get_data(G_OBJECT(dialog), "selection");
	GtkListStore* store = (GtkListStore*)g_object_get_data(G_OBJECT(dialog), "model");

	gdk_threads_enter();
	std::string postid;
	GList* selected = gtk_tree_selection_get_selected_rows(selection, NULL);
	if (selected) {
		GtkTreePath *path = (GtkTreePath*)selected->data;
		GtkTreeIter* iter = new GtkTreeIter;
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), iter, path)) {
			GValue value = { 0, };
			gtk_tree_model_get_value(GTK_TREE_MODEL(store), iter, COLUMN_POSTID, &value);
			postid = g_value_get_string(&value);
		}
		delete iter;
	}
	gdk_threads_leave();
	if (!postid.size()) return NULL;

	if (user_configs[user_profile]["engine"] == "xmlrpc") {
		XmlRpc::XmlRpcValue response;
		XmlRpc::XmlRpcValue::ValueArray valuearray;
		std::vector<XmlRpc::XmlRpcValue> requests;
		requests.clear();
		requests.push_back(postid);
		requests.push_back(user_configs[user_profile]["userid"]);
		requests.push_back(user_configs[user_profile]["passwd"]);
		if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "metaWeblog.getPost", requests, response) == 0) {
			current_postid = postid;

			gdk_threads_enter();
			GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
			GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
			GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
			GtkListStore* category_model = (GtkListStore*)g_object_get_data(G_OBJECT(toplevel), "category_model");
			std::string title_str, body_str, category_str;
			title_str = response["title"].valueString();
			if (response["categories"].getType() == XmlRpc::XmlRpcValue::TypeArray)
				category_str = response["categories"][0].valueString();
			else
				category_str = response["categories"].valueString();
			body_str = response["description"].valueString();

			destroy_undo_info();

			gtk_entry_set_text(GTK_ENTRY(blog_title), sanitize_html(title_str).c_str());
			GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));
			if (user_configs[user_profile]["format"] == "html")
				set_html(buffer, body_str.c_str());
			else
			if (user_configs[user_profile]["format"] == "wiki")
				set_wiki(buffer, body_str.c_str());
			GtkTreeIter* iter = new GtkTreeIter;
			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(category_model), iter)) {
				while(true) {
					GValue value = { 0, };
					gtk_tree_model_get_value(GTK_TREE_MODEL(category_model), iter, 0, &value);
					if (category_str == g_value_get_string(&value)) {
						gtk_combo_box_set_active_iter(GTK_COMBO_BOX(blog_category), iter);
						break;
					}
					if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(category_model), iter))
						break;
				}
			}
			delete iter;

			GtkTextIter* start = new GtkTextIter;
			gtk_text_buffer_get_start_iter(buffer, start);
			gtk_text_buffer_place_cursor(buffer, start);
			delete start;

			clear_undo_info();

			gdk_threads_leave();
		} else {
			requests.clear();
			requests.push_back(user_configs[user_profile]["blogid"]);
			requests.push_back(user_configs[user_profile]["userid"]);
			requests.push_back(user_configs[user_profile]["passwd"]);
			requests.push_back(10);
			if (XmlRpc::execXmlRpc(user_configs[user_profile]["server"], "metaWeblog.getRecentPosts", requests, response) == 0) {
				current_postid = postid;

				gdk_threads_enter();

				GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
				GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
				GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
				GtkListStore* category_model = (GtkListStore*)g_object_get_data(G_OBJECT(toplevel), "category_model");
				std::string title_str, body_str, category_str;

				XmlRpc::XmlRpcValue::ValueStruct st;
				if (response.getType() == XmlRpc::XmlRpcValue::TypeArray) {
					for(int n = 0; n < response.size(); n++) {
						if (response[n]["postid"] == postid) {
							st = response[n];
							break;
						}
					}
				} else
					st = response;

				title_str = st["title"].valueString();
				if (st["categories"].getType() == XmlRpc::XmlRpcValue::TypeArray)
					category_str = st["categories"][0].valueString();
				else
					category_str = st["categories"].valueString();
				body_str = st["description"].valueString();

				destroy_undo_info();

				gtk_entry_set_text(GTK_ENTRY(blog_title), sanitize_html(title_str).c_str());
				GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));
				if (user_configs[user_profile]["format"] == "html")
					set_html(buffer, body_str.c_str());
				else
				if (user_configs[user_profile]["format"] == "wiki")
					set_wiki(buffer, body_str.c_str());
				GtkTreeIter* iter = new GtkTreeIter;
				if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(category_model), iter)) {
					while(true) {
						GValue value = { 0, };
						gtk_tree_model_get_value(GTK_TREE_MODEL(category_model), iter, 0, &value);
						if (category_str == g_value_get_string(&value)) {
							gtk_combo_box_set_active_iter(GTK_COMBO_BOX(blog_category), iter);
							break;
						}
						if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(category_model), iter))
							break;
					}
				}
				delete iter;

				GtkTextIter* start = new GtkTextIter;
				gtk_text_buffer_get_start_iter(buffer, start);
				gtk_text_buffer_place_cursor(buffer, start);
				delete start;

				clear_undo_info();

				gdk_threads_leave();
			} else {
				std::string faultMessage = _("Can't get recent post:");
				faultMessage += "\n";
				faultMessage += XmlRpc::getFaultResult(response);
				return g_strdup(faultMessage.c_str());
			}
		}
	} else
	if (user_configs[user_profile]["engine"] == "atompp") {
		AtomPP::AtomEntries entries;
		if (current_server == MAYBE_AMEBA_ATOMPP) {
			std::string passwd = AtomPP::StringToHex(AtomPP::md5(user_configs[user_profile]["passwd"]));
			entries = AtomPP::getEntries(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], passwd);
		} else
			entries = AtomPP::getEntries(user_configs[user_profile]["server"], user_configs[user_profile]["userid"], user_configs[user_profile]["passwd"]);
		std::vector<AtomPP::AtomEntry>::iterator it;
		for(it = entries.entries.begin(); it != entries.entries.end(); it++) {
			if (it->edit_uri == postid)
				break;
		}
		if (it != entries.entries.end()) {
			current_postid = postid;

			gdk_threads_enter();

			GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
			GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
			GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
			GtkListStore* category_model = (GtkListStore*)g_object_get_data(G_OBJECT(toplevel), "category_model");
			std::string title_str, body_str, category_str;
			title_str = it->title;
			category_str = it->summary;
			body_str = it->content.value;
			if (body_str.size() == 0 && current_server == MAYBE_HATENA_BOOKMARK) {
				std::vector<AtomPP::AtomLink>::iterator itlink;
				for(itlink = it->links.begin(); itlink != it->links.end(); itlink++) {
					if (itlink->rel == "related")
						body_str = itlink->href;
				}
			}

			gtk_entry_set_text(GTK_ENTRY(blog_title), sanitize_html(title_str).c_str());
			GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));

			destroy_undo_info();

			if (user_configs[user_profile]["format"] == "html")
				set_html(buffer, body_str.c_str());
			else
			if (user_configs[user_profile]["format"] == "wiki")
				set_wiki(buffer, body_str.c_str());
			GtkTreeIter* iter = new GtkTreeIter;
			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(category_model), iter)) {
				while(true) {
					GValue value = { 0, };
					gtk_tree_model_get_value(GTK_TREE_MODEL(category_model), iter, 0, &value);
					if (category_str == g_value_get_string(&value)) {
						gtk_combo_box_set_active_iter(GTK_COMBO_BOX(blog_category), iter);
						break;
					}
					if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(category_model), iter))
						break;
				}
			}
			delete iter;

			GtkTextIter* start = new GtkTextIter;
			gtk_text_buffer_get_start_iter(buffer, start);
			gtk_text_buffer_place_cursor(buffer, start);
			delete start;

			clear_undo_info();

			gdk_threads_leave();
		} else {
			std::string faultMessage = _("Can't get recent post:");
			faultMessage += "\n";
			faultMessage += entries.error;
			return g_strdup(faultMessage.c_str());
		}
	} else
	if (user_configs[user_profile]["engine"] == "ftpup") {
		std::string file;
		std::string text;

		file = user_configs[user_profile]["blogid"];
		if (postid[0] == '/')
			file += postid.substr(1);
		else
			file += postid;
		if (Blosxom::FtpDownload(
			user_configs[user_profile]["server"],
			user_configs[user_profile]["userid"],
			user_configs[user_profile]["passwd"],
			file,
			text) != 0)
		{
			return g_strdup(_("Can't get recent post:"));
		}

		current_postid = postid;

		gdk_threads_enter();

		GtkWidget* blog_title = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "title");
		GtkWidget* blog_category = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "category");
		GtkWidget* blog_body = (GtkWidget*)g_object_get_data(G_OBJECT(toplevel), "body");
		GtkListStore* category_model = (GtkListStore*)g_object_get_data(G_OBJECT(toplevel), "category_model");
		std::string title_str, body_str, category_str;
		text = XmlRpc::replace_string(text, "\r", "");
		std::vector<std::string> lines = XmlRpc::split_string(text, "\n");
		if (lines.size())
			title_str = lines[0];
		size_t pos = file.find_last_of("/");
		if (pos != std::string::npos)
			category_str = file.substr(0, pos+1);
		std::vector<std::string>::iterator it;
		if (lines.size() >= 2) {
			for(it = lines.begin()+1; it != lines.end(); it++) {
				body_str += *it;
				body_str += "\n";
			}
			printf("%s\n", u2l(body_str).c_str());
		}
		gtk_entry_set_text(GTK_ENTRY(blog_title), sanitize_html(title_str).c_str());
		GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(blog_body));

		destroy_undo_info();

		if (user_configs[user_profile]["format"] == "html")
			set_html(buffer, body_str.c_str());
		else
		if (user_configs[user_profile]["format"] == "wiki")
			set_wiki(buffer, body_str.c_str());
		GtkTreeIter* iter = new GtkTreeIter;
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(category_model), iter)) {
			while(true) {
				GValue value = { 0, };
				gtk_tree_model_get_value(GTK_TREE_MODEL(category_model), iter, 0, &value);
				if (category_str == g_value_get_string(&value)) {
					gtk_combo_box_set_active_iter(GTK_COMBO_BOX(blog_category), iter);
					break;
				}
				if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(category_model), iter))
					break;
			}
		}
		delete iter;

		GtkTextIter* start = new GtkTextIter;
		gtk_text_buffer_get_start_iter(buffer, start);
		gtk_text_buffer_place_cursor(buffer, start);
		delete start;

		clear_undo_info();

		gdk_threads_leave();
	}

	return NULL;
}

void on_recentposts_get(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = gtk_widget_get_toplevel(widget);
	gpointer retval = process_func(recentposts_get_thread, dialog, dialog, _("Getting recent post..."));
	if (retval) {
		error_dialog(dialog, (gchar*)retval);
		g_free(retval);
	}
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
}

void on_recentposts_row_activated(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog = gtk_widget_get_toplevel(widget);
	GtkTreeSelection* selection = (GtkTreeSelection*)g_object_get_data(G_OBJECT(dialog), "selection");
	GtkWidget* button_get = (GtkWidget*)g_object_get_data(G_OBJECT(dialog), "get");
	GtkListStore* store = (GtkListStore*)g_object_get_data(G_OBJECT(dialog), "model");

	std::string postid;
	GList* selected = gtk_tree_selection_get_selected_rows(selection, NULL);
	if (selected) {
		GtkTreePath *path = (GtkTreePath*)selected->data;
		GtkTreeIter* iter = new GtkTreeIter;
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), iter, path)) {
			GValue value = { 0, };
			gtk_tree_model_get_value(GTK_TREE_MODEL(store), iter, COLUMN_POSTID, &value);
			postid = g_value_get_string(&value);
		}
		delete iter;
	}
	if (!postid.size()) return;

	on_recentposts_get(button_get, dialog);
}

void on_menu_recentposts(GtkWidget* widget, gpointer user_data)
{
	GtkWidget* dialog;
	GtkWidget* swin;
	GtkWidget* treeview;
	GtkWidget* hbuttonbox;
	GtkWidget* button_get;
	GtkWidget* button_delete;
	GtkWidget* button_update;
	GtkListStore* store;
	GtkCellRenderer* renderer;
	GtkTreeViewColumn* column;
	GtkTreeSelection* selection;

	GtkTooltips* tooltips = (GtkTooltips*)g_object_get_data(G_OBJECT(toplevel), "tooltips");

	dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Recent Posts"));
	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			NULL);

	store = gtk_list_store_new(
			NUM_COLUMNS,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING);
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_signal_connect(G_OBJECT(treeview), "row-activated", G_CALLBACK(on_recentposts_row_activated), dialog);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(on_recentposts_selected), dialog);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("PostId",
			renderer,
			"text",
			COLUMN_POSTID,
			NULL);
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_POSTID);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Date"),
			renderer,
			"text",
			COLUMN_DATE,
			NULL);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_DATE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Title"),
			renderer,
			"text",
			COLUMN_TITLE,
			NULL);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_TITLE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Description"),
			renderer,
			"text",
			COLUMN_DESCRIPTION,
			NULL);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_DESCRIPTION);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(swin),
			GTK_POLICY_AUTOMATIC, 
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(swin), treeview); 
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), swin, TRUE, TRUE, 0);
	gtk_widget_show_all(swin);
	
	hbuttonbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
	
	button_update = gtk_button_new_with_label(_("_Update"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			button_update,
			_("Update this list."),
			_("Update this list."));
	gtk_button_set_use_underline(GTK_BUTTON(button_update), TRUE);
	g_signal_connect(G_OBJECT(button_update), "clicked", G_CALLBACK(on_recentposts_update), dialog);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), button_update);

	button_delete = gtk_button_new_with_label(_("_Delete"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			button_delete,
			_("Delete selected post."),
			_("Delete selected post."));
	gtk_button_set_use_underline(GTK_BUTTON(button_delete), TRUE);
	g_signal_connect(G_OBJECT(button_delete), "clicked", G_CALLBACK(on_recentposts_delete), dialog);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), button_delete);

	button_get = gtk_button_new_with_label(_("_Get"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			button_get,
			_("Edit selected post."),
			_("Edit selected post."));
	gtk_button_set_use_underline(GTK_BUTTON(button_get), TRUE);
	g_signal_connect(G_OBJECT(button_get), "clicked", G_CALLBACK(on_recentposts_get), dialog);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), button_get);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbuttonbox, FALSE, FALSE, 0);
	
	gtk_widget_show_all(hbuttonbox);
	
	gtk_widget_set_size_request(dialog, 300, 300);
	gtk_widget_queue_resize(dialog);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for(
			GTK_WINDOW(dialog),
			GTK_WINDOW(toplevel));
	g_object_set_data(G_OBJECT(dialog), "model", store);
	g_object_set_data(G_OBJECT(dialog), "update", button_update);
	g_object_set_data(G_OBJECT(dialog), "delete", button_delete);
	g_object_set_data(G_OBJECT(dialog), "get", button_get);
	g_object_set_data(G_OBJECT(dialog), "selection", selection);

	gtk_widget_show(dialog);

	on_recentposts_update(button_update, dialog);

	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
	gtk_window_present(GTK_WINDOW(toplevel));
}

gboolean on_window_key_press(GtkWidget *widget, GdkEventKey* event, gpointer user_data)
{
	if (event->keyval == GDK_q && event->state && GDK_CONTROL_MASK)
		gtk_window_iconify(GTK_WINDOW(widget));
	return false;
}

gpointer upload_file_to_server_thread(gpointer data)
{
	std::string errmsg;
	std::string filename = (char*)data;
	size_t pos = filename.find_last_of("\\/");
	if (pos != std::string::npos) {
		//filename = filename.substr(pos+1);
		filename = "aaa/bbb.jpg";
	}
	if (upload_file("", filename, (char*)data, errmsg).size() == 0) {
		return g_strdup(errmsg.c_str());
	}
	return NULL;
}

void on_menu_upload_file_to_server(GtkWidget* widget, gpointer user_data)
{
	std::string name = _("Any File");
	std::vector<std::string> filters;
	filters.push_back("*.*");
	std::vector<std::string> mimes;
	mimes.push_back("*/*");

	std::string filename = get_selected_file(widget, true, name, filters, mimes);
	if (filename.size()) {
		gpointer retval = process_func(upload_file_to_server_thread, (gpointer)filename.c_str(), toplevel, _("Uploading file to server..."));
		if (retval) {
			error_dialog(toplevel, (gchar*)retval);
			g_free(retval);
		}
	}
}

std::string get_flickr_token()
{
	std::string apikey = BLOGWRITER_FLICKR_APIKEY;
	std::string secret = BLOGWRITER_FLICKR_SECRET;
	std::string token = user_configs["global"]["flickr_token"];
	if (!token.size()) {
		std::string frob = Flickr::getFlickrFrob(apikey, secret);
		if (!frob.size())
			return "";
		std::string url = Flickr::getFlickrAuthUrl(apikey, secret, frob);
		std::string command = user_configs["global"]["browser_command"];
#ifdef _WIN32
		ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOW);
#else
		if (command.size() == 0) command = "firefox";
		command += " \"";
		command += url;
		command += "\"";
		g_spawn_command_line_async(command.c_str(), NULL);
#endif
		if (!confirm_dialog(toplevel, _("If \"Success\", click \"OK\"."), "flickr.gif"))
			return "";
		token = Flickr::getFlickrToken(apikey, secret, frob);
		if (!token.size())
			return "";
		user_configs["global"]["flickr_token"] = token;
		save_configs();
	}
	return token;
}

void on_menu_auth_for_flickr(GtkWidget* widget, gpointer user_data)
{
	user_configs["global"]["flickr_token"] = "";
	if (!get_flickr_token().size()) {
		error_dialog(toplevel, _("failed to authenticate for flickr."), "flickr.gif");
	}
}

gpointer upload_image_to_flickr_thread(gpointer data)
{
	std::string apikey = BLOGWRITER_FLICKR_APIKEY;
	std::string secret = BLOGWRITER_FLICKR_SECRET;
	std::string token = get_flickr_token();
	if (Flickr::FlickrUpload(apikey, secret, token, (char*)data) != 0) {
		return g_strdup(_("Failed to upload image to flickr."));
	}
	return NULL;
}

void on_menu_upload_image_to_flickr(GtkWidget* widget, gpointer user_data)
{
	std::string token = get_flickr_token();
	if (!token.size()) {
		error_dialog(toplevel, _("failed to authenticate for flickr."), "flickr.gif");
		return;
	}

	std::string name = _("Image File");
	std::vector<std::string> filters;
	filters.push_back("*.jpg");
	filters.push_back("*.png");
	filters.push_back("*.gif");
	std::vector<std::string> mimes;
	mimes.push_back("image/*");

	std::string filename = get_selected_file(widget, true, name, filters, mimes);
	if (filename.size()) {
		gpointer retval = process_func(upload_image_to_flickr_thread, (gpointer)filename.c_str(), toplevel, _("Uploading image to flickr..."));
		if (retval) {
			error_dialog(toplevel, (gchar*)retval);
			g_free(retval);
		}
	}
}

int main(int argc, char* argv[])
{
	GtkWidget* window;
	GtkWidget* menubar;
	GtkWidget* menuitem;
	GtkWidget* submenuitem;
	GtkWidget* menu;
	GtkWidget* vbox;
	GtkWidget* hbox;
	GtkWidget* ebox;
	GtkWidget* table;
	GtkWidget* label;
	GtkWidget* title;
	GtkWidget* category;
	GtkWidget* swin;
	GtkWidget* body;
	GtkWidget* publish;
	GtkWidget* alignment;
	GtkWidget* icon_view;
	GtkListStore* category_model;
	GtkListStore* icon_model;
	GtkTooltips* tooltips;
	GtkTargetList* targetlist;
	GtkTargetEntry drag_types[] = {
		{ "text/uri-list",	0, TARGET_URI_LIST },
		{ "UTF8_STRING",	0, TARGET_UTF8_STRING },
		{ "TEXT",			0, TARGET_TEXT },
		{ "COMPOUND_TEXT",	0, TARGET_COMPOUND_TEXT },
		{ "STRING",			0, TARGET_STRING },
	};
	gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);
	GtkTextBuffer* buffer;
	GtkAccelGroup* accelgroup;
	gchar* emoji_file;
	gulong buffer_insert_text_signal_id;
	gulong buffer_delete_range_signal_id;
	gulong buffer_insert_pixbuf_signal_id;

#ifdef _MSC_VER
	CoInitialize(NULL);
#endif

	setlocale(LC_CTYPE, "");

#ifdef LOCALE_SISO639LANGNAME
	if (getenv("LANG") == NULL) {
		char szLang[256] = {0};
		if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, szLang, sizeof(szLang))) {
			char szEnv[256] = {0};
			sprintf(szEnv, "LANG=%s", szLang);
			putenv(szEnv);
		}
	}
#endif

	bindtextdomain(APP_TITLE, "share/locale");
	bind_textdomain_codeset(APP_TITLE, "utf-8");
	textdomain(APP_TITLE);

	g_thread_init(NULL);
	gdk_threads_init();
	gdk_threads_enter();

	gtk_init(&argc, &argv);

	//-------------------------------------------------
	// window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	toplevel = window;
	gtk_window_set_title(GTK_WINDOW(window), _(APP_TITLE));
	g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, window);
	accelgroup = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(window), accelgroup);

	//-------------------------------------------------
	// tooltip
	tooltips = gtk_tooltips_new();

	//-------------------------------------------------
	// body
	body = gtk_text_view_new();

	g_signal_connect(G_OBJECT(body), "drag-data-received", G_CALLBACK(on_drag_data_received), body);
	targetlist = gtk_drag_dest_get_target_list(body);
	gtk_target_list_add_table(targetlist, drag_types, n_drag_types);
	gtk_drag_dest_add_image_targets(body);
	/*
	gtk_drag_dest_set(
		body,
		(GtkDestDefaults)(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP),
		drag_types,
		G_N_ELEMENTS(drag_types),
		GDK_ACTION_COPY);
	*/
	g_signal_connect(G_OBJECT(body), "populate-popup", G_CALLBACK(on_populate_popup), body);

	//-------------------------------------------------
	// buffer
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(body));

	buffer_insert_text_signal_id = g_signal_connect(G_OBJECT(buffer), "insert-text", G_CALLBACK(on_buffer_insert_text), body);
	buffer_delete_range_signal_id = g_signal_connect(G_OBJECT(buffer), "delete-range", G_CALLBACK(on_buffer_delete_range), body);
	buffer_insert_pixbuf_signal_id = g_signal_connect(G_OBJECT(buffer), "insert-pixbuf", G_CALLBACK(on_buffer_insert_pixbuf), body);

	g_object_set_data(G_OBJECT(buffer), "buffer-insert-text-signal-id", (void*)buffer_insert_text_signal_id);
	g_object_set_data(G_OBJECT(buffer), "buffer-delete-range-signal-id", (void*)buffer_delete_range_signal_id);
	g_object_set_data(G_OBJECT(buffer), "buffer-insert-pixbuf-signal-id", (void*)buffer_insert_pixbuf_signal_id);
	undo_data = g_list_alloc();

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	//-------------------------------------------------
	// menu
	menubar = gtk_menu_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	menuitem = gtk_menu_item_new_with_label(_("_File"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(menuitem)->child), TRUE);
	menu = gtk_menu_new();

	submenuitem = gtk_menu_item_new_with_label(_("_New Entry"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_new_entry), NULL);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'n', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Recent Posts"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_recentposts), window);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'h', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Profile Settings"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_profile_settings), NULL);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Update Profile"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_update_profile), NULL);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_View Blog"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_viewblog), NULL);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'w', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Load Blog"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_loadblog), NULL);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'o', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Save Blog"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_saveblog), NULL);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 's', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Configuration"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_configuration), NULL);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("E_xit"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", gtk_main_quit, window);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	menuitem = gtk_menu_item_new_with_label(_("_Edit"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(menuitem)->child), TRUE);
	menu = gtk_menu_new();

	submenuitem = gtk_menu_item_new_with_label(_("_Undo"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_undo), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'z', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Redo"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_redo), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'r', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Bold"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_bold), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'b', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Italic"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_italic), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'i', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Stri_ke"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_strike), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'k', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Underline"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_underline), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'u', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Link"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_link), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'l', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Ima_ge"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_image), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'g', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Header _1"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_header1), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, '1', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Header _2"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_header2), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, '2', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Header _3"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_header3), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, '3', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Pre"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_pre), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'p', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Block_quote"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_blockquote), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'q', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Align Left"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_align_left), body);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Align Center"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_align_center), body);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Align Right"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_align_right), body);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("_Embed Object"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_embedobj), body);
	gtk_widget_add_accelerator(submenuitem, "activate", accelgroup, 'e', GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	menuitem = gtk_menu_item_new_with_label(_("_Tool"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(menuitem)->child), TRUE);
	menu = gtk_menu_new();

	submenuitem = gtk_menu_item_new_with_label(_("_Upload file to server"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_upload_file_to_server), body);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Upload image to _flickr"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_upload_image_to_flickr), body);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	submenuitem = gtk_menu_item_new_with_label(_("Authenticate for flickr"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_auth_for_flickr), body);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	menuitem = gtk_menu_item_new_with_label(_("_Help"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(menuitem)->child), TRUE);
	gtk_menu_item_set_right_justified(GTK_MENU_ITEM(menuitem), TRUE);
	menu = gtk_menu_new();

	submenuitem = gtk_menu_item_new_with_label(_("_About..."));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_help), NULL);
	gtk_menu_append(GTK_MENU(menu), submenuitem);

#ifdef _DEBUG
	submenuitem = gtk_menu_item_new_with_label(_("_Debug"));
	gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(submenuitem)->child), TRUE);
	g_signal_connect(G_OBJECT(submenuitem), "activate", G_CALLBACK(on_menu_debug), NULL);
	gtk_menu_append(GTK_MENU(menu), submenuitem);
#endif

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);

	//-------------------------------------------------
	// table layout
	table = gtk_table_new(2, 6, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 6);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 6);
	gtk_table_set_row_spacing(GTK_TABLE(table), 1, 6);
	gtk_box_pack_end(GTK_BOX(vbox), table, TRUE, TRUE, 0);

	label = gtk_label_new(_("_Title:"));
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   0, 1,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	title = gtk_entry_new();
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			title,
			_("Title of this article"),
			_("Title of this article"));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), title);
	gtk_table_attach(
			GTK_TABLE(table),
			title,
			1, 2,                   0, 1,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	label = gtk_label_new(_("_Category:"));
	gtk_table_attach(
			GTK_TABLE(table),
			label,
			0, 1,                   1, 2,
			GTK_FILL,               GTK_FILL,
			0,                      0);
	category = gtk_combo_box_new_text();
	ebox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(ebox), category);
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			ebox,
			_("Category of this article"),
			_("Category of this article"));
	category_model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model(GTK_COMBO_BOX(category), GTK_TREE_MODEL(category_model));
	gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), category);
	gtk_table_attach(
			GTK_TABLE(table),
			ebox,
			1, 2,                   1, 2,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), GTK_FILL,
			0,                      0);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(swin),
			GTK_POLICY_AUTOMATIC, 
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(swin), body);
	gtk_table_attach(
			GTK_TABLE(table),
			swin,
			0, 2,                   2, 3,
			(GtkAttachOptions)(GTK_EXPAND|GTK_FILL), (GtkAttachOptions)(GTK_EXPAND|GTK_FILL),
			0,                      0);

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(body), GTK_WRAP_WORD);

	alignment = gtk_alignment_new(1, 0.5, 0, 0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 6, 6, 6, 6);
	hbox = gtk_hbox_new(FALSE, 6);

	swin = gtk_scrolled_window_new(NULL, NULL);
	icon_view = gtk_icon_view_new();
	icon_model = gtk_list_store_new(1, GDK_TYPE_PIXBUF);
	g_object_set_data(G_OBJECT(icon_view), "store", icon_model);
	g_object_set_data(G_OBJECT(icon_view), "path", icon_model);
	g_signal_connect(G_OBJECT(icon_view), "selection-changed", G_CALLBACK(on_emoji_item_selection_changed), body);
	gtk_icon_view_set_model(GTK_ICON_VIEW(icon_view), GTK_TREE_MODEL(icon_model));
	gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(icon_view), 0);

	gchar* path = g_path_get_dirname(argv[0]);
	module_path = (char*)path;
	g_free(path);
#ifdef _DEBUG
	if (module_path.size() && !stricmp(module_path.c_str()+module_path.size()-6, "\\Debug"))
		module_path = module_path.substr(0, module_path.size()-6);
#endif
#ifdef _WIN32
	module_path = XmlRpc::replace_string(module_path, "\\", "/");
#endif

	emoji_file = g_build_filename(module_path.c_str(), "emoji", "emoji.lst", NULL);
	FILE* fp = fopen(emoji_file, "rb");
	g_free(emoji_file);
	if (fp) {
		GdkPixbuf* pixbuf;
		GdkPixbuf* pixbuf_on;
		GtkWidget* image;
		GError* _error = NULL;
		gchar buff[256];
		while(fgets(buff, sizeof(buff), fp)) {
			gchar** emoji_info = g_strsplit(buff, ",", 3);
			gchar* emoji_name = *emoji_info++;
			gchar* emoji_tips = *emoji_info++;
			gchar* emoji_defs = *emoji_info++;
			if (!emoji_name || !emoji_tips || !emoji_defs) continue;

			emoji_file = g_strdup_printf("%s/emoji/%s.gif", module_path.c_str(), emoji_name);
			ebox = gtk_event_box_new();
			gtk_widget_add_events(ebox, GDK_BUTTON_RELEASE_MASK);
			pixbuf = gdk_pixbuf_new_from_file(emoji_file, &_error);
			image = gtk_image_new_from_pixbuf(pixbuf);
			gtk_container_add(GTK_CONTAINER(ebox), image);
			g_object_set_data(G_OBJECT(ebox), "pixbuf", pixbuf);
			gtk_tooltips_set_tip(
					GTK_TOOLTIPS(tooltips),
					ebox,
					emoji_tips,
					emoji_tips);
			g_signal_connect(G_OBJECT(ebox), "button-press-event", G_CALLBACK(on_emoji_clicked), body);
			gchar* emoji_tag = g_strdup_printf("&x-%s;", emoji_name);
			g_object_set_data(G_OBJECT(buffer), emoji_tag, pixbuf);
			g_object_set_data(G_OBJECT(pixbuf), "emoji", emoji_tag);
			if (atol(emoji_defs) == 0) {
				GtkTreeIter* iter = new GtkTreeIter;
				gtk_list_store_append(icon_model, iter);
				gtk_list_store_set(icon_model, iter, 0, pixbuf, -1);
				delete iter;
			}
			else
				gtk_container_add(GTK_CONTAINER(hbox), ebox);
			g_free(emoji_file);
		}
		fclose(fp);

		emoji_file = g_strdup_printf("%s/emoji/emoji.gif", module_path.c_str());
		pixbuf = gdk_pixbuf_new_from_file(emoji_file, &_error);
		g_free(emoji_file);

		emoji_file = g_strdup_printf("%s/emoji/emoji_on.gif", module_path.c_str());
		pixbuf_on = gdk_pixbuf_new_from_file(emoji_file, &_error);
		g_free(emoji_file);

		ebox = gtk_event_box_new();
		gtk_widget_add_events(ebox, GDK_BUTTON_RELEASE_MASK);
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_container_add(GTK_CONTAINER(ebox), image);
		g_object_set_data(G_OBJECT(ebox), "pixbuf", pixbuf);
		g_object_set_data(G_OBJECT(ebox), "pixbuf_on", pixbuf_on);
		g_object_set_data(G_OBJECT(ebox), "icon_view", icon_view);
		g_object_set_data(G_OBJECT(ebox), "panel", swin);
		g_object_set_data(G_OBJECT(ebox), "image", image);
		gtk_tooltips_set_tip(
				GTK_TOOLTIPS(tooltips),
				ebox,
				_("etc"),
				_("etc"));
		g_signal_connect(G_OBJECT(ebox), "button-release-event", G_CALLBACK(on_emoji_clicked), NULL);
		gtk_container_add(GTK_CONTAINER(hbox), ebox);
	}

	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(swin),
			GTK_POLICY_AUTOMATIC, 
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(swin), icon_view);
	gtk_table_attach(
			GTK_TABLE(table),
			swin,
			0, 2,                   3, 4,
			GTK_FILL,               GTK_FILL,
			0,                      0);

	gtk_container_add(GTK_CONTAINER(alignment), hbox);
	gtk_table_attach(
			GTK_TABLE(table),
			alignment,
			0, 2,                   4, 5,
			GTK_FILL,               GTK_FILL,
			0,                      0);

	alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
	publish = gtk_button_new_with_label(_("_Publish"));
	gtk_tooltips_set_tip(
			GTK_TOOLTIPS(tooltips),
			publish,
			_("Publish this article"),
			_("Publish this article"));
	gtk_button_set_use_underline(GTK_BUTTON(publish), TRUE);
	g_signal_connect(G_OBJECT(publish), "clicked", G_CALLBACK(on_publish_clicked), body);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 6, 6, 6, 6);
	gtk_container_add(GTK_CONTAINER(alignment), publish);
	gtk_table_attach(
			GTK_TABLE(table),
			alignment,
			0, 2,                   5, 6,
			GTK_FILL,               GTK_FILL,
			0,                      0);

	bold_tag = gtk_text_buffer_create_tag(
			buffer,
			"b",
			"weight",
			PANGO_WEIGHT_BOLD,
			NULL);
	italic_tag = gtk_text_buffer_create_tag(
			buffer,
			"i",
			"style",
			PANGO_STYLE_ITALIC,
			NULL);
	underline_tag = gtk_text_buffer_create_tag(
			buffer,
			"u",
			"underline",
			PANGO_UNDERLINE_SINGLE,
			NULL);
	strike_tag = gtk_text_buffer_create_tag(
			buffer,
			"strike",
			"strikethrough",
			TRUE,
			NULL);
	h3_tag = gtk_text_buffer_create_tag(
			buffer,
			"h3",
			"scale",
			PANGO_SCALE_LARGE,
			"underline",
			PANGO_UNDERLINE_SINGLE,
			"weight",
			PANGO_WEIGHT_BOLD,
			NULL);
	h2_tag = gtk_text_buffer_create_tag(
			buffer,
			"h2",
			"scale",
			PANGO_SCALE_X_LARGE,
			"underline",
			PANGO_UNDERLINE_SINGLE,
			"weight",
			PANGO_WEIGHT_BOLD,
			NULL);
	h1_tag = gtk_text_buffer_create_tag(
			buffer,
			"h1",
			"scale",
			PANGO_SCALE_XX_LARGE,
			"underline",
			PANGO_UNDERLINE_SINGLE,
			"weight",
			PANGO_WEIGHT_BOLD,
			NULL);
	pre_tag = gtk_text_buffer_create_tag(
			buffer,
			"pre",
#if GTK_CHECK_VERSION(2,8,0)
			"paragraph-background",
			"#E0E0FF",
#else
			"background",
			"#E0E0FF",
#endif
			"left-margin",
			10,
			"right-margin",
			10,
			NULL);
	blockquote_tag = gtk_text_buffer_create_tag(
			buffer,
			"blockquote",
#if GTK_CHECK_VERSION(2,8,0)
			"paragraph-background",
			"#F0F0F0",
#else
			"background",
			"#F0F0F0",
#endif
			"left-margin",
			10,
			"right-margin",
			10,
			NULL);
	align_right_tag = gtk_text_buffer_create_tag(
			buffer,
			"align_right",
			"justification",
			GTK_JUSTIFY_RIGHT,
			NULL);
	align_center_tag = gtk_text_buffer_create_tag(
			buffer,
			"align_center",
			"justification",
			GTK_JUSTIFY_CENTER,
			NULL);

	g_object_set_data(G_OBJECT(window), "title", title);
	g_object_set_data(G_OBJECT(window), "category", category);
	g_object_set_data(G_OBJECT(window), "body", body);
	g_object_set_data(G_OBJECT(window), "category_model", category_model);
	g_object_set_data(G_OBJECT(window), "tooltips", tooltips);
	g_object_set_data(G_OBJECT(window), "emoji", hbox);

	g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_window_key_press), window);

	load_configs();

	gtk_widget_set_size_request(window, 500, 400);
	gtk_widget_queue_resize(window);
	gtk_widget_show_all(vbox);
	gtk_widget_hide(swin);
	gtk_widget_show(window);

	gdk_threads_leave();

	std::string errmsg;
	load_profile(errmsg);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

#ifdef _MSC_VER
	CoUninitialize();
#endif
	
	return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hCurInst, HINSTANCE hPrevInst, LPSTR lpsCmdLine, int nCmdShow)
{
	return main(__argc, __argv);
}
#endif
