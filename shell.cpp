#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifndef __EMX__
#  include <dir.h>
#endif
#include <errno.h>

#include "nnvector.h"
#include "nnstring.h"
#include "nnhash.h"
#include "nndir.h"
#include "shell.h"
#include "getline.h"
#include "nua.h"
#include "mysystem.h"
#include "ntcons.h"

// #define TRACE 1

extern NnHash properties;

/* エイリアスを保持するハッシュテーブル */
NnHash aliases;
NnHash functions;

class NyadosCommand : public NnObject {
    int (*proc)( NyadosShell &shell , const NnString &argv );
public:
    NyadosCommand( int (*p)(NyadosShell &,const NnString &) ) : proc(p) {}
    int run(NyadosShell &s,const NnString &argv){ return (*proc)(s,argv);  }
};

int preprocessHistory( History &hisObj , const NnString &src , NnString &dst );
int which( const char *nm, NnString &which );
void brace_expand( NnString &s );

void NnStrFilter::operator() ( NnString &target )
{
    NnString result;

    NnString::Iter itr(target);
    this->filter( itr , result );
    target = result;
}

static void semi2spc( NnString::Iter &it , NnString &value )
{
    NnString temp;
    int has_spc=0;
    for(; *it ; ++it ){
	if( *it == ';' ){
	    if( has_spc ){
		value << '"' << temp << '"' ;
	    }else{
		value << temp ;
	    }
	    value << ' ';
	    temp.erase() ; has_spc = 0;
	}else{
	    if( it.space() )
		has_spc = 1;
	    temp << *it;
	}
    }
    if( has_spc ){
	value << '"' << temp << '"';
    }else{
	value << temp ;
    }
}

int VariableFilter::lookup( NnString &name_ , NnString &value )
{
    /* 変数名から .length や .suffix などの拡張部分を取得する */

    NnString name,from,to;
    name_.splitTo(name,from,"/");
    from.splitTo(from,to,"/");

    NnString base,suffix;
    for( NnString::Iter it(name) ; *it ; ++it ){
	if( *it == '.' ){
	    if( base.length() > 0 )
		base << '.';
	    base << suffix;
	    suffix.erase();
	}else{
	    suffix << *it;
	}
    }
    if( base.length() <= 0 ){
	base = suffix;
	suffix.erase();
    }

    NnString lowname(base) , upname(base) ;
    lowname.downcase(); upname.upcase();

    
    NnString *s = (NnString*)properties.get(lowname) ;
    if( s != NULL ){
	/* オプション定義されていた場合 */
	if( suffix.compare("defined") == 0 ){
	    value = "1";
	}else if( suffix.compare("length") == 0 ){
	    value.addValueOf( s->length() );
	}else if( suffix.compare("split") == 0 ){
	    NnString::Iter temp(*s);
	    semi2spc( temp , value );
	}else if( from.empty() ){
	    value = *s;
	}else{
            s->replace(from,to,value);
        }
	return 0;
    }
    const char *p=getEnv(upname.chars(),NULL);
    if( p != NULL ){
	/* 環境変数定義されている時 */
	if( suffix.compare("defined") == 0 ){
	    value = "2";
	}else if( suffix.compare("length") == 0 ){
	    value.addValueOf( strlen(p) );
	}else if( suffix.compare("split") == 0 ){
	    NnString::Iter temp(p);
	    semi2spc( temp , value );
	}else{
	    value = p;
            if( ! from.empty() ){
                value.replace(from,to,value);
            }
	}
	return 0;
    }
    /* 何も定義されていない時 */
    if( suffix.compare("defined") == 0 ){
	value = "0";
    }else if( suffix.compare("length") == 0 ){
	value = "0";
    }else if( suffix.compare("split") == 0 ){
	value ="";
    }else{
	return 1;
    }
    return 0;
}

/* %0 %1 %2 などの引数展開用 */
void VariableFilter::cnv_digit( NnString::Iter &p , NnString &result )
{
    const NnString *value;
    NnString orgstr("%");
    int n=0;

    do{
        orgstr << *p;
	n = n*10 + (*p-'0');
	++p;
    }while( p.digit() );

    if( (value=shell.argv(n)) != NULL )
	result << *value;
    else
        result << orgstr;
}

/* %* $* などの展開用 */
void VariableFilter::cnv_asterisk( NnString::Iter &p , NnString &result )
{
    for(int j=1;j< shell.argc();j++){
	const NnString *value=shell.argv(j);
	if( value != NULL )
	    result << *value << ' ';
    }
    ++p;
}

void VariableFilter::filter( NnString::Iter &p , NnString &result )
{
    int quote=0;
    while( *p ){
	if( *p == '"' ){
	    quote ^= 1;
	    result << *p;
	    ++p;
	}else if( *p == '%' ){
	    NnString name , value;
	    ++p;
	    if( p.digit() ){
		cnv_digit( p , result );
	    }else if( *p == '*' ){
		cnv_asterisk( p , result );
            }else if( *p == '%' ){
                result << '%';
                ++p;
	    }else{
		for(; *p != 0 && *p != '%' ; ++p )
		    name << *p;
		if( lookup(name,value)==0 ){
		    result << value;
		}else{
		    result << '%' << name << *p;
		}
		++p;
	    }
	}else if( *p == '$' && !quote ){
	    NnString name , value;
	    ++p;
	    if( p.digit() ){
		cnv_digit( p , result );
	    }else if( *p == '$' ){
		result << '$';
		++p;
	    }else if( *p == '*' ){
		cnv_asterisk( p , result );
	    }else if( *p == '{' ){
		while( ++p , *p && *p != '}' ){
		    name << *p ;
		}
		if( lookup(name,value)==0 ){
		    result << value;
		}else{
		    result << "${" << name << *p;
		}
		++p;
	    }else{
		for( ; p.alnum() || *p == '_' ; ++p )
		    name << *p;
		if( lookup(name,value)==0 ){
		    result << value;
		}else{
		    result << '$' << name ;
		}
	    }
	}else{
	    result << *p;
	    ++p;
	}
    }
}


/* コマンド名から関連付けコマンドを取得する。
 *      p - コマンド名
 * return
 *      実行コマンド(無い場合は NULL )
 */
static NnString *getInterpretor( const char *p )
{
    int lastdot=NnString::findLastOf(p,"/\\.");
    if( lastdot == -1  ||  p[lastdot] != '.' )
	return NULL;
    
    NnString suffix;
    NyadosShell::dequote( p+lastdot+1 , suffix );
    return (NnString*)DosShell::executableSuffix.get(suffix);
}

/* 空白の後の「-」を 「/」へ
 * 「/」を「\\」に置き換える.
 *	src (i) : 元文字列
 *	dst (i) : 変換後
 */
static void cnv_sla_opt( const NnString &src , NnString &dst )
{
    int spc=1;
    for( NnString::Iter p(src) ; *p ; ++p ){
	if( spc && *p == '-' ){
	    dst << '/';
	}else if( *p == '/' ){
	    dst << '\\';
	    if( p.space() || *p =='\0' )
		dst << '.';
	}else if( *p == '\\' && (isspace(*p.next) || *p.next=='\0' ) ){
	    dst << "\\.";
	}else{
	    dst << *p;
	}
	spc = p.space();
    }
}

/* $n を展開する。
 *      p       - '$' の位置
 *      param   - 引数リスト($1などの元ネタ)
 *      argv    - 引数を全て連結したもの
 *      replace - 結果
 */
static void dollar( const char *&p , const NnVector &param ,
                    const NnString &argv , NnString &replace )
{
    switch( p[1] ){
    default:           replace << '$';  ++p;    break;
    case '*':          replace << argv; p += 2; break;
    case '$':          replace << '$';  p += 2; break;
    case 't':case 'T': replace << " ;"; p += 2; break;
    case 'b':case 'B': replace << '|';  p += 2; break;
    case 'g':case 'G': replace << '>';  p += 2; break;
    case 'l':case 'L': replace << '<';  p += 2; break;
    case 'q':case 'Q': replace << '`';  p += 2; break;
    case 'p':case 'P': replace << '%';  p += 2; break;
    case '@': cnv_sla_opt(argv,replace);p += 2; break;

    case '0':case '1':case '2':case '3':case '4':
    case '5':case '6':case '7':case '8':case '9':
	/* $1,$2 など */
	int n=(int)strtol(p+1,(char**)&p,10); 
	const NnString *s=(const NnString*)param.const_at(n);
	if( s != NULL )
	    replace << *s;
	if( *p == '*' ){
	    /* 「$1*」など */
	    while( ++n < param.size() )
		replace << ' ' << *(const NnString*)param.const_at(n);
	    ++p ;
	}
	break;
    }
}

Status OneLineShell::readline( NnString &buffer )
{
    if( cmdline.empty() )
	return TERMINATE;
    buffer = cmdline;
    cmdline.erase();
    return NEXTLINE;
}

int OneLineShell::operator !() const
{
    if( cmdline.empty() )
	return !*parent_;
    return 0;
}

/* エイリアス用に $ 引数を置換する
 *    aliasValue - エイリアスの定義文字列
 *    param   - コマンドラインのパラメータ配列
 *    argv    - 分割前の param
 *    replace - 置換結果を格納するバッファ
 * return
 *    1 - $ による置換があった
 *    0 - $ による置換はなかった
 */
static int replaceDollars( const NnString *aliasValue ,
			    const NnVector &param ,
			    const NnString &argv ,
			    NnString &replace )
{
    int dollarflag=0;
    for( const char *p=aliasValue->chars() ; *p != '\0' ; ){
	if( *p == '$' ){
	    dollar( p , param , argv , replace );
	    dollarflag = 1;
	}else{
	    replace << *p++ ;
	}
    }
    return dollarflag;
}

int sub_brace_start( NyadosShell &bshell , 
		     const NnString &arg1 ,
		     const NnString &argv );
int sub_brace_erase( NyadosShell &bshell , 
		     const NnString &arg1 );

/* コマンドラインフィルター(フック箇所が複数あるので関数化)
 *    hookname - フック名 "filter" "filter2" 
 *    source - フィルター前文字列
 *    result - フィルター結果格納先
 */
static void filter_with_lua(
        const char *hookname,
        const NnString &source,
        NnString &result)
{
    result = source ;
    LuaHook L( hookname );
    while( L.next() ){
        lua_pushstring(L,result.chars() );
        if( lua_pcall(L,1,1,0) != 0 )
            return;
        if( lua_isstring(L,-1) )
            result = lua_tostring(L,-1); 
        lua_pop(L,1); /* drop return value */
    }
}


/* Lua関数の戻り値を適当に、整数戻り値にする。
 *     nil   ⇒ 0 (成功扱い)
 *     true  ⇒ 0 (成功扱い)
 *     false ⇒ 1 (エラー扱い)
 *     整数       (そのまま)
 *     その他 ⇒  (成功扱い)
 */
static int lua2exitStatus( lua_State *L )
{
    if( lua_isnumber(L,-1) ){
        return lua_tointeger(L,-1);
    }else if( lua_isnil(L,-1) ){
        return 0;
    }else if( lua_isboolean(L,-1) ){
        return !lua_toboolean(L,-1);
    }else{
        return 0;
    }
}

void NyadosShell::setExitStatus(int n)
{
    exitStatus_ = n;

    NnString *errorlevel=new NnString();
    errorlevel->addValueOf( exitStatus_ );
    properties.put( "errorlevel" , errorlevel );
}

/* nyaos.command2 に定義された Lua コマンドに合致すれば呼び出す
 *    cmdname - コマンド名
 *    params  - パラメータ文字列
 *    status  - 戻り値を格納する変数
 * return
 *    1 - 呼び出した
 *    0 - 合致するコマンドなし
 */
static int call_nyaos_command2( const char *cmdname , const char *params , int &status )
{
    NyaosLua L("command2");
    if( L.ok() && lua_istable(L,-1) ){
        lua_getfield(L,-1,cmdname );
        if( ! lua_isfunction(L,-1) )
            return 0;

        lua_pushstring(L,params);
        if( lua_pcall(L,1,1,0) != 0 ){
            const char *msg = lua_tostring( L , -1 );
            conErr << msg << '\n';
        }
        status = lua2exitStatus(L);
        return 1;
    }else{
        return 0;
    }
}

/* nyaos.command に定義された Lua コマンドに合致すれば呼び出す
 *    cmdname - コマンド名
 *    params  - パラメータ文字列
 *    status  - 戻り値を格納する変数
 * return
 *    1 - 呼び出した
 *    0 - 合致するコマンドなし
 *   -1 - エラー
 */
static int call_nyaos_command( const char *cmdname , const char *params , int &status )
{
    NyaosLua L("command");
    if( L.ok() && lua_istable(L,-1) ){
        lua_getfield(L,-1,cmdname);
        if( ! lua_isfunction(L,-1) ){
            return 0;
        }
        NnString argv2;
        NnVector argv3;
        int back_in , back_out , back_err;

        if( NyadosShell::explode4internal( params , argv2 ) != 0 )
            return -1;

        argv2.splitTo(argv3);
        if( lua_checkstack( L , argv3.size()) == 0 ){
            conErr << "Too many parameter for Lua stack.\n";
            return -1;
        }

        for(int i=0 ; i<argv3.size() ; i++){
            ((NnString*)argv3.at(i))->dequote();
            lua_pushstring( L , argv3.at(i)->repr() );
        }

        redirect_emu_to_real( back_in , back_out , back_err );

        if( lua_pcall(L,argv3.size(),1,0) != 0 ){
            const char *msg = lua_tostring( L , -1 );
            conErr << msg << '\n';
        }
        status = lua2exitStatus(L);
        redirect_rewind( back_in , back_out , back_err );
        return 1;
    }else{
        return 0;
    }
}


/* 「 ;」等で区切られた後の１コマンドを実行する。
 * (interpret1 から呼び出されます)
 *	replace - コマンド
 *	wait != 0 コマンド終了を待つ
 *	     == 0 コマンドをバックグラウンドで実行させる
 * return
 *      0
 */
int NyadosShell::interpret2( const NnString &replace_ , int wait )
{
    int rv=0;
    NnExecutable *func;
    NnString replace;

    filter_with_lua( "filter2" , replace_ , replace );

    VariableFilter variable_filter( *this );
    NyadosCommand *cmdp = NULL;

    /* 標準入出力のセーブ */
    Writer *save_out = conOut_;
    Writer *save_err = conErr_;
    Reader *save_in  = conIn_;

    /* 環境変数展開 */
    variable_filter( replace );
    NnString arg0 , argv , arg0low ;
    replace.splitTo( arg0 , argv );

    /* 関数定義文 */
    if( arg0.endsWith("{") ){
	sub_brace_start( *this , arg0 , argv );
        goto exit;
    }
    if( arg0.endsWith("{}") && arg0.length() >=3 ){ /* 関数削除 */
        sub_brace_erase( *this , arg0 );
        goto exit;
    }

    arg0low = arg0;
    arg0low.downcase();
    if( (func=(NnExecutable*)functions.get(arg0)) != NULL ){
        /* サブシェルを実行する */
        NnVector param;
        param.append( arg0.clone() );
        argv.splitTo( param );
        (*func)( param );
    }else if( call_nyaos_command2( arg0low.chars() , argv.chars() , rv )){
        setExitStatus( rv );
    }else if( call_nyaos_command( arg0low.chars() , argv.chars() , rv )){
        setExitStatus( rv );
    }else if( (cmdp=(NyadosCommand*)command.get( arg0low )) != NULL ){
    /* 内蔵コマンドを実行する */
        NnString argv2;
        if( explode4internal( argv , argv2 ) != 0 )
            goto exit;

        int rc=cmdp->run( *this , argv2  );

        if( rc != 0 ){
            rv = -1;
            goto exit;
        }
    }else{
        /* 外部コマンドを実行する */
        NnString cmdline2;
        if( explode4external( replace , cmdline2 ) != 0 )
            goto exit;

        int rc = mySystem( cmdline2.chars() , wait );
        if( wait == 0 ){
            conErr << '<' << rc << ">\n";
        }else{
            setExitStatus( rc );
        }
    }
  exit:
    if( ! heredocfn.empty() ){
	::remove( heredocfn.chars() );
	heredocfn.erase();
    }
    // 標準出力/エラー出力/入力を元に戻す.
    if( conOut_ != save_out ){
        delete conOut_;
        conOut_ = save_out;
    }
    if( conErr_ != save_err ){
        delete conErr_;
        conErr_ = save_err;
    }
    if( conIn_ != save_in ){
        delete conIn_;
        conIn_  = save_in;
    }
    return rv;
}

/* 文字列中の " の数を数える */
static int countQuote( const char *p )
{
    int qc=0;
    bool escape=false;
    bool quote =false;
    for( ; *p != '\0' ; ++p ){
	if( *p == '"' && !escape ){
	    ++qc;
            quote = !quote;
        }
        escape = (!escape && !quote && *p=='^');
    }
    return qc;
}

static bool isEscapeEnd( const char *p )
{
    bool escape = false;
    while( *p != '\0' ){
        escape = (!escape && *p == '^');
        ++p;
    }
    return escape;
}



/* ソースから、１コマンドずつ切り出す。
 * 切り出した内容は基本的にヒストリ置換されるだけで、他の加工はない。
 *    buffer - 取り出したコマンドを入れるバッファ。
 */
Status NyadosShell::readcommand( NnString &buffer )
{
    buffer.erase();
    if( current.empty() ){
	NnString temp;
	Status rc;

	/* 最初の１行取得 */
	for(;;){
	    rc=readline( current );
	    if( rc == TERMINATE || rc == CANCEL )
		return rc;
            NnString current_ = current;
            current_.trim();
	    if( current_.length() > 0 && ! current_.startsWith("#") )
		break;
	    /* コメント行の場合は、もう一度取得しなおし */
	    current.erase();
	}
	for(;;){
	    if( rc == CANCEL ){
                current.erase();
		return CANCEL;
	    }else if( rc == TERMINATE ){
                return TERMINATE;
            }
	    if( properties.get("history") != NULL ){
		History *hisObj =this->getHistoryObject();
		int isReplaceHistory=1;

		if( hisObj == NULL  &&  parent_ != NULL ){
		    hisObj = parent_->getHistoryObject();
		    isReplaceHistory = 0;
		}

		/* ヒストリが存在すれば、ヒストリ置換を行う */
		if( hisObj != NULL ){
		    NnString result;
		    if( preprocessHistory( *hisObj , current , result ) != 0 ){
			fputs( result.chars() , stderr );
			if( isReplaceHistory )
			    hisObj->drop(); // 入力自体を削除する.
			return NEXTLINE;
		    }
		    if( isReplaceHistory ){
			hisObj->set(-1,result);
			hisObj->pack();
		    }
		    current = result;
		}
		
	    }
	    if( properties.get("bracexp") != NULL )
		brace_expand( current );
		
	    if( countQuote(current.chars()) % 2 != 0 ){
		nesting.append(new NnString("quote>"));
	    }else if( isEscapeEnd(current.chars() )){
		nesting.append(new NnString("more>"));
		current.chop();
	    }else{
		break;
	    }
	    /* 継続行があるのであれば、続いて取得
	     * (継続行には、コメントはありえないものとする)
	     */
	    temp.erase();
	    rc = readline( temp );
	    delete nesting.pop();
	    temp.trim();
	    current << '\n' << temp;
	}
    }
    int quote=0;
    int i=0;
    int prevchar=0;
    for(;;){
	if( i>=current.length() ){
	    current.erase();
	    break;
	}
        if( !quote ){
            /* コマンドターミネータ「 ;」があった! */
            if( !quote  && isSpace(current.at(i)) && current.at(i+1)==';' ){
                current = current.chars()+i+2;
                break;
            }
            /* コマンドターミネータ「&」があった! */
            if( !quote  && prevchar != '>' && prevchar != '|' && current.at(i)=='&' ){
                if( i+1 < current.length() && current.at(i+1) == '&' ){
                    buffer << "&&";
                    i+=2;
                    continue;
                }
                if( i+1>=current.length() || current.at(i+1) != '&'  ){
                    buffer << '&'; // フラグの意味でわざと残しておく...
                    current = current.chars()+i+1;
                    break;
                }
            }
        }
	if( current.at(i) == '"' ){
	    quote ^= 1;
	}
	if( isKanji(current.at(i)) ){
	    buffer << (char)(prevchar = current.at(i++));
	    buffer << (char)current.at(i++);
	}else{
	    buffer << (char)(prevchar = current.at(i++));
	}
    }
    return NEXTLINE;
}

/* which のラッパー：パスに空白文字が含まれている場合、引用符で囲む。
 *	fn : ファイル名
 *	fullpath : フルパス名
 * return
 *	0: 成功 , !0 : 失敗
 */
static int which_pathquote( const char *fn , NnString &fullpath )
{
    NnString fn_;
    NyadosShell::dequote( fn , fn_ );
    int rc=which(fn_.chars(),fullpath);
    if( rc==0  &&  fullpath.findOf(" \t") >= 0 ){
	fullpath.insertAt(0,"\"");
	fullpath << '"';
    }
    return rc;
}

/* インタープリタ名挿入
 *	 script - スクリプト名
 *	 argv   - 引数(スクリプト名より後のパラメータ)
 *       buffer - 挿入先バッファ
 * return
 *       0 - 成功 , !0 - 失敗(replace 変更なし)
 */
static int insertInterpreter( const char *script ,
			      const NnString &argv ,
			      NnString &buffer )
{
    NnString *intnm=getInterpretor( script );
    if( intnm == NULL )
	return 1; /* 拡張子該当なし */

    /* スクリプトのフルパスを取得する */
    NnString path;
    if( which_pathquote(script,path) != 0 ){
        path = script;
    }
    /* $0 ～ $9 を作っておく */
    NnVector param;
    param.append( path.clone() );
    argv.splitTo( param );

    if( replaceDollars( intnm , param , argv , buffer ) == 0 ){
	buffer << ' ' << path << ' ' << argv;
    }
    return 0;
}

/* 一行インタープリタ.
 * ・引数文字列(コマンド行)を ; で分割し、おのおのを interpret2 で処理する.
 *      statement : コマンド行
 * return
 *      0 継続 -1 終了
 */
int NyadosShell::interpret1( const NnString &statement )
{
    NnString cmdline;

    filter_with_lua( "filter" , statement , cmdline );

    errno = 0;
    cmdline.trim();
    if( cmdline.length() <= 0 )
        return 0;
    /* コメント処理は readcommand 関数レベルで処理済み */

    if( cmdline.length()==2 && cmdline.at(1)==':' && isalpha(cmdline.at(0) & 0xFF) ){
	NnDir::chdrive( cmdline.at(0) );
        NnString title("NYAOS - ") , currdir;
        NnDir::getcwd( currdir );
        title << currdir ;
        Console::setConsoleTitle( title.chars() );
        return 0;
    }else if( cmdline.at(0) == ':' ){
        NnString label( cmdline.chars()+1 );

        label.trim();
        label.downcase();
        if( label.length() > 0 )
            setLabel( label );
        return 0;
    }

    int wait=1;
    NnString arg0,arg0low;
    NnString argv;
    NnString replace;
    if( cmdline.endsWith("&") ){
        wait = 0 ;
	cmdline.chop();
    }

    NnString rest(cmdline);
    while( ! rest.empty() ){
	NnString cmds;
	int dem=rest.splitTo(cmds,rest,"|","\"`");
	if( rest.at(0) == '&' ){
	    /* |& に対する対応 */
	    rest.shift();
	    cmds << " 2>&1";
	}

	cmds.splitTo( arg0 , argv );
	arg0.slash2yen();
        arg0low = arg0;
        arg0low.downcase();
        
	NnString *aliasValue=(NnString*)aliases.get(arg0low);
        if( aliasValue != NULL ){// エイリアスだった。
            /* パラメータを分解する */
            NnVector param;
            NnString buffer;

            param.append( arg0.clone() );
	    argv.splitTo( param );
	    if( replaceDollars( aliasValue , param , argv , buffer ) == 0 ){
	       buffer << ' ' << argv;
	    }

            buffer.splitTo( arg0 , argv );
            arg0.slash2yen();
            arg0low = arg0;
            arg0low.downcase();
	}
        if( insertInterpreter(arg0low.chars(),argv,replace) != 0 )
            replace << arg0 << ' ' << argv ;
	
	if( dem == '|' )
	    replace << "|";
    }
    return this->interpret2( replace  , wait );
}

int NyadosShell::interpret( const NnString &statement )
{
    const char *p=statement.chars();
    NnString replace;
    int quote=0;

    while( *p != '\0' ){
        if( quote == 0 ){
            if( p[0] == '&' && p[1] == '&' ){
                int result = this->interpret1( replace );
                if( this->exitStatus() != 0 ){
                    return result;
                }
                replace.erase();
                p += 2;
                continue;
            }
            if( p[0] == '|' && p[1] == '|' ){
                int result = this->interpret1( replace );
                if( this->exitStatus() == 0 ){
                    return result;
                }
                replace.erase();
                p += 2;
                continue;
            }
        }
        if( *p == '"' ){
            quote ^= 1;
        }
        if( *p == '^' && (
            (*(p+1) == '&' && *(p+2) == '&' ) ||
            (*(p+1) == '|' && *(p+2) == '|' ) ) )
        {
            replace << '^' << *(p+1) << *(p+2);
            p += 3;
            continue;
        }
        if( isKanji(*p) )
            replace << *p++;
        replace << *p++;
    }
    return this->interpret1( replace );
}


/* シェルのメインループ。
 *	シェルコマンドの取得先は readline関数として仮想化してある。
 * return
 *	0:成功 , -1:ネストし過ぎ
 */
int NyadosShell::mainloop()
{
    static int nesting=0;

    if( nesting >= MAX_NESTING ){
        conErr << "!!! too deep nesting shell !!!\n";
	return -1;
    }

    ++nesting;
	NnString cmdline;
	do{
	    cmdline.erase();
	}while( readcommand(cmdline) != TERMINATE && interpret(cmdline) != -1 );
    --nesting;
    return 0;
}

int cmd_ls      ( NyadosShell & , const NnString & );
int cmd_bindkey ( NyadosShell & , const NnString & );
int cmd_endif   ( NyadosShell & , const NnString & );
int cmd_else    ( NyadosShell & , const NnString & );
int cmd_if      ( NyadosShell & , const NnString & );
int cmd_set     ( NyadosShell & , const NnString & );
int cmd_exit    ( NyadosShell & , const NnString & );
int cmd_source  ( NyadosShell & , const NnString & );
int cmd_echoOut ( NyadosShell & , const NnString & );
int cmd_chdir   ( NyadosShell & , const NnString & );
int cmd_goto    ( NyadosShell & , const NnString & );
int cmd_shift   ( NyadosShell & , const NnString & );
int cmd_alias   ( NyadosShell & , const NnString & );
int cmd_unalias ( NyadosShell & , const NnString & );
int cmd_suffix  ( NyadosShell & , const NnString & );
int cmd_unsuffix( NyadosShell & , const NnString & );
int cmd_open    ( NyadosShell & , const NnString & );
int cmd_option  ( NyadosShell & , const NnString & );
int cmd_unoption( NyadosShell & , const NnString & );
int cmd_history ( NyadosShell & , const NnString & );
int cmd_pushd   ( NyadosShell & , const NnString & );
int cmd_popd    ( NyadosShell & , const NnString & );
int cmd_dirs    ( NyadosShell & , const NnString & );
int cmd_foreach ( NyadosShell & , const NnString & );
int cmd_pwd     ( NyadosShell & , const NnString & );
int cmd_folder  ( NyadosShell & , const NnString & );
int cmd_lua_e   ( NyadosShell & , const NnString & );
int cmd_function_list( NyadosShell & , const NnString & );
int cmd_cls     ( NyadosShell & , const NnString & );

int cmd_eval( NyadosShell &shell , const NnString &argv )
{
    NnString tmp(argv);
    OneLineShell oShell(&shell,tmp);
    oShell.mainloop();
    return 0;
}

/* シェルのオブジェクトのコンストラクタ */
NyadosShell::NyadosShell( NyadosShell *parent )
{
    const static struct Command {
	const char *name;
	int (*proc)( NyadosShell & , const NnString & );
    } cmdlist[]={
	{ "alias"   , &cmd_alias    },
	{ "bindkey" , &cmd_bindkey  },
	{ "cd"	    , &cmd_chdir    },
        { "cls"     , &cmd_cls      },
	{ "dirs"    , &cmd_dirs     },
	{ "else"    , &cmd_else     },
	{ "endif"   , &cmd_endif    },
	{ "eval"    , &cmd_eval     },
	{ "exit"    , &cmd_exit     },
	{ "folder"  , &cmd_folder   },
	{ "foreach" , &cmd_foreach  },
	{ "goto"    , &cmd_goto     },
	{ "history" , &cmd_history  },
	{ "if"      , &cmd_if       },
	{ "ls"      , &cmd_ls       },
        { "lua_e"   , &cmd_lua_e    },
	{ "open"    , &cmd_open     },
	{ "option"  , &cmd_option   },
	{ "popd"    , &cmd_popd     },
	{ "echo"    , &cmd_echoOut  },
	{ "pushd"   , &cmd_pushd    },
	{ "pwd"     , &cmd_pwd      },
	{ "set"	    , &cmd_set      },
	{ "shift"   , &cmd_shift    },
	{ "source"  , &cmd_source   },
	{ "suffix"  , &cmd_suffix   },
	{ "unalias" , &cmd_unalias  },
	{ "unoption", &cmd_unoption },
	{ "unsuffix", &cmd_unsuffix },
	{ "{}"      , &cmd_function_list },
	{ 0 , 0 },
    };
    /* 静的メンバ変数を一度だけ初期化する */
    static int flag=1;
    if( flag ){
	flag = 0;
	for( const struct Command *ptr=cmdlist ; ptr->name != NULL ; ++ptr ){
	    NnString key( ptr->name );
	    key.downcase();
	    command.put( key , new NyadosCommand(ptr->proc) );

	    key.insertAt(0,"__");
	    key << "__";
	    command.put( key , new NyadosCommand(ptr->proc) );
	}
    }
    parent_ = parent ;
}
NnHash NyadosShell::command;

NyadosShell::~NyadosShell(){}
int NyadosShell::setLabel( NnString & ){ return -1; }
int NyadosShell::goLabel( NnString & ){ return -1; }
int NyadosShell::argc() const { return 0; }
const NnString *NyadosShell::argv(int) const { return NULL; }
void NyadosShell::shift(){}
