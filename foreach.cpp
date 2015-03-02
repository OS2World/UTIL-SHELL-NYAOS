#include <assert.h>
#include <stdlib.h>
#include "nnstring.h"
#include "nnvector.h"
#include "nnhash.h"
#include "shell.h"
#include "nndir.h"

Status BufferedShell::readline( NnString &line )
{
    if( count >= buffers.size() )
        return TERMINATE;
    line = *(NnString*)buffers.at(count++);
    return NEXTLINE;
}

extern NnHash properties;

/* 末尾が、対応する'{'のない'}' であれば真を返す. */
static int end_with_unmatched_closing_brace( const NnString &s )
{
    int nest=0;
    letter_t lastchar=0;
    for( NnString::Iter it(s) ; *it ; ++it ){
	lastchar = *it;
	if( *it == '{' )
	    nest++;
	else if( *it == '}' )
	    --nest;
    }
    return lastchar=='}' && nest < 0 ;
}

/* foreach など、特定のキーワードまでの複文を BufferedShell へ読みこむ
 *  	shell  - 入力元シェル(コマンドライン・スクリプトなど)
 *	bShell - 出力先シェル
 *	prompt - プロンプト
 *	startKeyword - ブロック開始キーワード(ネスト用)
 *	endKeyword - ブロック終了キーワード
 * return
 *       0 - 実施
 *      -1 - キャンセル
 */
static int loadToBufferedShell( NyadosShell   &shell ,
				 BufferedShell &bShell ,
				 const char *prompt ,
				 const char *startKeyword ,
				 const char *endKeyword )
{
    NnString cmdline;
    int nest = 1;
    Status rc=TERMINATE;

    shell.nesting.append(new NnString(prompt));
    while( (rc=shell.readcommand(cmdline)) == NEXTLINE ){
        NnString arg1,left;
        cmdline.splitTo( arg1 , left );
	if( strcmp(startKeyword,"{") == 0 ){
	    /* 中括弧形式のブロック構文の時 */
	    if( end_with_unmatched_closing_brace(cmdline) && --nest <= 0 ){
		cmdline.chop();
		bShell.append( (NnString*)cmdline.clone() );
		break;
	    }
	    if( arg1.endsWith(startKeyword) )
		++nest;
	}else{
	    /* キーワード形式のブロック構文の時 */
	    if( arg1.icompare(endKeyword)==0 && --nest <= 0 )
		break;
	    if( arg1.icompare(startKeyword)==0 )
		++nest;
	}
	/* ブロック構文が中括弧の時と、キーワードの時で
	 * ブロックの境界の判定を変える */
        bShell.append( (NnString*)cmdline.clone() );
        cmdline.erase();
    }
    delete shell.nesting.pop();
    return rc==CANCEL ? -1 : 0;
}

extern NnHash functions;

class NnExecutable_BufferedShell : public NnExecutable{
    BufferedShell *shell_;
public:
    NnExecutable_BufferedShell( BufferedShell *shell ) : shell_(shell){}
    ~NnExecutable_BufferedShell(){ delete shell_; }

    BufferedShell *shell(){ return shell_; }
    int operator()( const NnVector &args );
};

int NnExecutable_BufferedShell::operator()( const NnVector &args )
{
    shell_->setArgv((NnVector*)args.clone());
    return shell_->mainloop();
}

/* 関数の一覧発行「{}」 */
int cmd_function_list( NyadosShell &shell , const NnString &argv )
{
    for( NnHash::Each e(functions) ; e.more() ; ++e ){
        conOut << e->key() << "{\n";
        NnExecutable_BufferedShell *bShell
            = dynamic_cast<NnExecutable_BufferedShell*>( e->value() );
        assert( bShell != NULL );
        for(int i=0 ; i<bShell->shell()->size() ; ++i ){
            conOut << "\t" << bShell->shell()->statement(i) << "\n";
        }
        conOut << "}\n";
    }
    return 0;
}

/* 関数削除 */
int sub_brace_erase( NyadosShell &shell , const NnString &arg1 )
{
    NnString funcname(arg1);
    funcname.chop(); /* remove } */
    funcname.chop(); /* remove { */

    functions.remove( funcname );

    return 0;
}

/* 関数宣言 */
int sub_brace_start( NyadosShell &shell , 
		     const NnString &arg1 ,
		     const NnString &argv )
{
    /* 「{」部分を削除して、関数名を取得する */
    NnString funcname(arg1);
    funcname.chop(); 

    BufferedShell *bShell=new BufferedShell();
    if( argv.length() > 0 ){
	/* 引数部分があれば、関数内構文の一部とみなす */
	bShell->append( (NnString*)argv.clone() );
    }
    if( loadToBufferedShell( shell, *bShell , "brace>" , "{" , "}" ) == 0 ){
        functions.put( funcname , new NnExecutable_BufferedShell(bShell) );
    }
    return 0;
}

int cmd_foreach( NyadosShell &shell , const NnString &argv )
{
    /* パラメータ分析 */
    NnString varname,rest;
    argv.splitTo(varname,rest);
    if( varname.empty() ){
        conErr << "foreach: foreach VARNAME VALUE-1 VALUE-2 ...\n";
        return 0;
    }
    varname.downcase();

    /* バッファに foreach ～ end までの文字列をためる */
    BufferedShell bshell;
    if( loadToBufferedShell( shell , bshell , "foreach>" , "foreach" , "end" ) != 0 )
        return 0;

    NnString *orgstr=(NnString*)properties.get( varname );
    NnString *savevar=(NnString*)( orgstr != NULL ? orgstr->clone() : NULL );

    /* リストの前後の括弧を取り除く */
    rest.trim();
    if( rest.startsWith("(") )
	rest.shift();
    if( rest.endsWith(")") )
	rest.chop();
    rest.trim();

    /* 引数展開 */
    NnVector list;
    for(;;){
        NnString arg1;

        rest.splitTo( arg1 , rest );
        if( arg1.empty() )
            break;
        
        /* ワイルドカード展開 */
        NnVector sublist;
        if( fnexplode( arg1.chars() , sublist ) != 0 )
            goto memerror;
        
        if( sublist.size() <= 0 ){
            if( list.append( arg1.clone() ) )
                goto memerror;
        }else{
            while( sublist.size() > 0 ){
		NnFileStat *stat=(NnFileStat*)sublist.shift();

		list.append( new NnString(stat->name()) );
		delete stat;
            }
        }
    }
    {
	NnVector *param=new NnVector();
	if( param == NULL )
	    goto memerror;
	for(int j=0 ; j<shell.argc() ; ++j )
	    param->append( shell.argv(j)->clone() );
	bshell.setArgv(param);
    }

    /* コマンド実行 */
    for(int i=0 ; i<list.size() ; i++){
        properties.put( varname , list.at(i)->clone() );
        bshell.mainloop();
        bshell.rewind();
    }
    properties.put_( varname , savevar );
    return 0;

  memerror:
    conErr << "foreach: memory allocation error\n";
    return 0;
}
