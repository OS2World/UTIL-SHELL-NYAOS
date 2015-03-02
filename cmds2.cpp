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
	    /* �����������́A���X�g�\���p�̃n�b�V���ɂ��o�^���� */
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

/* ���̍s�� endif �L�[���[�h���܂ނ����肷��.
 *      line - �s
 * return
 *      not 0 �c endif �߂��܂܂�Ă���
 *      0 �c �܂܂�Ȃ�
 */
static int has_keyword_endif( const NnString &line )
{
    return line.istartsWith("endif");
}

/* ���̍s�� else �L�[���[�h���܂ނ����肷��.
 *      line - �s
 * return
 *      not 0 �c else �߂��܂܂�Ă���
 *      0 �c �܂܂�Ȃ�
 */
static int has_keyword_else( const NnString &line )
{
    return line.istartsWith("else");
}

/* ���̍s�� if �` then �߂������Ă��邩�ǂ������肷��.
 *      line - �s
 * return
 *      not 0 �c if �` then �߂��܂܂�Ă���
 *      0 �c �܂܂�Ȃ�
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
        /* exist ���Z�q */
        left.splitTo( arg1 , left );
	NyadosShell::dequote( arg1 );
        if( NnDir::access( arg1.chars() )==0 )
            flag = !flag;
    }else if( arg1.icompare("errorlevel")==0 ){
        /* errorlevel ���Z�q */
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
    /* == ���Z�q�����͑O��ɋ󔒂̂Ȃ��g���������� */
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
        /* == or != ���Z�q */
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
    if( flag ){ /* ���������� */
        if( stricmp( then.chars() , "then") != 0 )
            return shell.interpret( left );
        /* then �߂̏ꍇ�́A���̂܂܎��̍s�֏������ڍs������ */
        shell.nesting.append( new NnString("then>") );
	if( ! then_left.empty() ){
	    shell.interpret( then_left );
	}
    }else{ /* �����s������ */
        if( stricmp( then.chars() , "then") == 0 ){
            /* else �܂Ńl�X�g���X�L�b�v����B*/
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

/* �z��ɁA�������������B���ꕶ���񂪂���ꍇ�͒ǉ����Ȃ�.
 *      list - �z��
 *      ele - ������ւ̃|�C���^(���L���Ϗ�)
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

/* �z�񂩂�A���蕶���������
 *      list - �z��
 *      ele - ������ւ̃|�C���^(���L���Ϗ�)
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


/* �z��ɁA�������������/�����B���ꕶ����͋ɗ͏�������B
 * ������́u;�v�ŋ�؂��āA�e�v�f�ʂɒǉ�/��������B
 *      list - �z��
 *      env - ������
 *      func - �ǉ��E�����֐��̃|�C���^
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


/* �z��̓��e���u;�v�Ōq���ŘA������
 *      list - �z��
 *      result - ���ʕ�����
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

/* putenv �̃��b�p�[(���������[�N���p)
 *      name    ���ϐ���
 *      value   �l
 */
void putenv_( const NnString &name , const NnString &value)
{
    static NnHash envList;
    NnString *env=new NnString();

    *env << name << "=" << value;
    putenv( env->chars() );
    envList.put( name , env );
}

/* ���ϐ����Z�b�g
 *	argv	�uXXX=YYY�v�`���̕�����
 * return
 *	��� 0 (�C���^�[�v���^���I�������Ȃ�)
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
	/* �uset ENV�v�̂� �� �w�肵�����ϐ��̓��e��\��. */
	conOut << argv << '=' << getEnv(argv.chars(),"") << '\n';
	return 0;
    }
    NnString name( argv.chars() , equalPos );  name.upcase();
    NnString value( argv.chars() + equalPos + 1 );

    if( value.length() == 0 ){
	/*** �uset ENV=�v�� ���ϐ� ENV ���폜���� ***/
        static NnString null;
        putenv_( name , null );
    }else if( name.endsWith("+") ){
	/*** �uset ENV+=VALUE�v ***/
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
	/***�uset ENV-=VALUE�v***/
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
	/*** �uset ENV=VALUE�v���̂܂ܑ������ ***/
        putenv_( name , value );
    }
    return 0;
}

/* �C���^�[�v���^�̏I��
 * return
 *	��� 1 (�I�����Ӗ�����)
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
