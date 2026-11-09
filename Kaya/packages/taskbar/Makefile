CC = g++
CFLAGS = `pkg-config --cflags gtk+-3.0 gdk-pixbuf-2.0`
LIBS = `pkg-config --libs gtk+-3.0 gdk-pixbuf-2.0`

all: taskbar

taskbar: main.cpp
	$(CC) -o $@ main.cpp $(CFLAGS) $(LIBS) -std=c++11

clean:
	rm -f taskbar
