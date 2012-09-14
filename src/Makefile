# ULCC make file

CC= gcc
LIB_NAME=		libulcc.so

# Compile and link options for libulcc.so
LIB_DEFS=		-D _ULCC_LIB
#LIB_DEFS=		-D _ULCC_LIB -D _ULCC_DBG
LIB_INCS=		-I.
LIB_CFLAGS=		-O -fPIC -Wall $(LIB_DEFS)
#LIB_CFLAGS=		-g -fPIC -Wall $(LIB_DEFS)
LIB_LDFLAGS=	-shared
LIB_OBJS=		remapper.o translator.o

all: $(LIB_NAME)

$(LIB_NAME): $(LIB_OBJS)
	$(CC) -o $(LIB_NAME) $(LIB_LDFLAGS) $(LIB_OBJS)

.c.o:
	$(CC) -c $< -o $@ $(LIB_CFLAGS) $(LIB_INCS)

clean:
	rm -f $(LIB_OBJS) $(LIB_NAME)
