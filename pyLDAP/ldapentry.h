#ifndef LDAPENTRY_H_
#define LDAPENTRY_H_

#include <Python.h>
#include "structmember.h"

//MS Windows
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)

#include <windows.h>
#include <winldap.h>

//Unix
#else
#include <ldap.h>

#endif

#include "ldapclient.h"
#include "ldapvaluelist.h"

typedef struct {
    PyDictObject dict;
    PyObject *dn;
    UniqueList *attributes;
    UniqueList *deleted;
    LDAPClient *client;
} LDAPEntry;

extern PyTypeObject LDAPEntryType;

LDAPEntry *LDAPEntry_New(void);
int LDAPEntry_Check(PyObject *obj);
LDAPMod **LDAPEntry_CreateLDAPMods(LDAPEntry *self);
void LDAPEntry_DismissLDAPMods(LDAPEntry *self, LDAPMod **mods);
LDAPEntry *LDAPEntry_FromLDAPMessage(LDAPMessage *entrymsg, LDAPClient *client);
int LDAPEntry_UpdateFromDict(LDAPEntry *self, PyObject *dict);
int LDAPEntry_UpdateFromSeq2(LDAPEntry *self, PyObject *seq);
PyObject *LDAPEntry_GetItem(LDAPEntry *self, PyObject *key);
PyObject *LDAPEntry_GetItemString(LDAPEntry *self, const char *key);
int LDAPEntry_SetItem(LDAPEntry *self, PyObject *key, PyObject *value);
int LDAPEntry_SetClient(LDAPEntry *self, LDAPClient *client);
int LDAPEntry_SetStringDN(LDAPEntry *self, char *value);

#endif /* LDAPENTRY_H_ */
