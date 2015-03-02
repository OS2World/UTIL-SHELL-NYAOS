#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static IShellLink *m_pShellLink;      // IShellLink�ւ̃|�C���^�B
static IPersistFile *m_pPersistFile;  // IPersistFile�ւ̃|�C���^�B

static BOOL CT_Filer2View_LinkInit()
{
    HRESULT hRes;//�e���ʁB

    //�@�����o�ϐ������������܂��B
    m_pPersistFile = NULL;
    m_pShellLink = NULL;

    //�@�܂��A�n�k�d���g�����߂ɏ��������Ă����܂��B
    hRes = ::CoInitialize( NULL );

    if( hRes == E_OUTOFMEMORY )
	return FALSE;
    if( hRes == E_INVALIDARG )
	return FALSE;
    if( hRes == E_UNEXPECTED )
	return FALSE;

    //�@��̃C���^�[�t�F�C�X��p�ӂ��܂��B
    hRes = ::CoCreateInstance( CLSID_ShellLink, NULL,
    CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *)&m_pShellLink );

    if( hRes == CLASS_E_NOAGGREGATION )
	return FALSE;
    if( hRes == REGDB_E_CLASSNOTREG )
	return FALSE;


    //�@IPersistFile�ւ̃|�C���^���擾���܂��B
    hRes = m_pShellLink->QueryInterface( IID_IPersistFile, (LPVOID *)&m_pPersistFile );

    if( hRes != S_OK )
	return FALSE;

    return TRUE;
}

static BOOL CT_Filer2View_UnInit()
{
    //�@IPersistFile�ւ̃|�C���^��j�����܂��B
    if( m_pPersistFile != NULL )
	m_pPersistFile->Release();

    //�@IShellLink�ւ̃|�C���^��j�����܂��B
    if( m_pShellLink != NULL )
	m_pShellLink->Release();

    ::CoUninitialize();

    return TRUE;
}

static BOOL CT_Filer2View_Load(LPCTSTR p_pchFile)
{
    HRESULT hRes;//�e���ʁB
    OLECHAR ochLinkFile[MAX_PATH];

    //�@���j�R�[�h�ɕϊ����܂��B
    ::MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, p_pchFile, -1,
			    ochLinkFile, MAX_PATH );

    //�@�V���[�g�J�b�g��ǂݍ��݂܂��B
    hRes = m_pPersistFile->Load( ochLinkFile, STGM_READ );
    if( hRes != S_OK )
	return FALSE;

    //�@�����N�����肵�܂��B
    hRes = m_pShellLink->Resolve( NULL, SLR_UPDATE );
    if( hRes != NOERROR )
	return FALSE;

    return TRUE;
}

int read_shortcut(const char *src,char *buffer,int size)
{
    WIN32_FIND_DATA f;
    int rc=0;

    // printf("(read_shortcut) src=[%s]\n",src);

    if( CT_Filer2View_LinkInit() != TRUE )
	return -1;
    
    if( CT_Filer2View_Load( src ) != TRUE 
	|| m_pShellLink->GetPath( buffer , size, &f, 0 ) != NOERROR )
	rc = -2;
    
    CT_Filer2View_UnInit();
    return rc;
}
