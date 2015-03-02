#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "nnstring.h"
#include "getline.h"
#include "shell.h"
#include "nua.h"
#include "ntcons.h"

int cmd_bindkey( NyadosShell &shell, const NnString &argv )
{
    static NnHash mapping;
    NnString keystr , left , funcname;

    argv.splitTo( keystr , left );
    if( keystr.empty() ){
	for( NnHash::Each e(mapping) ; e.more() ; ++e ){
	    conOut << e->key() << ' ' << *(NnString*)e->value() << '\n';
	}
        return 0;
    }
    while( left.splitTo(funcname,left) , !funcname.empty() ){
	switch( GetLine::bindkey( keystr.chars() , funcname.chars() ) ){
	case 0:
	    /* 成功した時は、リスト表示用のハッシュにも登録する */
	    keystr.upcase();
	    funcname.downcase();
	    mapping.put( keystr , new NnString(funcname) );
	    break;
	case -1:
	    conErr << "function name " << funcname << " was invalid.\n" ;
	    break;
	case -2:
	    conErr << "key number " << keystr << " was invalid.\n" ;
	    break;
	case -3:
	    conErr << "key name " << keystr << " was invalid.\n" ;
	    break;
	default:
	    conErr << "Unexpected error was occured.\n";
	    break;
	}
    }
    return 0;
}

int cmd_endif( NyadosShell &shell , const NnString & )
{
    if( shell.nesting.size() <= 0 ){
        conErr << "Unexpected endif.\n";
    }else{
        delete shell.nesting.pop();
    }
    return 0;
}

/* その行が endif キーワードを含むか判定する.
 *      line - 行
 * return
 *      not 0 … endif 節が含まれている
 *      0 … 含まれない
 */
static int has_keyword_endif( const NnString &line )
{
    return line.istartsWith("endif");
}

/* その行が else キーワードを含むか判定する.
 *      line - 行
 * return
 *      not 0 … else 節が含まれている
 *      0 … 含まれない
 */
static int has_keyword_else( const NnString &line )
{
    return line.istartsWith("else");
}

/* その行に if ～ then 節が入っているかどうか判定する.
 *      line - 行
 * return
 *      not 0 … if ～ then 節が含まれている
 *      0 … 含まれない
 */
static int has_keyword_if_then( const NnString &line )
{
    return line.istartsWith("if") && line.iendsWith("then");
}

int cmd_else( NyadosShell &shell , const NnString & )
{
    if( shell.nesting.size() <= 0 ){
        conErr << "Unexpected else.\n";
        return 0;
    }
    NnString *keyword=(NnString*)shell.nesting.pop();
    if( keyword == NULL ||
	(keyword->icompare( "then>"     ) !=0 &&
	 keyword->icompare( "skip:then>") !=0 ))
    {
	delete keyword;
        conErr << "Unexpected else.\n";
        return 0;
    }
    delete keyword;

    NnString line;
    int nest=0;
    shell.nesting.append( new NnString("skip:else>") );

    for(;;){
        line.erase();
        if( shell.readcommand(line) != NEXTLINE ){
	    delete shell.nesting.pop();
            return -1;
	}
        line.trim();
        if( has_keyword_endif(line) ){
            if( nest == 0 ){
		delete shell.nesting.pop();
                return 0;
            }
            --nest;
        }
        if( has_keyword_if_then(line) )
            ++nest;
    }
}

int cmd_if( NyadosShell &shell, const NnString &argv )
{
    NnString arg1 , left;
    const char *temp;

    int flag=0;
    argv.splitTo( arg1 , left );
    if( arg1.icompare("not")==0 ){
        flag = !flag;
        left.splitTo( arg1 , left );
    }
    if( arg1.icompare("exist")==0 ){
        /* exist 演算子 */
        left.splitTo( arg1 , left );
	NyadosShell::dequote( arg1 );
        if( NnDir::access( arg1.chars() )==0 )
            flag = !flag;
    }else if( arg1.icompare("errorlevel")==0 ){
        /* errorlevel 演算子 */
        left.splitTo( arg1 , left );
        if( shell.exitStatus() >= atoi( arg1.chars() ) )
            flag = !flag;
#if 0
    }else if( arg1.icompare("directory") == 0 ){
        left.splitTo( arg1 , left );
        struct stat statBuf;

        if(    ::stat((char far*)arg1.chars() , &statBuf)==0 
            && (statBuf.st_mode & S_IFMT)==S_IFDIR )
            flag = !flag;
#endif
    /* == 演算子だけは前後に空白のない使われ方がある */
    }else if( (temp=strstr(arg1.chars(),"==")) != NULL  ){
        NnString lhs( arg1.chars() , (size_t)(temp-arg1.chars()) );
        NnString rhs;
        if( temp[2]=='\0'){
            left.splitTo(rhs,left);
        }else{
            rhs = temp+2;
        }
        lhs.trim();

        if( lhs.compare(rhs) == 0 )
            flag = !flag;
    }else{
        /* == or != 演算子 */
        NnString lhs(arg1),rhs;

        left.splitTo(arg1,left);
        left.splitTo(rhs,left);

        if( arg1.compare("==")==0 || arg1.compare("=")==0 ){
            if( lhs.compare(rhs) == 0 )
                flag = !flag;
        }else if( arg1.startsWith("==") ){
            if( lhs.compare( arg1.chars()+2 ) == 0 )
                flag = !flag;
            rhs << ' ' << left;
            left = rhs;
        }else if( arg1.compare("!=")==0 ){
            if( lhs.compare(rhs) != 0 )
                flag = !flag;
        }else if( arg1.icompare("-ne")==0 ){
            if( atoi(lhs.chars()) != atoi(rhs.chars()))
                flag = !flag;
        }else if( arg1.icompare("-eq")==0 ){
            if( atoi(lhs.chars()) == atoi(rhs.chars()))
                flag = !flag;
        }else if( arg1.icompare("-lt")==0 ){
            if( atoi(lhs.chars()) < atoi(rhs.chars()))
                flag = !flag;
        }else if( arg1.icompare("-gt")==0 ){
            if( atoi(lhs.chars()) > atoi(rhs.chars()))
                flag = !flag;
        }else if( arg1.icompare("-le")==0 ){
            if( atoi(lhs.chars()) <= atoi(rhs.chars()))
                flag = !flag;
        }else if( arg1.icompare("-ge")==0 ){
            if( atoi(lhs.chars()) >= atoi(rhs.chars()))
                flag = !flag;
        }else{
            conErr << arg1 << ": Syntax Error\n";
            return 0;
        }
    }
    NnString then,then_left;
    left.splitTo( then , then_left );
    if( flag ){ /* 条件成立時 */
        if( stricmp( then.chars() , "then") != 0 )
            return shell.interpret( left );
        /* then 節の場合は、そのまま次の行へ処理を移行させる */
        shell.nesting.append( new NnString("then>") );
	if( ! then_left.empty() ){
	    shell.interpret( then_left );
	}
    }else{ /* 条件不成立時 */
        if( stricmp( then.chars() , "then") == 0 ){
            /* else までネストをスキップする。*/
            NnString line;
            int nest=0;
            shell.nesting.append( new NnString("skip:then>") );
            for(;;){
                line.erase();
                if( shell.readcommand( line ) != NEXTLINE ){
                    delete shell.nesting.pop();
                    return -1;
                }
                line.trim();
                if( has_keyword_endif(line) ){
                    if( nest == 0 ){
                        delete shell.nesting.pop();
                        return 0;
                    }
                    --nest;
                }else if( nest==0  &&  has_keyword_else(line) ){
                    delete shell.nesting.pop();
                    shell.nesting.append( new NnString("else>") );
		    NnString one1,left1;
		    line.splitTo(one1,left1);
		    if( ! left1.empty() ){
			shell.interpret(left1);
		    }
                    return 0;
                }else if( has_keyword_if_then(line) ){
                    ++nest;
                }
            }
        }
    }
    return 0;
}

/* 配列に、文字列を加える。同一文字列がある場合は追加しない.
 *      list - 配列
 *      ele - 文字列へのポインタ(所有権委譲)
 */
static void appendArray( NnVector &list , NnString *ele )
{
    for(int k=0;k<list.size();k++){
        if( ((NnString*)list.at(k))->icompare(*ele) == 0 ){
            delete ele;
            return;
        }
    }
    list.append( ele );
}

/* 配列から、特定文字列を除く
 *      list - 配列
 *      ele - 文字列へのポインタ(所有権委譲)
 */
static void removeArray( NnVector &list , NnString *ele )
{
    for(int k=0;k<list.size();k++){
        if( ((NnString*)list.at(k))->icompare(*ele) == 0 ){
            list.removeAt( k );
            delete ele;
            return;
        }
    }
    delete ele;
}


/* 配列に、文字列を加える/除く。同一文字列は極力除去する。
 * 文字列は「;」で区切って、各要素別に追加/除去する。
 *      list - 配列
 *      env - 文字列
 *      func - 追加・除去関数のポインタ
 */
static void manipArray( NnVector &list , const char *env ,
                        void (*func)( NnVector &, NnString *) )
{
    NnString rest(env);
    while( ! rest.empty() ){
	NnString path;
	rest.splitTo( path , rest , ";" , "");
	if( ! path.empty() )
	    (*func)( list , (NnString*)path.clone() );
    }
}


/* 配列の内容を「;」で繋いで連結する
 *      list - 配列
 *      result - 結果文字列
 */
static void joinArray( NnVector &list , NnString &result )
{
    if( list.size() <= 0 )
        return;
    result = *(NnString*)list.at(0);
    for(int i=1;i<list.size();i++){
        result << ';' << *(NnString*)list.at(i);
    }
}

/* putenv のラッパー(メモリリーク回避用)
 *      name    環境変数名
 *      value   値
 */
void putenv_( const NnString &name , const NnString &value)
{
    static NnHash envList;
    NnString *env=new NnString();

    *env << name << "=" << value;
    putenv( env->chars() );
    envList.put( name , env );
}

/* 環境変数をセット
 *	argv	「XXX=YYY」形式の文字列
 * return
 *	常に 0 (インタープリタを終了させない)
 */
int cmd_set( NyadosShell &shell , const NnString &argv )
{
    if( argv.empty() ){
        for( char **p=environ; *p != NULL ; ++p )
            conOut << *p << '\n';
        return 0;
    }
    int equalPos=argv.findOf( "=" );
    if( equalPos < 0 ){
	/* 「set ENV」のみ → 指定した環境変数の内容を表示. */
	conOut << argv << '=' << getEnv(argv.chars(),"") << '\n';
	return 0;
    }
    NnString name( argv.chars() , equalPos );  name.upcase();
    NnString value( argv.chars() + equalPos + 1 );

    if( value.length() == 0 ){
	/*** 「set ENV=」→ 環境変数 ENV を削除する ***/
        static NnString null;
        putenv_( name , null );
    }else if( name.endsWith("+") ){
	/*** 「set ENV+=VALUE」 ***/
	NnVector list;
	NnString newval;
        NnString value_;

	name.chop();
        NyadosShell::dequote(value.chars(),value_);
	manipArray( list , value_.chars() , &appendArray );
	const char *oldval=getEnv(name.chars());
	if( oldval != NULL ){
	    manipArray( list , oldval , &appendArray );
	}
	manipArray( list , oldval , &appendArray );
	joinArray( list , newval );
        putenv_( name , newval );
    }else if( name.endsWith("-") ){
	/***「set ENV-=VALUE」***/
	name.chop();
	const char *oldval=getEnv(name.chars());
	if( oldval != NULL ){
	    NnString newval;
	    NnVector list;
            NnString value_;

            NyadosShell::dequote(value.chars(),value_);
	    manipArray( list , oldval        , &appendArray );
	    manipArray( list , value_.chars() , &removeArray );
	    joinArray( list , newval );
            putenv_( name , newval );
	}
    }else if( name.endsWith(":") ){
        name.chop();
        NnString value_;
        NyadosShell::dequote(value.chars(),value_);
        putenv_( name , value_ );
    }else{
	/*** 「set ENV=VALUE」そのまま代入する ***/
        putenv_( name , value );
    }
    return 0;
}

/* インタープリタの終了
 * return
 *	常に 1 (終了を意味する)
 */
int cmd_exit( NyadosShell & , const NnString & )
{  
    return -1;
}

int cmd_echoOut( NyadosShell & , const NnString &argv )
{
    conOut << argv << '\n';
    return 0;
}

int cmd_cls( NyadosShell & , const NnString & )
{
    Console::clear();
    return 0;
}
