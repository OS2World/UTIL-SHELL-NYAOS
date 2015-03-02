/* 一行入力モジュール Getline での、
 * キーにバインドされるコマンドの動作を記述
 */

#include <ctype.h>
#include <stdio.h>
#include "getline.h"
#include "ntcons.h"

#ifdef NYACUS
#  include <windows.h>
#endif

/* コマンド：CTRL-A：カーソルを先頭へ移動させる。*/
Status GetLine::goto_head(int)
{
    putbs( pos-offset );
    pos = offset = 0;
    repaint_after();
    return NEXTCHAR;
}

void GetLine::backward_( int n )
{
    pos -= n;
    if( pos < offset ){
        offset -= n;
        repaint_after();
    }else{
        putbs(n);
    }
}

/* コマンド：CTRL-B：カーソルを一つ前に移動させる。*/
Status GetLine::backward(int)
{
    if( pos > 0 )
	backward_( buffer.preSizeAt(pos) );
    return NEXTCHAR;
}

Status GetLine::backward_word(int)
{
    int top=0;
    int i=0;
    for(;;){
	for(;;){ /* 空白のスキップ */
	    if( i >= pos )
		goto exit;
	    if( !isspace(buffer[i] & 255) )
		break;
	    ++i;
	}
	top = i;
	for(;;){ /* 単語のスキップ */
	    if( i >= pos )
		goto exit;
	    if( isspace( buffer[i] & 255) )
		break;
	    ++i;
	}
    }
  exit:
    if( pos > top )
	backward_( pos - top );
    return NEXTCHAR;
}


/* コマンド：CTRL-C：入力キャンセルするが、表示は消さない */
Status GetLine::abort(int)
{
    buffer.erase_line(0);
    return CANCEL;
}

/* コマンド：CTRL-D：カーソル上の文字を削除するか、補完対象一覧を出す */
Status GetLine::erase_or_listing(int key)
{
    return pos == buffer.length() ? this->listing(key) : this->erase(key);
}

/* コマンド：CTRL-D：
 *     入力文字数 0 文字→ EOF とみなして終了
 *     行末尾 → 補完対象一覧
 *     その他 → 単純削除
 */
Status GetLine::erase_listing_or_bye(int key)
{
    if( buffer.length() == 0 )
	return this->bye(key);
    else if( pos == buffer.length() )
	return this->listing(key);
    else
	return this->erase(key);
}


/* コマンド：CTRL-E：カーソルを末尾へ移動させる。*/
Status GetLine::goto_tail(int)
{
    if( buffer.length()+1-offset >= width ){
        // 一番最後尾[width-1]には空白文字が必要.
        putbs( pos-offset );
        offset = buffer.length()+1-width;
        puts_between( offset , buffer.length() );
        putchr(' ');
        putbs(1);
    }else{
        puts_between( pos , buffer.length() );
    }
    pos = buffer.length() ;
    return NEXTCHAR;
}

/* n 桁分カーソルを進める */
void GetLine::foreward_(int n)
{
    int oldpos = pos;
    pos += n;
    int nxtpos = pos + buffer.sizeAt(pos);
    if( nxtpos-offset >= width ){
        // スクロール
        putbs( oldpos-offset );
        offset = nxtpos-width;
        puts_between( offset , nxtpos );
        putbs( nxtpos - pos );
    }else{
        puts_between( oldpos , pos );
    }
    repaint_after();
}

/* コマンド：CTRL-F：カーソル右移動 */
Status GetLine::foreward(int)
{
    if( pos < buffer.length() )
	foreward_( buffer.sizeAt(pos) );
    return NEXTCHAR;
}

Status GetLine::foreward_word(int)
{
    int i=0;
    while( pos+i < buffer.length() && !isspace(buffer[pos+i]) )
	++i;
    while( pos+i < buffer.length() && isspace(buffer[pos+i]) )
	++i;
    foreward_( i );
    return NEXTCHAR;
}


/* コマンド：CTRL-H：バックスペース */
Status GetLine::backspace(int key)
{
    if( pos <= 0 )
        return NEXTCHAR;
    
    backward(key);
    erase(key);

    return NEXTCHAR;
}

void GetLine::savekillring( int from , int to )
{
    NnString killbuffer;
    
    buffer.decode(from,to-from,killbuffer);
    Console::writeClipBoard( killbuffer.chars() , killbuffer.length() );
}

/* コマンド：CTRL-K：カーソル以降の文字を削除する。*/
Status GetLine::erase_line(int)
{
    savekillring( pos , buffer.length() );
    putbs( putspc( tail() - pos ) );
    buffer.erase_line( pos );
    return NEXTCHAR;
}

/* コマンド：CTRL-L：画面をクリアする */
Status GetLine::cls(int)
{
    clear();
    prompt();

    puts_between( offset , pos );
    repaint_after();

    return NEXTCHAR;
}

/* コマンド：CTRL-M：入力 */
Status GetLine::enter(int)
{
    return NEXTLINE;
}

/* コマンド：CTRL_N：ヒストリ参照（未来方向）*/
Status GetLine::next(int)
{
    if( history.size() <= 0 )
        return NEXTCHAR;

    if( history_pointor == history.size()-1 )
	buffer.decode( history[ history_pointor ]->body() );

    if( ++history_pointor >= history.size() )
        history_pointor = 0;

    replace_all_repaint( history[ history_pointor ]->body().chars() );

    return NEXTCHAR;
}


/* コマンド：CTRL-P：ヒストリ参照 */
Status GetLine::previous(int)
{
    if( history.size() <= 0 )
        return NEXTCHAR;

    if( history_pointor == history.size()-1 )
	buffer.decode( history[ history_pointor ]->body() );

    if( --history_pointor < 0 )
        history_pointor = history.size()-1;

    replace_all_repaint( history[ history_pointor ]->body().chars() );

    return NEXTCHAR;
}

/* コマンド：Vz ライクなヒストリ参照（未来）*/
Status GetLine::vz_next(int key)
{
    return this->vz_previous(key);
}

/* コマンド：Vz ライクなヒストリ参照（過去） */
Status GetLine::vz_previous(int key)
{
    int at,size;
    NnString curword=current_word(at,size);
    // printf("\n(%d,%d)\n",at,size);
    NnString found;

    int m=history.size()-1;
    if( m < 0 )
        return NEXTCHAR;

    int n=0;
    int sealsize=0;
    NnString temp;
    /*** 行単位の検索 ***/
    if( at <= 0 ){
	buffer.decode(temp);
        /* 行単位の Vz ライクなヒストリ参照 */
        if( ( which_command(key) == &GetLine::vz_previous
            ? seekLineForward(m,temp.chars() )
            : seekLineBackward(m,temp.chars() ) ) < 0 )
            return NEXTCHAR;
        for(;;){
            sealsize = printSeal( history[m]->body().chars()+buffer.length() , sealsize );
            key=getkey();
            if( which_command(key) == &GetLine::vz_previous ){
		/* 次に入力されたコマンドが過去検索の場合 */
                if( seekLineForward(m,temp.chars() ) < 0 )
                    break; /* 発見できなかった場合、終了 */
            }else if( which_command(key) == &GetLine::vz_next ){
		/* 次に入力されたコマンドが未来検索の場合 */
                if( seekLineBackward(m,temp.chars() ) < 0 )
                    break;
            }else{
		/* 次に入力されたコマンドが検索以外だった場合 */
		eraseSeal( sealsize );
                insertHere( history[m]->body().chars()+buffer.length() );
		/* 入力されたキー自体を解釈し直して終了 */
                return interpret(key);
            }
        }
	eraseSeal( sealsize );
        return NEXTCHAR;
    }
    
    /*** 単語単位の検索 ***/
    if( ( which_command(key) == &GetLine::vz_previous
        ? seekWordForward(m,n,curword,found) 
        : seekWordBackward(m,n,curword,found) ) < 0 )
    {
	/* 一つもマッチしない場合は何もせずに終了する */
        return NEXTCHAR;
    }

    for(;;){
        /* 単語単位のヒストリ参照 */
        sealsize = printSeal( found.chars() + curword.length() , sealsize );
        key=getkey();
        if( which_command(key) == &GetLine::vz_previous ){
	    /* 次に入力したキーは、次検索 */
            if( seekWordForward(m,n,curword,found) < 0 )
                break;
        }else if( which_command(key) == &GetLine::vz_next ){
	    /* 次に入力したキーは、前検索 */
            if( seekWordBackward(m,n,curword,found) < 0 )
                break;
        }else{
	    /* いずれでもない…確定＆キー入力 */
	    eraseSeal( sealsize );
            insertHere( found.chars() + curword.length() );
            return interpret(key);
        }
    }
    eraseSeal( sealsize );
    return NEXTCHAR;
}

/* コマンド：CTRL-T：直前の文字を入れ替える。*/
Status GetLine::swap_char(int)
{
    char tmp[ 4 ];
    int  i;

    if( pos == 0 )
	return NEXTCHAR;

    if( pos >= buffer.length() )
	backward(0);
    backward(0);

    for(i=0;i<buffer.sizeAt(pos);++i)
	tmp[ i ] = buffer[ pos+i ];
    tmp[ i ] = '\0';
    buffer.delroom( pos , i );
    buffer.insert( tmp , pos + buffer.sizeAt(pos) );
    repaint_after( 0 );

    foreward(0);
    foreward(0);

    return NEXTCHAR;
}
/* コマンド：CTRL-U：先頭からカーソル直前までを削除 */
Status GetLine::erase_before(int)
{
    putbs( pos - head() );
    savekillring( 0 , pos );
    buffer.delroom( 0 , pos );
    int bs=pos;
    offset = pos = 0;
    repaint_after( bs );
    return NEXTCHAR;
}

/* コマンド：CTRL-V：コントロールキーを挿入する */
Status GetLine::insert_ctrl(int)
{
    return insert(getkey());
}

/* コマンド：Ctrl-W：カーソル直前の単語を消去 */
Status GetLine::erase_word(int)
{
    if( pos <= 0 )
        return NEXTCHAR;
    
    int at=0,bs;

    int spc=1;
    for(int i=0; i<pos; i++){
        int spc_=isSpace(buffer[i]);
        if( spc && !spc_ )
            at = i; 
        spc = spc_;
    }
    bs = pos - at;
    if( at > offset ){
        putbs( bs );
    }else{
        putbs( pos - offset );
        offset = at;
    }
    savekillring( at , pos );
    buffer.delroom( at , pos - at );
    pos -= bs;
    repaint_after( bs );
    return NEXTCHAR;
}

/* コマンド：CTRL-Y：ペースト */
Status GetLine::yank(int)
{
    NnString killbuffer;

    Console::readClipBoard( killbuffer );

    if( killbuffer.at( killbuffer.length()-1 ) == '\n' )
        killbuffer.chop();
    if( killbuffer.at( killbuffer.length()-1 ) == '\r' )
        killbuffer.chop();

    insertHere( killbuffer.chars() );
    
    return NEXTCHAR;
}


/* コマンド：CTRL-Z：NYA*OS終了 */
Status GetLine::bye(int)
{
    return TERMINATE;
}

/* コマンド：Ctrl-[：全文字を削除する */
Status GetLine::erase_all(int)
{
    /* カーソル以降の文字を消す */
    putspc( tail() - pos );
    putbs( tail() - head() );
    /* カーソル以前の文字を消して、プロンプトの直後へカーソルを移動 */
    putbs( putspc( pos - head() ) );
    
    offset = pos = 0;
    buffer.erase_line( 0 );
    return NEXTCHAR;
}


/* コマンド：入力無視 */
Status GetLine::do_nothing(int)
{
    return NEXTCHAR;
}

/* コマンド：表示可能文字を挿入する */
Status GetLine::insert(int key)
{
    int oldpos = pos;
    int size1=buffer.insert1(key,pos);
    if( size1 <= 0 ){ // memory allocation error.
        return NEXTCHAR;
    }
    pos += size1;
    int nxtpos = pos + buffer.sizeAt(pos);
    // 表示の更新.
    if( nxtpos-offset > width ){
        // 右端を超えそうなとき.
        putbs( oldpos-offset );
        offset = nxtpos-width;
        puts_between( offset , nxtpos );
        putbs( nxtpos - pos );
    }else{
        puts_between( oldpos , pos );
    }
    repaint_after();
    return NEXTCHAR;
}

/* コマンド：DEL：カーソル上の文字を削除 */
Status GetLine::erase(int)
{
    repaint_after( buffer.erase1(pos) );
    return NEXTCHAR;
}

Status GetLine::ime_toggle(int)
{
#ifdef NYACUS
  /* キー注入結果を反映させるための待ち（手元の環境では単なる Yield 扱いの 
     0 でも問題なさそうだった。環境によってはもっと大きめの値がいるかも） */
  const DWORD dwYieldMSecForKey = 1;
  
  keybd_event(VK_MENU, 0 /* 0x38 */, 0, 0);                 /* down Alt */
  keybd_event(VK_KANJI, 0 /* 0x29 */, 0, 0);                /* down Kanji */
  keybd_event(VK_MENU, 0 /* 0x38 */, KEYEVENTF_KEYUP, 0);   /* up Alt */
  keybd_event(VK_KANJI, 0 /* 0x29 */, KEYEVENTF_KEYUP, 0);  /* up Kanji */
  Sleep(dwYieldMSecForKey);  /* wait for a proof */
#endif
  return NEXTCHAR;
}
