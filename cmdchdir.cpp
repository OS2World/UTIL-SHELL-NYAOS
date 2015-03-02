/* �f�B���N�g������֌W�̃R�}���h�̒�`�\�[�X */
#include <ctype.h>
#include <stdlib.h>
#include "ntcons.h"
#include "nndir.h"
#include "shell.h"

NnVector dirstack;
static NnString  prevdir , currdir ;

/* �X�^�b�N�̓��e�� ~n �ɔ��f���� */
static void chg_tilde()
{
    int i;
    for( i=1 ; i <= 9 ; ++i ){
	char no[]={ static_cast<char>('0' + i) , '\0' };
	int n=dirstack.size()-i;
	if( n >= 0 ){
	    NnDir::specialFolder.put( no , ((NnString*)dirstack.at(n))->clone() );
	}else{
	    NnDir::specialFolder.remove( no );
	}
    }
}

/* chdir �̃��b�p�[. ��O�̃f�B���N�g�����L������.
 *	dir - �ړ���f�B���N�g��
 * return
 *	0 - ���� , not 0 - ���s
 */
static int chdir_( const NnString &dir )
{
    int rc;
    if( currdir.empty() ){
        NnDir::getcwd( currdir );
    }
    
    if( (rc=NnDir::chdir( dir.chars() )) == 0 ){
        prevdir = currdir;
        NnString title("NYAOS - ");
        NnDir::getcwd( currdir );
        title << currdir ;
	Console::setConsoleTitle( title.chars() );
    }
    return rc;
}

/* �J�����g�f�B���N�g�����X�^�b�N�� */
static void pushd_here()
{
    NnString *cwd=new NnString();
    NnDir::getcwd(*cwd);
    dirstack.append( cwd );
}

/* �X�^�b�N�g�b�v���J�����g�f�B���N�g���� 
 *    return 
 *	NULL: ���� 
 *      not NULL: ���s�������̃f�B���N�g����
 *          (�Ăяo�����ɂāAdelete ���邱��)
 */
static NnString *popd_here()
{
    NnString *cwd = (NnString*)dirstack.shift();
    if( chdir_( *cwd ) != 0 ){
	return cwd;
    }else{
	delete cwd;
	return NULL;
    }
}

/* �X�^�b�N�g�b�v�ƃJ�����g�f�B���N�g��������
 * return
 *	NULL - ����
 *	not NULL - �ړ��ł��Ȃ������ꍇ�̃X�^�b�N�g�b�v
 */
static NnString *swap_here()
{
    NnString *top=(NnString*)dirstack.top();
    NnString newcwd( *top );
    NnDir::getcwd( *top );

    if( chdir_( newcwd ) != 0 ){
	delete (NnString*)dirstack.shift();
	return new NnString(newcwd);
    }
    return NULL;
}

/* �����R�}���h dirs */
int cmd_dirs( NyadosShell &shell , const NnString & )
{
    NnString cwd;

    NnDir::getcwd(cwd);
    conOut << cwd ;
    for(int i=dirstack.size()-1 ; i>=0 ; i-- ){
        conOut << ' ' << *(NnString*)dirstack.at(i);
    }
    conOut << '\n';
    return 0;
}

/* �����R�}���h pushd */
int cmd_pushd( NyadosShell &shell , const NnString &argv_ )
{
    NnString argv(argv_);
    int here_flag = 0;
    if( argv.startsWith("-") ){
        NnString arg1,left;

        argv.splitTo( arg1,left);
        argv = left;
        if( arg1.compare("-h")==0 ){
            here_flag = 1;
        }else if( arg1.compare("-H")==0 ){
            here_flag = 2;
        }
    }

    if( argv.empty() ){
        /* �X�^�b�N�g�b�v�̃f�B���N�g���ƃJ�����g�f�B���N�g�������ւ��� */
        if( dirstack.size() < 1 ){
            if( here_flag ){
                pushd_here();
            }else{
                conErr << "pushd: directory stack empty.\n";
                return 0;
            }
        }else{
            if( here_flag == 2 ){
                pushd_here();
            }else{
		NnString *dir=swap_here();
                if( dir != NULL ){
		    conErr << "pushd: stack top `" << *dir << "' not found.\n";
		    delete dir;
		    return 0;
		}
            }
        }
    }else if( argv.at(0) == '+' && isDigit(argv.at(1)) ){
        /* �X�^�b�N�� +n �`���Ŏw�肷��Bn ���]������ */
        int n=atoi(argv.chars()+1);
        if( n > 0 ){
            /* �����ɃJ�����g�f�B���N�g���� push �������_�ŁA
             * ���łɈ��]���� 
             */
            pushd_here();
            while( --n > 0 )
                dirstack.append( dirstack.shift() );
	    NnString *faildir=popd_here();
	    if( faildir != NULL ){
		conErr << "pushd: directory `" << *faildir << "' not found.\n";
		delete faildir;
		return 0;
	    }
        }
    }else{
        /* ���݂̃f�B���N�g�����X�^�b�N�ɕۑ� */
        pushd_here();
        /* �f�B���N�g�����ړ� */
        NyadosShell::dequote( argv );
	if( chdir_( argv ) != 0 ){
	    conErr << argv << ": no such directory.\n";
	    delete popd_here();
	    return 0;
	}
    }
    chg_tilde();
    return cmd_dirs(shell,argv);
}

/* �����R�}���h popd �������[�`��  */
int cmd_popd( NyadosShell &shell , const NnString &argv )
{
    if( dirstack.size() <= 0 ){
        conErr << "popd: dir stack is empty.\n";
        return 0;
    }
    if( argv.at(0) == '+' && isDigit(argv.at(1)) ){
        NnString *dir=(NnString*)dirstack.at(atoi(argv.chars()+1));
        if( dir == NULL ){
            conErr << "popd: " << argv << ": bad directory stack index\n";
            return 0;
        }else{
	    if( chdir_( *dir ) != 0 ){
		conErr << "popd: " << *dir << ": not found.\n";
		return 0;
	    }
        }
    }else{
        NnString *dir=(NnString*)dirstack.pop();
	chg_tilde();
	if( chdir_( *dir ) != 0 ){
	    conErr << "popd: " << *dir << ": not found.\n";
	    delete dir;
	    return 0;
	}
        delete dir;
    }
    return cmd_dirs(shell,argv);
}

int cmd_shift( NyadosShell &shell , const NnString & )
{
    shell.shift();
    return 0;
}

int cmd_goto( NyadosShell &shell , const NnString &argv )
{
    NnString label( argv );
    label.trim();
    shell.goLabel(label);
    return 0;
}

int cmd_pwd( NyadosShell &shell , const NnString & )
{
    NnString pwd;
    if( NnDir::getcwd( pwd ) == 0 ){
        conOut << pwd << '\n';
    }else{
        conErr << "can't get current working directory.\n";
    }
    return 0;
}


/* �����R�}���h chdir �������[�`�� */
int cmd_chdir( NyadosShell &shell , const NnString &argv )
{
    int basedir=0;
    NnString newdir;
    if( argv.empty() ){
	 /* ��������̏ꍇ */
	if( getEnv("HOME") == NULL &&
            getEnv("USERPROFILE") == NULL )
	    return cmd_pwd(shell,argv);
	newdir = "~";
    }else if( argv.startsWith("-") ){
	/* �I�v�V�������� */
	NnString tmp , opt;

	argv.splitTo( opt , tmp );
	if( opt.icompare("--basedir") == 0 ){
	    basedir = 1;
        }else if( opt.compare("-") == 0 ){
            if( prevdir.empty() ){
                conErr << opt << ": can not chdir.\n";
                return 0;
            }
            tmp = prevdir;
	}else{
	    conErr << opt << ": no such option.\n";
	    return 0;
	}
	NyadosShell::dequote( tmp.chars() , newdir );
    }else{
	NyadosShell::dequote( argv.chars() , newdir );
    }
    
    /* chdir ���� */
    if( chdir_( newdir ) == 0 )
        return 0;
    
    /* chdir ���̂����s�����ꍇ�A--basedir �I�v�V�����ŏ�������ꍇ������ */
    if( basedir ){
        int lastroot=NnDir::lastRoot( newdir.chars() );
        if( lastroot != -1 ){
            NnString bdir( newdir.chars() , lastroot );
            bdir << "\\."; // root�f�B���N�g���΍�
            if( chdir_( bdir ) == 0 )
                return 0;
        }
    }
    /* �G���[�I�� */
    conErr << newdir << ": no such file or directory.\n";
    return 0;
}
