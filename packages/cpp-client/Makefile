.PHONY: clean

LIB_DIRS = \
  lib/websocketpp \
  lib/nlohmann-json \
  lib/apply-patch \
  lib/asio/asio/include \
	include

LDIR = $(patsubst %,-I%,$(LIB_DIRS))

LDFLAGS = 
LDLIBS = -lz -lcurl

ODIR=obj
SRCDIR=src

LIBNAME = libbased

uname_S := $(shell uname -s)

ifeq ($(uname_S),Linux)
	TARGET = $(LIBNAME).so
	DISTDIR=linux
endif
ifeq ($(uname_S),Darwin) # macOS
	TARGET = $(LIBNAME).dylib
	DISTDIR=darwin
endif
ifndef TARGET
$(error TARGET is not set)
endif

HEADERS = $(wildcard $(SRCDIR)/*.hpp)
OBJS = basedclient.o \
	connection.o \
	utility.o \
	based.o

CXXFLAGS += -Wall -std=c++14 -D_WEBSOCKETPP_CPP11_STL_ -DASIO_STANDALONE -DBASED_VERBOSE

all: tls

.PHONY: tls
tls: LIB_DIRS += /usr/local/opt/openssl@3/include
tls: CXXFLAGS += -DBASED_TLS
tls: LDLIBS += -lssl -lcrypto
tls: LDFLAGS += -L/usr/local/opt/openssl@3/lib
tls: $(TARGET)

.PHONY: no-tls
no-tls: $(TARGET)

$(ODIR)/%.o: $(SRCDIR)/%.cpp
	+@[ -d $(ODIR) ] || mkdir -p $(ODIR)
	$(CXX) -MMD -fPIC $(LDIR) $(CXXFLAGS) -c -o $@ $<

example: $(patsubst %,$(ODIR)/%,$(OBJS))
	$(CXX) example/example.cpp -o $@ $^ $(LDIR) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

$(TARGET): $(patsubst %,$(ODIR)/%,$(OBJS))
  #+@[ -d $(DISTDIR) ] || mkdir -p $(DISTDIR)
	$(CXX) -o $@ -shared -fPIC -g $^ $(LDFLAGS) $(LDLIBS) -fvisibility=hidden

linked-example:
	$(CXX) example/example.cpp -o $@ $(CXXFLAGS) -Iinclude -L$(DISTDIR) -lbased

clean:
	$(RM) $(ODIR)
	$(RM) $(DISTDIR)
	$(RM) docker-build
	$(RM) linked-example


# TODO: Add target to make lib in root folder, otherwise it gets added to the libname
# check using otool -L libbased.dylib