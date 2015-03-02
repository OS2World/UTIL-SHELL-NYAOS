#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <process.h>
#include <io.h>

#include "shell.h"
#include "nnstring.h"
#include "nnhash.h"
#include "getline.h"
#include "reader.h"

//#define DBG(s) (s)
#define DBG(s)

static void glob( NnString &line )
{
    if( properties.get("glob") ){
        NnString result;
        glob_all( line.chars() , result );
        line = result;
    }
}

/* 逆クォートで読みこめる文字列量を戻す */
static int getQuoteMax()
{
    NnString *max=(NnString*)properties.get("backquote");
    return max == NULL ? 0 : 8192 ;
}

#ifdef THREAD

struct BufferingInfo {
    NnString buffer;
    int fd;
    int max;
    BufferingInfo(int f,int m) : fd(f),max(m){}
};

static void buffering_thread( void *b_ )
{
    BufferingInfo *b= static_cast<BufferingInfo*>( b_ );
    char c;

    while( ::read(b->fd,&c,1) > 0 ){
        if( b->buffer.length()+1 < b->max ){
            b->buffer << c;
        }
    }
    ::close( b->fd );
}

#endif

/* コマンドの標準出力の内容を文字列に取り込む. 
 *   cmdline - 実行するコマンド
 *   dst - 標準出力の内容
 *   max - 引用の上限. 0 の時は上限無し
 *   shrink - 二つ以上の空白文字を一つにする時は true とする
 * return
 *   0  : 成功
 *   -1 : オーバーフロー(max までは dst に取り込んでいる)
 *   -2 : テンポラリファイルが作成できなかった
 */
int eval_cmdline( const char *cmdline, NnString &dst, int max , bool shrink)
{
#ifdef THREAD
    extern int mkpipeline( int pipefd[] );
    int pipefd[2];

    if( mkpipeline( pipefd ) != 0 )
        return -1;

    /* 出力を受け止めるスレッドを先行して走らせておく */
    BufferingInfo buffer(pipefd[0],max);
#ifdef __EMX__
    if( _beginthread( buffering_thread , NULL , 655350u , &buffer ) == -1 )
        return -1;
#else
    if( _beginthread( buffering_thread , 0 , &buffer ) == (uintptr_t)-1L)
        return -1;
#endif

    int savefd = dup(1);
    dup2( pipefd[1] , 1 );
    ::close(pipefd[1]);
#else
    char *tmppath=NULL;
    if( (tmppath=_tempnam("\\","NYA"))==NULL ){
        return -2;
    }
    FILE *fp=fopen(tmppath,"w+");
    if( fp==NULL ){
        free(tmppath);
        return -2;
    }
    int savefd = dup(1);
    dup2( fileno(fp) , 1 );
#endif

    OneLineShell shell( cmdline );
    shell.mainloop();

    dup2( savefd , 1 );
    int lastchar=' ';
#ifdef THREAD
    for( const char *p=buffer.buffer.chars() ; *p != '\0' ; ++p ){
        if( shrink && isSpace(*p) ){
            if( ! isSpace(lastchar) )
                dst += ' ';
        }else{
            dst += (char)*p;
        }
        lastchar = *p;
    }
#else
    rewind( fp );
    int ch;
    int rc=0;
    while( !feof(fp) && (ch=getc(fp)) != EOF ){
        if( shrink && isSpace(ch) ){
            if( ! isSpace(lastchar) )
                dst += ' ';
        }else{
            dst += (char)ch;
        }
        lastchar = ch;
        if( max && dst.length() >= max ){
            rc = -1;
            break;
        }
    }
    fclose(fp);
    if( tmppath != NULL ){
        remove(tmppath);
        free(tmppath);
    }
#endif
    dst.trim();
    return rc;
}

/* 逆クォート処理
 *      sp  元文字列(`の次を指していること)
 *      dst 先文字列
 *      max 上限
 *      quote 引用符に囲まれているなら 非0 にセットする。
 *           (0 の時、& や > といった文字をクォートする)
 * return
 *      0  成功
 *      -1 オーバーフロー
 *      -2 テンポラリファイルが作成できない。
 */
static int doQuote( const char *&sp , NnString &dst , int max , int quote )
{
    bool escape = false;
    NnString q;
    while( *sp != '\0' ){
        if( *sp =='`' && !escape ){
            /* 連続する `` は、一つの ` へ変換する。 */
            if( *(sp+1) != '`' )
                break;
	    ++sp;
        }
        if( !quote ){
            escape = (!escape && *sp == '^');
        }
        if( !escape ){
            if( isKanji(*sp) )
                q += *sp++;
            q += *sp++;
        }
    }
    if( q.length() <= 0 ){
        dst += '`';
        return 0;
    }

    NnString buffer;
    int rc=eval_cmdline( q.chars() , buffer , max , !quote );
    if( rc < 0 ){
        return rc;
    }
    if( quote ){
        for(const char *p = buffer.chars() ; *p != '\0' ; ++p ){
            if( *p == '"' ){
                dst << "\"\"";
            }else{
                dst << *p;
            }
        }
    }else{
        for(const char *p = buffer.chars() ; *p != '\0' ; ++p ){
            if( *p == '&' || *p == '|' || *p == '<' || *p=='>' ){
                dst << '"' << *p << '"';
            }else if( *p == '"' ){
                dst << "\"\"\"\"";
            }else if( *p == '^' ){
                dst << "^^";
            }else{
                dst << *p;
            }
        }
    }
    return rc;
}


/* リダイレクト先を読み取り、結果を FileWriter オブジェクトで返す.
 * オープンできない場合はエラーメッセージも勝手に出力.
 *	sp 最初の'>'の直後
 *         実行後は、
 * return
 *      出力先
 */
static FileWriter *readWriteRedirect( const char *&sp )
{

    // append ?
    const char *mode = "w";
    if( *sp == '>' ){
	++sp;
	mode = "a";
    }
    NnString fn;
    NyadosShell::readNextWord(sp,fn);
    if( fn.empty() ){
        conErr << "syntax error near unexpected token `newline'\n";
	return NULL;
    }

    FileWriter *fw=new FileWriter(fn.chars(),mode);

    if( fw != NULL && fw->ok() ){
	return fw;
    }else{
	perror( fn.chars() );
	delete fw;
	return NULL;
    }
}

/* sp が特殊フォルダーを示す文字列から始まっていたら真を返す
 *	sp … 先頭位置
 *	quote … 二重引用符内なら真にする
 *	spc … 直前の文字が空白なら真にしておく
 * return
 *	doFolder で変換すべき文字列であれば、その長さ
 */
int isFolder( const char *sp , int quote , int spc )
{
    if( ! spc  ||  quote )
	return 0;

    if( *sp=='~' && 
        (getEnv("HOME")!=NULL || getEnv("USERPROFILE") != NULL ) && 
        properties.get("tilde")!=NULL)
    {
	int n=1;
	while( isalnum(sp[n] & 255) || sp[n]=='_' )
	    ++n;
	return n;
    }

    if( sp[0]=='.' && sp[1]=='.' && sp[2]=='.' && properties.get("dots")!=NULL ){
	int n=3;
	while( sp[n] == '.' )
	    ++n;
	return n;
    }
    return 0;
}
/* sp で始まる特殊フォルダー名を本来のフォルダー名へ変換する
 *	sp … 先頭位置
 *	len … 長さ
 *	dst … 本来のフォルダー名を入れるところ
 */	
static void doFolder( const char *&sp , int len , NnString &dst )
{
    NnString name(sp,len),value;
    NnDir::filter( name.chars() , value );
    sp += len;
    if( value.findOf(" \t\v\r\n") >= 0 ){
	dst << '"' << value << '"';
    }else{
	dst << value;
    }
}

/* 環境変数などを展開する(内蔵コマンド向け)
 *	src - 元文字列
 *	dst - 結果が入る
 * return
 *      0 - 成功 , -1 - 失敗
 *
 * ここで「^」文字の削除も行う
 */
int NyadosShell::explode4internal( const NnString &src , NnString &dst )
{
    /* パイプまわりの処理を先に実行する */
    NnString firstcmd; /* 最初のパイプ文字までのコマンド */
    NnString restcmd;  /* それ移行のコマンド */
    src.splitTo( firstcmd , restcmd , "|" , "\"`" );
    if( ! restcmd.empty() ){
	if( restcmd.at(0) == '&' ){ /* 標準出力+エラー出力 */
	    NnString pipecmds( restcmd.chars()+1 );

            PipeWriter *pw=new PipeWriter(pipecmds);
            if( pw == NULL || ! pw->ok() ){
                /* PipeWriter 内でエラーを出力しているので、
                 * メッセージは出さなくてよい。
                 * perror( pipecmds.chars() );
                 */
                delete pw;
                return -1;
	    }
            conOut_ = pw ;
            conErr_ = new WriterClone(pw);
	}else{ /* 標準出力のみ */
            NnString pipecmds( restcmd.chars() );

            PipeWriter *pw=new PipeWriter(pipecmds);
            if( pw == NULL || ! pw->ok() ){
                /* PipeWriter 内でエラーを出力しているので、
                 * メッセージは出さなくてよい。
                 * perror( pipecmds.chars() );
                 */
                delete pw;
                return -1;
	    }
            conOut_ = pw;
	}
    }
    int prevchar=' ';
    bool quote  = false;
    bool escape = false;
    int len;
    dst.erase();
    int backquotemax=getQuoteMax();

    const char *sp=firstcmd.chars();
    while( *sp != '\0' ){

	if( *sp == '"' )
	    quote = !quote;
	
	int prevchar1=(*sp & 255);
	if( !escape && (len=isFolder(sp,quote,isSpace(prevchar))) != 0 ){
	    doFolder( sp , len , dst );
        }else if( *sp == '<' && !quote && !escape ){
            ++sp;
            NnString fn;
            NyadosShell::readNextWord(sp,fn);
            if( fn.empty() ){
                conErr << "syntax error near unexpected token `newline'\n";
                return -1;
            }
            FileReader *fr=new FileReader(fn.chars());
            if( fr != NULL && !fr->eof() ){
                conIn_ = fr;
            }else{
                conErr << "can't redirect stdin.\n";
                return -1;
            }
        }else if( *sp == '>' && !quote && !escape ){
	    ++sp;
	    FileWriter *fw=readWriteRedirect( sp );
	    if( fw != NULL ){
                conOut_ = fw;
	    }else{
		conErr << "can't redirect stdout.\n";
		return -1;
	    }
	}else if( *sp == '1' && *(sp+1) == '>' && !escape && !quote ){
	    if( ! restcmd.empty() ){
		/* すでにリダイレクトしていたら、エラー */
		conErr << "ambigous redirect.\n";
		return -1;
	    }
	    sp += 2;
	    if( *sp == '&'  &&  *(sp+1) == '2' ){
		sp += 2;
                conOut_ = new WriterClone(conErr_);
	    }else if( *sp == '&' && *(sp+1) == '-' ){
		sp += 2;
                conOut_ = new NullWriter();
	    }else{
		FileWriter *fw=readWriteRedirect( sp );
		if( fw != NULL ){
                    conOut_ = fw;
		}else{
		    conErr << "can't redirect stdout.\n";
		    return -1;
		}
	    }
	}else if( *sp == '2' && *(sp+1) == '>' && !escape && !quote ){
	    sp += 2;
	    if( *sp == '&' && *(sp+1) == '1' ){
		sp += 2;
                conErr_ = new WriterClone(conOut_);
	    }else if( *sp == '&' && *(sp+1) == '-' ){
		sp += 2;
                conErr_ = new NullWriter();
	    }else{
		FileWriter *fw=readWriteRedirect( sp );
		if( fw != NULL ){
                    conErr_ = fw;
		}else{
		    conErr << "can't redirect stderr.\n";
		    return -1;
		}
	    }
        }else if( *sp == '`' && backquotemax > 0 && !escape ){
	    ++sp;
            switch( doQuote( sp , dst , backquotemax , quote ) ){
            case -1:
                conErr << "Over flow commandline.\n";
                return -1;
            case -2:
                conErr << "Can not make temporary file.\n";
                return -1;
            default:
                break;
            }
	    ++sp;
        }else if( !quote && !escape && *sp=='^' ){
            ++sp;
            escape = true;
	}else{
            escape = false;
	    if( isKanji(*sp) )
		dst += *sp++;
	    dst += *sp++;
	}
	prevchar = prevchar1;
    }
    glob( dst );
    return 0;
}

/* ヒアドキュメントを行う
 *    sp - 「<<END」の最初の < の位置を差す。
 *    dst - 結果(「< 一時ファイル名」等)が入る
 *    prefix - 「<」or「 」
 */
void NyadosShell::doHereDocument( const char *&sp , NnString &dst , char prefix )
{
    int quote_mode = 0 , quote = 0 ;
    sp += 2;

    /* 終端マークを読み取る */
    NnString endWord;
    while( *sp != '\0' && (!isspace(*sp & 255) || quote ) ){
	if( *sp == '"' ){
	    quote ^= 1;
	    quote_mode = 1;
	}else{
	    endWord << *sp ;
	}
	++sp;
    }
    /* ドキュメント部分を一時ファイルに書き出す */
    this->heredocfn=NnDir::tempfn();
    FILE *fp=fopen( heredocfn.chars() , "w" );
    if( fp != NULL ){
	VariableFilter  variable_filter(*this);
	NnString  line;

	NnString *prompt=new NnString(endWord);
	*prompt << '>';
	this->nesting.append( prompt );

	while( this->readline(line) >= 0 && !line.startsWith(endWord) ){
	    /* <<"EOF" ではなく、<<EOF 形式の場合は、
	     * %...% 形式の単語を置換する必要あり
	     */
	    if( ! quote_mode )
		variable_filter( line );

	    fputs( line.chars() , fp );
	    if( ! line.endsWith("\n") )
		putc('\n',fp);
	    line.erase();
	}
	delete this->nesting.pop();
	fclose( fp );
	dst << prefix << heredocfn ;
    }
}


/* 外部コマンド用プリプロセス.
 * ・%1,%2 を変換する
 * ・プログラム名の / を ￥へ変換する。
 *      src - 元文字列
 *      dst - 加工した結果文字列の入れ先
 * return 0 - 成功 , -1 失敗(オーバーフローなど)
 */
int NyadosShell::explode4external( const NnString &src , NnString &dst )
{
    DBG( printf("NyadosShell::explode4external('%s',...)\n",src.chars()) );
    /* プログラム名をフィルターにかける：チルダ変換など */
    NnString progname,args;
    src.splitTo( progname,args );
    NnDir::filter( progname.chars() , dst );

    /* プログラム名に空白が含まれていたら、"" で囲み直す */
    if( dst.findOf(" ") >= 0 ){
        dst.dequote();
        dst.unshift('"');
        dst << '"';
    }

    if( args.empty() )
	return 0;

    dst << ' ';
    int backquotemax=getQuoteMax();
    bool quote=false;
    bool escape=false;
    int len;

    // 引数のコピー.
    int spc=1;
    for( const char *sp=args.chars() ; *sp != '\0' ; ++sp ){
        if( *sp == '"' )
            quote = !quote;

	if( !escape && (len=isFolder(sp,quote,spc)) != 0 ){
	    doFolder( sp , len , dst );
	    --sp;
        }else if( *sp == '`'  && backquotemax > 0  && !escape ){
	    ++sp;
            switch( doQuote( sp , dst , backquotemax , quote ) ) {
            case -1:
                conErr << "Over flow commandline.\n";
                return -1;
            case -2:
                conErr << "Can not make temporary file.\n";
                return -1;
            default:
                break;
            }
	}else if( *sp == '<' && *(sp+1) == '<' && !quote && !escape ){
	    /* ヒアドキュメント */
	    doHereDocument( sp , dst , '<' );
	}else if( *sp == '<' && *(sp+1) == '=' && !quote && !escape ){
	    /* インラインファイルテキスト */
	    doHereDocument( sp , dst , ' ' );
	}else if( *sp == '|' && *(sp+1) == '&' && !escape ){
	    dst << "2>&1 |";
        }else{
            escape = (!escape && !quote && *sp == '^');
	    if( isKanji(*sp) )
		dst += *sp++;
            dst += *sp;
        }
        spc = isSpace( *sp );
    }
    glob( dst );
    return 0;
}
