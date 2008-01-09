all : BlogWriter

BlogWriter : BlogWriter.o XmlRpcUtils.o AtomPP.o Blosxom.o Flickr.o
	gcc -g -o BlogWriter \
		BlogWriter.o \
		XmlRpcUtils.o \
		AtomPP.o \
		Blosxom.o \
		Flickr.o \
		`pkg-config --libs gtk+-2.0 libxml-2.0 gthread-2.0` \
		`curl-config --libs` \
		-lboost_regex \
		-lstdc++

BlogWriter.o : BlogWriter.cpp
	gcc -g -c \
		BlogWriter.cpp \
		`pkg-config --cflags gtk+-2.0`

XmlRpcUtils.o : XmlRpcUtils.cpp
	gcc -g -c \
		`curl-config --cflags` \
		`pkg-config --cflags libxml-2.0` \
		-o XmlRpcUtils.o \
		XmlRpcUtils.cpp

AtomPP.o : AtomPP.cpp
	gcc -g -c \
		`curl-config --cflags` \
		`pkg-config --cflags libxml-2.0` \
		-o AtomPP.o \
		AtomPP.cpp

Blosxom.o : Blosxom.cpp
	gcc -g -c \
		`curl-config --cflags` \
		-o Blosxom.o \
		Blosxom.cpp

Flickr.o : Flickr.cpp
	gcc -g -c \
		`curl-config --cflags` \
		-o Flickr.o \
		Flickr.cpp

message:
	-mv -f messages.pot messages.pot.orig
	xgettext -o messages.pot --keyword=_ BlogWriter.cpp

ja:
	-mv -f ja.po ja.po.orig
	msgmerge -o ja.po ja.po.orig messages.pot

clean:
	rm *.o BlogWriter
