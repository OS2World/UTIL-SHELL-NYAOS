#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <string.h>
#include "nndir.h"
#include "getline.h"
#include "nua.h"

#ifdef NYACUS
extern int read_shortcut(const char *src,char *buffer,int size);
#endif

int NnPair::compare( const NnSortable &x ) const
{
    return first()->compare( *((const NnPair&)x).first() );
}

/* 環境変数の補完を行う.
 *    startpos … 環境変数表現開始位置('%'の位置)
 *    endpos   … 同末尾(ない時は 負)
 *    wildcard … 展開前文字列全体
 *    array    … 補完候補を入れる配列
 * return
 *     >=0 補完候補数.
 *     < 0 環境変数の補完の必要はなく、処理としては展開しただけ.
 */
static int expand_environemnt_variable(
	int startpos , 
	int endpos ,
	const char *terminate_string ,
	NnString &wildcard ,
	NnVector &array )
{
    NnString name;
    if( endpos >= 0 ){
	/* 環境変数名が完結しているので、展開するのみ */
	name.assign( wildcard.chars()+startpos+1 , endpos-startpos-1 );
	const char *value=getEnv( name.chars() , NULL );
	if( value != NULL ){
	    NnString tmp( wildcard.chars() , startpos );
	    tmp << value << (wildcard.chars()+endpos+1) ;
	    wildcard = tmp;
	}
	return -1;
    }else{
	/* 環境変数名が完結していない → 環境変数名を補完の対象とする */
	name = wildcard.chars()+startpos+1 ;
	name.upcase();
	for( char **p=environ ; *p != NULL ; ++p ){
	    if( strnicmp( *p , name.chars() , name.length()) == 0 ){
		NnString *str = new NnString( wildcard.chars() , startpos+1 );
		int eqlpos=NnString::findOf(*p,"=");
		if( eqlpos >= 0 ){
		    str->insertAt( str->length() , *p , eqlpos );
		}else{
		    *str << *p;
		}
		*str << terminate_string;

		array.append( new NnPair(str) );
	    }
	}
	return array.size();
    }
}

static int nondir_filter( NnDir &dir , void *xt )
{
    if( *static_cast<int*>(xt) == 0 )
        return 1;
    if( dir.isDir() )
        return 1;
    if( dir->endsWith(".lnk") )
        return 1;
    return 0;
}


/* 補完候補リストを作成する.
 *      region  パスを含む範囲(引用符含む)
 *      array   候補リストを入れる先
 * return
 *      候補数
 */
int GetLine::makeCompletionList( const NnString &region, NnVector &array )
{
    /* ディレクトリを排除するコマンドかどうかを判定する */
    int directory_only=0;
    static const char *command_for_dironly[]={
        "cd ","pushd ",NULL
    };
    for(const char **p=command_for_dironly ; *p != NULL ; ++p ){
        if( strncmp( buffer.chars() , *p , strlen(*p) )==0 ){
            directory_only = 1;
            break;
        }
    }
    return makeCompletionListCore( region , array ,
            nondir_filter , &directory_only);
}
int GetLine::makeCompletionListCore( const NnString &region, NnVector &array ,
        int (*filter)(NnDir &,void *xt),void *xt )
{
    int i;
    NnString path;

    /* 引用符を全て削除する */
    path = region;
    path.dequote();

    int lastroot=NnDir::lastRoot( path.chars() );
    char rootchar=( lastroot != -1 && path.at(lastroot)=='/' ? '/' : '\\' );

    /* ワイルドカード文字列の作成 */
    NnString wildcard;
    NnDir::f2b( path.chars() , wildcard );
    if( wildcard.at(0) == '~' && 
	(isalnum(wildcard.at(1) & 255) || wildcard.at(1)=='_' ) && 
	lastroot == -1 )
    {
	/* ~ で始まり、ルートが含まれておらず、二文字目に英数字がある場合は
	 * 不完全な特殊フォルダー名が入っているとみなす。
	 */
	wildcard.shift();
	for( NnHash::Each itr(NnDir::specialFolder) ; *itr != NULL ; ++itr ){
	    if( itr->key().startsWith(wildcard) ){
		NnString *str=new NnString();
		*str << '~' << itr->key() << rootchar ;
		array.append( new NnPair(str) );
	    }
	}
	return array.size();
    }
    /* パスに環境変数が含まれていたら変換しておく */
    /* %…% 形式の環境変数表記 */
    if( (i=wildcard.findOf("%")) >= 0 ){
	int j=wildcard.findOf("%",i+1);
	int result = expand_environemnt_variable(i,j,"%",wildcard,array);
	if( result >= 0 )
	    return result;
    }
    /* $… 形式の環境変数表記 */
    if( (i=wildcard.findOf("$")) >= 0 ){
	int result;
	if( wildcard.at(i+1) == '{' ){
	    /* ${…} 形式 */
	    result = expand_environemnt_variable(
		    i+1, wildcard.findOf("}",i+2),
		    "}", wildcard,array );
	}else{
	    /* $… 形式 */
	    result = expand_environemnt_variable(
		    i, wildcard.findOf(" \t",i+1),
		    "",wildcard,array );
	}
	if( result >= 0 )
	    return result;
    }

    int has_wildcard=0;
    if( wildcard.findOf("*?") >= 0 )
	has_wildcard = 1;
    else
	wildcard += '*';

    /* ディレクトリ部分を抽出する */
    NnString basename;
    if( lastroot >= 0 )
        basename.assign( path.chars() , lastroot+1 );
  
    /* 候補を作成する */
    i=0;
    for( NnDir dir(wildcard) ; dir.more() ; dir.next() ){
        // 「.」「..」を排除する.
        if(    dir->at(0) == '.' 
            && (    dir->at(1) == '\0'
                || (dir->at(1) == '.' && dir->at(2)=='\0' ) )){
            continue;
        }
        // ロングファイル名はマッチしないが、ショートファイル名がマッチ
        // してしまうケースを排除する.
	// (ただし、ワイルドカードを「明示的に」利用している場合は除く)
        if( strnicmp( dir.name() , path.chars() +(lastroot+1)
                    , path.length()-(lastroot+1) ) != 0 
	    && ! has_wildcard )
            continue;

        if( filter != NULL && (*filter)(dir,xt)==0 )
            continue;

        NnStringIC *name=new NnStringIC(basename);
        if( name == NULL )
            break;

        *name += dir.name();
        if( dir.isDir() )
            *name += rootchar;
        if( array.append( 
                // [ 全補完文字列 , 純粋ファイル名(非ディレクトリ部分) ] のペア.
		new NnPair(name,new NnString(name->chars()+basename.length() )) 
	    ) != 0 )
	{
            delete name;
            break;
        }
        ++i;
    }
    return i;
}
/* コマンド名補完候補リストを作成する.
 * (本クラスでは、makeCompletionList と同じ。同メソッドは派生クラスで
 *  オーバーライドされる)
 *      region - 対象の文字列
 *      array - 補完候補が入るベクター
 * return
 *      候補数
 */
int GetLine::makeTopCompletionList( const NnString &region , NnVector &array )
{
    return makeCompletionListCore( region , array );
}

/* 文字列 s1 と 文字列 s2 との共通部分の長さを得る */
static int sameLength(const char *s1 , const char *s2)
{
    int len=0;
    while( *s1 != '\0' && *s2 != '\0' ){
        if( isKanji(*s1 & 255) ){
            if( ! isKanji(*s2 & 255) )
                break;
            if( s1[0] != s2[0] || s1[1] != s2[1] )
                break;
            s1 += 2;
            s2 += 2;
            len += 2;
        }else{
            if( isKanji(*s2 & 255) )
                break;
            if( toupper(*s1 & 255) != toupper(*s2 & 255) )
                break;
            ++s1;
            ++s2;
            ++len;
        }
    }
    return len;
}

struct Completion {
    NnString word ;
    int at , size , n ;
    NnVector list ;

    /* ディレクトリ名等を含まないファイル名(リスト表示のために使う) */
    NnString &name_of(int n){
	return *(NnString*)((NnPair*)list.at(n))->second_or_first();
    }
    /* フルパス名(主に実際の補完操作のために使う) */
    NnString &path_of(int n){
        return *(NnString*)((NnPair*)list.at(n))->first();  
    }
    int maxlen();
};

/* 候補リストの最大サイズを得る */
int Completion::maxlen()
{
    int max=0;
    for(int i=0;i<n;i++){
        int temp=this->name_of(i).length();
	if( temp > max )
            max = temp;
    }
    return max;
}


/* カーソル位置の文字列を読みとって、候補を挙げる.
 *      r - 補完状況
 * return
 *    >=0 : 候補数
 *    -1  : 単語がない.
 */
int GetLine::read_complete_list( Completion &r )
{
    r.word=current_word( r.at , r.size );
    if( r.word.empty()  && properties.get("nullcomplete") == NULL )
	return -1;

    r.n = 0;
    NyaosLua L("complete");
    if( L.ok() && lua_isfunction(L,-1) ){
        /* 第一引数：ベース文字列 */
        NnString word( r.word );
        word.dequote();
        lua_pushstring( L , word.chars() );

        /* 第二引数：文字列開始位置 */
        lua_pushinteger( L , r.at );

        /* 第三引数：補足情報 */
        lua_newtable( L );
        lua_pushstring( L , buffer.chars() ); 
        lua_setfield( L , -2 , "text" );
        lua_pushinteger( L , pos );
        lua_setfield( L , -2 , "cursor" );

        if( lua_pcall(L,3,1,0) == 0 ){
            if( lua_istable(L,-1) ){
                lua_pushnil(L);
                while( lua_next(L,-2) != 0 ){
                    const char *s=lua_tostring(L,-1);
                    if( s != NULL ){
                        if( strnicmp(word.chars(),s,word.length())==0 ){
                            r.list.append( new NnPair(new NnStringIC(s)) );
                        }
                    }else if( lua_istable(L,-1) ){
                        /* Full Path */
                        lua_pushinteger(L,1); /* +1 */
                        lua_gettable(L,-2);
                        lua_pushinteger(L,2); /* +2 */
                        lua_gettable(L,-3);
                        const char *fullpath = lua_tostring(L,-2);
                        const char *purename = lua_tostring(L,-1);
                        if( purename == NULL ){
                            purename = fullpath;
                        }
                        if( fullpath != NULL ){
                            if( strnicmp( word.chars() , fullpath , word.length()) == 0 ){
                                r.list.append( new NnPair(
                                            new NnStringIC(fullpath) ,
                                            new NnStringIC(purename) ) );
                            }
                        }
                        lua_pop(L,2);
                    }
                    lua_pop(L,1); /* drop each value(nextを使う時の約束事) */
                }
            }else if( ! lua_isnil(L,-1) ){
                r.list.append( new NnPair( new NnString(
                                "<nyaos.complete was requires "
                                "returning one table>")));
            }
            lua_pop(L,1); /* drop returned table */
        }else{
            r.list.append( new NnPair(new NnString(lua_tostring(L,-1))) );
            lua_pop(L,1); /* drop error message */
        }
    }else{
        // 補完リスト作成
        if( r.at == 0 ){
            makeTopCompletionList( r.word , r.list );
        }else{
            makeCompletionList( r.word , r.list );
        }
    }
    return r.n = r.list.size();
}

/* カーソル位置～size文字前までの部分の置換 & 再表示を行う。
 *	size - 置換元サイズ
 *	match - 置換文字列
 * 条件
 * ・size < match.length() である必要がある。
 * ・カーソルは at+size の位置にあることが必要
 */
void GetLine::replace_repaint_here( int size , const NnString &match )
{
    int at=pos-size;
    buffer.replace( at, size , match.chars() );
    int oldpos=pos;
    pos += match.length() - size ;

    // 表示の更新.
    if( pos >= offset+width ){
        // カーソル位置がウインドウ外にあるとき.
        putbs( oldpos-offset );
        offset = pos-width;
        puts_between(offset,pos);
    }else if( at < offset ){
        putbs( oldpos-offset );
        puts_between(offset,pos);
    }else{
        putbs( oldpos-at );
        puts_between(at,pos);
    }
    repaint_after( size <= match.length() ? 1 : size-match.length() );
}


/* 補完キー 対応メソッド
 */
Status GetLine::complete(int)
{
    Completion comp;
    int i,n,hasSpace=0;

    if( (n=read_complete_list(comp)) <= 0 )
	return NEXTCHAR;
    
    if( comp.word.findOf(" \t\r\n!") != -1 )
	hasSpace = 1;

    // 補完候補の第一候補をとりあえずバッファへコピー.
    NnString match=comp.path_of(0);
    if( strchr( match.chars() , ' ' ) != NULL || comp.word.at(0) == '"' )
	hasSpace = 1;

    // 候補が複数ある場合は、
    // 二番目以降の候補と比較してゆき、共通部分だけを残してゆく.
    for( i=1; i < n ; ++i ){
	if( strchr( comp.path_of(i).chars() , ' ' ) != NULL )
	    hasSpace = 1;
	match.chop( sameLength( comp.path_of(i).chars() , match.chars() )); 
    }

    // (基本的にありえないが)、補完候補の方が、元文字列より短い場合は
    // 何もせず終了する。
    if( match.length() < comp.size && ! hasSpace )
	return NEXTCHAR;
    
    // ワイルドカード補完したときに、候補の共通部が全くない場合、
    // 引用符だけになるのを防ぐ…
    if( match.length() == 0 )
	return NEXTCHAR;
    
#ifdef NYACUS
    /* ショートカット展開 */
    if( n==1 && comp.path_of(0).iendsWith(".lnk") && properties.get("lnkexp") != NULL )
    {
	/* 候補が一つで、それがショートカットの場合 */

	char buffer[ FILENAME_MAX ];
	if( read_shortcut( comp.path_of(0).chars() , buffer , sizeof(buffer)-1 ) == 0 ){
	    comp.path_of(0) = buffer;

	    NnDir stat( comp.path_of(0) );
	    if( stat.isDir() ){
		comp.path_of(0) << '\\';
	    }
	    match = comp.path_of(0);
	}
    }
#endif

    // 空白を含むとき、先頭に”を入れる。
    if( hasSpace ){
	if( match.at(0)=='~' ){
	    for(int i=1; match.at(i) != '\0' ; ++i ){
		if( match.at(i) == '\\' || match.at(i) == '/' ){
		    match.insertAt(i+1,"\"");
		    break;
		}
	    }
	}else{
	    match.unshift( '"' );
	}
    }

    if( n==1 && match.length() > 0 ){ // 候補が一つしかない場合は…
	int tail=match.lastchar();
	// 空白がある場合は、引用符を閉じる。
	if( hasSpace )
	    match += '"';
	if( tail != '\\' && tail != '/' ){
	    // 非ディレクトリの場合は、末尾に空白を入れる。
	    match += ' ';
        }else{
            currKey = -1;
        }
    }
    replace_repaint_here( comp.size , match );
    return NEXTCHAR;
}

void GetLine::listing_( Completion &comp )
{
    int maxlen = comp.maxlen();

    int nx= (80/(maxlen+1));
    if( nx <= 1 )
	nx = 1;
    int n=comp.n;
    int ny= (n+nx-1) / nx;
    NnString *prints=new NnString[ ny ];
    if( prints == NULL )
        return ;
    
    int i=0;
    for(int x=0;x<nx;x++){
        for(int y=0;y<ny;y++){
            if( i >= n )
               goto exit;
            prints[ y ] += comp.name_of(i);
            for( int j=comp.name_of(i).length() ; j<maxlen+1 ; j++ ){
                prints[ y ] << ' ';
            }
            i++;
        }
    }
  exit:
    putchr('\n');
    for(i=0;i<ny;i++){
        prints[i].trim();
        for(int j=0 ; j<prints[i].length() ; j++ )
            putchr( prints[i].at(j) );
        if( prints[i].length() > 0 )
            putchr('\n');
    }
    delete [] prints;
    prompt();
    /*DEL*puts_between( offset , buffer.length() ); *2004.10.17*/
    puts_between( offset , pos );
    repaint_after();
}

Status GetLine::listing(int)
{
    // 被補完文字列を抜き出す.
    Completion comp;

    if( read_complete_list(comp) <= 0 )
	return NEXTCHAR;
  
    comp.list.sort();
    listing_( comp );
  
    return NEXTCHAR;
}

Status GetLine::complete_prev(int)
{ return complete_vzlike(-1); }

Status GetLine::complete_next(int)
{ return complete_vzlike(+1); }


enum{
    KEY_ALT_RETURN    = 0x11c ,
    KEY_ALT_BACKSPACE = 0x10e ,
    KEY_RIGHT         = 0x14d ,
    KEY_LEFT          = 0x14b ,
    KEY_DOWN          = 0x150 ,
    KEY_UP            = 0x148 ,
};
#define CTRL(x) ((x) & 0x1F)

Status GetLine::complete_vzlike(int direct)
{
    Completion comp;
    if( read_complete_list( comp ) <= 0 )
	return NEXTCHAR;
    
    int i=0,key=0,bs=0;
    if( comp.list.size() >= 2 ){
	comp.list.sort();

	if( direct < 0 )
	    i = comp.list.size()-1;
	int baseSize = comp.word.length();
	if( comp.word.at(0) == '"' )
	    baseSize--;
	
	if( pos - baseSize < offset ){
	    /* 単語の先頭が画面端より前になってしまっている */
	    bs = pos-offset ;
	    /* 画面端までカーソルを戻す */
	}else{
	    /* 単語の先頭は画面上に見えている */
	    bs = baseSize;
	    /* 単語端までカーソルを戻す */
	}
	putbs( bs );
	pos      -= bs ;
	baseSize -= bs ;
	
	int sealSize=0;
	int maxlen1=comp.maxlen();
	for(;;){
	    NnString seal;
	    seal << (comp.path_of(i).chars() + baseSize );
	    for(int j=comp.path_of(i).length() ; j<maxlen1+1 ; ++j ){
		seal << ' ';
	    }
	    seal << '[';
	    seal.addValueOf( i+1 ) << '/' ;
	    seal.addValueOf( comp.list.size() ) << ']' ;
	    sealSize = printSeal( seal.chars() , sealSize );

	    key=getkey();
	    if(    which_command(key) == &GetLine::complete_next 
		|| which_command(key) == &GetLine::next 
		|| which_command(key) == &GetLine::vz_next ){
		/* 次候補 */
		if( ++i >= comp.list.size() ) 
		    i=0;
	    }else if(  which_command(key) == &GetLine::complete_prev 
		    || which_command(key) == &GetLine::previous
		    || which_command(key) == &GetLine::vz_previous ){
		/* 前候補 */
		if( --i < 0 )
		    i=comp.list.size()-1;
	    }else if(   which_command(key) == &GetLine::erase_all 
		     || which_command(key) == &GetLine::abort ){
		/* キャンセル */
		eraseSeal( sealSize );
		/* カーソル位置を戻す */
		while( bs-- > 0 )
		   putchr( buffer[pos++] );
		return NEXTCHAR;
	    }else if(   which_command(key) == &GetLine::erase_or_listing
		     || which_command(key) == &GetLine::listing 
		     || which_command(key) == &GetLine::complete_or_listing ){
		/* リスト出力 */
		listing_( comp );
		sealSize = 0;
		continue;
	    }else{ /* 確定 */
		break;
	    }
	}
	eraseSeal( sealSize );
    }

    NnString buf;
    if( comp.path_of(i).findOf(" \t\a\r\n\"") >= 0 ){
	buf << '"' << comp.path_of(i) << "\"";
    }else{
	buf << comp.path_of(i);
    }
    while( bs-- > 0 ){
	putchr(buffer[pos++]);
    }

    /* size文字列前～カーソル位置までを置換してくれる */
    replace_repaint_here( comp.size , buf );
    if( which_command(key) != &GetLine::enter )
	return interpret(key);

    return NEXTCHAR;
}

/* タブキーの処理。補完するか、リストを出す。
 */
Status GetLine::complete_or_listing(int key)
{
    if( lastKey == key )
        return this->listing(key);
    else
        return this->complete(key);
}

