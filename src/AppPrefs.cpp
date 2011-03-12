/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BencUtil.h"
#include "TStrUtil.h"
#include "FileUtil.h"

#include "AppPrefs.h"
#include "DisplayState.h"
#include "FileHistory.h"

extern bool CurrLangNameSet(const char* langName);

static bool ParseDisplayMode(const char *txt, DisplayMode *resOut)
{
    assert(txt);
    if (!txt) return false;
    return DisplayModeEnumFromName(txt, resOut);
}

#define GLOBAL_PREFS_STR "gp"

static BencDict* Prefs_SerializeGlobal(SerializableGlobalPrefs *globalPrefs)
{
    BencDict *prefs = new BencDict();
    if (!prefs)
        return NULL;

    prefs->Add(SHOW_TOOLBAR_STR, globalPrefs->m_showToolbar);
    prefs->Add(SHOW_TOC_STR, globalPrefs->m_showToc);
    prefs->Add(TOC_DX_STR, globalPrefs->m_tocDx);
    prefs->Add(PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs->m_pdfAssociateDontAskAgain);
    prefs->Add(PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs->m_pdfAssociateShouldAssociate);

    prefs->Add(BG_COLOR_STR, globalPrefs->m_bgColor);
    prefs->Add(ESC_TO_EXIT_STR, globalPrefs->m_escToExit);
    prefs->Add(ENABLE_AUTO_UPDATE_STR, globalPrefs->m_enableAutoUpdate);
    prefs->Add(REMEMBER_OPENED_FILES_STR, globalPrefs->m_rememberOpenedFiles);
    prefs->Add(GLOBAL_PREFS_ONLY_STR, globalPrefs->m_globalPrefsOnly);

    const char *txt = DisplayModeNameFromEnum(globalPrefs->m_defaultDisplayMode);
    prefs->Add(DISPLAY_MODE_STR, txt);

    txt = str_printf("%.4f", globalPrefs->m_defaultZoom);
    if (txt) {
        prefs->Add(ZOOM_VIRTUAL_STR, txt);
        free((void*)txt);
    }
    prefs->Add(WINDOW_STATE_STR, globalPrefs->m_windowState);
    prefs->Add(WINDOW_X_STR, globalPrefs->m_windowPos.x);
    prefs->Add(WINDOW_Y_STR, globalPrefs->m_windowPos.y);
    prefs->Add(WINDOW_DX_STR, globalPrefs->m_windowPos.dx);
    prefs->Add(WINDOW_DY_STR, globalPrefs->m_windowPos.dy);

    if (globalPrefs->m_inverseSearchCmdLine)
        prefs->Add(INVERSE_SEARCH_COMMANDLINE, globalPrefs->m_inverseSearchCmdLine);
    prefs->Add(ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs->m_enableTeXEnhancements);
    if (globalPrefs->m_versionToSkip)
        prefs->Add(VERSION_TO_SKIP_STR, globalPrefs->m_versionToSkip);
    if (globalPrefs->m_lastUpdateTime)
        prefs->Add(LAST_UPDATE_STR, globalPrefs->m_lastUpdateTime);
    prefs->Add(UI_LANGUAGE_STR, globalPrefs->m_currentLanguage);
    prefs->Add(FWDSEARCH_OFFSET, globalPrefs->m_fwdsearchOffset);
    prefs->Add(FWDSEARCH_COLOR, globalPrefs->m_fwdsearchColor);
    prefs->Add(FWDSEARCH_WIDTH, globalPrefs->m_fwdsearchWidth);
    prefs->Add(FWDSEARCH_PERMANENT, globalPrefs->m_fwdsearchPermanent);

    return prefs;
}

static BencDict *DisplayState_Serialize(DisplayState *ds, bool globalPrefsOnly)
{
    BencDict *prefs = new BencDict();
    if (!prefs)
        return NULL;

    prefs->Add(FILE_STR, ds->filePath);
    if (ds->decryptionKey)
        prefs->Add(DECRYPTION_KEY_STR, ds->decryptionKey);

    if (globalPrefsOnly || ds->useGlobalValues) {
        prefs->Add(USE_GLOBAL_VALUES_STR, TRUE);
        return prefs;
    }

    const char *txt = DisplayModeNameFromEnum(ds->displayMode);
    if (txt)
        prefs->Add(DISPLAY_MODE_STR, txt);
    prefs->Add(PAGE_NO_STR, ds->pageNo);
    prefs->Add(ROTATION_STR, ds->rotation);
    prefs->Add(SCROLL_X_STR, ds->scrollPos.x);
    prefs->Add(SCROLL_Y_STR, ds->scrollPos.y);
    prefs->Add(WINDOW_STATE_STR, ds->windowState);
    prefs->Add(WINDOW_X_STR, ds->windowPos.x);
    prefs->Add(WINDOW_Y_STR, ds->windowPos.y);
    prefs->Add(WINDOW_DX_STR, ds->windowPos.dx);
    prefs->Add(WINDOW_DY_STR, ds->windowPos.dy);

    prefs->Add(SHOW_TOC_STR, ds->showToc);
    prefs->Add(TOC_DX_STR, ds->tocDx);

    txt = str_printf("%.4f", ds->zoomVirtual);
    if (txt) {
        prefs->Add(ZOOM_VIRTUAL_STR, txt);
        free((void*)txt);
    }

    if (ds->tocState && ds->tocState[0] > 0) {
        BencArray *tocState = new BencArray();
        if (tocState) {
            for (int i = 1; i <= ds->tocState[0]; i++)
                tocState->Add(ds->tocState[i]);
            prefs->Add(TOC_STATE_STR, tocState);
        }
    }

    return prefs;
}

static BencArray *FileHistoryList_Serialize(FileHistoryList *fileHistory, bool globalPrefsOnly)
{
    assert(fileHistory);
    if (!fileHistory) return NULL;

    BencArray *arr = new BencArray();
    if (!arr)
        goto Error;

    // Don't save more file entries than will be useful
    int maxRememberdItems = globalPrefsOnly ? MAX_RECENT_FILES_IN_MENU : INT_MAX;
    for (int index = 0; index < maxRememberdItems; index++) {
        DisplayState *state = fileHistory->Get(index);
        if (!state)
            break;
        BencDict *obj = DisplayState_Serialize(state, globalPrefsOnly);
        if (!obj)
            goto Error;
        arr->Add(obj);
    }
    return arr;

Error:
    delete arr;
    return NULL;      
}

static const char *Prefs_Serialize(SerializableGlobalPrefs *globalPrefs, FileHistoryList *root, size_t* lenOut)
{
    const char *data = NULL;

    BencDict *prefs = new BencDict();
    if (!prefs)
        goto Error;
    BencDict* global = Prefs_SerializeGlobal(globalPrefs);
    if (!global)
        goto Error;
    prefs->Add(GLOBAL_PREFS_STR, global);
    BencArray *fileHistory = FileHistoryList_Serialize(root, globalPrefs->m_globalPrefsOnly);
    if (!fileHistory)
        goto Error;
    prefs->Add(FILE_HISTORY_STR, fileHistory);

    data = prefs->Encode();
    *lenOut = StrLen(data);

Error:
    delete prefs;
    return data;
}


static void dict_get_int(BencDict *dict, const char *key, int& value)
{
    BencInt *intObj = dict->GetInt(key);
    if (intObj)
        value = (int)intObj->Value();
}

static void dict_get_bool(BencDict *dict, const char *key, bool& value)
{
    BencInt *intObj = dict->GetInt(key);
    if (intObj)
        value = intObj->Value() != 0;
}

static char *dict_get_str(BencDict *dict, const char *key)
{
    BencString *string = dict->GetString(key);
    if (string)
        return string->RawValue();
    return NULL;
}

static void dict_get_str(BencDict *dict, const char *key, char *& value)
{
    char *string = dict_get_str(dict, key);
    if (string) {
        string = StrCopy(string);
        if (string) {
            free(value);
            value = string;
        }
    }
}

static void dict_get_tstr(BencDict *dict, const char *key, TCHAR *& value)
{
    BencString *string = dict->GetString(key);
    if (string) {
        TCHAR *str = string->Value();
        if (str) {
            free(value);
            value = str;
        }
    }
}

static DisplayState * DisplayState_Deserialize(BencDict *dict, bool globalPrefsOnly)
{
    DisplayState *ds = new DisplayState();
    if (!ds)
        return NULL;

    dict_get_tstr(dict, FILE_STR, ds->filePath);
    if (!ds->filePath) {
        delete ds;
        return NULL;
    }

    dict_get_str(dict, DECRYPTION_KEY_STR, ds->decryptionKey);
    if (globalPrefsOnly) {
        ds->useGlobalValues = TRUE;
        return ds;
    }

    const char* txt = dict_get_str(dict, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &ds->displayMode);
    dict_get_int(dict, PAGE_NO_STR, ds->pageNo);
    dict_get_int(dict, ROTATION_STR, ds->rotation);
    dict_get_int(dict, SCROLL_X_STR, ds->scrollPos.x);
    dict_get_int(dict, SCROLL_Y_STR, ds->scrollPos.y);
    dict_get_int(dict, WINDOW_STATE_STR, ds->windowState);
    dict_get_int(dict, WINDOW_X_STR, ds->windowPos.x);
    dict_get_int(dict, WINDOW_Y_STR, ds->windowPos.y);
    dict_get_int(dict, WINDOW_DX_STR, ds->windowPos.dx);
    dict_get_int(dict, WINDOW_DY_STR, ds->windowPos.dy);
    dict_get_bool(dict, SHOW_TOC_STR, ds->showToc);
    dict_get_int(dict, TOC_DX_STR, ds->tocDx);
    txt = dict_get_str(dict, ZOOM_VIRTUAL_STR);
    if (txt)
        ds->zoomVirtual = (float)atof(txt);
    dict_get_bool(dict, USE_GLOBAL_VALUES_STR, ds->useGlobalValues);

    BencArray *tocState = dict->GetArray(TOC_STATE_STR);
    if (tocState) {
        size_t len = tocState->Length();
        ds->tocState = SAZA(int, len + 1);
        if (ds->tocState) {
            ds->tocState[0] = (int)len;
            for (size_t i = 0; i < len; i++) {
                BencInt *intObj = tocState->GetInt(i);
                if (intObj)
                    ds->tocState[i + 1] = (int)intObj->Value();
            }
        }
    }

    return ds;
}

static bool Prefs_Deserialize(const char *prefsTxt, SerializableGlobalPrefs *globalPrefs, FileHistoryList *root)
{
    BencObj *obj = BencObj::Decode(prefsTxt);
    if (!obj || obj->Type() != BT_DICT)
        goto Error;
    BencDict *prefs = static_cast<BencDict *>(obj);
    BencDict *global = prefs->GetDict(GLOBAL_PREFS_STR);
    if (!global)
        goto Error;

    dict_get_bool(global, SHOW_TOOLBAR_STR, globalPrefs->m_showToolbar);
    dict_get_bool(global, SHOW_TOC_STR, globalPrefs->m_showToc);
    dict_get_int(global, TOC_DX_STR, globalPrefs->m_tocDx);
    dict_get_bool(global, PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs->m_pdfAssociateDontAskAgain);
    dict_get_bool(global, PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs->m_pdfAssociateShouldAssociate);
    dict_get_bool(global, ESC_TO_EXIT_STR, globalPrefs->m_escToExit);
    dict_get_int(global, BG_COLOR_STR, globalPrefs->m_bgColor);
    dict_get_bool(global, ENABLE_AUTO_UPDATE_STR, globalPrefs->m_enableAutoUpdate);
    dict_get_bool(global, REMEMBER_OPENED_FILES_STR, globalPrefs->m_rememberOpenedFiles);
    dict_get_bool(global, GLOBAL_PREFS_ONLY_STR, globalPrefs->m_globalPrefsOnly);

    const char* txt = dict_get_str(global, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &globalPrefs->m_defaultDisplayMode);
    txt = dict_get_str(global, ZOOM_VIRTUAL_STR);
    if (txt)
        globalPrefs->m_defaultZoom = (float)atof(txt);
    dict_get_int(global, WINDOW_STATE_STR, globalPrefs->m_windowState);

    dict_get_int(global, WINDOW_X_STR, globalPrefs->m_windowPos.x);
    dict_get_int(global, WINDOW_Y_STR, globalPrefs->m_windowPos.y);
    dict_get_int(global, WINDOW_DX_STR, globalPrefs->m_windowPos.dx);
    dict_get_int(global, WINDOW_DY_STR, globalPrefs->m_windowPos.dy);

    dict_get_tstr(global, INVERSE_SEARCH_COMMANDLINE, globalPrefs->m_inverseSearchCmdLine);
    dict_get_bool(global, ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs->m_enableTeXEnhancements);
    dict_get_tstr(global, VERSION_TO_SKIP_STR, globalPrefs->m_versionToSkip);
    dict_get_str(global, LAST_UPDATE_STR, globalPrefs->m_lastUpdateTime);

    txt = dict_get_str(global, UI_LANGUAGE_STR);
    CurrLangNameSet(txt);

    dict_get_int(global, FWDSEARCH_OFFSET, globalPrefs->m_fwdsearchOffset);
    dict_get_int(global, FWDSEARCH_COLOR, globalPrefs->m_fwdsearchColor);
    dict_get_int(global, FWDSEARCH_WIDTH, globalPrefs->m_fwdsearchWidth);
    dict_get_bool(global, FWDSEARCH_PERMANENT, globalPrefs->m_fwdsearchPermanent);

    BencArray *fileHistory = prefs->GetArray(FILE_HISTORY_STR);
    if (!fileHistory)
        goto Error;
    size_t dlen = fileHistory->Length();
    for (size_t i = 0; i < dlen; i++) {
        BencDict *dict = fileHistory->GetDict(i);
        assert(dict);
        if (!dict) continue;
        DisplayState *state = DisplayState_Deserialize(dict, globalPrefs->m_globalPrefsOnly);
        if (state)
            root->Append(state);
    }
    delete obj;
    return true;

Error:
    delete obj;
    return false;
}

namespace Prefs {

/* Load preferences from the preferences file.
   Returns true if preferences file was loaded, false if there was an error.
*/
bool Load(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistoryList *fileHistory)
{
    bool            ok = false;

#ifdef DEBUG
    static bool     loaded = false;
    assert(!loaded);
    loaded = true;
#endif

    assert(filepath);
    if (!filepath)
        return false;

    size_t prefsFileLen;
    ScopedMem<char> prefsTxt(file_read_all(filepath, &prefsFileLen));
    if (!str_empty(prefsTxt)) {
        ok = Prefs_Deserialize(prefsTxt, globalPrefs, fileHistory);
        assert(ok);
    }

    // TODO: add a check if a file exists, to filter out deleted files
    // but only if a file is on a non-network drive (because
    // accessing network drives can be slow and unnecessarily spin
    // the drives).
#if 0
    for (int index = 0; fileHistory->Get(index); index++) {
        DisplayState *state = fileHistory->Get(index);
        if (!file_exists(state->filePath)) {
            DBG_OUT_T("Prefs_Load() file '%s' doesn't exist anymore\n", state->filePath);
            fileHistory->Remove(state);
            delete state;
        }
    }
#endif

    return ok;
}

bool Save(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistoryList *fileHistory)
{
    assert(filepath);
    if (!filepath)
        return false;

    size_t dataLen;
    ScopedMem<const char> data(Prefs_Serialize(globalPrefs, fileHistory, &dataLen));
    if (!data)
        return false;

    assert(dataLen > 0);
    /* TODO: consider 2-step process:
        * write to a temp file
        * rename temp file to final file */
    return write_to_file(filepath, (void*)data.Get(), dataLen);
}

}
