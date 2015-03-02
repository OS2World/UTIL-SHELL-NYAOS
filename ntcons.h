#ifndef NTCONSOLE_H
#define NTCONSOLE_H

class NnString;
class Console {
public:
    static void backspace(int n=1);
    static void clear();
    static unsigned int getShiftKey();
    enum { SHIFT = 0x8000 };
#ifdef NYACUS
    static void restore_default_console_mode();
    static void enable_ctrl_c();
    static void disable_ctrl_c();
    static void getLocate(int &x,int &y);
    static void color(int c);
    static int color();
    static int foremask(int n);
    static int backmask(int n);
    static int getWidth();
#endif
    /* enum{ SHIFT = 0x3 }; */
    static void locate(int x,int y);
    static int getkey();
    static void writeClipBoard( const char *s , int len );
    static void readClipBoard( NnString &buffer );
    static void readTextVram( int x , int y , char *buffer , int size );
    static void setConsoleTitle( const char *s );
    static void getConsoleTitle( char *s , int size );
};

#endif
