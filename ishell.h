#ifndef ISHELL_H
#define ISHELL_H

#include "getline.h"
#include "shell.h"

class InteractiveShell : public NyadosShell {
    DosShell dosShell;
protected:
    Status readline(NnString &line);
public:
    virtual History *getHistoryObject(){ return &dosShell.history; }
    InteractiveShell(){}
    ~InteractiveShell(){}
    int operator ! () const { return 0; }
    const char *className() const { return "ishell"; }
};

#endif
