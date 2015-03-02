#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "getline.h"
#include "nnhash.h"
#include "nua.h"

#undef  CTRL
#define CTRL(x) ((x) & 0x1F)

/* 機能を名前から引けるデータベース */
NnHash  KeyFunction::function_dictionary;

void KeyFunction::regist()
{
    function_dictionary.put( this->funcName() , this );
}
int KeyFunction::bind( const char *keyName , const char *funcName )
{
    KeyFunction *f = (KeyFunction*)function_dictionary.get( funcName );
    if( f == NULL )
	return -1;
    if( f->bind( keyName ) != 0 )
	return -3;
    return 0;
}

static struct {
    const char *name;
    int code;
} keynames[] = {
  { "ALT_0",                 0x181    /* <Alt>+<0> */ },
  { "ALT_1",                 0x178    /* <Alt>+<1> */ },
  { "ALT_2",                 0x179    /* <Alt>+<2> */ },
  { "ALT_3",                 0x17a    /* <Alt>+<3> */ },
  { "ALT_4",                 0x17b    /* <Alt>+<4> */ },
  { "ALT_5",                 0x17c    /* <Alt>+<5> */ },
  { "ALT_6",                 0x17d    /* <Alt>+<6> */ },
  { "ALT_7",                 0x17e    /* <Alt>+<7> */ },
  { "ALT_8",                 0x17f    /* <Alt>+<8> */ },
  { "ALT_9",                 0x180    /* <Alt>+<9> */ },
  { "ALT_A",                 0x11e    /* <Alt>+<A> */ },
  { "ALT_B",                 0x130    /* <Alt>+<B> */ },
  { "ALT_BACKSLASH",         0x12b    /* <Alt>+<\> */ },
  { "ALT_BACKSPACE",         0x10e    /* <Alt>+<Backspace> */ },
  { "ALT_C",                 0x12e    /* <Alt>+<C> */ },
  { "ALT_COMMA",             0x133    /* <Alt>+<,> */ },
  { "ALT_D",                 0x120    /* <Alt>+<D> */ },
  { "ALT_DEL",               0x1a3    /* <Alt>+<Del> */ },
  { "ALT_DOWN",              0x1a0    /* <Alt>+<Down arrow> */ },
  { "ALT_E",                 0x112    /* <Alt>+<E> */ },
  { "ALT_END",               0x19f    /* <Alt>+<End> */ },
  { "ALT_EQUAL",             0x183    /* <Alt>+<=> */ },
  { "ALT_ESC",               0x101    /* <Alt>+<Esc>    [DOS]*/ },
  { "ALT_F",                 0x121    /* <Alt>+<F> */ },
  { "ALT_F1",                0x168    /* <Alt>+<F1> */ },
  { "ALT_F10",               0x171    /* <Alt>+<F10> */ },
  { "ALT_F11",               0x18b    /* <Alt>+<F11> */ },
  { "ALT_F12",               0x18c    /* <Alt>+<F12> */ },
  { "ALT_F2",                0x169    /* <Alt>+<F2> */ },
  { "ALT_F3",                0x16a    /* <Alt>+<F3> */ },
  { "ALT_F4",                0x16b    /* <Alt>+<F4> */ },
  { "ALT_F5",                0x16c    /* <Alt>+<F5> */ },
  { "ALT_F6",                0x16d    /* <Alt>+<F6> */ },
  { "ALT_F7",                0x16e    /* <Alt>+<F7> */ },
  { "ALT_F8",                0x16f    /* <Alt>+<F8> */ },
  { "ALT_F9",                0x170    /* <Alt>+<F9> */ },
  { "ALT_G",                 0x122    /* <Alt>+<G> */ },
  { "ALT_H",                 0x123    /* <Alt>+<H> */ },
  { "ALT_HOME",              0x197    /* <Alt>+<Home> */ },
  { "ALT_I",                 0x117    /* <Alt>+<I> */ },
  { "ALT_INS",               0x1a2    /* <Alt>+<Ins> */ },
  { "ALT_J",                 0x124    /* <Alt>+<J> */ },
  { "ALT_K",                 0x125    /* <Alt>+<K> */ },
  { "ALT_L",                 0x126    /* <Alt>+<L> */ },
  { "ALT_LEFT",              0x19b    /* <Alt>+<Left arrow> */ },
  { "ALT_LEFT_BRACKET",      0x11a    /* <Alt>+<[> */ },
  { "ALT_LEFT_QUOTE",        0x129    /* <Alt>+<`> */ },
  { "ALT_M",                 0x132    /* <Alt>+<M> */ },
  { "ALT_MINUS",             0x182    /* <Alt>+<-> */ },
  { "ALT_N",                 0x131    /* <Alt>+<N> */ },
  { "ALT_O",                 0x118    /* <Alt>+<O> */ },
  { "ALT_P",                 0x119    /* <Alt>+<P> */ },
  { "ALT_PAD_ASTERISK",      0x137    /* <Alt>+<*> (numeric keypad) */ },
  { "ALT_PAD_ENTER",         0x1a6    /* <Alt>+<Enter> (numeric keypad) */ },
  { "ALT_PAD_MINUS",         0x14a    /* <Alt>+<-> (numeric keypad) */ },
  { "ALT_PAD_PLUS",          0x14e    /* <Alt>+<+> (numeric keypad) */ },
  { "ALT_PAD_SLASH",         0x1a4    /* <Alt>+</> (numeric keypad) */ },
  { "ALT_PAGEDOWN",          0x1a1    /* <Alt>+<Page down> */ },
  { "ALT_PAGEUP",            0x199    /* <Alt>+<Page up> */ },
  { "ALT_PERIOD",            0x134    /* <Alt>+<.> */ },
  { "ALT_Q",                 0x110    /* <Alt>+<Q> */ },
  { "ALT_R",                 0x113    /* <Alt>+<R> */ },
  { "ALT_RETURN",            0x11c    /* <Alt>+<Return> */ },
  { "ALT_RIGHT",             0x19d    /* <Alt>+<Right arrow> */ },
  { "ALT_RIGHT_BRACKET",     0x11b    /* <Alt>+<]> */ },
  { "ALT_RIGHT_QUOTE",       0x128    /* <Alt>+<'> */ },
  { "ALT_S",                 0x11f    /* <Alt>+<S> */ },
  { "ALT_SEMICOLON",         0x127    /* <Alt>+<;> */ },
  { "ALT_SLASH",             0x135    /* <Alt>+</> */ },
  { "ALT_SPACE",             0x139    /* <Alt>+<Space>  [OS2] */ },
  { "ALT_T",                 0x114    /* <Alt>+<T> */ },
  { "ALT_TAB",               0x1a5    /* <Alt>+<Tab>  [DOS] */ },
  { "ALT_U",                 0x116    /* <Alt>+<U> */ },
  { "ALT_UP",                0x198    /* <Alt>+<Up arrow> */ },
  { "ALT_V",                 0x12f    /* <Alt>+<V> */ },
  { "ALT_W",                 0x111    /* <Alt>+<W> */ },
  { "ALT_X",                 0x12d    /* <Alt>+<X> */ },
  { "ALT_Y",                 0x115    /* <Alt>+<Y> */ },
  { "ALT_Z",                 0x12c    /* <Alt>+<Z> */ },
  { "BACKSPACE", 0x8  },
  { "CENTER",                0x14c    /* Center cursor */ },
  { "CTRL_A", 0x1  },
  { "CTRL_AT",               0x103    /* <Ctrl>+<@> */ },
  { "CTRL_B", 0x2  },
  { "CTRL_C", 0x3  },
  { "CTRL_CENTER",           0x18f    /* <Ctrl>+<Center> */ },
  { "CTRL_D", 0x4  },
  { "CTRL_DEL",              0x193    /* <Ctrl>+<Del> */ },
  { "CTRL_DOWN",             0x191    /* <Ctrl>+<Down arrow> */ },
  { "CTRL_E", 0x5  },
  { "CTRL_END",              0x175    /* <Ctrl>+<End> */ },
  { "CTRL_F", 0x6  },
  { "CTRL_F1",               0x15e    /* <Ctrl>+<F1> */ },
  { "CTRL_F10",              0x167    /* <Ctrl>+<F10> */ },
  { "CTRL_F11",              0x189    /* <Ctrl>+<F11> */ },
  { "CTRL_F12",              0x18a    /* <Ctrl>+<F12> */ },
  { "CTRL_F2",               0x15f    /* <Ctrl>+<F2> */ },
  { "CTRL_F3",               0x160    /* <Ctrl>+<F3> */ },
  { "CTRL_F4",               0x161    /* <Ctrl>+<F4> */ },
  { "CTRL_F5",               0x162    /* <Ctrl>+<F5> */ },
  { "CTRL_F6",               0x163    /* <Ctrl>+<F6> */ },
  { "CTRL_F7",               0x164    /* <Ctrl>+<F7> */ },
  { "CTRL_F8",               0x165    /* <Ctrl>+<F8> */ },
  { "CTRL_F9",               0x166    /* <Ctrl>+<F9> */ },
  { "CTRL_G", 0x7  },
  { "CTRL_H", 0x8  },
  { "CTRL_HOME",             0x177    /* <Ctrl>+<Home> */ },
  { "CTRL_I", 0x9  },
  { "CTRL_INS",              0x192    /* <Ctrl>+<Ins> */ },
  { "CTRL_J", 0xa  },
  { "CTRL_K", 0xb  },
  { "CTRL_L", 0xc  },
  { "CTRL_LEFT",             0x173    /* <Ctrl>+<Left arrow> */ },
  { "CTRL_M", 0xd  },
  { "CTRL_N", 0xe  },
  { "CTRL_O", 0xf  },
  { "CTRL_P", 0x10  },
  { "CTRL_PAD_ASTERISK",     0x196    /* <Ctrl>+<*> (numeric keypad) */ },
  { "CTRL_PAD_MINUS",        0x18e    /* <Ctrl>+<-> (numeric keypad) */ },
  { "CTRL_PAD_PLUS",         0x190    /* <Ctrl>+<+> (numeric keypad) */ },
  { "CTRL_PAD_SLASH",        0x195    /* <Ctrl>+</> (numeric keypad) */ },
  { "CTRL_PAGEDOWN",         0x176    /* <Ctrl>+<Page down> */ },
  { "CTRL_PAGEUP",           0x184    /* <Ctrl>+<Page up> */ },
  { "CTRL_PRTSC",            0x172    /* <Ctrl>+<PrtSc> */ },
  { "CTRL_Q", 0x11  },
  { "CTRL_R", 0x12  },
  { "CTRL_RIGHT",            0x174    /* <Ctrl>+<Right arrow> */ },
  { "CTRL_S", 0x13  },
  { "CTRL_SPACE",            0x102    /* <Ctrl>+<Space> */ },
  { "CTRL_T", 0x14  },
  { "CTRL_TAB",              0x194    /* <Ctrl>+<Tab> */ },
  { "CTRL_U", 0x15  },
  { "CTRL_UP",               0x18d    /* <Ctrl>+<Up arrow> */ },
  { "CTRL_V", 0x16  },
  { "CTRL_W", 0x17  },
  { "CTRL_X", 0x18  },
  { "CTRL_Y",                0x19  },
  { "CTRL_Z",                0x1a  },
  { "DEL",                   0x7F     /* <Del> */ },
  { "DOWN",                  0x150    /* <Down arrow> */ },
  { "END",                   0x14f    /* <End> */ },
  { "ENTER",                 0xd  },
  { "ESCAPE",               0x1b  },
  { "F1",                    0x13b    /* <F1> */ },
  { "F10",                   0x144    /* <F10> */ },
  { "F11",                   0x185    /* <F11> */ },
  { "F12",                   0x186    /* <F12> */ },
  { "F2",                    0x13c    /* <F2> */ },
  { "F3",                    0x13d    /* <F3> */ },
  { "F4",                    0x13e    /* <F4> */ },
  { "F5",                    0x13f    /* <F5> */ },
  { "F6",                    0x140    /* <F6> */ },
  { "F7",                    0x141    /* <F7> */ },
  { "F8",                    0x142    /* <F8> */ },
  { "F9",                    0x143    /* <F9> */ },
  { "HOME",                  0x147    /* <Home> */ },
  { "INS",                   0x152    /* <Ins> */ },
  { "LEFT",                  0x14b    /* <Left arrow> */ },
  { "PAGEDOWN",              0x151    /* <Page down> */ },
  { "PAGEUP",                0x149    /* <Page up> */ },
  { "RETURN",                0xd  },
  { "RIGHT",                 0x14d    /* <Right arrow> */ },
  { "SHIFT_DEL",             0x105    /* <Shift>+<Del>  [OS2]*/ },
  { "SHIFT_F1",              0x154    /* <Shift>+<F1> */ },
  { "SHIFT_F10",             0x15d    /* <Shift>+<F10> */ },
  { "SHIFT_F11",             0x187    /* <Shift>+<F11> */ },
  { "SHIFT_F12",             0x188    /* <Shift>+<F12> */ },
  { "SHIFT_F2",              0x155    /* <Shift>+<F2> */ },
  { "SHIFT_F3",              0x156    /* <Shift>+<F3> */ },
  { "SHIFT_F4",              0x157    /* <Shift>+<F4> */ },
  { "SHIFT_F5",              0x158    /* <Shift>+<F5> */ },
  { "SHIFT_F6",              0x159    /* <Shift>+<F6> */ },
  { "SHIFT_F7",              0x15a    /* <Shift>+<F7> */ },
  { "SHIFT_F8",              0x15b    /* <Shift>+<F8> */ },
  { "SHIFT_F9",              0x15c    /* <Shift>+<F9> */ },
  { "SHIFT_INS",             0x104    /* <Shift>+<Ins>  [OS2]*/ },
  { "SHIFT_TAB",             0x10f    /* <Shift>+<Tab> */ },
  { "SPACE",                 0x20  },
  { "TAB",                   0x9 },
  { "UP",                    0x148    /* <Up arrow> */ },
};

/* キーの名前から、キーコードを得る */
int KeyFunction::code_sub( const char *keyname1 , int start , int end )
{
    if( start >= end ){
	if( stricmp( keyname1 , keynames[start].name )==0 )
	    return keynames[start].code;
	return -1;
    }
    int mid = (start+end) >> 1;
    int diff=stricmp( keynames[mid].name , keyname1 );
    if( diff < 0 )
	return code_sub( keyname1 , mid+1 , end );
    else if( diff > 0 )
	return code_sub( keyname1 , start , mid-1 );
    else
	return keynames[mid].code;
}
int KeyFunction::code( const char *keyname )
{
    int keyno = atoi(keyname);
    if( keyno != 0 )
	return keyno;
    return code_sub(keyname , 0 , numof(keynames)-1 );
}
int KeyFunction::bind(const char *name)
{
    int keycode=code(name);
    if( keycode < 0 )
	return -1;
    return this->bind( keycode );
}

/*********************************************************************/

Status (GetLine::*GetLine::bindmap[ KeyFunction::MAPSIZE ])(int);

class KeyFunctionEdit : public KeyFunction {
    Status (GetLine::*func)(int);
protected:
    int bind(int n);
public:
    KeyFunctionEdit( const char *p_name , Status (GetLine::*p_func)(int) )
	: KeyFunction(p_name) , func(p_func){ }
};

int KeyFunctionEdit::bind(int keycode)
{
    if( keycode > 0 && (size_t)keycode > numof(GetLine::bindmap) )
	return -1;
    GetLine::bindmap[ keycode ] = this->func;
    return 0;
}

/** キーをバインドする。
 *	keyno	- キーコード(数値)
 *	funcname - 機能名(文字列)
 * return
 *	 0 : 成功
 *	-1 : 機能名が不適
 *	-2 : キー番号が不適(範囲エラー)
 *      -3 : キーが不適(文字列として)
 */
int GetLine::bindkey( const char *keyname , const char *funcname )
{
    bindinit();
    return KeyFunction::bind( keyname , funcname ) ;
}

enum {
    KEY_CTRL_TAB   = 0x0194 ,
    KEY_CTRL_RIGHT = 0x0174 ,
    KEY_CTRL_LEFT  = 0x0173 ,
    KEY_CTRL_UP    = 0x018D ,
    KEY_ALT_RETURN = 0x011c ,
    KEY_ALT_F      = 0x0121 ,
    KEY_ALT_B      = 0x0130 ,
};

void GetLine::bindinit()
{
    const static struct Func_t{
	const char *name;
	Status (GetLine::*func)(int);
    } list[]={
	{ "abort"               , &GetLine::abort },
	{ "accept-line"         , &GetLine::enter },     // bash.
	{ "backward"	        , &GetLine::backward },
	{ "backward-char"       , &GetLine::backward },  // bash.
	{ "backward-delete-char", &GetLine::backspace }, // bash.
	{ "backward-word"	, &GetLine::backward_word },
	{ "back_and_erase"      , &GetLine::backspace },
	{ "beginning-of-line"   , &GetLine::goto_head }, // bash.
	{ "bye"		        , &GetLine::bye },
	{ "cancel"		, &GetLine::erase_all },
	{ "clear"		, &GetLine::cls },
	{ "clear-screen"        , &GetLine::cls },       // bash.
	{ "complete-next"       , &GetLine::complete_next },
	{ "complete-previous"   , &GetLine::complete_prev },
	{ "complete-or-list"    , &GetLine::complete_or_listing }, //right.
	{ "complete_or_list"    , &GetLine::complete_or_listing }, //wrong.
	{ "delete-char"         , &GetLine::erase },     // bash.
	{ "end-of-line"         , &GetLine::goto_tail }, // bash.
	{ "enter"		, &GetLine::enter },
	{ "erase"		, &GetLine::erase },
	{ "erase-or-list"	, &GetLine::erase_or_listing },
	{ "erase_all"	        , &GetLine::erase_all },
	{ "erase_line"	        , &GetLine::erase_line },
	{ "erase_list_or_bye"   , &GetLine::erase_listing_or_bye },
	{ "erase_or_list"	, &GetLine::erase_or_listing },
	{ "forward" 	        , &GetLine::foreward },
	{ "forward-char"        , &GetLine::foreward },   // bash.
	{ "forward-word"	, &GetLine::foreward_word },
	{ "head"		, &GetLine::goto_head },
	{ "insert-self"         , &GetLine::insert },
	{ "kill-line"           , &GetLine::erase_line }, // bash.
	{ "kill-whole-line"     , &GetLine::erase_all },  // bash.
        { "i-search"            , &GetLine::i_search },
        { "ime-toggle"          , &GetLine::ime_toggle },
	{ "next"		, &GetLine::next },
	{ "next-history"        , &GetLine::next },       // bash.
	{ "none"                , &GetLine::do_nothing }, 
	{ "paste"               , &GetLine::yank },
	{ "possible-completions", &GetLine::listing },    // bash.
	{ "previous"	        , &GetLine::previous },
	{ "previous-history"    , &GetLine::previous },   // bash.
	{ "quote"		, &GetLine::insert_ctrl } ,
	{ "swap-char"           , &GetLine::swap_char },
	{ "tail"		, &GetLine::goto_tail },
	{ "unix-line-discard"   , &GetLine::erase_all },  // bash.
	{ "unix-word-rubout"    , &GetLine::erase_word }, // bash.
	{ "vzlike-next-history" , &GetLine::vz_next },    // bash.
	{ "vzlike-previous-history",&GetLine::vz_previous },// bash.
	{ "vz_next"             , &GetLine::vz_next },
	{ "vz_previous"         , &GetLine::vz_previous },
#ifdef NYACUS
	{ "xscript:start"       , &GetLine::xscript },
#endif
	{ "yank"		, &GetLine::yank },
    };
    size_t i;
    static int bindmap_inited=0;
    if( bindmap_inited != 0 )
        return;
    bindmap_inited=1;

    for( i=0 ; i< numof(list) ; ++i ){
	KeyFunctionEdit *f=new KeyFunctionEdit(list[i].name,list[i].func);
	if( f != NULL )
	    f->regist();
    }
    for(i=0;i<numof(bindmap);i++)
	bindmap[ i ] = &GetLine::do_nothing;
    for(i=' ';i<256;i++)
	bindmap[ i ] = &GetLine::insert;

    bindmap[ CTRL('C') ] = &GetLine::abort;
    bindmap[ 0x0153 ]    =  &GetLine::erase; // DEL
    bindmap[ 0x7F ]      =  &GetLine::erase; // DEL
    bindmap[ CTRL('D') ] = &GetLine::erase_listing_or_bye;
    bindmap[ 0x014D ]    = // LEFT
    bindmap[ CTRL('F') ] = &GetLine::foreward;
    bindmap[ 0x014B ] =  // RIGHT
    bindmap[ CTRL('B') ] = &GetLine::backward;
    bindmap[ CTRL('M') ] = &GetLine::enter;
    bindmap[ CTRL('H') ] = &GetLine::backspace;
    bindmap[ 0x0147 ] = // HOME
    bindmap[ CTRL('A') ] = &GetLine::goto_head;
    bindmap[ 0x014F ] = // END
    bindmap[ CTRL('E') ] = &GetLine::goto_tail;
    bindmap[ CTRL('K') ] = &GetLine::erase_line;
    bindmap[ CTRL('U') ] = &GetLine::erase_before;
    bindmap[ CTRL('T') ] = &GetLine::swap_char ;
    bindmap[ CTRL('[') ] = &GetLine::erase_all;
    bindmap[ 0x0148 ]    = &GetLine::previous;
    bindmap[ CTRL('P') ] = &GetLine::previous;
    bindmap[ 0x0150 ] =    &GetLine::next;
    bindmap[ CTRL('N') ] = &GetLine::next;
    bindmap[ CTRL('I') ] = &GetLine::complete_or_listing;
    bindmap[ CTRL('Y') ] = &GetLine::yank;
    bindmap[ CTRL('Z') ] = &GetLine::bye;
    bindmap[ CTRL('L') ] = &GetLine::cls;
    bindmap[ CTRL('W') ] = &GetLine::erase_word;
    bindmap[ CTRL('V') ] = &GetLine::insert_ctrl;
    bindmap[ CTRL('\\') ] = &GetLine::ime_toggle;
    bindmap[ CTRL('R') ] = &GetLine::i_search;

    /* bindmap[ KEY_ALT_RETURN ] = * ←ALT_RETURN はDOS窓では使えない */
    bindmap[ CTRL('O') ]      =
    bindmap[ KEY_CTRL_TAB ]   = &GetLine::complete_next ;
    
    bindmap[ KEY_CTRL_RIGHT ] = 
    bindmap[ KEY_ALT_F      ] = &GetLine::foreward_word;

    bindmap[ KEY_CTRL_LEFT  ] =
    bindmap[ KEY_ALT_B      ] = &GetLine::backward_word;
#ifdef NYACUS
    bindmap[ CTRL(']') ] =
    bindmap[ KEY_CTRL_UP ]    = &GetLine::xscript;
    KeyFunctionXScript::init();
#endif
    NyaosLua L(NULL);
    if( L != NULL ){
        lua_newtable(L);
        for(size_t i=0 ; i<numof(keynames) ; ++i ){
            lua_pushstring(L,keynames[i].name );
            lua_pushinteger(L,keynames[i].code );
            lua_settable(L,-3);
        }
        lua_setfield(L,-2,"key");
    }
}

