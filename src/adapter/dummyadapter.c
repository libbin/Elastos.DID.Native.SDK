#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>
#include <time.h>
#include <jansson.h>

#include "ela_did.h"
#include "dummyadapter.h"
#include "didtransactioninfo.h"
#include "didrequest.h"
#include "crypto.h"
#include "common.h"
#include "diderror.h"
#include "diddocument.h"

#define TXID_LEN            32

static const char elastos_did_prefix[] = "did:elastos:";

static DummyAdapter adapterInstance;
static DIDTransactionInfo *infos[256];
static int num;

static int get_txid(char *txid)
{
    static char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int i;

    assert(txid);

    for (i = 0; i < TXID_LEN; i++)
        txid[i] = chars[rand() % 62];

    txid[TXID_LEN] = 0;
    return 0;
}

static DIDTransactionInfo *get_lasttransaction(DID *did)
{
    DIDTransactionInfo *info;
    int i;

    assert(did);

    for (i = num - 1; i >= 0; i--) {
        info = infos[i];
        if (DID_Equals(did, DIDTransactionInfo_GetOwner(info)))
            return info;
    }
    return NULL;
}

static DIDTransactionInfo *get_transaction(DID *did, int index)
{
    DIDTransactionInfo *info;

    assert(did);
    assert(index >= 0 && index < num);

    info = infos[index];
    return DID_Equals(did, DIDTransactionInfo_GetOwner(info)) ? info : NULL;
}

static bool check_ticket(const char* data, DIDDocument *doc)
{
    size_t len;
    char *ticketJson;
    TransferTicket *ticket;

    assert(data);

    len = strlen(data) + 1;
    ticketJson = (char*)malloc(len);
    len = base64_url_decode((uint8_t *)ticketJson, data);
    if (len <= 0) {
        free((void*)ticketJson);
        return false;
    }
    ticketJson[len] = 0;

    ticket = TransferTicket_FromJson(ticketJson);
    free((void*)ticketJson);
    if (!ticket)
        return false;

    if (!TransferTicket_IsValid(ticket))
        return false;

    if (!DIDDocument_GetControllerDocument(doc, &ticket->to))
        return false;

    return true;
}

static bool check_controllers(DIDDocument *doc1, DIDDocument *doc2)
{
    int i, j;
    DID *controller;

    assert(doc1);
    assert(doc2);

    if (doc1->controllers.size != doc2->controllers.size)
        return false;

    for (i = 0; i < doc2->controllers.size; i++) {
        controller = &doc2->controllers.docs[i]->did;
        if (!DIDDocument_GetControllerDocument(doc1, controller))
            return false;
    }

    return true;
 }

static bool DummyAdapter_CreateIdTransaction(DIDAdapter *_adapter, const char *payload, const char *memo)
{
    DIDTransactionInfo *info = NULL, *lastinfo;
    json_t *root;
    json_error_t error;
    DIDDocument *doc;

    assert(_adapter);
    assert(payload);

    if (num >= sizeof(infos)) {
        DIDError_Set(DIDERR_OUT_OF_MEMORY, "The DIDTransactionInfo array should be larger.");
        return false;
    }

    root = json_loads(payload, JSON_COMPACT, &error);
    if (!root) {
        DIDError_Set(DIDERR_TRANSACTION_ERROR, "Get payload json failed, error: %s.", error.text);
        return false;
    }

    info = (DIDTransactionInfo*)calloc(1, sizeof(DIDTransactionInfo));
    if (!info) {
        DIDError_Set(DIDERR_OUT_OF_MEMORY, "Malloc buffer for DIDTransactionInfo failed.");
        return false;
    }

    doc = DIDRequest_FromJson(&info->request, root);
    if (strcmp(info->request.header.op, "deactivate")) {
        if (!doc || !DIDDocument_IsValid(doc))
        goto errorExit;
    }

    lastinfo = get_lasttransaction(&info->request.did);
    if (!strcmp(info->request.header.op, "create")) {
        if (lastinfo) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "DID already exist.");
            goto errorExit;
        }
    } else if (!strcmp(info->request.header.op, "update")) {
        if (!lastinfo) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "DID not exist.");
            goto errorExit;
        }
        if (!strcmp(lastinfo->request.header.op, "deactivate")) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "DID already deactivate.");
            goto errorExit;
        }
        if (strcmp(info->request.header.prevtxid, lastinfo->txid)) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "Previous transaction id missmatch.");
            goto errorExit;
        }
        if (Is_CustomizedDID(info->request.doc) &&
                !check_controllers(info->request.doc, lastinfo->request.doc))
            goto errorExit;
    } else if (!strcmp(info->request.header.op, "transfer")) {
        if (!lastinfo) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "DID not exist.");
            goto errorExit;
        }
        if (!strcmp(lastinfo->request.header.op, "deactivate")) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "DID already deactivate.");
            goto errorExit;
        }
        if (strcmp(info->request.header.prevtxid, lastinfo->txid)) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "Previous transaction id missmatch.");
            goto errorExit;
        }
        if (!info->request.header.ticket) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "Transfer operation must attach the ticket.");
            goto errorExit;
        }
        //check ticket
        if (check_ticket(info->request.header.ticket, info->request.doc))
            goto errorExit;
    } else if (!strcmp(info->request.header.op, "deactivate")) {
        if (!lastinfo) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "DID not exist.");
            goto errorExit;
        }
        if (!strcmp(lastinfo->request.header.op, "deactivate")) {
            DIDError_Set(DIDERR_TRANSACTION_ERROR, "DID already dactivated.");
            goto errorExit;
        }
    } else {
        DIDError_Set(DIDERR_UNSUPPOTED, "No this operation.");
        goto errorExit;
    }

    if (get_txid(info->txid) == -1) {
        DIDError_Set(DIDERR_TRANSACTION_ERROR, "Generate transaction id failed.");
        goto errorExit;
    }

    info->timestamp = time(NULL);
    infos[num++] = info;
    json_decref(root);
    return true;

errorExit:
    if (info)
        DIDTransactionInfo_Destroy(info);
    if (root)
        json_decref(root);

    return false;
}

static int result_tojson(JsonGenerator *gen, DID *did, bool all)
{
    DIDTransactionInfo *info;
    char idstring[ELA_MAX_DID_LEN];
    int status;

    assert(gen);

    CHECK(JsonGenerator_WriteStartObject(gen));
    CHECK(JsonGenerator_WriteStringField(gen, "did",
            DID_ToString(did, idstring, sizeof(idstring))));

    info = get_lasttransaction(did);
    if (!info)
        return -1;

    if (!strcmp(info->request.header.op, "deactivate")) {
        status = 2;
    } else {
        if (DIDDocument_IsExpires(info->request.doc))
            status = 1;
        else
            status = 0;
    }

    CHECK(JsonGenerator_WriteFieldName(gen, "status"));
    CHECK(JsonGenerator_WriteNumber(gen, status));

    if (status == 3) {
        CHECK(JsonGenerator_WriteEndObject(gen));
        return 0;
    }

    CHECK(JsonGenerator_WriteFieldName(gen, "transaction"));
    CHECK(JsonGenerator_WriteStartArray(gen));
    if (all) {
        int i;
        for (i = num - 1; i >= 0; i--) {
            info = get_transaction(did, i);
            if (info)
                CHECK(DIDTransactionInfo_ToJson_Internal(gen, info));
        }
    } else {
        info = get_lasttransaction(did);
        CHECK(DIDTransactionInfo_ToJson_Internal(gen, info));
    }
    CHECK(JsonGenerator_WriteEndArray(gen));
    CHECK(JsonGenerator_WriteEndObject(gen));
    return 0;
}

static int resolve_tojson(JsonGenerator *gen, DID *did, bool all)
{
    assert(gen);
    assert(did);

    CHECK(JsonGenerator_WriteStartObject(gen));
    CHECK(JsonGenerator_WriteStringField(gen, "jsonrpc", "2.0"));
    CHECK(JsonGenerator_WriteFieldName(gen, "result"));
    CHECK(result_tojson(gen, did, all));
    CHECK(JsonGenerator_WriteEndObject(gen));
    return 0;
}

const char* DummyAdapter_Resolve(DIDResolver *resolver, const char *did, int all)
{
    JsonGenerator g, *gen;
    DID *_did;
    int rc;

    if (!resolver || !did) {
        DIDError_Set(DIDERR_INVALID_ARGS, "Invalid arguments.");
        return NULL;
    }

    if (strncmp(did, elastos_did_prefix, strlen(elastos_did_prefix))) {
        DIDError_Set(DIDERR_UNSUPPOTED, "Unsupport this did.");
        return NULL;
    }

    _did = DID_FromString(did);
    if (!_did)
        return NULL;

    gen = JsonGenerator_Initialize(&g);
    if (!gen)
        return NULL;

    rc = resolve_tojson(gen, _did, all);
    DID_Destroy(_did);
    if (rc < 0) {
        JsonGenerator_Destroy(gen);
        return NULL;
    }

    return JsonGenerator_Finish(gen);
}

static void DummyAdapter_Reset(DummyAdapter *adapter)
{
    int i;
    for (i = 0; i < num; i++) {
        DIDTransactionInfo_Destroy(infos[i]);
        free(infos[i]);
    }
    memset(infos, 0, sizeof(infos));
    num = 0;
}

DummyAdapter *DummyAdapter_Create(void)
{
    adapterInstance.adapter.createIdTransaction = DummyAdapter_CreateIdTransaction;
    adapterInstance.resolver.resolve = DummyAdapter_Resolve;
    adapterInstance.reset = DummyAdapter_Reset;
    return &adapterInstance;
}

void DummyAdapter_Destroy(void)
{
    int i;
    for (i = 0; i < num; i++) {
        DIDTransactionInfo_Destroy(infos[i]);
        free(infos[i]);
    }

    memset(infos, 0, sizeof(infos));
    num = 0;
}