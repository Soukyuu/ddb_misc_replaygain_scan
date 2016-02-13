PLUG_OUT?=ddb_misc_replaygain_scan.so
GTK2_OUT?=ddb_misc_replaygain_scan_GTK2.so
GTK3_OUT?=ddb_misc_replaygain_scan_GTK3.so

PLUG_LIBS?=-lm

GTK2_CFLAGS?=`pkg-config --cflags gtk+-2.0`
GTK3_CFLAGS?=`pkg-config --cflags gtk+-3.0`

GTK2_LIBS?=`pkg-config --libs gtk+-2.0`
GTK3_LIBS?=`pkg-config --libs gtk+-3.0`

CC?=gcc

CFLAGS += -std=c99 -D_GNU_SOURCE -fPIC -O2 -Wall

all: plugin gtk2 gtk3

plugin: plugin.o
	@echo "Linking the plugin"
	@$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $(PLUG_OUT) ddb_misc_rg_scan.o ebur128.o $(PLUG_LIBS)
	@echo "Done!"
plugin.o:
	@echo "Compiling the plugin"
	@$(CC) $(CFLAGS) -c -Iebur128 ddb_misc_rg_scan.c ebur128/ebur128.c $(PLUG_LIBS)
	@echo "Done!"

gtk2: misc-gtk2 ui-gtk2.o
	@echo "Linking the GTK2 UI plugin"
	@$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $(GTK2_OUT) ddb_rg_scan_gui.o interface.o callbacks.o support.o $(GTK2_LIBS)
	@echo "Done!"

gtk3: misc-gtk3 ui-gtk3.o
	@echo "Linking the GTK3 UI plugin"
	@$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $(GTK3_OUT) ddb_rg_scan_gui.o interface.o callbacks.o support.o $(GTK3_LIBS)
	@echo "Done!"
	
ui-gtk2.o:
	@echo "Compiling the GTK2 UI plugin"
	@$(CC) $(CFLAGS) $(GTK2_CFLAGS) -c ddb_rg_scan_gui.c
	@echo "Done!"
	
ui-gtk3.o:
	@echo "Compiling the GTK3 UI plugin"
	@$(CC) $(CFLAGS) $(GTK3_CFLAGS) -c ddb_rg_scan_gui.c
	@echo "Done!"

misc-gtk2: interface.o-gtk2 callbacks.o-gtk2 support.o-gtk2

interface.o-gtk2:
	@echo "Compiling interface.o"
	@$(CC) $(CFLAGS) $(GTK2_CFLAGS) -c interface.c
	@echo "Done!"

callbacks.o-gtk2:
	@echo "Compiling callbacks.o"
	@$(CC) $(CFLAGS) $(GTK2_CFLAGS) -c callbacks.c
	@echo "Done!"

support.o-gtk2:
	@echo "Compiling support.o"
	@$(CC) $(CFLAGS) $(GTK2_CFLAGS) -c support.c
	@echo "Done!"
	
misc-gtk3: interface.o-gtk3 callbacks.o-gtk3 support.o-gtk3

interface.o-gtk3:
	@echo "Compiling interface.o"
	@$(CC) $(CFLAGS) $(GTK3_CFLAGS) -c interface.c
	@echo "Done!"

callbacks.o-gtk3:
	@echo "Compiling callbacks.o"
	@$(CC) $(CFLAGS) $(GTK3_CFLAGS) -c callbacks.c
	@echo "Done!"

support.o-gtk3:
	@echo "Compiling support.o"
	@$(CC) $(CFLAGS) $(GTK3_CFLAGS) -c support.c
	@echo "Done!"

clean:
	@rm -f *.o $(PLUG_OUT) $(GTK2_OUT) $(GTK3_OUT)
