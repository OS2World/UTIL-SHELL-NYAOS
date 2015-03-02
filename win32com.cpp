#include "win32com.h"

//#define DEBUG

#include <stdlib.h>

#ifdef DEBUG
#  define DBG(x) (x)
#  include <stdio.h>
#else
#  define DBG(x)
#endif

BSTR Unicode::c2b(const char *s)
{
    size_t csize=strlen(s);
    size_t wsize=MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,s,csize,NULL,0);
    BSTR w=SysAllocStringLen(NULL,wsize );
    if( w == NULL ){
#ifdef DEBUG
        puts("memory allocation error");
#endif
        return NULL;
    }
    MultiByteToWideChar(CP_ACP,0,s,csize,w,wsize);
    return w;
}

char *Unicode::b2c(BSTR w)
{
    size_t csize=WideCharToMultiByte(CP_ACP,0,(OLECHAR*)w,-1,NULL,0,NULL,NULL);
    char *p=new char[ csize+1 ];
    WideCharToMultiByte(CP_ACP,0,(OLECHAR*)w,-1,p,csize,NULL,NULL);
    return p;
}

Variants::~Variants()
{
    if( v != NULL ){
        for(int i=0;i<size_;++i){
            VariantClear( &v[i] );
        }
        free(v);
    }
}

int ActiveXObject::instance_count = 0;

ActiveXObject::~ActiveXObject()
{
    if( pApplication != NULL )
        pApplication->Release();
    if( --instance_count <= 0 )
        OleUninitialize();
}

ActiveXObject::ActiveXObject(const char *name,bool isNewInstance)
{
    CLSID clsid;

    this->pApplication = NULL;
    construct_error_ = 0;

    if( instance_count++ <= 0 )
        OleInitialize(NULL);

    Unicode className(name);

    /* クラスID取得 */
    construct_error_ = CLSIDFromProgID( className , &clsid );
    if( FAILED(construct_error_) ){
#ifdef DEBUG
        puts("Error: CLSIDFromProgID");
#endif
        return;
    }

    if( isNewInstance ){ // create_object
        /* インスタンス作成 */
        construct_error_ = CoCreateInstance(
                clsid ,
                NULL ,
                CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER ,
                IID_IDispatch ,
                (void**)&this->pApplication );
        if( FAILED(construct_error_) ){
#ifdef DEBUG
            puts("Error: CoCreateInstance");
#endif
            if( pApplication != NULL ){
                pApplication->Release();
                pApplication = NULL;
            }
        }
    }else{ // get_object
        IUnknown *pUnknown=NULL;
        construct_error_ = GetActiveObject( clsid , 0 , &pUnknown );
        if( FAILED(construct_error_) ){
            if( pUnknown != NULL )
                pUnknown->Release();
            return;
        }
        construct_error_ = pUnknown->QueryInterface(
                IID_IDispatch ,
                (void**)&this->pApplication );
        if( pUnknown != NULL )
            pUnknown->Release();
        if( FAILED(construct_error_) ){
            if( pApplication != NULL ){
                pApplication->Release();
                pApplication = NULL;
            }
        }
    }
}

int ActiveXObject::const_load(
        void *L ,
        void (*setter)(void *L,const char *,VARIANT &) )
{
    ITypeInfo *pTypeInfo;
    LCID lcid = LOCALE_SYSTEM_DEFAULT;

    HRESULT hr=pApplication->GetTypeInfo(0,lcid,&pTypeInfo);
    if( FAILED(hr) )
        return -1;

    ITypeLib *pTypeLib;
    unsigned int index;
    hr=pTypeInfo->GetContainingTypeLib(&pTypeLib,&index);
    pTypeInfo->Release();

    int n=pTypeLib->GetTypeInfoCount();
    for( int i=0 ; i < n ; i++ ){
        hr = pTypeLib->GetTypeInfo(i,&pTypeInfo);
        if( FAILED(hr) )
            continue;
        TYPEATTR *pTypeAttr;
        pTypeInfo->GetTypeAttr(&pTypeAttr);
        if( FAILED(hr) ){
            pTypeInfo->Release();
            continue;
        }
        for( int j=0 ; j < pTypeAttr->cVars ; j++ ){
            VARDESC *pVarDesc;
            hr = pTypeInfo->GetVarDesc(j,&pVarDesc);
            if( FAILED(hr) )
                continue;
            if( pVarDesc->varkind == VAR_CONST &&
                    ( pVarDesc->wVarFlags & (
                        VARFLAG_FHIDDEN |
                        VARFLAG_FRESTRICTED |
                        VARFLAG_FNONBROWSABLE ) )==0 )
            {
                BSTR bstr;
                unsigned int len;

                hr = pTypeInfo->GetNames(pVarDesc->memid,&bstr,1,&len);
                if( FAILED(hr) || len==0 || bstr==0 )
                    continue;
                
                char *name=Unicode::b2c(bstr);
                SysFreeString(bstr);

                (*setter)(L , name , *pVarDesc->lpvarValue );

                delete[]name;
            }

        }
        pTypeInfo->Release();
    }
    pTypeLib->Release();
    return 0;
}


ActiveXMember::ActiveXMember( ActiveXObject &instance , const char *name ) : instance_(instance)
{
    Unicode methodName(name);

    this->construct_error_ = instance.getIDispatch()->GetIDsOfNames(
            IID_NULL , /* 将来のための予約フィールド */
            &methodName ,
            1 ,
            LOCALE_USER_DEFAULT , 
            &dispid_ );
}

int ActiveXMember::invoke(
        WORD wflags ,
        VARIANT *argv ,
        int argc , 
        VARIANT &result ,
        HRESULT *pHr ,
        char **error_info )
{
    HRESULT hr;
    DISPPARAMS disp_params;
    DISPID dispid_propertyput=DISPID_PROPERTYPUT;
    UINT puArgerr = 0;
    EXCEPINFO excepinfo;

    disp_params.cArgs  = argc ;
    disp_params.rgvarg = argv ;

    if( wflags & DISPATCH_PROPERTYPUT ){
        disp_params.cNamedArgs = 1;
        disp_params.rgdispidNamedArgs = &dispid_propertyput;
    }else{
        disp_params.cNamedArgs = 0;
        disp_params.rgdispidNamedArgs = NULL;
    }

    excepinfo.bstrDescription = 0;
    excepinfo.bstrSource = 0;
    excepinfo.bstrHelpFile = 0;

    hr = this->instance_.getIDispatch()->Invoke(
            this->dispid() ,
            IID_NULL ,
            LOCALE_SYSTEM_DEFAULT ,
            wflags ,
            &disp_params ,
            &result ,
            &excepinfo ,
            &puArgerr );
    if( pHr != NULL ){
        *pHr = hr;
    }
    int rc=0;
    if( FAILED(hr) ){
        if( hr == DISP_E_EXCEPTION && error_info != 0 ){
            *error_info = Unicode::b2c( excepinfo.bstrDescription );
        }
        rc = -1;
    }
    if( excepinfo.bstrDescription )
        SysFreeString( excepinfo.bstrDescription );
    if( excepinfo.bstrSource )
        SysFreeString( excepinfo.bstrSource );
    if( excepinfo.bstrHelpFile )
        SysFreeString( excepinfo.bstrHelpFile );
    return rc;
}

int ActiveXObject::invoke(
        const char *name,
        WORD wflags ,
        VARIANT *argv ,
        int argc ,
        VARIANT &result ,
        HRESULT *hr,
        char **error_info 
        )
{
    ActiveXMember method( *this , name );
    if( ! method.ok() )
        return -1;

    return method.invoke(wflags,argv,argc,result,hr,error_info);
}

void Variants::grow()
{
    v = static_cast<VARIANTARG*>( realloc(v,++size_*sizeof(VARIANTARG)) );
    VariantInit( &v[size_-1] );
}

void Variants::add_as_string(const char *s)
{
    grow();
    v[size_-1].vt = VT_BSTR;
    v[size_-1].bstrVal = Unicode::c2b(s);
}

void Variants::add_as_number(double d)
{
    grow();
    v[size_-1].vt     = VT_R8;
    v[size_-1].dblVal = d;
}

void Variants::add_as_boolean(int n)
{
    grow();
    v[size_-1].vt = VT_BOOL;
    v[size_-1].boolVal = n ? VARIANT_TRUE : VARIANT_FALSE ;
}

void Variants::add_as_null()
{
    grow();
    v[size_-1].vt = VT_NULL;
}

VARIANT *Variants::add_anything()
{
    grow();
    return &v[size_-1];
}

ActiveXIterator::ActiveXIterator( ActiveXObject &parent )
{
    DBG( puts("[Enter] new ActiveXIterator") );
    VariantInit( &var_ );
    status_ = true;

    unsigned argErr;

    EXCEPINFO excepinfo;
    memset(&excepinfo, 0, sizeof(excepinfo));

    DISPPARAMS dispParams;
    dispParams.rgvarg = NULL;
    dispParams.rgdispidNamedArgs = NULL;
    dispParams.cNamedArgs = 0;
    dispParams.cArgs = 0;

    HRESULT hr = parent.getIDispatch()->Invoke(
                DISPID_NEWENUM , 
                IID_NULL ,
                LOCALE_SYSTEM_DEFAULT ,
                DISPATCH_METHOD | DISPATCH_PROPERTYGET ,
                &dispParams ,
                &var_ ,
                &excepinfo ,
                &argErr );

    if( FAILED(hr) ){
        DBG( puts("[Fail] new ActiveXIterator (Invoke Error)") );
        status_ = false;
        pEnumVariant_ = 0;
    }else{
        hr = var_.pdispVal->QueryInterface(
                IID_IEnumVARIANT ,
                (void**) &pEnumVariant_ );
        if( FAILED(hr) ){
            DBG( puts("[Fail] new ActiveXIterator (QueryInterface)") );
            status_ = false;
        }
    }
    VariantClear( &var_ );
    VariantInit( &var_ );
    DBG( puts("[Leave] new ActiveXIterator") );
}

ActiveXIterator::~ActiveXIterator()
{
    if( pEnumVariant_ != NULL )
        pEnumVariant_->Release();
}

bool ActiveXIterator::nextObj()
{
    DBG( puts("[Enter] ActiveXIterator::nextObj") );
    if( ! ok() ){
        DBG( puts("[Leave] ActiveXIterator::nextObj ( !ok() )") );
        return false;
    }

    VariantClear( &var_ );
    VariantInit( &var_ );
    if( pEnumVariant_->Next(1,&var_,NULL) != S_OK ){
        DBG( puts("[Leave] ActiveXIterator::nextObj ( Next()!=S_OK )") );
        VariantClear( &var_ );
        return false;
    }
    DBG( puts("[Leave] ActiveXIterator::nextObj (true)") );
    return true;
}
