/* -*- c++ -*- */

#ifndef GETLINE_H
#define GETLINE_H

#include <stdlib.h>
#include "const.h"
#include "nnstring.h"
#include "nnvector.h"
#include "history.h"
#include "nnhash.h"

#undef PROMPT_SHIFT
#undef max

class TwinBuffer {
    char    *strbuf;    // 入力文字列 ASCII/ShiftJISコード
    char    *atrbuf;    // 入力文字列 属性コード
    int      len;	// 入力byte数
    int      max;	// 現バッファサイズ
    enum{
        SBC ,		// 半角文字        SBCS 文字
        DBC1ST ,	// 倍角文字 左側 , DBCS 文字 第１バイト目
        DBC2ND ,	// 倍角文字 右側 , DBCS 文字 第２バイト目
    };
public:
    const char *chars() const { return strbuf; }
    int      length() const { return len; }
    int      makeroom(int at,int size);	      // at から sizeバイト分の桁を作る。
    void     delroom(int at,int size);	      // at から sizeバイト分を削除する。
    void     move(int at,int size,int dst);
    int      replace(int at,int nchars, const char *string );
    int      insert1(int key,int at);
    int      erase1(int at);
    void     erase_line(int at);
    int      insert(const char *s,int at);

    int      operator[](int at){ return strbuf ? (strbuf[ at ] & 255 ) : 0 ; }
    int      isAnk (int at) const { return atrbuf[at]==SBC; }
    int      isKnj (int at) const { return atrbuf[at]!=SBC; }
    int      isKnj1(int at) const { return atrbuf[at]==DBC1ST; }
    int      isKnj2(int at) const { return atrbuf[at]==DBC2ND; }
    int      sizeAt(int at) const { return atrbuf[at]==DBC1ST ? 2 : 1 ; }
    int      preSizeAt(int at) const { return atrbuf[at-1]==DBC2ND ? 2 : 1 ;}

    int      init();
    void     term();

    TwinBuffer() : strbuf(NULL) , atrbuf(NULL) , len(0) , max(0){}
    ~TwinBuffer(){ term(); }

    static int strlen_ctrl(const char *s);
    static int isCtrl(int x){ return (x & ~0x1F )==0 ; }
    /* ^x 形式になっているCtrlコードをデコードする */
    int decode(NnString &buffer);
    int decode(int at,int len,NnString &buffer);
};

struct Completion;

/* KeyFunction のインスタンスであるところの「機能」は、
 * 自分の使われ時(マップ)を知っている。
 * 分からないのは、対応するキーと、バインドマップの存在だけ.
 */
class KeyFunction : public NnObject {
    static int code_sub( const char *key , int start , int end );
    static int code( const char *key );
    static NnHash function_dictionary;
    NnStringIC funcName_;
protected:
    virtual int bind(int n)=0;
public:
    enum { MAPSIZE = 512 };
    /* 注意:regist したインスタンスは、
     * 所有権が KeyFunction クラス内に移動する */
    void regist();
    const NnString &funcName() const { return funcName_; }
    int bind(const char *keyName);
    KeyFunction( const char *fn ) : funcName_(fn){}
    static int bind(const char *keyName , const char *funcName);
};
class KeyFunctionEdit;
class XScript;
class KeyFunctionXScript : public KeyFunction {
    friend class XScript;
public:
    enum XScriptFuncCode {
        XK_NOBOUND ,
        XK_LEFT ,
        XK_RIGHT ,
        XK_UP ,
        XK_DOWN ,
        XK_HOME ,
        XK_END ,
        XK_CTRLHOME ,
        XK_CTRLEND ,
        XK_PGUP ,
        XK_PGDN ,
        XK_COPY ,
        XK_LEAVE ,
	XK_TAGJUMP ,
    };
private:
    static XScriptFuncCode map[ KeyFunction::MAPSIZE ];
    XScriptFuncCode function_no;
protected:
    int bind(int n);
public:
    KeyFunctionXScript( const char *name , XScriptFuncCode code )
        : KeyFunction(name) , function_no(code){}
    static void init();
};

class NnDir;

class GetLine {
    friend class KeyFunctionEdit;
public:
    static History history;
protected:
    int history_pointor;
    enum{ 
        // 画面幅。実際に使用する値(width)は、これより -1 少ない値.
        DEFAULT_WIDTH=80 , 
    };
    TwinBuffer buffer;
    int pos;	// カーソル位置(byte,桁位置)
    int offset; // 表示位置オフセット
    int width;  // 表示幅
#ifdef PROMPT_SHIFT
    int prompt_offset;  // プロンプトのオフセット
    int prompt_size; // プロンプトのサイズ
#endif
    /* 表示している先頭のバッファ位置を得る */
    int head() const { return offset; }

    /* 表示している最後尾のバッファ位置を得る */
    int tail() const
        { return buffer.length() > offset+width ? offset+width : buffer.length(); }

    virtual int  getkey()=0;		// 化工済み一文字入力.
    virtual void putchr(int ch)=0;	// 文字 1 byte分 出力.
    virtual void putbs(int n)=0;        // カーソルを後退させる.
    virtual void start(){}		// 編集開始の時に呼ばれるフック.
    virtual void end(){}	        // 編集終了の再に呼ばれるフック.
    virtual void clear(){}
    virtual const char *getClipBoardValue(){ return NULL; }
    virtual int  makeCompletionList   ( const NnString &s , NnVector & );
    virtual int  makeTopCompletionList( const NnString &s , NnVector & );

    void repaint_after(int rm=0);   // カーソル以降を再描画
    void replace_all_repaint( const NnString & );

    /* 補完リスト関係 */
    virtual int prompt();
    void  setWidth( int w ){ width = w-1 ; }

    int puts_between(int from,int to);
    int putspc(int n);
    int printedSize() const {
        return buffer.length()-offset > width
                ? width : buffer.length()-offset;
    }
    NnString current_word(int &at,int &size); // カーソル位置を含む単語を抽出する.
    void get_nline_nword(int m,int n,NnString &word);
    int seekWordForward(int &m,int &n,const NnString &word , NnString &found );
    int seekWordBackward(int &m,int &n,const NnString &word , NnString &found );
    int seekLineForward(int &m,const char *line);
    int seekLineBackward(int &m,const char *line);
    int insertHere( const char *s );
    int printSeal( const char *,int clr);
    void eraseSeal(int clr);
    void savekillring( int from , int to );

    void foreward_(int n);
    void backward_(int n);
private:
    int read_complete_list( Completion & );
    void listing_( Completion &comp );
    void replace_repaint_here( int size , const NnString &toStr );
    Status complete_vzlike(int direct);
public:
    // コマンド.
    Status enter(int)    , cancel(int)    , do_nothing(int) , insert(int);
    Status foreward(int) , backward(int)  , goto_head(int)  , goto_tail(int);
    Status erase(int)    , backspace(int) , erase_line(int) , yank(int);
    Status complete(int) , previous(int)  , next(int)       , erase_all(int);
    Status bye(int)      , cls(int)       , abort(int)      , erase_word(int);
    Status listing(int)  , vz_previous(int) , vz_next(int)  , erase_before(int);
    Status erase_or_listing(int) , complete_or_listing(int) , insert_ctrl(int);
    Status complete_next(int) , complete_prev(int) ;
    Status foreward_word(int) , backward_word(int) ;
    Status erase_listing_or_bye(int) , swap_char(int) ;
    Status xscript(int) , ime_toggle(int) , i_search(int) ;
private:
    int currKey;
    int lastKey;				/* 前回押されたキー */

    typedef Status (GetLine::*BindFunc)(int ch);
    static BindFunc bindmap[512];
    static void bindinit();
public:
    static int bindkey(const char *keyname,const char *funcname);
    BindFunc which_command(int key);
    Status   interpret(int key);
    GetLine(){ bindinit(); }
    virtual ~GetLine();

    Status operator() ( NnString &result );
    const char *operator() ( const char *defaultString=0 );

    static int makeCompletionListCore( const NnString &s , NnVector & ,
            int (*)(NnDir &,void *xt)=0,void *xt=0 );
};

class NnHash;

// --- 普通のコンソール経由の入力ルーチン --- 
class DosShell : public GetLine {
    NnString prompt_;
    NnString clear_;
    NnString cursor_on_;
protected:
    virtual int makeTopCompletionList( const NnString &s , NnVector & );
public:
    static NnHash executableSuffix;

    void putchr(int ch);
    void putbs(int n);
    int getkey();
    void start();
    void end();
    int prompt();
    void clear();
#ifdef NYACUS
    void title();
#endif

    void setPrompt( const NnString &p ){ prompt_ = p; }

    static int makeTopCompletionListCore( const NnString &s , NnVector & );
};

class NnPair : public NnSortable {
    NnSortable *pair[2];
public:
    NnPair(){ pair[0] = pair[1] = 0 ; }
    NnPair( NnSortable *x ){ pair[0]=x ; pair[1] = 0; }
    NnPair( NnSortable *x , NnSortable *y ){ pair[0] = x ; pair[1] = y; }
    ~NnPair(){ delete pair[0] ; delete pair[1]; }

    int compare( const NnSortable &x ) const;
    NnSortable *first() const { return pair[0]; }
    NnSortable *second()const { return pair[1]; }
    NnSortable *second_or_first() const {
	return pair[1] ? pair[1] : pair[0];
    }
};

#endif
