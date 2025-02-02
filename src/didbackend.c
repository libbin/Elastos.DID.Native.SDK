/*
 * Copyright (c) 2019 - 2021 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <jansson.h>

#include "ela_did.h"
#include "common.h"
#include "did.h"
#include "didurl.h"
#include "didrequest.h"
#include "didbackend.h"
#include "didmeta.h"
#include "diddocument.h"
#include "didresolver.h"
#include "resolveresult.h"
#include "resolvercache.h"
#include "diderror.h"
#include "didbiography.h"
#include "credentialbiography.h"

#define DEFAULT_TTL    (24 * 60 * 60 * 1000)
#define DID_RESOLVE_REQUEST "{\"method\":\"did_resolveDID\",\"params\":[{\"did\":\"%s\",\"all\":%s}], \"id\":\"%s\"}"
#define DID_RESOLVEVC_REQUEST "{\"method\":\"did_listCredentials\",\"params\":[{\"did\":\"%s\",\"skip\":%d,\"limit\":%d}], \"id\":\"%s\"}"
#define VC_RESOLVE_REQUEST "{\"method\":\"did_resolveCredential\",\"params\":[{\"id\":\"%s\"}], \"id\":\"%s\"}"
#define VC_RESOLVE_WITH_ISSUER_REQUEST "{\"method\":\"did_resolveCredential\",\"params\":[{\"id\":\"%s\", \"issuer\":\"%s\"}], \"id\":\"%s\"}"

static DIDLocalResovleHandle *gLocalResolveHandle;
static CreateIdTransaction_Callback *gCreateIdTransaction;
static Resolve_Callback *gResolve;

long ttl = DEFAULT_TTL;

static void get_txid(char *txid)
{
    static char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int i;

    assert(txid);

    for (i = 0; i < 31; i++)
        txid[i] = chars[rand() % 62];

    txid[31] = 0;
}

int DIDBackend_InitializeDefault(CreateIdTransaction_Callback *createtransaction,
        const char *url, const char *cachedir)
{
    DIDERROR_INITIALIZE();

    CHECK_ARG(!url || !*url, "No url string.", -1);
    CHECK_ARG(!cachedir || !*cachedir, "No cache directory.", -1);
    CHECK_ARG(strlen(url) >= URL_LEN, "Url is too long.", -1);

    if (DefaultResolve_Init(url) < 0)
        return -1;

    if (createtransaction)
        gCreateIdTransaction = createtransaction;

    gResolve = DefaultResolve_Resolve;

    if (ResolverCache_SetCacheDir(cachedir) < 0) {
        DIDError_Set(DIDERR_INVALID_ARGS, "Invalid cache directory.");
        return -1;
    }

    return 0;

    DIDERROR_FINALIZE();
}

int DIDBackend_Initialize(CreateIdTransaction_Callback *createtransaction,
        Resolve_Callback *resolve, const char *cachedir)
{
    DIDERROR_INITIALIZE();

    CHECK_ARG(!cachedir || !*cachedir, "No cache directory.", -1);

    if (createtransaction)
       gCreateIdTransaction = createtransaction;
    if (resolve)
       gResolve = resolve;

    if (ResolverCache_SetCacheDir(cachedir) < 0){
        DIDError_Set(DIDERR_INVALID_ARGS, "Invalid cache directory.");
        return -1;
    }

    return 0;

    DIDERROR_FINALIZE();
}

bool DIDBackend_IsInitialized()
{
    return gResolve ? true : false;
}

int DIDBackend_CreateDID(DIDDocument *document, DIDURL *signkey, const char *storepass)
{
    const char *reqstring;
    bool success;

    assert(document);
    assert(signkey);
    assert(storepass && *storepass);

    if (!gCreateIdTransaction) {
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "No method to create transaction.\
                Please use 'DIDBackend_InitializeDefault' or 'DIDBackend_Initialize' to initialize backend.");
        return -1;
    }

    if (!DIDMetadata_AttachedStore(&document->metadata)) {
        DIDError_Set(DIDERR_NO_ATTACHEDSTORE, "No attached store with document.");
        return -1;
    }

    reqstring = DIDRequest_Sign(RequestType_Create, document, signkey, NULL, NULL, storepass);
    if (!reqstring)
        return -1;

    success = gCreateIdTransaction(reqstring, "");
    free((void*)reqstring);
    if (!success)
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Create Id transaction(create) failed.");

    return success;
}

int DIDBackend_UpdateDID(DIDDocument *document, DIDURL *signkey, const char *storepass)
{
    const char *reqstring;
    bool success;

    assert(document);
    assert(signkey);
    assert(storepass && *storepass);

    if (!gCreateIdTransaction) {
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "No method to create transaction.\
                Please use 'DIDBackend_InitializeDefault' or 'DIDBackend_Initialize' to initialize backend.");
        return -1;
    }

    if (!DIDMetadata_AttachedStore(&document->metadata)) {
        DIDError_Set(DIDERR_NO_ATTACHEDSTORE, "No attached store with document.");
        return -1;
    }

    reqstring = DIDRequest_Sign(RequestType_Update, document, signkey, NULL, NULL, storepass);
    if (!reqstring)
        return -1;

    success = gCreateIdTransaction(reqstring, "");
    free((void*)reqstring);
    if (!success)
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Create Id transaction(update) failed.");

    return success;
}

int DIDBackend_TransferDID(DIDDocument *document, TransferTicket *ticket,
        DIDURL *signkey, const char *storepass)
{
    const char *reqstring;
    bool success;

    assert(document);
    assert(ticket);
    assert(signkey);
    assert(storepass && *storepass);

    if (!gCreateIdTransaction) {
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "No method to create transaction.\
                Please use 'DIDBackend_InitializeDefault' or 'DIDBackend_Initialize' to initialize backend.");
        return -1;
    }

    if (!DIDMetadata_AttachedStore(&document->metadata)) {
        DIDError_Set(DIDERR_NO_ATTACHEDSTORE, "No attached store with document.");
        return -1;
    }

    reqstring = DIDRequest_Sign(RequestType_Transfer, document, signkey, NULL, ticket, storepass);
    if (!reqstring)
        return -1;

    success = gCreateIdTransaction(reqstring, "");
    free((void*)reqstring);
    if (!success)
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Create Id transaction(transfer) failed.");

    return success;
}

//signkey provides sk, creater is real key in proof. If did is deactivated by ownerself, signkey and
//creater is same.
int DIDBackend_DeactivateDID(DIDDocument *signerdoc, DIDURL *signkey,
        DIDURL *creater, const char *storepass)
{
    const char *reqstring;
    bool success;

    assert(signerdoc);
    assert(signkey);
    assert(storepass && *storepass);

    if (!gCreateIdTransaction) {
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "No method to create transaction.\
                Please use 'DIDBackend_InitializeDefault' or 'DIDBackend_Initialize' to initialize backend.");
        return -1;
    }

    if (!DIDMetadata_AttachedStore(&signerdoc->metadata)) {
        DIDError_Set(DIDERR_NO_ATTACHEDSTORE, "No attached store with document.");
        return -1;
    }

    reqstring = DIDRequest_Sign(RequestType_Deactivate, signerdoc, signkey, creater, NULL, storepass);
    if (!reqstring)
        return -1;

    success = gCreateIdTransaction(reqstring, "");
    free((void*)reqstring);
    if (!success)
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Create Id transaction(deactivated) failed.");

    return success;
}

static json_t *get_resolve_result(json_t *json)
{
    json_t *item, *field;
    int code;

    assert(json);

    item = json_object_get(json, "result");
    if (!item || !json_is_object(item)) {
        item = json_object_get(json, "error");
        if (!item || !json_is_null(item)) {
            DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Missing or invalid error field.");
        } else {
            field = json_object_get(item, "code");
            if (field && json_is_integer(field)) {
                code = json_integer_value(field);
                field = json_object_get(item, "message");
                if (field && json_is_string(field))
                    DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Resolve did error(%d): %s", code, json_string_value(field));
            }
        }
        return NULL;
    }

    return item;
}

static int resolvedid_from_backend(ResolveResult *result, DID *did, bool all)
{
    const char *data = NULL, *forAll;
    json_t *root = NULL, *item;
    json_error_t error;
    char _idstring[ELA_MAX_DID_LEN], request[256], *didstring, txid[32];
    int rc = -1;

    assert(result);
    assert(did);

    didstring = DID_ToString(did, _idstring, sizeof(_idstring));
    if (!didstring)
        return rc;

    forAll = !all ? "false" : "true";
    get_txid(txid);
    if (sprintf(request, DID_RESOLVE_REQUEST, didstring, forAll, txid) == -1) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_REQUEST, "Generate resolve request failed.");
        return rc;
    }

    data = gResolve(request);
    if (!data) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESPONSE, "No resolve data %s from chain failed.", DIDSTR(did));
        return rc;
    }

    root = json_loads(data, JSON_COMPACT, &error);
    if (!root) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESPONSE, "Deserialize resolved data failed, error: %s.", error.text);
        goto errorExit;
    }

    item = get_resolve_result(root);
    if (!item)
        goto errorExit;

    if (ResolveResult_FromJson(result, item, all) == -1)
        goto errorExit;

    if (ResolveResult_GetStatus(result) != DIDStatus_NotFound && ResolveCache_StoreDID(result, did) == -1)
        goto errorExit;

    rc = 0;

errorExit:
    if (root)
        json_decref(root);
    if (data)
        free((void*)data);
    return rc;
}

static ssize_t listvcs_result_fromjson(json_t *json, DIDURL **buffer, size_t size, const char *did)
{
    json_t *item, *field;
    DIDURL *id;
    size_t len = 0;
    int i;

    assert(json);
    assert(buffer);

    item = json_object_get(json, "did");
    if (!item) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Missing did filed.");
        return -1;
    }
    if (!json_is_string(item)) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Invalid did filed.");
        return -1;
    }
    if (strcmp(did, json_string_value(item))) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Response is not for this DID.");
        return -1;
    }

    item = json_object_get(json, "credentials");
    if (!item)
        return 0;

    if (!json_is_array(item)) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Invalid credentials.");
        return -1;
    }

    for (i = 0; i < json_array_size(item); i++) {
        field = json_array_get(item, i);
        if (field) {
            id = DIDURL_FromString(json_string_value(field), NULL);
            if (id)
                buffer[len++] = id;
        }
    }
    return len;
}

static ssize_t listvcs_from_backend(DID *did, DIDURL **buffer, size_t size, int skip, int limit)
{
    const char *data = NULL;
    json_t *root = NULL, *item;
    json_error_t error;
    char _idstring[ELA_MAX_DID_LEN], request[256], txid[32], *didstring;
    ssize_t rc = -1, len = 0;

    assert(buffer);
    assert(did);
    assert(size > 0);
    assert(skip >= 0);
    assert(limit >= 0);

    didstring = DID_ToString(did, _idstring, sizeof(_idstring));
    if (!didstring)
        return rc;

    get_txid(txid);
    if (sprintf(request, DID_RESOLVEVC_REQUEST, didstring, skip, limit, txid) == -1) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_REQUEST, "Get resolve request to list credentials failed.");
        return rc;
    }

    data = gResolve(request);
    if (!data) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESPONSE, "No resolve did %s failed.", did->idstring);
        return rc;
    }

    root = json_loads(data, JSON_COMPACT, &error);
    if (!root) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESPONSE, "Deserialize resolved data failed, error: %s.", error.text);
        goto errorExit;
    }

    item = get_resolve_result(root);
    if (!item)
        goto errorExit;

    rc = listvcs_result_fromjson(item, buffer, size, didstring);

errorExit:
    if (root)
        json_decref(root);
    if (data)
        free((void*)data);
    return rc;
}

static CredentialBiography *resolvevc_from_backend(DIDURL *id, DID *issuer)
{
    CredentialBiography *biography = NULL;
    const char *data = NULL;
    json_t *root = NULL, *item;
    json_error_t error;
    char _idstring[ELA_MAX_DIDURL_LEN], _didstring[ELA_MAX_DID_LEN], request[256], txid[32];
    char *idstring, *didstring = NULL;

    assert(id);

    idstring = DIDURL_ToString_Internal(id, _idstring, sizeof(_idstring), false);
    if (!idstring)
        return NULL;

    get_txid(txid);
    if (issuer) {
        didstring = DID_ToString(issuer, _didstring, sizeof(_didstring));
        if (!didstring || sprintf(request, VC_RESOLVE_WITH_ISSUER_REQUEST, idstring, didstring, txid) == -1) {
            DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Get resolve request to resolve credentials failed.");
            return NULL;
        }
    } else {
        if (sprintf(request, VC_RESOLVE_REQUEST, idstring, txid) == -1) {
            DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Get resolve request to resolve credentials failed.");
            return NULL;
        }
    }

    data = gResolve(request);
    if (!data) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESPONSE, "Resolve data %s from chain failed.", idstring);
        return NULL;
    }

    root = json_loads(data, JSON_COMPACT, &error);
    if (!root) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESPONSE, "Deserialize resolved data failed, error: %s.", error.text);
        goto errorExit;
    }

    item = get_resolve_result(root);
    if (!item)
        goto errorExit;

    biography = CredentialBiography_FromJson(item);
    if (!biography)
        goto errorExit;

    if (CredentialBiography_GetStatus(biography) != CredentialStatus_NotFound &&
            ResolveCache_StoreCredential(biography, id) == -1) {
        CredentialBiography_Destroy(biography);
        biography = NULL;
    }

errorExit:
    if (root)
        json_decref(root);
    if (data)
        free((void*)data);
    return biography;
}

static int resolve_internal(ResolveResult *result, DID *did, bool all, bool force)
{
    assert(result);
    assert(did);
    assert(!all || (all && force));

    if (!force && ResolverCache_LoadDID(result, did, ttl) == 0)
        return 0;

    if (resolvedid_from_backend(result, did, all) < 0)
        return -1;

    return 0;
}

static CredentialBiography *resolvevc_internal(DIDURL *id, DID *issuer, bool force)
{
    CredentialBiography *biography;

    assert(id);

    if (!force) {
        biography = ResolverCache_LoadCredential(id, issuer, ttl);
        if (biography)
            return biography;
    }

    return resolvevc_from_backend(id, issuer);
}

DIDDocument *DIDBackend_ResolveDID(DID *did, int *status, bool force)
{
    DIDDocument *doc = NULL;
    ResolveResult result;
    DIDTransaction *info = NULL;
    const char *op;
    size_t i;

    assert(did);

    //If user give document to verify, sdk use it first.
    if (gLocalResolveHandle) {
        doc = gLocalResolveHandle(did);
        if (doc)
            return doc;
    }

    if (!gResolve) {
        *status = DIDStatus_Error;
        DIDError_Set(DIDERR_DID_RESOLVE_ERROR, "No Resolver.");
        return NULL;
    }

    memset(&result, 0, sizeof(ResolveResult));
    if (resolve_internal(&result, did, false, force) == -1)
        goto errorExit;

    switch (result.status) {
        case DIDStatus_NotFound:
            *status = DIDStatus_NotFound;
            ResolveResult_Destroy(&result);
            return NULL;

        case DIDStatus_Deactivated:
            if (result.txs.size != 2) {
                DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Invalid DID biography, wrong transaction count.");
                goto errorExit;
            }

            if (strcmp("deactivate", result.txs.txs[0].request.header.op) ||
                    !strcmp("deactivate", result.txs.txs[1].request.header.op)) {
                DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Invalid DID biography, wrong status.");
                goto errorExit;
            }

            info = &result.txs.txs[1];
            doc = info->request.doc;
            if (!doc) {
                DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Invalid DID biography, missing document.");
                goto errorExit;
            }

            if (!DIDRequest_IsValid(&result.txs.txs[0].request, doc)) {
                DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Document is not valid.");
                goto errorExit;
            }

            *status = DIDStatus_Deactivated;
            i = 2;
            break;

        case DIDStatus_Valid:
            info = &result.txs.txs[0];
            doc = info->request.doc;
            if (!doc) {
                DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Invalid DID biography, missing document.");
                goto errorExit;
            }

            *status = DIDStatus_Valid;
            i = 1;
            break;

        default:
            DIDError_Set(DIDERR_UNSUPPORTED, "Unsupport other transaction status.");
            goto errorExit;
    }

    op = info->request.header.op;
    if (strcmp("create", op) && strcmp("update", op) && strcmp("transfer", op)) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Wrong transaction status.");
        goto errorExit;
    }

    if (!DIDRequest_IsValid(&info->request, doc)) {
        DIDError_Set(DIDERR_MALFORMED_RESOLVE_RESULT, "Invalid transaction.");
        goto errorExit;
    }

    for (; i < result.txs.size; i++)
        DIDDocument_Destroy(result.txs.txs[i].request.doc);
    ResolveResult_Free(&result);
    return doc;

errorExit:
    *status = DIDStatus_Error;
    ResolveResult_Destroy(&result);
    return NULL;
}

DIDBiography *DIDBackend_ResolveDIDBiography(DID *did)
{
    ResolveResult result;

    assert(did);

    if (!gResolve) {
        DIDError_Set(DIDERR_DID_RESOLVE_ERROR, "No Resolver.");
        return NULL;
    }

    memset(&result, 0, sizeof(ResolveResult));
    if (resolve_internal(&result, did, true, true) == -1) {
        ResolveResult_Destroy(&result);
        return NULL;
    }

    if (ResolveResult_GetStatus(&result) == DIDStatus_NotFound) {
        ResolveResult_Destroy(&result);
        DIDError_Set(DIDERR_NOT_EXISTS, "DID not exists.");
        return NULL;
    }

    return ResolveResult_ToDIDBiography(&result);
}

ssize_t DIDBackend_ListCredentials(DID *did, DIDURL **buffer, size_t size,
        int skip, int limit)
{
    assert(did);
    assert(buffer);
    assert(size > 0);
    assert(skip >= 0 && limit >= 0);

    if (!gResolve) {
        DIDError_Set(DIDERR_DID_RESOLVE_ERROR, "No Resolver.");
        return -1;
    }

    return listvcs_from_backend(did, buffer, size, skip, limit);
}

int DIDBackend_DeclareCredential(Credential *vc, DIDURL *signkey,
        DIDDocument *document, const char *storepass)
{
    const char *reqstring;
    bool success;

    assert(vc);
    assert(signkey);
    assert(document);
    assert(storepass && *storepass);

    if (!gCreateIdTransaction) {
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "No method to create transaction.\
                Please use 'DIDBackend_InitializeDefault' or 'DIDBackend_Initialize' to initialize backend.");
        return -1;
    }

    if (!DIDMetadata_AttachedStore(&document->metadata)) {
        DIDError_Set(DIDERR_NO_ATTACHEDSTORE, "No attached store with document.");
        return -1;
    }

    reqstring = CredentialRequest_Sign(RequestType_Declare, NULL, vc, signkey, document, storepass);
    if (!reqstring)
        return -1;

    success = gCreateIdTransaction(reqstring, "");
    free((void*)reqstring);
    if (!success)
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Create Id transaction(declare) failed.");

    return success;
}

int DIDBackend_RevokeCredential(DIDURL *credid, DIDURL *signkey, DIDDocument *document,
        const char *storepass)
{
    const char *reqstring;
    bool success;

    assert(credid);
    assert(signkey);
    assert(document);
    assert(storepass && *storepass);

    if (!gCreateIdTransaction) {
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "No method to create transaction.\
                Please use 'DIDBackend_InitializeDefault' or 'DIDBackend_Initialize' to initialize backend.");
        return -1;
    }

    if (!DIDMetadata_AttachedStore(&document->metadata)) {
        DIDError_Set(DIDERR_NO_ATTACHEDSTORE, "No attached store with document.");
        return -1;
    }

    reqstring = CredentialRequest_Sign(RequestType_Revoke, credid, NULL, signkey, document, storepass);
    if (!reqstring)
        return -1;

    success = gCreateIdTransaction(reqstring, "");
    free((void*)reqstring);
    if (!success)
        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "create Id transaction(revoke) failed.");

    return success;
}

Credential *DIDBackend_ResolveCredential(DIDURL *id, int *status, bool force)
{
    CredentialBiography *biography;
    CredentialTransaction *info;
    Credential *cred = NULL;
    DIDDocument *issuerdoc = NULL;
    int i;

    assert(id);

    if (!gResolve) {
        DIDError_Set(DIDERR_DID_RESOLVE_ERROR, "No resolver.");
        return NULL;
    }

    biography = resolvevc_internal(id, NULL, false);
    if (!biography) {
        *status = CredentialStatus_Error;
        return NULL;
    }

    switch (biography->status) {
        case CredentialStatus_NotFound:
            *status = CredentialStatus_NotFound;
            CredentialBiography_Destroy(biography);
            return NULL;

        case CredentialStatus_Revoked:
            if (biography->txs.size > 2 || biography->txs.size == 0) {
                DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Invalid Credential biography, wrong transaction count.");
                break;
            }

            for (i = biography->txs.size - 1; i >= 0 ; i--) {
                info = &biography->txs.txs[i];
                if (!strcmp("declare", info->request.header.op)) {
                    if (i == 0) {
                        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Invalid declare transaction.");
                        goto errorExit;
                    }
                    cred = info->request.vc;
                    if (!cred) {
                        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Miss credential in transaction.");
                        goto errorExit;
                    }
                }

                if (!strcmp("revoke", info->request.header.op)) {
                    if (info->request.vc) {
                        DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Invalid revoke transaction.");
                        goto errorExit;
                    }
                }

                if (!CredentialRequest_IsValid(&info->request, cred))
                    goto errorExit;
            }
            *status = CredentialStatus_Revoked;
            CredentialBiography_Free(biography);
            return cred;

        case CredentialStatus_Valid:
            if (biography->txs.size != 1) {
                DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Invalid Credential biography, wrong transaction count.");
                goto errorExit;
            }

            info = &biography->txs.txs[0];
            if (strcmp("declare", info->request.header.op)) {
                DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Invalid Credential biography, wrong transaction status.");
                goto errorExit;
            }

            cred = info->request.vc;
            if (!cred) {
                DIDError_Set(DIDERR_DID_TRANSACTION_ERROR, "Declare transaction must have credential.");
                goto errorExit;
            }

            if (!CredentialRequest_IsValid(&info->request, cred))
                goto errorExit;

            *status = CredentialStatus_Valid;
            CredentialBiography_Free(biography);
            return cred;

        default:
            DIDError_Set(DIDERR_UNSUPPORTED, "Unsupport other status.");
            break;
    }

errorExit:
    *status = CredentialStatus_Error;
    CredentialBiography_Destroy(biography);
    return NULL;
}

int DIDBackend_ResolveRevocation(DIDURL *id, DID *issuer)
{
    CredentialBiography *biography;
    int exist;

    assert(id);
    assert(issuer);

    if (!gResolve) {
        DIDError_Set(DIDERR_DID_RESOLVE_ERROR, "No Resolver.");
        return -1;
    }

    biography = resolvevc_from_backend(id, issuer);
    if (!biography)
        return -1;

    exist = (CredentialBiography_GetStatus(biography) == CredentialStatus_Revoked) ? 1 : 0;
    CredentialBiography_Destroy(biography);
    return exist;
}

CredentialBiography *DIDBackend_ResolveCredentialBiography(DIDURL *id, DID *issuer)
{
    CredentialBiography *biography;

    assert(id);

    if (!gResolve) {
        DIDError_Set(DIDERR_DID_RESOLVE_ERROR, "No resolver.");
        return NULL;
    }

    biography = resolvevc_internal(id, issuer, true);
    if (biography && CredentialBiography_GetStatus(biography) == CredentialStatus_NotFound) {
        CredentialBiography_Destroy(biography);
        return NULL;
    }

    return biography;
}

void DIDBackend_SetTTL(long _ttl)
{
    DIDERROR_INITIALIZE();

    ttl = _ttl;

    DIDERROR_FINALIZE();
}

void DIDBackend_SetLocalResolveHandle(DIDLocalResovleHandle *handle)
{
    gLocalResolveHandle = handle;
}
