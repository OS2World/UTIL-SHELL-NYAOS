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

/* �������A�Ή�����'{'�̂Ȃ�'}' �ł���ΐ^��Ԃ�. */
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

/* foreach �ȂǁA����̃L�[���[�h�܂ł̕����� BufferedShell �֓ǂ݂���
 *  	shell  - ���͌��V�F��(�R�}���h���C���E�X�N���v�g�Ȃ�)
 *	bShell - �o�͐�V�F��
 *	prompt - �v�����v�g
 *	startKeyword - �u���b�N�J�n�L�[���[�h(�l�X�g�p)
 *	endKeyword - �u���b�N�I���L�[���[�h
 * return
 *       0 - ���{
 *      -1 - �L�����Z��
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
	    /* �����ʌ`���̃u���b�N�\���̎� */
	    if( end_with_unmatched_closing_brace(cmdline) && --nest <= 0 ){
		cmdline.chop();
		bShell.append( (NnString*)cmdline.clone() );
		break;
	    }
	    if( arg1.endsWith(startKeyword) )
		++nest;
	}else{
	    /* �L�[���[�h�`���̃u���b�N�\���̎� */
	    if( arg1.icompare(endKeyword)==0 && --nest <= 0 )
		break;
	    if( arg1.icompare(startKeyword)==0 )
		++nest;
	}
	/* �u���b�N�\���������ʂ̎��ƁA�L�[���[�h�̎���
	 * �u���b�N�̋��E�̔����ς��� */
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

/* �֐��̈ꗗ���s�u{}�v */
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

/* �֐��폜 */
int sub_brace_erase( NyadosShell &shell , const NnString &arg1 )
{
    NnString funcname(arg1);
    funcname.chop(); /* remove } */
    funcname.chop(); /* remove { */

    functions.remove( funcname );

    return 0;
}

/* �֐��錾 */
int sub_brace_start( NyadosShell &shell , 
		     const NnString &arg1 ,
		     const NnString &argv )
{
    /* �u{�v�������폜���āA�֐������擾���� */
    NnString funcname(arg1);
    funcname.chop(); 

    BufferedShell *bShell=new BufferedShell();
    if( argv.length() > 0 ){
	/* ��������������΁A�֐����\���̈ꕔ�Ƃ݂Ȃ� */
	bShell->append( (NnString*)argv.clone() );
    }
    if( loadToBufferedShell( shell, *bShell , "brace>" , "{" , "}" ) == 0 ){
        functions.put( funcname , new NnExecutable_BufferedShell(bShell) );
    }
    return 0;
}

int cmd_foreach( NyadosShell &shell , const NnString &argv )
{
    /* �p�����[�^���� */
    NnString varname,rest;
    argv.splitTo(varname,rest);
    if( varname.empty() ){
        conErr << "foreach: foreach VARNAME VALUE-1 VALUE-2 ...\n";
        return 0;
    }
    varname.downcase();

    /* �o�b�t�@�� foreach �` end �܂ł̕���������߂� */
    BufferedShell bshell;
    if( loadToBufferedShell( shell , bshell , "foreach>" , "foreach" , "end" ) != 0 )
        return 0;

    NnString *orgstr=(NnString*)properties.get( varname );
    NnString *savevar=(NnString*)( orgstr != NULL ? orgstr->clone() : NULL );

    /* ���X�g�̑O��̊��ʂ���菜�� */
    rest.trim();
    if( rest.startsWith("(") )
	rest.shift();
    if( rest.endsWith(")") )
	rest.chop();
    rest.trim();

    /* �����W�J */
    NnVector list;
    for(;;){
        NnString arg1;

        rest.splitTo( arg1 , rest );
        if( arg1.empty() )
            break;
        
        /* ���C���h�J�[�h�W�J */
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

    /* �R�}���h���s */
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
