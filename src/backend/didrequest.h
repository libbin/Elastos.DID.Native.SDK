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

#ifndef __DIDREQUEST_H__
#define __DIDREQUEST_H__

#include <jansson.h>

#include "ela_did.h"
#include "JsonGenerator.h"
#include "did.h"
#include "didurl.h"
#include "common.h"
#include "ticket.h"

#ifdef __cplusplus
extern "C" {
#endif

#define  MAX_SPEC_LEN             32
#define  MAX_OP_LEN               32

typedef struct DIDRequest {
    struct {
        char spec[MAX_SPEC_LEN];
        char op[MAX_OP_LEN];
        char prevtxid[ELA_MAX_TXID_LEN];
        const char *ticket;
    } header;

    const char *payload;
    DIDDocument *doc;
    DID did;

    struct {
        DIDURL verificationMethod;
        char signatureValue[MAX_SIGNATURE_LEN];
    } proof;
} DIDRequest;

typedef enum DIDRequest_Type
{
   RequestType_Create,
   RequestType_Update,
   RequestType_Transfer,
   RequestType_Deactivate
} DIDRequest_Type;

int DIDRequest_FromJson(DIDRequest *request, json_t *json);

void DIDRequest_Destroy(DIDRequest *request);

void DIDRequest_Free(DIDRequest *request);

const char *DIDRequest_Sign(DIDRequest_Type type, DIDDocument *document,
        DIDURL *signkey, DIDURL *creater, TransferTicket *ticket, const char *storepass);

DIDDocument *DIDRequest_GetDocument(DIDRequest *request);

int DIDRequest_ToJson_Internal(JsonGenerator *gen, DIDRequest *req);

bool DIDRequest_IsValid(DIDRequest *request, DIDDocument *document);

#ifdef __cplusplus
}
#endif

#endif //__DIDREQUEST_H__
