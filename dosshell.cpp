#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "ntcons.h"
#include "nnhash.h"
#include "getline.h"
#include "nndir.h"
#include "writer.h"
#include "nua.h"

/* ���̃n�b�V���e�[�u���ɁA�g���q���o�^����Ă���ƁA
 * ���̊g���q�̃t�@�C�����́A���s�\�ƌ��Ȃ����B
 * (��{�I�ɓo�^���e�́A������ => ������ ���O��)
 * �Ȃ��A�g���q�͏������ɕϊ����Ċi�[���邱�ƁB
 */
NnHash DosShell::executableSuffix;

/* ���̃t�@�C�������f�B���N�g�����ǂ����𖖔��ɃX���b�V���Ȃǂ�
 * ���Ă��邩�ǂ����ɂ�蔻�肷��B
 *      path - �t�@�C����
 * return
 *      ��0 - �f�B���N�g�� , 0 - ��ʃt�@�C��
 */
static int isDirectory( const char *path )
{
    int lastroot=NnDir::lastRoot(path);
    return lastroot != -1 && path[lastroot+1] == '\0';
}

/* ���̃t�@�C���������s�\���ǂ������g���q��蔻�肷��B
 *      path - �t�@�C����
 * return
 *      ��0 - ���s�\ , 0 - ���s�s�\
 */
static int isExecutable( const char *path )
{
    const char *suffix=strrchr(path,'.');
    /* �g���q���Ȃ���΁A���s�s�\ */
    if( suffix == NULL || suffix[0]=='\0' || suffix[1]=='\0' )
        return 0;
    
    /* �������� */
    NnString sfxlwr(suffix+1);
    sfxlwr.downcase();

    return sfxlwr.compare("com") == 0
        || sfxlwr.compare("exe") == 0
        || sfxlwr.compare("bat") == 0
	|| sfxlwr.compare("cmd") == 0
        || DosShell::executableSuffix.get(sfxlwr) != NULL ;
}

/* $e �Ȃǂ̕�����𐧌䕶�����܂񂾃e�L�X�g�֕ϊ�����
 * (��Ƀv�����v�g�p���������)
 *    sp - ��������
 *    result - �ϊ��㕶����̊i�[��
 */
void eval_dollars_sequence( const char *sp , NnString &result )
{
    if( sp == NULL || *sp == '\0' ){
        NnString pwd;
        NnDir::getcwd(pwd);
        result << pwd << '>';
        return;
    }

    time_t now;
    time( &now );
    struct tm *thetime = localtime( &now );

    result.erase();
    
    while( *sp != '\0' ){
        if( *sp == '$' ){
            ++sp;
            switch( toupper(*sp) ){
                case '_': result << '\n';   break;
                case '$': result << '$'; break;
                case 'A': result << '&'; break;
                case 'B': result << '|'; break;
                case 'C': result << '('; break;
                case 'D':
                    result.addValueOf(thetime->tm_year + 1900);
                    result << '-';
                    if( thetime->tm_mon + 1 < 10 )
                        result << '0';
                    result.addValueOf(thetime->tm_mon + 1);
                    result << '-';
                    if( thetime->tm_mday < 10 )
                        result << '0';
                    result.addValueOf( thetime->tm_mday );
                    switch( thetime->tm_wday ){
                        case 0: result << " (��)"; break;
                        case 1: result << " (��)"; break;
                        case 2: result << " (��)"; break;
                        case 3: result << " (��)"; break;
                        case 4: result << " (��)"; break;
                        case 5: result << " (��)"; break;
                        case 6: result << " (�y)"; break;
                    }
                    break;
                case 'E': result << '\x1B'; break;
                case 'F': result << ')'; break;
                case 'G': result << '>';    break;
                case 'H': result << '\b';   break;
                case 'I': break;
                case 'L': result << '<';    break;
                case 'N': result << (char)NnDir::getcwdrive(); break;
                case 'P':{
                    NnString pwd;
                    NnDir::getcwd(pwd) ;
                    result << pwd; 
                    break;
                }
                case 'Q': result << '='; break;
                case 'S': result << ' ';    break;
                case 'T':
                    if( thetime->tm_hour < 10 )
                        result << '0';
                    result.addValueOf(thetime->tm_hour);
                    result << ':';
                    if( thetime->tm_min < 10 )
                        result << '0';
                    result.addValueOf(thetime->tm_min);
                    result << ':';

                    if( thetime->tm_sec < 10 )
                        result << '0';
                    result.addValueOf(thetime->tm_sec) ;
                    break;
                case 'V':
#ifdef __EMX__
                    result << "NYAOS2" ; break;
#else
                    result << "NYACUS" ; break;
#endif
                case 'W':{
                    NnString pwd;
                    NnDir::getcwd(pwd);
                    int print_dots = 0;

                    int depth = 1;
                    if ((*(sp+1) != '\0') &&
                        ((0 < (*(sp+1)-'0')) && ((*(sp+1)-'0') < 10)) // 0 < depth < 10
                       ){
                        depth = (*(sp+1) - '0');
                        sp++;
                        print_dots = 1;
                    }
                    int *posbuf=new int[depth];
                    memset(posbuf, -1, sizeof(int)*depth);

                    int rootpos = 2;
                    // findLastOf w/ buffering
                    while ((rootpos = pwd.findOf("/\\", rootpos)) != -1){
                        for (int i = 0; i < depth-1; i++) posbuf[i] = posbuf[i+1];
                        posbuf[depth-1] = rootpos++;
                    }

                    if( posbuf[0] == -1 || posbuf[0] == 2 ){
                        result << pwd;
                    }else{
                        result << (char)pwd.at(0) << (char)pwd.at(1);
                        if( print_dots ){
                            result << "..." << pwd.chars()+posbuf[0];
                        }else{
                            result << pwd.chars()+posbuf[0]+1;
                        }
                    }
                    delete[]posbuf;
                    break;
                }
                default:  result << '$' << *sp; break;
            }
            ++sp;
        }else{
            result += *sp++;
        }
    }
}

/* �R�}���h���⊮�̂��߂̌�⃊�X�g���쐬����B
 *      region - ��⊮������͈̔�
 *      array - �⊮��������ꏊ
 * return
 *      ���̐�
 */
int DosShell::makeTopCompletionList( const NnString &region , NnVector &array )
{
    return makeTopCompletionListCore( region , array );
}

int DosShell::makeTopCompletionListCore( const NnString &region , NnVector &array )
{
    NnString pathcore;

    /* �擪�̈��p�������� */
    if( region.at(0) == '"' ){
        pathcore = region.chars() + 1;
    }else{
        pathcore = region;
    }

    GetLine::makeCompletionListCore( region , array );
    for(int i=0 ; i<array.size() ; ){
        NnString *fname=(NnString *)((NnPair*)array.at(i))->first();
        if( isExecutable( fname->chars())  ||  isDirectory( fname->chars() )){
            ++i;
        }else{
            array.deleteAt( i );
        }
    }

    if( region.findOf("/\\") != -1 )
        return array.size();
    
    const char *path=getEnv("PATH",NULL);
    if( path == NULL )
        return array.size();

    /* ���ϐ� PATH �𑀍삷�� */
    NnString rest(path);
    while( ! rest.empty() ){
	NnString path1;

	rest.splitTo(path1,rest,";");
	if( path1.empty() )
	    continue;
	path1.dequote();
        path1 += "\\*";
        for( NnDir dir(path1) ; dir.more() ; dir.next() ){
            if(    ! dir.isDir()
                && dir->istartsWith(pathcore) 
                && isExecutable(dir.name()) )
            {
                if( array.append(new NnPair(new NnStringIC(dir.name()))) )
                    break;
            }
        }
    }
    /* �G�C���A�X�E�֐��������ɂ䂭 : (����)�O���[�o���ϐ����Q�Ƃ��Ă��� */
    extern NnHash aliases,functions;
    static NnHash *hash_list[]={ &aliases , &functions , NULL };
    for( NnHash **hash=hash_list ; *hash != NULL ; ++hash ){
        for( NnHash::Each p(**hash) ; p.more() ; p.next() ){
            if( p->key().istartsWith(pathcore) ){
                if( array.append( new NnPair(new NnStringIC(p->key()) )) )
                    break;
            }
        }
    }
    /* nyaos.command �����ɂ䂭 */
    const static char *commands[]={ "command" , "command2" , NULL };
    for( const char **p=commands ; *p != NULL ; ++p ){
        NyaosLua L(*p);
        if( L.ok() ){
            lua_pushnil(L);
            while( lua_next(L,-2) != 0 ){
                lua_pushvalue(L,-2);
                const char *cmdname = lua_tostring(L,-1);
                if( cmdname != NULL && 
                        strnicmp(cmdname,pathcore.chars(),pathcore.length())==0 )
                {
                    array.append( new NnPair(new NnString( cmdname )) );
                }
                lua_pop(L,2);
            }
        }
    }

    array.sort();
    array.uniq();
    return array.size();
}

void DosShell::putchr(int ch)
{
    conOut << (char)ch;
}

void DosShell::putbs(int n)
{
    while( n-- > 0 )
	conOut << (char)'\b';
}

int DosShell::getkey()
{
    fflush(stdout);
#ifdef NYACUS
    conOut << cursor_on_;
#endif
    return Console::getkey();
}

/* �ҏW�J�n�t�b�N */
void DosShell::start()
{
    /* ��ʏ����R�[�h�̓ǂݎ�� */
    NnString *clear = dynamic_cast<NnString*>( properties.get("term_clear") );
    if( clear != NULL ){
        eval_dollars_sequence(clear->chars() , this->clear_ );
    }else{
        this->clear_ = "\x1B[2J";
    }
    /* �J�[�\���\���R�[�h�̓ǂݎ�� */
    NnString *cursor_on = dynamic_cast<NnString*>( properties.get("term_cursor_on") );
    if( cursor_on != NULL ){
        eval_dollars_sequence(cursor_on->chars() , this->cursor_on_ );
    }else{
        this->cursor_on_ = "\x1B[>5l";
    }
    prompt();
#ifdef NYACUS
    title();
#endif
}

/* �ҏW�I���t�b�N */
void DosShell::end()
{
    putchar('\n');
#ifdef NYACUS
    Console::restore_default_console_mode();
#endif
}

#ifdef PROMPT_SHIFT
/* �v�����v�g�̂����An�J�����ڂ�������o��
 *      prompt - ��������
 *      offset - ���o���ʒu
 *      result - ���o��������
 * return
 *      ���o�v�����v�g�̌���
 */
static int prompt_substr( const NnString &prompt , int offset , NnString &result )
{
    int i=0;
    int column=0;
    int nprints=0;
    int knj=0;
    for(;;){
        if( prompt.at(i) == '\x1B' ){
            for(;;){
                if( prompt.at(i) == '\0' )
                    goto exit;
                result << (char)prompt.at(i);
                if( isAlpha(prompt.at(i++)) )
                    break;
            }
        }else if( prompt.at(i) == '\0' ){
            break;
        }else{
            if( column == offset ){
                if( knj )
                    result << ' ';
                else
                    result << (char)prompt.at(i);
                ++nprints;
            }else if( column > offset ){
                result << (char)prompt.at(i);
                ++nprints;
            }
            if( knj == 0 && isKanji(prompt.at(i)) )
                knj = 1;
            else
                knj = 0;
            ++i;
            column++;
        }
    }
  exit:
    return nprints;
}


#else
/* �G�X�P�[�v�V�[�P���X���܂܂Ȃ��������𓾂�. 
 *      p - ������擪�|�C���^
 * return
 *      ������̂����A�G�X�P�[�v�V�[�P���X���܂܂Ȃ����̒���
 *      (�����Ȍ���)
 */
static int strlenNotEscape( const char *p )
{
    int w=0,escape=0;
    while( *p != '\0' ){
        if( *p == '\x1B' )
            escape = 1;
        if( ! escape )
            ++w;
        if( isAlpha(*p) )
            escape = 0;
	if( *p == '\n' )
	    w = 0;
        ++p;
    }
    return w;
}
#endif


/* �v�����v�g��\������.
 * return �v�����v�g�̌���(�o�C�g���ł͂Ȃ�=>�G�X�P�[�v�V�[�P���X���܂܂Ȃ�)
 */
int DosShell::prompt()
{
    const char *sp=prompt_.chars();
    NnString prompt;

    NyaosLua L("prompt");
    if( lua_isfunction(L,-1) ){
        lua_pushstring(L,sp && *sp ? sp : "");
        if( lua_pcall(L,1,2,0) == 0 ){
            if( lua_isnil(L,-2) ){
                /* nil �� �v�����v�g�ɕύX�Ȃ� */
                eval_dollars_sequence( sp , prompt );
            }else if( lua_toboolean(L,-2) ){
                /* true,"������" �� $�}�N����]�����ĕ\�� */
                eval_dollars_sequence( lua_tostring(L,-1) , prompt );
            }else{
                /* false,"������" �� $�}�N����]�����Ȃ��ŕ\�� */
                prompt = lua_tostring(L,-1);
            }
        }else{
            conErr << lua_tostring(L,1) << "\n";
            eval_dollars_sequence( sp , prompt );
        }
    }else{
        eval_dollars_sequence( sp , prompt );
    }

#ifdef PROMPT_SHIFT
    NnString prompt2;
    prompt_size = prompt_substr( prompt , prompt_offset , prompt2 );
    conOut << prompt2;
#else
    int prompt_size = strlenNotEscape( prompt.chars() );
    conOut << prompt;
#endif

    /* �K���L�[�v���Ȃ��Ă͂����Ȃ��ҏW�̈�̃T�C�Y���擾���� */
    NnString *temp;
    int minEditWidth = 10;
    if(    (temp=(NnString*)properties.get("mineditwidth")) != NULL
        && (minEditWidth=atoi(temp->chars())) < 1 )
    {
        minEditWidth = 10;
    }

    int whole_width=DEFAULT_WIDTH;
    if(    (temp=(NnString*)properties.get("width")) != NULL
        && (whole_width=atoi(temp->chars())) < 1 )
    {
        whole_width = DEFAULT_WIDTH;
    }
#ifdef NYACUS
    else {
        whole_width = Console::getWidth();
    }
#endif

    int width=whole_width - prompt_size % whole_width ;
    if( width <= minEditWidth ){
	conOut << '\n';
        width = whole_width;
    }
    setWidth( width );
    return width;
}

void DosShell::clear()
{
    conOut << clear_ ;
}

#ifdef NYACUS
/* �^�C�g����ݒ肷��. */
void DosShell::title()
{
    NyaosLua L("title");
    if( lua_isfunction(L,-1) ){
        char title_[1024];
        Console::getConsoleTitle( title_ , sizeof(title_) );
        lua_pushstring(L,title_);
        if( lua_pcall(L,1,2,0) == 0 ){
            if( lua_toboolean(L,-2) ){
                /* true,"������" �� $�}�N����]�����ĕ\�� */
                NnString title;
                eval_dollars_sequence( lua_tostring(L,-1) , title );
                Console::setConsoleTitle( title.chars() );
            }else if( ! lua_isnil(L,-2) ){
                /* false,"������" �� $�}�N����]�����Ȃ��ŕ\�� */
                Console::setConsoleTitle( lua_tostring(L,-1) );
            }
        }else{
            conErr << lua_tostring(L,1) << "\n";
        }
    }else{
        const char *t1=getEnv("TITLE",NULL);
        NnObject *t2;
        NnString title;
        if( t1 != NULL ){
            eval_dollars_sequence( t1 , title );
            Console::setConsoleTitle( title.chars() );
        }else if( (t2=properties.get("title")) != NULL ){
            NnString *t3=dynamic_cast<NnString*>( t2 );
            if( t3 != NULL ){
                eval_dollars_sequence( t3->chars() , title );
                Console::setConsoleTitle( title.chars() );
            }
        }
    }
}
#endif
