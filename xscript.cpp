#include "ntcons.h"
#include "writer.h"

#include "getline.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef NYACUS
/* ���݂� NYACUS �ł̂� xscript �T�|�[�g
 * ������́A���̃v���b�g�t�H�[���ł��T�|�[�g������.
 * */
Status GetLine::xscript(int)
{
    return NEXTCHAR;
}
#else

#include <windows.h>

class NnScreen {
    COORD size_ , cursor_ ;
    int top_,bottom_;
public:
    void load();

    const COORD &cursor() const { return cursor_; }
    int cursor_x() const { return cursor_.X; }
    int cursor_y() const { return cursor_.Y; }
    /* ���ݕ\������Ă���̈�̍s�ʒu */
    int top()    const { return top_; }
    int bottom() const { return bottom_; }

    int width()    const { return size_.X; }
    int height()   const { return size_.Y; }
};

KeyFunctionXScript::XScriptFuncCode 
    KeyFunctionXScript::map[ KeyFunction::MAPSIZE ];

int KeyFunctionXScript::bind(int n)
{
    if( unsigned(n) < MAPSIZE ){
        map[ n ] = this->function_no;
        return 0;
    }else{
        return 1;
    }
}
void KeyFunctionXScript::init()
{
    /* bind �R�}���h�ŃJ�X�^�}�C�Y�ł���悤�ɁA
     * �@�\���ƁA�@�\�R�[�h�̊֘A�t�����s��
     */
    (new KeyFunctionXScript("xscript:none"         ,XK_NOBOUND ))->regist();
    (new KeyFunctionXScript("xscript:previous"     ,XK_UP      ))->regist();
    (new KeyFunctionXScript("xscript:next"         ,XK_DOWN    ))->regist();
    (new KeyFunctionXScript("xscript:backward"     ,XK_LEFT    ))->regist();
    (new KeyFunctionXScript("xscript:forward"      ,XK_RIGHT   ))->regist();
    (new KeyFunctionXScript("xscript:head"         ,XK_HOME    ))->regist();
    (new KeyFunctionXScript("xscript:tail"         ,XK_END     ))->regist();
    (new KeyFunctionXScript("xscript:heaven"       ,XK_CTRLHOME))->regist();
    (new KeyFunctionXScript("xscript:earth"        ,XK_CTRLEND ))->regist();
    (new KeyFunctionXScript("xscript:previous-page",XK_PGUP    ))->regist();
    (new KeyFunctionXScript("xscript:next-page"    ,XK_PGDN    ))->regist();
    (new KeyFunctionXScript("xscript:copy"         ,XK_COPY    ))->regist();
    (new KeyFunctionXScript("xscript:leave"        ,XK_LEAVE   ))->regist();

    /* �L�[�}�b�v��S�āA�@�\�Ȃ��ɏ��������� */
    for(size_t i=0;i<numof(map);++i)
        map[i] = XK_NOBOUND;

    /* �@�\�R�[�h�����ۂɃL�[�ɕR�t���� */
    if( KeyFunction::bind("CTRL_HOME","xscript:heaven") ||
        KeyFunction::bind("HOME"     ,"xscript:head") ||
        KeyFunction::bind("CTRL_A"   ,"xscript:head") ||
        KeyFunction::bind("END"      ,"xscript:tail") ||
        KeyFunction::bind("CTRL_E"   ,"xscript:tail") ||
        KeyFunction::bind("CTRL_END" ,"xscript:earth") ||
        KeyFunction::bind("UP"       ,"xscript:previous") ||
        KeyFunction::bind("CTRL_UP"  ,"xscript:previous") ||
        KeyFunction::bind("CTRL_P"   ,"xscript:previous") ||
        KeyFunction::bind("DOWN"     ,"xscript:next") ||
        KeyFunction::bind("CTRL_DOWN","xscript:next") ||
        KeyFunction::bind("CTRL_N"   ,"xscript:next") ||
        KeyFunction::bind("RIGHT"     ,"xscript:forward") ||
        KeyFunction::bind("CTRL_RIGHT","xscript:forward") ||
        KeyFunction::bind("CTRL_F"   ,"xscript:forward") ||
        KeyFunction::bind("LEFT"     ,"xscript:backward") ||
        KeyFunction::bind("CTRL_LEFT","xscript:backward") ||
        KeyFunction::bind("CTRL_B"   ,"xscript:backward") ||
        KeyFunction::bind("PAGEUP"   ,"xscript:previous-page") ||
        KeyFunction::bind("CTRL_PAGEUP","xscript:previous-page") ||
        KeyFunction::bind("CTRL_Z"   ,"xscript:previous-page") ||
        KeyFunction::bind("CTRL_V"   ,"xscript:next-page") ||
        KeyFunction::bind("PAGEDOWN" ,"xscript:next-page") ||
        KeyFunction::bind("CTRL_PAGEDOWN","xscript:next-page") ||
        KeyFunction::bind("ENTER"    ,"xscript:copy") ||
        KeyFunction::bind("CTRL_G"   ,"xscript:leave") ||
        KeyFunction::bind("ESCAPE"   ,"xscript:leave")  )
    {
        /* ���̃G���[�̓L�[���̂��Ԉ���Ă��鎞�ɏo�͂���� */
        conErr << "internal runtime error: bad default binding.\n";
    }
}

void NnScreen::load()
{
    HANDLE  hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO  csbi;

    GetConsoleScreenBufferInfo( hConsoleOutput, &csbi);
    size_.X   = csbi.dwSize.X;
    size_.Y   = csbi.dwSize.Y;
    top_      = csbi.srWindow.Top;
    bottom_   = csbi.srWindow.Bottom;
    cursor_.X = csbi.dwCursorPosition.X; 
    cursor_.Y = csbi.dwCursorPosition.Y;
}

class XScript {
    HANDLE    hConsoleOutput ;
    NnScreen  screen ;
    COORD     cursor , start , end ;
private:
    int  create_commandline( NnString &commandline );
public:
    XScript();
    KeyFunctionXScript::XScriptFuncCode  inputFuncCode();
    void loop();
    void invartRect(int x0,int y0,int x1,int y1);
    void invartRect();
    void invartPoint( const COORD& cursor);
    void expandTo( const COORD& cursor);
    void copyToClipboard();
};

#define CTRL(x) ((x) & 0x1F)

XScript::XScript()
{
    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    screen.load();
    cursor = screen.cursor();
}

void XScript::invartPoint( const COORD &cursor)
{
    WORD attr;
    DWORD size;

    ::ReadConsoleOutputAttribute( hConsoleOutput, &attr, 1, cursor, &size);
    attr = ((attr&0xF)<<4)|(attr>>4);
    ::WriteConsoleOutputAttribute( hConsoleOutput, &attr, 1, cursor, &size);
}

/* �ꕶ�����L�[�{�[�h������͂��A������@�\�R�[�h�֕ϊ����� */
KeyFunctionXScript::XScriptFuncCode XScript::inputFuncCode()
{
    int ch = Console::getkey();
    if( (unsigned)ch >= KeyFunction::MAPSIZE )
        return  KeyFunctionXScript::XK_NOBOUND ;

    /* �L�[ �� �@�\�R�[�h�ϊ� */
    return KeyFunctionXScript::map[ ch ];
}

void XScript::loop()
{
    char title[1024];
    bool isSelecting = false;

    GetConsoleTitle( title , sizeof(title) );
    SetConsoleTitle( "XScript" );
    for(;;){
        KeyFunctionXScript::XScriptFuncCode  code = inputFuncCode();

        switch( code ){
        case KeyFunctionXScript::XK_COPY:
            if( isSelecting )
                copyToClipboard();
            goto exit;
        case KeyFunctionXScript::XK_LEAVE:
            goto exit;
        default:
            break;
        }

	if( !isSelecting){
	    start = end = cursor;
	}

	//�J�[�\���̈ړ�
	switch( code ){
        case KeyFunctionXScript::XK_CTRLHOME:
	    cursor.X = cursor.Y = 0;
	    break;
        case KeyFunctionXScript::XK_HOME:
	    cursor.X = 0;
	    break;
        case KeyFunctionXScript::XK_END:
	    cursor.X = screen.width()-1;
	    break;
        case KeyFunctionXScript::XK_CTRLEND:
	    cursor.X = screen.width()-1;
	    cursor.Y = screen.cursor_y();
	    break;
        case KeyFunctionXScript::XK_UP:
            cursor.Y--;
	    break;
        case KeyFunctionXScript::XK_PGUP:
	    cursor.Y -= screen.bottom() - screen.top();
	    break;
        case KeyFunctionXScript::XK_PGDN:
	    cursor.Y += screen.bottom() - screen.top();
	    break;
        case KeyFunctionXScript::XK_LEFT:
	    cursor.X--;
	    break;
        case KeyFunctionXScript::XK_DOWN:
	    cursor.Y++;
	    break;
        case KeyFunctionXScript::XK_RIGHT:
	    cursor.X++;
	    break;
	default:
            continue;
	}
	if( cursor.Y > screen.cursor_y() ){
	    cursor.Y = screen.cursor_y();
	}else if( cursor.Y < 0){
	    cursor.Y = 0;
	}
        Console::locate( cursor.X , cursor.Y );

	// �I������.
        if( Console::getShiftKey() & Console::SHIFT ){
	    if( !isSelecting){ /* ���I�����I����� */
		SetConsoleTitle( "XScript[Selecting�c]");
		isSelecting = true;
		end = cursor;
		invartRect();
	    }else{ /* �I�������܂܁A�̈���ړ� */
		expandTo( cursor );
	    }
	}else{ //��I��
	    if( isSelecting){
		SetConsoleTitle( "XScript");
		isSelecting = false;
		invartRect();
	    }
	}
    }
exit:
    if( isSelecting )
        invartRect();
    Console::locate( screen.cursor_x() , screen.cursor_y() );
    SetConsoleTitle( title );
}

/* ��`�̈�w��ׂ̈ɍ���̃|�C���g�ƉE���̃|�C���g�������悤��
 * ���W���\�[�g����.
 */
static void sortCoord( 
        int &x0, int &y0 ,
        int &x1, int &y1 ,
        const COORD &start ,
        const COORD &end )
{
    if( start.X<end.X){
	x0 = start.X; x1 = end.X;
    }else{
	x0 = end.X; x1 = start.X;
    }
    if( start.Y<end.Y){
	y0 = start.Y; y1 = end.Y;
    }else{
	y0 = end.Y; y1 = start.Y;
    }
}

/* �w�肵����`�̈�𔽓]������ */
void XScript::invartRect(int x0,int y0,int x1,int y1)
{
    COORD cursor;

    for( cursor.X=x0; cursor.X< x1; ++cursor.X )
	for( cursor.Y=y0; cursor.Y< y1; ++cursor.Y )
	    invartPoint( cursor );
}

/* ��`�̈�𔽓]������ */
void XScript::invartRect()
{
    int x0, x1, y0, y1;

    sortCoord(x0,y0,x1,y1,start,end);
    invartRect(x0,y0,x1+1,y1+1);
}

/* �I��̈���g�� or �k�� */
void XScript::expandTo( const COORD &cursor )
{
    /* �I��̈悪 2�s or 2���ȏ㍷�����鎞�́A�f���ɕ\�����Ȃ��� */
    if( end.X - cursor.X > 1 || cursor.X - end.X > 1 ||
	end.Y - cursor.Y > 1 || cursor.Y - end.Y > 1 )
    {
	invartRect();
	end = cursor;
	invartRect();
	return;
    }
    /* x �����ňړ� */
    if( cursor.X != end.X ){
	int x0=0, x1=0, y0=0, y1=0;
	if( cursor.X < end.X ){
	    if( cursor.X < start.X){
		x0 = cursor.X;x1 = end.X;
	    }else{
		x0 = cursor.X+1; x1 = end.X+1;
	    }
	}else{
	    if( cursor.X <= start.X){
		x0 = end.X;  x1 = cursor.X;
	    }else if( cursor.X ){
		x0 = end.X+1; x1 = cursor.X+1;
	    }
	}
	if( start.Y <= end.Y){
	    y0 = start.Y;	y1 = end.Y;
	}else{
	    y0 = end.Y;		y1 = start.Y;
	}
        invartRect(x0,y0,x1,y1+1);
	end.X = cursor.X;
    }
    /* y �����ňړ����� */
    if( cursor.Y!=end.Y){
	int x0=0, x1=0, y0=0, y1=0;
	if( cursor.Y<end.Y){
	    if( cursor.Y<start.Y){
                /* c < s,e */
		y0 = cursor.Y;	y1 = end.Y;
	    }else{
                /* s < c < e */
		y0 = cursor.Y+1;y1 = end.Y+1;
	    }
	}else{
	    if( cursor.Y<=start.Y ){
		y0 = end.Y;	y1 = cursor.Y;
	    }else if( cursor.Y ){
		y0 = end.Y+1;	y1 = cursor.Y+1;
	    }
	}
	if( start.X <= end.X){
	    x0 = start.X;	x1 = end.X+1;
	}else{
	    x0 = end.X;		x1 = start.X+1;
	}
        invartRect(x0,y0,x1,y1);
	end.Y = cursor.Y;
    }
}

void XScript::copyToClipboard()
{
    int x0, x1, y0, y1;
    NnString buffer;
    COORD cursor={ 0 , 0 };

    sortCoord(x0,y0,x1,y1,start,end);

    /* �]���p�̕�����𐶐�����B */
    for( cursor.Y=y0 ; cursor.Y <= y1; ++cursor.Y ){
        char line[10000];

        Console::readTextVram( 0 , cursor.Y , line, screen.width() );
        int knj=0;
	for( int x=0; x <= x1 ; ++x ){
	    if( knj ){
		if( x == x0)
                    buffer << line[x-1] << line[x];
                knj = 0;
	    }else{
		if( isKanji(line[x]) ){
                    knj = 1;
		    if( x >= x0)
                        buffer << line[x] << line[x+1];
		}else{
		    if( x >= x0)
			buffer += line[x] ? line[x] : ' ';
		}
	    }
	}
        buffer << "\r\n";
    }

    /* �N���b�v�{�[�h�ɓ]�� */
    Console::writeClipBoard( buffer.chars() , buffer.length() );
}

Status GetLine::xscript(int)
{
    XScript xscript;
    xscript.loop();
    return NEXTCHAR;
}
#endif
/* vim:set sw=4 ts=8 et: */
