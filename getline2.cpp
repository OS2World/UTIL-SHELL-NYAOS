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

/* ���ϐ��̕⊮���s��.
 *    startpos �c ���ϐ��\���J�n�ʒu('%'�̈ʒu)
 *    endpos   �c ������(�Ȃ����� ��)
 *    wildcard �c �W�J�O������S��
 *    array    �c �⊮��������z��
 * return
 *     >=0 �⊮��␔.
 *     < 0 ���ϐ��̕⊮�̕K�v�͂Ȃ��A�����Ƃ��Ă͓W�J��������.
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
	/* ���ϐ������������Ă���̂ŁA�W�J����̂� */
	name.assign( wildcard.chars()+startpos+1 , endpos-startpos-1 );
	const char *value=getEnv( name.chars() , NULL );
	if( value != NULL ){
	    NnString tmp( wildcard.chars() , startpos );
	    tmp << value << (wildcard.chars()+endpos+1) ;
	    wildcard = tmp;
	}
	return -1;
    }else{
	/* ���ϐ������������Ă��Ȃ� �� ���ϐ�����⊮�̑ΏۂƂ��� */
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


/* �⊮��⃊�X�g���쐬����.
 *      region  �p�X���܂ޔ͈�(���p���܂�)
 *      array   ��⃊�X�g�������
 * return
 *      ��␔
 */
int GetLine::makeCompletionList( const NnString &region, NnVector &array )
{
    /* �f�B���N�g����r������R�}���h���ǂ����𔻒肷�� */
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

    /* ���p����S�č폜���� */
    path = region;
    path.dequote();

    int lastroot=NnDir::lastRoot( path.chars() );
    char rootchar=( lastroot != -1 && path.at(lastroot)=='/' ? '/' : '\\' );

    /* ���C���h�J�[�h������̍쐬 */
    NnString wildcard;
    NnDir::f2b( path.chars() , wildcard );
    if( wildcard.at(0) == '~' && 
	(isalnum(wildcard.at(1) & 255) || wildcard.at(1)=='_' ) && 
	lastroot == -1 )
    {
	/* ~ �Ŏn�܂�A���[�g���܂܂�Ă��炸�A�񕶎��ڂɉp����������ꍇ��
	 * �s���S�ȓ���t�H���_�[���������Ă���Ƃ݂Ȃ��B
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
    /* �p�X�Ɋ��ϐ����܂܂�Ă�����ϊ����Ă��� */
    /* %�c% �`���̊��ϐ��\�L */
    if( (i=wildcard.findOf("%")) >= 0 ){
	int j=wildcard.findOf("%",i+1);
	int result = expand_environemnt_variable(i,j,"%",wildcard,array);
	if( result >= 0 )
	    return result;
    }
    /* $�c �`���̊��ϐ��\�L */
    if( (i=wildcard.findOf("$")) >= 0 ){
	int result;
	if( wildcard.at(i+1) == '{' ){
	    /* ${�c} �`�� */
	    result = expand_environemnt_variable(
		    i+1, wildcard.findOf("}",i+2),
		    "}", wildcard,array );
	}else{
	    /* $�c �`�� */
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

    /* �f�B���N�g�������𒊏o���� */
    NnString basename;
    if( lastroot >= 0 )
        basename.assign( path.chars() , lastroot+1 );
  
    /* �����쐬���� */
    i=0;
    for( NnDir dir(wildcard) ; dir.more() ; dir.next() ){
        // �u.�v�u..�v��r������.
        if(    dir->at(0) == '.' 
            && (    dir->at(1) == '\0'
                || (dir->at(1) == '.' && dir->at(2)=='\0' ) )){
            continue;
        }
        // �����O�t�@�C�����̓}�b�`���Ȃ����A�V���[�g�t�@�C�������}�b�`
        // ���Ă��܂��P�[�X��r������.
	// (�������A���C���h�J�[�h���u�����I�Ɂv���p���Ă���ꍇ�͏���)
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
                // [ �S�⊮������ , �����t�@�C����(��f�B���N�g������) ] �̃y�A.
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
/* �R�}���h���⊮��⃊�X�g���쐬����.
 * (�{�N���X�ł́AmakeCompletionList �Ɠ����B�����\�b�h�͔h���N���X��
 *  �I�[�o�[���C�h�����)
 *      region - �Ώۂ̕�����
 *      array - �⊮��₪����x�N�^�[
 * return
 *      ��␔
 */
int GetLine::makeTopCompletionList( const NnString &region , NnVector &array )
{
    return makeCompletionListCore( region , array );
}

/* ������ s1 �� ������ s2 �Ƃ̋��ʕ����̒����𓾂� */
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

    /* �f�B���N�g���������܂܂Ȃ��t�@�C����(���X�g�\���̂��߂Ɏg��) */
    NnString &name_of(int n){
	return *(NnString*)((NnPair*)list.at(n))->second_or_first();
    }
    /* �t���p�X��(��Ɏ��ۂ̕⊮����̂��߂Ɏg��) */
    NnString &path_of(int n){
        return *(NnString*)((NnPair*)list.at(n))->first();  
    }
    int maxlen();
};

/* ��⃊�X�g�̍ő�T�C�Y�𓾂� */
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


/* �J�[�\���ʒu�̕������ǂ݂Ƃ��āA����������.
 *      r - �⊮��
 * return
 *    >=0 : ��␔
 *    -1  : �P�ꂪ�Ȃ�.
 */
int GetLine::read_complete_list( Completion &r )
{
    r.word=current_word( r.at , r.size );
    if( r.word.empty()  && properties.get("nullcomplete") == NULL )
	return -1;

    r.n = 0;
    NyaosLua L("complete");
    if( L.ok() && lua_isfunction(L,-1) ){
        /* �������F�x�[�X������ */
        NnString word( r.word );
        word.dequote();
        lua_pushstring( L , word.chars() );

        /* �������F������J�n�ʒu */
        lua_pushinteger( L , r.at );

        /* ��O�����F�⑫��� */
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
                    lua_pop(L,1); /* drop each value(next���g�����̖񑩎�) */
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
        // �⊮���X�g�쐬
        if( r.at == 0 ){
            makeTopCompletionList( r.word , r.list );
        }else{
            makeCompletionList( r.word , r.list );
        }
    }
    return r.n = r.list.size();
}

/* �J�[�\���ʒu�`size�����O�܂ł̕����̒u�� & �ĕ\�����s���B
 *	size - �u�����T�C�Y
 *	match - �u��������
 * ����
 * �Esize < match.length() �ł���K�v������B
 * �E�J�[�\���� at+size �̈ʒu�ɂ��邱�Ƃ��K�v
 */
void GetLine::replace_repaint_here( int size , const NnString &match )
{
    int at=pos-size;
    buffer.replace( at, size , match.chars() );
    int oldpos=pos;
    pos += match.length() - size ;

    // �\���̍X�V.
    if( pos >= offset+width ){
        // �J�[�\���ʒu���E�C���h�E�O�ɂ���Ƃ�.
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


/* �⊮�L�[ �Ή����\�b�h
 */
Status GetLine::complete(int)
{
    Completion comp;
    int i,n,hasSpace=0;

    if( (n=read_complete_list(comp)) <= 0 )
	return NEXTCHAR;
    
    if( comp.word.findOf(" \t\r\n!") != -1 )
	hasSpace = 1;

    // �⊮���̑������Ƃ肠�����o�b�t�@�փR�s�[.
    NnString match=comp.path_of(0);
    if( strchr( match.chars() , ' ' ) != NULL || comp.word.at(0) == '"' )
	hasSpace = 1;

    // ��₪��������ꍇ�́A
    // ��Ԗڈȍ~�̌��Ɣ�r���Ă䂫�A���ʕ����������c���Ă䂭.
    for( i=1; i < n ; ++i ){
	if( strchr( comp.path_of(i).chars() , ' ' ) != NULL )
	    hasSpace = 1;
	match.chop( sameLength( comp.path_of(i).chars() , match.chars() )); 
    }

    // (��{�I�ɂ��肦�Ȃ���)�A�⊮���̕����A����������Z���ꍇ��
    // ���������I������B
    if( match.length() < comp.size && ! hasSpace )
	return NEXTCHAR;
    
    // ���C���h�J�[�h�⊮�����Ƃ��ɁA���̋��ʕ����S���Ȃ��ꍇ�A
    // ���p�������ɂȂ�̂�h���c
    if( match.length() == 0 )
	return NEXTCHAR;
    
#ifdef NYACUS
    /* �V���[�g�J�b�g�W�J */
    if( n==1 && comp.path_of(0).iendsWith(".lnk") && properties.get("lnkexp") != NULL )
    {
	/* ��₪��ŁA���ꂪ�V���[�g�J�b�g�̏ꍇ */

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

    // �󔒂��܂ނƂ��A�擪�Ɂh������B
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

    if( n==1 && match.length() > 0 ){ // ��₪������Ȃ��ꍇ�́c
	int tail=match.lastchar();
	// �󔒂�����ꍇ�́A���p�������B
	if( hasSpace )
	    match += '"';
	if( tail != '\\' && tail != '/' ){
	    // ��f�B���N�g���̏ꍇ�́A�����ɋ󔒂�����B
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
    // ��⊮������𔲂��o��.
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
	    /* �P��̐擪����ʒ[���O�ɂȂ��Ă��܂��Ă��� */
	    bs = pos-offset ;
	    /* ��ʒ[�܂ŃJ�[�\����߂� */
	}else{
	    /* �P��̐擪�͉�ʏ�Ɍ����Ă��� */
	    bs = baseSize;
	    /* �P��[�܂ŃJ�[�\����߂� */
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
		/* ����� */
		if( ++i >= comp.list.size() ) 
		    i=0;
	    }else if(  which_command(key) == &GetLine::complete_prev 
		    || which_command(key) == &GetLine::previous
		    || which_command(key) == &GetLine::vz_previous ){
		/* �O��� */
		if( --i < 0 )
		    i=comp.list.size()-1;
	    }else if(   which_command(key) == &GetLine::erase_all 
		     || which_command(key) == &GetLine::abort ){
		/* �L�����Z�� */
		eraseSeal( sealSize );
		/* �J�[�\���ʒu��߂� */
		while( bs-- > 0 )
		   putchr( buffer[pos++] );
		return NEXTCHAR;
	    }else if(   which_command(key) == &GetLine::erase_or_listing
		     || which_command(key) == &GetLine::listing 
		     || which_command(key) == &GetLine::complete_or_listing ){
		/* ���X�g�o�� */
		listing_( comp );
		sealSize = 0;
		continue;
	    }else{ /* �m�� */
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

    /* size������O�`�J�[�\���ʒu�܂ł�u�����Ă���� */
    replace_repaint_here( comp.size , buf );
    if( which_command(key) != &GetLine::enter )
	return interpret(key);

    return NEXTCHAR;
}

/* �^�u�L�[�̏����B�⊮���邩�A���X�g���o���B
 */
Status GetLine::complete_or_listing(int key)
{
    if( lastKey == key )
        return this->listing(key);
    else
        return this->complete(key);
}

