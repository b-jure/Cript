/*
** csloadlib.c
** Dynamic library loader for CScript
** See Copyright Notice in cscript.h
*/

#define cloadlib_c
#define CS_LIB

#include <string.h>
#include <stdlib.h>

#include "cscript.h"

#include "ctrace.h"
#include "cauxlib.h"
#include "cslib.h"


/* prefix for open functions in C libraries */
#define CS_POF	    "csopen_"

/* separator for open functions in C libraries */
#define CS_OFSEP    "_"


/*
** Key for fulluserdata in the global table that keeps handles
** for all loaded C libraries.
*/
static const char *const CLIBS = "__CLIBS";

#define LIB_FAIL    "open"


#define setprogdir(C)       ((void)0)


/*
** Unload library 'lib' and return 0.
** In case of error, returns non-zero plus an error string
** in the stack.
*/
static int csys_unloadlib(cs_State *C, void *lib);

/*
** Load C library in file 'path'. If 'global', load with all names
** in the library global.
** Returns the library; in case of error, returns NULL plus an error
** string in the stack.
*/
static void *csys_load(cs_State *C, const char *path, int global);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static cs_CFunction csys_symbolf(cs_State *C, void *lib, const char *sym);


#if defined(CS_USE_DLOPEN)  /* { */

#include <dlfcn.h>

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__ (cs_CFunction)(p))
#else
#define cast_func(p) ((cs_CFunction)(p))
#endif


static int csys_unloadlib(cs_State *C, void *lib) {
    int res = dlclose(lib);
    if (c_unlikely(res != 0))
        cs_push_fstring(C, dlerror());
    return res;
}


static void *csys_load(cs_State *C, const char *path, int global) {
    void *lib = dlopen(path, RTLD_LAZY | (global ? RTLD_GLOBAL : RTLD_LOCAL));
    if (c_unlikely(lib == NULL))
        cs_push_fstring(C, dlerror());
    return lib;
}


static cs_CFunction csys_symbolf(cs_State *C, void *lib, const char *sym) {
    cs_CFunction f;
    const char *msg;
    dlerror(); /* clear any old error conditions before calling 'dlsym' */
    f = cast_func(dlsym(lib, sym));
    if (c_unlikely(f == NULL && (msg = dlerror()) != NULL))
        cs_push_fstring(C, msg);
    return f;
}


#elif defined(CS_DL_DLL)    /* }{ */

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(CS_LLE_FLAGS)
#define CS_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of CS_EXEC_DIR with the executable's path.
*/
static void setprogdir(cs_State *C) {
    char buff[MAX_PATH + 1];
    char *lb;
    DWORD nsize = sizeof(buff)/sizeof(char);
    DWORD n = GetModuleFileNameA(NULL, buff, nsize); /* get exec. name */
    if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
        csL_error(C, "unable to get ModuleFileName");
    else {
        *lb = '\0'; /* cut name on the last '\\' to get the path */
        csL_gsub(C, cs_to_string(C, -1), CS_EXEC_DIR, buff);
        cs_remove(C, -2); /* remove original string */
    }
}



static void pusherror(cs_State *C) {
    int error = GetLastError();
    char buffer[128];
    if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
        cs_push_string(C, buffer);
    else
        cs_push_fstring(C, "system error %d\n", error);
}

static int csys_unloadlib(cs_State *C, void *lib) {
    int res = FreeLibrary((HMODULE)lib);
    if (c_unlikely(res == 0)) pusherror(C);
    return res;
}


static void *csys_load(cs_State *C, const char *path, int global) {
    HMODULE lib = LoadLibraryExA(path, NULL, CS_LLE_FLAGS);
    (void)(global); /* not used: symbols are 'global' by default */
    if (lib == NULL) pusherror(C);
    return lib;
}


static cs_CFunction csys_symbolf(cs_State *C, void *lib, const char *sym) {
    cs_CFunction f = (cs_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
    if (f == NULL) pusherror(C);
    return f;
}

#else                       /* }{ */

#undef LIB_FAIL
#define LIB_FAIL    "absent"

#define DLMSG   "dynamic libraries not enabled; check your CScript installation"

static int csys_unloadlib(cs_State *C, void *lib) {
    (void)(lib); /* unused */
    cs_push_literal(C, DLMSG);
    return 1;
}


static void *csys_load(cs_State *C, const char *path, int global) {
    (void)(path); (void)(global); /* unused */
    cs_push_literal(C, DLMSG);
    return NULL;
}


static cs_CFunction csys_symbolf(cs_State *C, void *lib, const char *sym) {
    (void)(lib); (void)(sym); /* unused */
    cs_push_literal(C, DLMSG);
    return NULL;
}

#endif                      /* } */


static int searcher_preload(cs_State *C) {
    const char *name = csL_check_string(C, 1);
    cs_push_globaltable(C); /* get __G table */
    cs_get_fieldstr(C, -1, CS_PRELOAD_TABLE); /* get __PRELOAD table */
    if (cs_get_fieldstr(C, -1, name) == CS_TNIL) { /* not found? */
        cs_push_fstring(C, "no field package.preload[\"%s\"]", name);
        return 1;
    } else {
        cs_push_literal(C, ":preload:");
        return 2;
    }
}


/*
** Return package C library.
*/
static void *checkclib(cs_State *C, const char *path) {
    void *plib;
    cs_get_global(C, CLIBS); /* get CLIBS userdata */
    cs_get_uservalue(C, -1, 2); /* get query table uservalue */
    cs_get_fieldstr(C, -1, path);
    plib = cs_to_userdata(C, -1); /* plib = qtable[path] */
    cs_pop(C, 3); /* pop CLIBS table, query table and 'plib' */
    return plib;
}


/*
** Adds 'plib' to CLIBS.
** (CLIBS query table)      qtable[path] = plib
** (CLIBS libraries array)  arr[len(arr)] = plib
*/
static void addtoclibs(cs_State *C, const char *path, void *plib) {
    cs_get_global(C, CLIBS); /* get CLIBS userdata */
    cs_get_uservalue(C, -1, 1); /* get array of libraries */
    cs_get_uservalue(C, -2, 2); /* get query table */
    cs_push_lightuserdata(C, plib);
    cs_push(C, -1);
    cs_set_index(C, -4, cs_len(C, -4)); /* arr[len(arr)] = plib */
    cs_set_fieldstr(C, -2, path); /* qtable[path] = plib */
    cs_pop(C, 3); /* pop array, qtable and CLIBS userdata */
}


/* error codes for 'lookforfunc' */
#define ERRLIB		1 /* unable to load library */
#define ERRFUNC		2 /* unable to find function */

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 and 'true' or a function in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc(cs_State *C, const char *path, const char *sym) {
    void *reg = checkclib(C, path); /* check loaded C libraries */
    if (reg == NULL) { /* must load library? */
        reg = csys_load(C, path, *sym == '*'); /* global symbols if 'sym'=='*' */
        if (reg == NULL) return ERRLIB; /* unable to load library */
        addtoclibs(C, path, reg);
    }
    if (*sym == '*') { /* loading only library (no function)? */
        cs_push_bool(C, 1); /* return 'true' */
    } else {
        cs_CFunction f = csys_symbolf(C, reg, sym);
        if (f == NULL) return ERRFUNC; /* unable to find function */
        cs_push_cfunction(C, f); /* else create new function */
    }
    return 0; /* no errors */
}


static int l_loadlib(cs_State *C) {
    const char *path = csL_check_string(C, 0);
    const char *init = csL_check_string(C, 1);
    int res = lookforfunc(C, path, init);
    if (c_likely(res == 0)) /* no errors? */
        return 1; /* return the loaded function */
    else { /* error; error message is on top of the stack */
        csL_push_fail(C);
        cs_insert(C, -2);
        cs_push_string(C, (res == ERRLIB) ? LIB_FAIL : "init");
        return 3;  /* return fail, error message, and where */
    }
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
    char *sep;
    char *name = *path;
    if (name == end) {
        return NULL; /* no more names */
    } else if (*name == '\0') { /* from previous iteration? */
        *name = *CS_PATH_SEP; /* restore separator */
        name++;  /* skip it */
    }
    sep = strchr(name, *CS_PATH_SEP); /* find next separator */
    if (sep == NULL) /* separator not found? */
        sep = end; /* name goes until the end */
    *sep = '\0'; /* finish file name */
    *path = sep; /* will start next search from here */
    return name;
}


static int readable (const char *filename) {
    FILE *f = fopen(filename, "r"); /* try to open file */
    if (f == NULL) return 0; /* open failed */
    fclose(f);
    return 1;
}


/*
** Given a path such as "blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (cs_State *C, const char *path) {
    csL_Buffer b;
    csL_buff_init(C, &b);
    csL_buff_push_string(&b, "no file \"");
    csL_buff_push_gsub(&b, path, CS_PATH_SEP, "'\n\tno file \"");
    csL_buff_push_string(&b, "\"");
    csL_buff_end(&b);
}


static const char *searchpath(cs_State *C, const char *name, const char *path,
                              const char *sep, const char *dirsep) {
    csL_Buffer buff;
    char *pathname; /* path with name inserted */
    char *endpathname; /* its end */
    const char *filename;
    /* separator is non-empty and appears in 'name'? */
    if (*sep != '\0' && strchr(name, *sep) != NULL)
        name = csL_gsub(C, name, sep, dirsep);  /* replace it by 'dirsep' */
    csL_buff_init(C, &buff);
    /* add path to the buffer, replacing marks ('?') with the file name */
    csL_buff_push_gsub(&buff, path, CS_PATH_MARK, name);
    csL_buff_push(&buff, '\0');
    pathname = csL_buffptr(&buff); /* writable list of file names */
    endpathname = pathname + csL_bufflen(&buff) - 1;
    while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
        if (readable(filename)) /* does file exist and is readable? */
            return cs_push_string(C, filename); /* save and return name */
    }
    csL_buff_end(&buff); /* push path to create error message */
    pusherrornotfound(C, cs_to_string(C, -1)); /* create error message */
    return NULL; /* not found */
}


static int l_searchpath(cs_State *C) {
    const char *fname = searchpath(C, csL_check_string(C, 1),
                                      csL_check_string(C, 2),
                                      csL_opt_string(C, 3, "."),
                                      csL_opt_string(C, 4, CS_DIRSEP));
    if (fname != NULL) {
        return 1;
    } else { /* error message is on top of the stack */
        csL_push_fail(C);
        cs_insert(C, -2);
        return 2; /* return fail + error message */
    }
}


static const char *findfile(cs_State *C, const char *name, const char *pname,
                            const char *dirsep) {
    const char *path;
    cs_get_fieldstr(C, cs_upvalueindex(0), pname);
    path = cs_to_string(C, -1);
    if (c_unlikely(path == NULL))
        csL_error(C, "'package.%s' must be a string", pname);
    return searchpath(C, name, path, ".", dirsep);
}


static int checkload(cs_State *C, int res, const char *filename) {
    if (c_likely(res)) { /* module loaded successfully? */
        cs_push_string(C, filename); /* will be 2nd argument to module */
        return 2; /* return open function and file name */
    } else
        return csL_error(C, "error loading module '%s' from file '%s':\n\t%s",
                            cs_to_string(C, 1), filename, cs_to_string(C, -1));
}


static int searcher_CScript(cs_State *C) {
    const char *filename;
    const char *name = csL_check_string(C, 1);
    filename = findfile(C, name, "path", CS_DIRSEP);
    if (filename == NULL) return 1; /* module not found in this path */
    return checkload(C, (csL_loadfile(C, filename) == CS_OK), filename);
}


static const cs_Entry package_funcs[] = {
    {"loadlib", l_loadlib},
    {"searchpath", l_searchpath},
    /* placeholders */
    {"preload", NULL},
    {"cpath", NULL},
    {"path", NULL},
    {"searchers", NULL},
    {"loaded", NULL},
    {NULL, NULL}
};


static void findloader(cs_State *C, const char *name) {
    int i;
    csL_Buffer msg; /* to build error message */
    /* push 'package.searchers' array to index 2 in the stack */
    if (c_unlikely(cs_get_fieldstr(C, cs_upvalueindex(0), "searchers")
                != CS_TARRAY))
        csL_error(C, "'package.searchers' must be array value");
    csL_buff_init(C, &msg);
    /* iterate over available searchers to find a loader */
    for (i = 1; ; i++) {
        csL_buff_push_string(&msg, "\n\t"); /* error-message prefix */
        if (c_unlikely(cs_get_index(C, 2, i) == CS_TNIL)) { /* no more searchers? */
            cs_pop(C, 1); /* remove nil */
            csL_buffsub(&msg, 2); /* remove prefix */
            csL_buff_end(&msg); /* create error message */
            csL_error(C, "module '%s' not found:%s", name, cs_to_string(C, -1));
        }
        cs_push_string(C, name);
        cs_call(C, 1, 2); /* call it */
        if (cs_is_function(C, -2)) { /* did it find a loader? */
            return; /* module loader found */
        } else if (cs_is_string(C, -2)) { /* searcher returned error message? */
            cs_pop(C, 1); /* remove extra return */
            csL_buff_push_stack(&msg); /* concatenate error message */
        } else { /* no error message */
            cs_pop(C, 2); /* remove both returns */
            csL_buffsub(&msg, 2); /* remove prefix */
        }
    }
}


static int l_include(cs_State *C) {
    const char *name = csL_check_string(C, 0);
    cs_setntop(C, 1); /* __LOADED table will be at index 1 */
    cs_get_global(C, CS_LOADED_TABLE);
    cs_get_fieldstr(C, 1, name); /* __LOADED[name] */
    if (cs_to_bool(C, -1)) /* is it there? */
        return 1; /* package is already loaded */
    /* else must load package */
    cs_pop(C, 1); /* remove 'get_fieldstr' result */
    findloader(C, name);
    cs_rotate(C, -2, 1); /* loader function <-> loader data */
    cs_push(C, 0); /* name is 1st argument to module loader */
    cs_push(C, -3); /* loader data is 2nd argument */
    /* stack: ...; loader data; loader function; mod. name; loader data */
    cs_call(C, 2, 1); /* run loader to load module */
    /* stack: ...; loader data; result from loader */
    if (!cs_is_nil(C, -1)) /* non-nil return? */
        cs_set_fieldstr(C, 1, name); /* __LOADED[name] = returned value */
    else
        cs_pop(C, 1); /* pop nil */
    if (cs_get_fieldstr(C, 1, name) == CS_TNIL) { /* module set no value? */
        cs_push_bool(C, 1); /* use true as result */
        cs_copy(C, -1, -2); /* replace loader result */
        cs_set_fieldstr(C, 1, name); /* __LOADED[name] = true */
    }
    cs_rotate(C, -2, 1); /* loader data <-> module result  */
    return 2; /* return module result and loader data */
}


static const cs_Entry load_funcs[] = {
    {"include", l_include},
    {NULL, NULL}
};


/*
** __gc metamethod for CLIBS table: calls 'csys_unloadlib' for all lib
** handles in list CLIBS.
*/
static int gcmm(cs_State *C) {
    cs_Integer n;
    cs_get_uservalue(C, -1, 1); /* get lib handles array (uservalue of CLIBS) */
    n = cs_len(C, -1); /* get the number of lib handles */
    while (--n >= 0) { /* for each handle, in reverse order */
        cs_get_index(C, -1, n); /* get handle */
        if (c_unlikely(csys_unloadlib(C, cs_to_userdata(C, -1)) != 0))
            cs_error(C); /* unloading failed; error string is on top */
        cs_pop(C, 1); /* pop handle */
    }
    cs_pop(C, 1); /* pop lib handles array */
    return 0;
}


static void createclibs(cs_State *C) {
    /* global table is on stack top */
    if (cs_get_fieldstr(C, -1, CLIBS) != CS_TUSERDATA) {
        cs_pop(C, 1); /* remove value */
        cs_push_array(C, 0); /* array uservalue for storing lib handles */
        cs_push_table(C, 0); /* table uservalue for queries */
        cs_newuserdata(C, 0, 2); /* fulluserdata for CLIBS */
        cs_push(C, -1); /* copy of fulluserdata */
        cs_set_fieldstr(C, -3, CLIBS); /* __G[CLIBS] = fulluserdata */
    }
    cs_push_cfunction(C, gcmm);
    cs_set_usermm(C, -2, CS_MM_GC); /* set finalizer for CLIBS */
    cs_pop(C, 1); /* pop userdata (CLIBS) */
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "csopen_X" and look for it.
** If there is no ignore mark, look for a function named "csopen_modname".
*/
static int loadfunc(cs_State *C, const char *filename, const char *modname) {
    const char *openfunc;
    const char *mark;
    modname = csL_gsub(C, modname, ".", CS_OFSEP);
    mark = strchr(modname, *CS_IGMARK);
    if (mark) { /* have '-' (ignore mark)? */
        openfunc = cs_push_lstring(C, modname, mark - modname);
        openfunc = cs_push_fstring(C, CS_POF"%s", openfunc);
    } else /* no ignore mark (will try "csopen_modname") */
        openfunc = cs_push_fstring(C, CS_POF"%s", modname);
    return lookforfunc(C, filename, openfunc);
}


static int searcher_C(cs_State *C) {
    const char *name = csL_check_string(C, 0);
    const char *filename = findfile(C, name, "cpath", CS_DIRSEP);
    if (filename == NULL) return 1;  /* module not found in this path */
    return checkload(C, (loadfunc(C, filename, name) == 0), filename);
}


static int searcher_Croot(cs_State *C) {
    const char *filename;
    const char *name = csL_check_string(C, 0);
    const char *p = strchr(name, '.');
    int res;
    if (p == NULL) return 0; /* is root */
    cs_push_lstring(C, name, p - name);
    filename = findfile(C, cs_to_string(C, -1), "cpath", CS_DIRSEP);
    if (filename == NULL) return 1; /* root not found */
    if ((res = loadfunc(C, filename, name)) != 0) { /* error? */
        if (res != ERRFUNC) {
            return checkload(C, 0, filename); /* real error */
        } else { /* open function not found */
            cs_push_fstring(C, "no module '%s' in file '%s'", name, filename);
            return 1;
        }
    }
    cs_push_string(C, filename); /* will be 2nd argument to module */
    return 2; /* return open function and filename */
}


static void createsearchersarray(cs_State *C) {
    static const cs_CFunction searchers[] = {
        searcher_preload,
        searcher_CScript,
        searcher_C,
        searcher_Croot,
        NULL
    };
    /* create 'searchers' array ('package' table is on stack top) */
    cs_push_array(C, sizeof(searchers)/sizeof(searchers[0]) - 1);
    /* fill it with predefined searchers */
    for (int i = 0; searchers[i] != NULL; i++) {
        cs_push(C, -2); /* set 'package' as upvalue for all searchers */
        cs_push_cclosure(C, searchers[i], 1);
        cs_set_index(C, -2, i);
    }
    cs_set_fieldstr(C, -2, "searchers"); /* package.searchers = array */
}


/*
** CS_PATH_VAR and CS_CPATH_VAR are the names of the environment
** variables that CScript checks to set its paths.
*/
#if !defined(CS_PATH_VAR)
#define CS_PATH_VAR     "CS_PATH"
#endif

#if !defined(CS_CPATH_VAR)
#define CS_CPATH_VAR    "CS_CPATH"
#endif


/*
** Return __G["CS_NOENV"] as a boolean.
*/
static int noenv(cs_State *C) {
    int b;
    cs_get_global(C, "CS_NOENV");
    b = cs_to_bool(C, -1);
    cs_pop(C, 1); /* remove value */
    return b;
}


/* set a path */
static void setpath(cs_State *C, const char *fieldname, const char *envname,
                     const char *dflt) {
    const char *dfltmark;
    const char *nver = cs_push_fstring(C, "%s%s", envname, CS_VERSUFFIX);
    const char *path = getenv(nver); /* try versioned name */
    if (path == NULL) /* no versioned environment variable? */
        path = getenv(envname); /* try unversioned name */
    if (path == NULL || noenv(C)) /* no environment variable? */
        cs_push_string(C, dflt); /* use default */
    else if ((dfltmark = strstr(path, CS_PATH_SEP CS_PATH_SEP)) == NULL)
        cs_push_string(C, path); /* nothing to change */
    else { /* path contains a ";;": insert default path in its place */
        size_t len = strlen(path);
        csL_Buffer b;
        csL_buff_init(C, &b);
        if (path < dfltmark) { /* is there a prefix before ';;'? */
            csL_buff_push_lstring(&b, path, dfltmark - path); /* add it */
            csL_buff_push(&b, *CS_PATH_SEP);
        }
        csL_buff_push_string(&b, dflt); /* add default */
        if (dfltmark < path + len - 2) { /* is there a suffix after ';;'? */
            csL_buff_push(&b, *CS_PATH_SEP);
            csL_buff_push_lstring(&b, dfltmark+2, (path+len-2)-dfltmark);
        }
        csL_buff_end(&b);
    }
    setprogdir(C);
    cs_set_fieldstr(C, -3, fieldname); /* package[fieldname] = path value */
    cs_pop(C, 1); /* pop versioned variable name ('nver') */
}


CSMOD_API int csopen_package(cs_State *C) {
    cs_push_globaltable(C);
    createclibs(C);
    csL_newlib(C, package_funcs); /* create 'package' table */
    createsearchersarray(C);
    /* set paths */
    setpath(C, "path", CS_PATH_VAR, CS_PATH_DEFAULT);
    setpath(C, "cpath", CS_CPATH_VAR, CS_CPATH_DEFAULT);
    /* package.config = configstring */
    cs_push_literal(C, CS_DIRSEP "\n" CS_PATH_SEP "\n" CS_PATH_MARK "\n"
                       CS_EXEC_DIR "\n" CS_IGMARK "\n");
    cs_set_fieldstr(C, -2, "config");
    /* package.loaded = __LOADED */
    csL_get_subtable(C, -2, CS_LOADED_TABLE);
    cs_set_fieldstr(C, -2, "loaded");
    /* package.preload = __PRELOAD */
    csL_get_subtable(C, -2, CS_PRELOAD_TABLE);
    cs_set_fieldstr(C, -2, "preload");
    cs_push(C, -2); /* push global table */
    cs_push(C, -2); /* set 'package' as upvalue for next lib */
    csL_setfuncs(C, load_funcs, 1); /* open lib into global table */
    cs_pop(C, 1);  /* pop global table */
    return 1;  /* return 'package' table */
}
