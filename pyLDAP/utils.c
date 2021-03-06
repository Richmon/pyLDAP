#include "utils.h"

/*	Converts char* to a lower-case form. Returns with the lower-cased char *. */
char *
lowercase(char *str) {
	int i;

	if (str == NULL) return NULL;

	for(i = 0; str[i]; i++){
		str[i] = tolower(str[i]);
	}
	return str;
}

/* Create a berval structure from a char*. */
struct berval *
createBerval(char *value) {
	struct berval *bval = NULL;
	bval = malloc(sizeof(struct berval));
	if (bval == NULL) return NULL;
	bval->bv_len = strlen(value);
	bval->bv_val = value;
	return bval;
}

/*	Converts a berval structure to a Python bytearray or if it's possible to string. */
PyObject *
berval2PyObject(struct berval *bval) {
	PyObject *bytes;
	PyObject *obj;

	bytes = PyBytes_FromStringAndSize(bval->bv_val, bval->bv_len);
	if (bytes == NULL) {
		PyErr_BadInternalCall();
		return NULL;
	}
	obj = PyUnicode_FromEncodedObject(bytes, NULL, NULL);
	/* Unicode converting is failed, set bytearray to return value. */
	if (obj == NULL) {
		obj = bytes;
	} else {
		Py_DECREF(bytes);
	}
	/* Check for errors. */
	if (PyErr_Occurred()) {
		/* Should be a reason why there is nothing about
		   PyExc_UnicodeDecodeError in the official documentation. */
		if (PyErr_ExceptionMatches(PyExc_UnicodeDecodeError) == 1) {
			/* UnicodeDecode error is excepted and will be ignored.*/
			PyErr_Clear();
		}
	}
	return obj;
}

LDAPMod *
createLDAPModFromItem(int mod_op, PyObject *key, PyObject *value) {
	LDAPMod *mod;

	mod = (LDAPMod *)malloc(sizeof(LDAPMod));
	if (mod == NULL) return NULL;

	mod->mod_op = mod_op;
	mod->mod_type = PyObject2char(key);
	mod->mod_vals.modv_bvals = PyList2BervalList(value);
	return mod;
}

/*	Converts Python simple objects (String, Long, Float, Boolean, Bytes, and None) to C string.
	If the `obj` is none of these types raise BadInternalCall() error and return NULL.
*/
char *
PyObject2char(PyObject *obj) {
	char *str = NULL;
	char *tmp = NULL;
	const wchar_t *wstr;
	Py_ssize_t length = 0;
	PyObject *ldapdn_type = load_python_object("pyLDAP.ldapdn", "LDAPDN");
	const unsigned int len = 24; /* The max length that a number's char* representation can be. */

	if (obj == NULL) return NULL;
	if (ldapdn_type == NULL) return NULL;

	/* If Python objects is a None return an empty("") char*. */
	if (obj == Py_None) {
		str = (char *)malloc(sizeof(char));
		str[0] = '\0';
		return str;
	}
	if (PyUnicode_Check(obj)) {
		/* Python string converting. From Python 3.3 could be use PyUnicode_AsUTF8AndSize(). */
		wstr = PyUnicode_AsWideCharString(obj, &length);
		str = (char *)malloc(sizeof(char) * (length + 1));
		if (str == NULL) return (char *)PyErr_NoMemory();
		wcstombs(str, wstr, length);
		/* Put the delimiter at the end. */
		str[length] = '\0';
	} else if (PyLong_Check(obj)) {
		/* Python integer converting. Could be longer, literally. */
		long int inum = PyLong_AsLong(obj);
		tmp = malloc(sizeof(char) * len);
		if (tmp == NULL) return (char *)PyErr_NoMemory();
		sprintf(tmp, "%ld", inum);
	} else if (PyFloat_Check(obj)) {
		/* Python floating point number converting. */
		double dnum = PyFloat_AsDouble(obj);
		tmp = malloc(sizeof(char) * len);
		if (tmp == NULL) return (char *)PyErr_NoMemory();
		sprintf(tmp, "%lf", dnum);
	} else if (PyBool_Check(obj)) {
		/* Python boolean converting to number representation (0 or 1). */
		if (obj == Py_True) {
			str = "1";
		} else {
			str = "0";
		}
	} else if (PyBytes_Check(obj)) {
		/* Python bytes converting. */
		tmp = PyBytes_AsString(obj);
		if (tmp == NULL) return NULL;
		str = (char *)malloc(sizeof(char) * (strlen(tmp) + 1));
		strcpy(str, tmp);
		return str;
	} else if (PyObject_IsInstance(obj, ldapdn_type)) {
		/* LDAPDN object converting. */
		PyObject *tmpobj = PyObject_Str(obj);
		str = PyObject2char(tmpobj);
		Py_DECREF(tmpobj);
	} else {
		PyErr_BadInternalCall();
		return NULL;
	}
	/* In case of converting numbers, optimizing the memory allocation. */
	if (tmp != NULL) {
		str = strdup(tmp);
		free(tmp);
	}
	return str;
}

struct berval **
PyList2BervalList(PyObject *list) {
	int i = 0;
	char *strvalue;
	struct berval **berval_arr = NULL;
	PyObject *iter;
	PyObject *item;

	if (list == NULL || !PyList_Check(list)) return NULL;

	berval_arr = (struct berval **)malloc(sizeof(struct berval *) * ((int)PyList_Size(list) + 1));
	iter = PyObject_GetIter(list);
	if (iter == NULL) return NULL;

	for (item = PyIter_Next(iter); item != NULL; item = PyIter_Next(iter)) {
		strvalue = PyObject2char(item);
		berval_arr[i++] = createBerval(strvalue);
		Py_DECREF(item);
	}
	Py_DECREF(iter);
	berval_arr[i] = NULL;
	return berval_arr;
}

/*	Converts Python list to a C string list. Retruns NULL if it's failed. */
char **
PyList2StringList(PyObject *list) {
	int i = 0;
	char **strlist;
	PyObject *iter;
	PyObject *item;

	if (list == NULL || !PyList_Check(list)) return NULL;

	strlist = malloc(sizeof(char*) * ((int)PyList_Size(list) + 1));
	iter = PyObject_GetIter(list);
	if (iter == NULL) return NULL;

	for (item = PyIter_Next(iter); item != NULL; item = PyIter_Next(iter)) {
		strlist[i++] = PyObject2char(item);
		Py_DECREF(item);
	}
	Py_DECREF(iter);
	strlist[i] = NULL;
	return strlist;
}

/*	Compare lower-case representations of two Python objects.
	Returns 1 they are matched, -1 if it's failed, and 0 otherwise. */
int
lowerCaseMatch(PyObject *o1, PyObject *o2) {
	int match = 0;
	char *str1 = lowercase(PyObject2char(o1));
	char *str2 = lowercase(PyObject2char(o2));

	if (str1 == NULL || str2 == NULL) return -1;

	if (strcmp(str1, str2) == 0) match = 1;

	free(str1);
	free(str2);

	return match;
}

/*	Load the `object_name` Python object from the `module_name` Python module.
	Returns the object or Py_None if it's failed.
 */
PyObject *
load_python_object(char *module_name, char *object_name) {
	PyObject *module, *object;

	module = PyImport_ImportModule(module_name);
	if (module == NULL) {
		PyErr_Format(PyExc_ImportError, "The import of %s is failed.", module_name);
		return NULL;
	}

	object = PyObject_GetAttrString(module, object_name);
	if (object == NULL) {
		PyErr_Format(PyExc_ImportError, "%s is not found in %s module.", object_name, module_name);
		Py_DECREF(module);
		return NULL;
	}

	Py_DECREF(module);
	return object;
}

PyObject *
get_error(char *error_name) {
	return load_python_object("pyLDAP.errors", error_name);
}

PyObject *
get_error_by_code(int code) {
	PyObject *error;
	PyObject *get_error = load_python_object("pyLDAP.errors", "get_error");
	if (get_error == NULL) return NULL;

	error = PyObject_CallFunction(get_error, "(i)", code);
	return error;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)

int _LDAP_initialization(LDAP **ld, PyObject *url) {
	int rc;
	int portnum;
	char *hoststr = NULL;
	const int version = LDAP_VERSION3;
	PyObject *scheme = PyObject_GetAttrString(url, "scheme");
	PyObject *host = PyObject_GetAttrString(url, "host");
	PyObject *port = PyObject_GetAttrString(url, "port");

	if (scheme == NULL || host == NULL || port == NULL) return -1;

	hoststr = PyObject2char(host);
	portnum = PyLong_AsLong(port);
	Py_DECREF(host);
	Py_DECREF(port);

	if (hoststr == NULL) return -1;

	if (PyUnicode_CompareWithASCIIString(scheme, "ldaps") == 0) {
		*ld = ldap_sslinit(hoststr, portnum, 1);
	} else {
		*ld = ldap_init(hoststr, portnum);
	}
	Py_DECREF(scheme);
	if (ld == NULL) return -1;
	ldap_set_option(*ld, LDAP_OPT_PROTOCOL_VERSION, &version);
	rc = ldap_connect(*ld, NULL);
	return rc;
}

int _LDAP_bind_s(LDAP *ld, char *mech, char* binddn, char *pswstr, char *authcid, char *realm, char *authzid) {
	int rc;
	int method = -1;
	SEC_WINNT_AUTH_IDENTITY creds;

	creds.User = (unsigned char*)authcid;
	if (authcid != NULL) creds.UserLength = strlen(authcid);
	else creds.UserLength = 0;
	creds.Password = (unsigned char*)pswstr;
	if (pswstr != NULL) creds.PasswordLength = strlen(pswstr);
	else creds.PasswordLength = 0;
	/* Is SASL realm equivalent with Domain? */
	creds.Domain = (unsigned char*)realm;
	if (realm != NULL) creds.DomainLength = strlen(realm);
	else creds.DomainLength = 0;
	creds.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

	/* Mechanism is set use SEC_WINNT_AUTH_IDENTITY. */
	if (mech != NULL) {
		if (strcmpi(mech, "DIGEST-MD5") == 0) {
			method = LDAP_AUTH_DIGEST;
		} else {
			method = LDAP_AUTH_SASL;
		}
		// TODO: it's depricated. Should use ldap_sasl_bind_sA instead?
		rc = ldap_bind_sA(ld, binddn, (PCHAR)&creds, method);
	} else {
		rc = ldap_simple_bind_sA(ld, binddn, pswstr);
	}

	return rc;
}

int _LDAP_unbind(LDAP *ld) {
	return ldap_unbind(ld);
}

int ldap_whoami_s(LDAP *ld, struct berval **authzid, LDAPControl **sctrls, LDAPControl **cctrls) {
	int rc = -1;

	rc = ldap_extended_operation_sA(ld, "1.3.6.1.4.1.4203.1.11.3", NULL, sctrls, cctrls, NULL, authzid);
	return rc;
}

#else

int _LDAP_initialization(LDAP **ld, PyObject *url) {
	int rc;
	char *addrstr;
	const int version = LDAP_VERSION3;

	PyObject *addr = PyObject_CallMethod(url, "get_address", NULL);
	if (addr == NULL) return -1;
	addrstr = PyObject2char(addr);
	Py_DECREF(addr);
	if (addrstr == NULL) return -1;

	rc = ldap_initialize(ld, addrstr);
	if (rc != LDAP_SUCCESS) return rc;

	ldap_set_option(*ld, LDAP_OPT_PROTOCOL_VERSION, &version);
	return rc;
}

int _LDAP_bind_s(LDAP *ld, char *mech, char* binddn, char *pswstr, char *authcid, char *realm, char *authzid) {
	int rc;
	LDAPControl	**sctrlsp = NULL;
	struct berval passwd;
	struct berval *servdata;
	void *defaults;

	/* Mechanism is set, use SASL interactive bind. */
	if (mech != NULL) {
		if (pswstr == NULL) pswstr = "";
		defaults = create_sasl_defaults(ld, mech, realm, authcid, pswstr, authzid);
		if (defaults == NULL) return -1;
		rc = ldap_sasl_interactive_bind_s(ld, binddn, mech, sctrlsp, NULL, LDAP_SASL_QUIET, sasl_interact, defaults);
	} else {
		if (pswstr == NULL) {
			passwd.bv_len = 0;
		} else {
			passwd.bv_len = strlen(pswstr);
		}
		passwd.bv_val = pswstr;
		rc = ldap_sasl_bind_s(ld, binddn, LDAP_SASL_SIMPLE, &passwd, sctrlsp, NULL, &servdata);
	}
	return rc;
}

int _LDAP_unbind(LDAP *ld) {
	return ldap_unbind_ext_s((ld), NULL, NULL);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
* The following three functions are copies from the OpenLDAP sasl.c source file.
* Basically, they are used with the "it's working why should touch anything" principle,
* but probably I should check them out in time.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void *
create_sasl_defaults(LDAP *ld, char *mech, char *realm, char *authcid, char *passwd, char *authzid) {
	lutilSASLdefaults *defaults;

	defaults = ber_memalloc(sizeof(lutilSASLdefaults));
	if(defaults == NULL) return (void *)PyErr_NoMemory();

	defaults->mech = mech ? ber_strdup(mech) : NULL;
	defaults->realm = realm ? ber_strdup(realm) : NULL;
	defaults->authcid = authcid ? ber_strdup(authcid) : NULL;
	defaults->passwd = passwd ? ber_strdup(passwd) : NULL;
	defaults->authzid = authzid ? ber_strdup(authzid) : NULL;

	if (defaults->mech == NULL) {
		ldap_get_option(ld, LDAP_OPT_X_SASL_MECH, &defaults->mech);
	}
	if (defaults->realm == NULL) {
		ldap_get_option(ld, LDAP_OPT_X_SASL_REALM, &defaults->realm);
	}
	if (defaults->authcid == NULL) {
		ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHCID, &defaults->authcid);
	}
	if (defaults->authzid == NULL) {
		ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHZID, &defaults->authzid);
	}
	defaults->resps = NULL;
	defaults->nresps = 0;

	return defaults;
}

static int
sasl_interaction(unsigned flags, sasl_interact_t *interact, lutilSASLdefaults *defaults) {
	const char *dflt = interact->defresult;

	switch(interact->id) {
		case SASL_CB_GETREALM:
			if (defaults) dflt = defaults->realm;
			break;
		case SASL_CB_AUTHNAME:
			if (defaults) dflt = defaults->authcid;
			break;
		case SASL_CB_PASS:
			if (defaults) dflt = defaults->passwd;
			break;
		case SASL_CB_USER:
			if (defaults) dflt = defaults->authzid;
			break;
		case SASL_CB_NOECHOPROMPT:
			break;
		case SASL_CB_ECHOPROMPT:
			break;
	}
	if (interact->len > 0) {
		/* duplicate */
		char *p = (char *)interact->result;
		interact->result = defaults->resps[defaults->nresps++];
		/* zap */
		memset( p, '\0', interact->len );
	} else {
		/* input must be empty */
		interact->result = dflt;
		interact->len = strlen(interact->result);
	}

	return LDAP_SUCCESS;
}

int
sasl_interact(LDAP *ld, unsigned flags, void *defaults, void *in) {
	int rc = 0;
	sasl_interact_t *interact = in;

	while (interact->id != SASL_CB_LIST_END) {
		rc = sasl_interaction(flags, interact, defaults);
		if (rc) return rc;
		interact++;
	}
	return LDAP_SUCCESS;
}
#endif
