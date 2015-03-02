#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static IShellLink *m_pShellLink;      // IShellLinkへのポインタ。
static IPersistFile *m_pPersistFile;  // IPersistFileへのポインタ。

static BOOL CT_Filer2View_LinkInit()
{
    HRESULT hRes;//各結果。

    //　メンバ変数を初期化します。
    m_pPersistFile = NULL;
    m_pShellLink = NULL;

    //　まず、ＯＬＥを使うために初期化しておきます。
    hRes = ::CoInitialize( NULL );

    if( hRes == E_OUTOFMEMORY )
	return FALSE;
    if( hRes == E_INVALIDARG )
	return FALSE;
    if( hRes == E_UNEXPECTED )
	return FALSE;

    //　空のインターフェイスを用意します。
    hRes = ::CoCreateInstance( CLSID_ShellLink, NULL,
    CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *)&m_pShellLink );

    if( hRes == CLASS_E_NOAGGREGATION )
	return FALSE;
    if( hRes == REGDB_E_CLASSNOTREG )
	return FALSE;


    //　IPersistFileへのポインタを取得します。
    hRes = m_pShellLink->QueryInterface( IID_IPersistFile, (LPVOID *)&m_pPersistFile );

    if( hRes != S_OK )
	return FALSE;

    return TRUE;
}

static BOOL CT_Filer2View_UnInit()
{
    //　IPersistFileへのポインタを破棄します。
    if( m_pPersistFile != NULL )
	m_pPersistFile->Release();

    //　IShellLinkへのポインタを破棄します。
    if( m_pShellLink != NULL )
	m_pShellLink->Release();

    ::CoUninitialize();

    return TRUE;
}

static BOOL CT_Filer2View_Load(LPCTSTR p_pchFile)
{
    HRESULT hRes;//各結果。
    OLECHAR ochLinkFile[MAX_PATH];

    //　ユニコードに変換します。
    ::MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, p_pchFile, -1,
			    ochLinkFile, MAX_PATH );

    //　ショートカットを読み込みます。
    hRes = m_pPersistFile->Load( ochLinkFile, STGM_READ );
    if( hRes != S_OK )
	return FALSE;

    //　リンクを決定します。
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
