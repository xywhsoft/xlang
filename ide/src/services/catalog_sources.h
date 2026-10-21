#ifndef XLANG_IDE_CATALOG_SOURCES_H
#define XLANG_IDE_CATALOG_SOURCES_H
#include "services/command_catalog.h"
#include "platform/text_file.h"
int xideCatalogDigest(const void* bytes,size_t size,char digest[65]);
/* Read only the snapshot whose raw digest is declared by the loaded catalog.
   Failure leaves no previously loaded text in the output. */
int xideCatalogSourceRead(const xide_command_catalog* catalog,const char* source,
    xide_text_file* file,xide_model_error* error);
#endif
