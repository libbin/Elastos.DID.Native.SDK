#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <CUnit/Basic.h>
#include <limits.h>
#include <crystal.h>

#include "ela_did.h"
#include "HDkey.h"
#include "constant.h"
#include "loader.h"
#include "did.h"
#include "didmeta.h"
#include "diddocument.h"
#include "credential.h"

#define MAX_DOC_SIGN              128

static DIDStore *store;

static bool has_type(const char **types, size_t size, const char *type)
{
    int i;

    if (!types || size <= 0 || !type || !*type)
        return false;

    for (i = 0; i < size; i++) {
        if (!strcmp(types[i], type))
            return true;
    }

    return false;
}

static void test_idchain_publishdid(void)
{
    DIDURL *signkey;
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    char *signs[3];
    DIDDocument *resolvedoc = NULL, *doc;
    RootIdentity *rootidentity;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish", *sign;
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    signkey = DIDDocument_GetDefaultPublicKey(doc);
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, signkey, false, storepass);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n-- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    DIDMetadata *metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);

    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    sign = DIDDocument_GetProofSignature(doc, 0);
    CU_ASSERT_STRING_EQUAL(DIDDocument_GetProofSignature(doc, 0), DIDDocument_GetProofSignature(resolvedoc, 0));
    signs[0] = alloca(strlen(sign) + 1);
    strcpy(signs[0], sign);

    DIDDocument_Destroy(doc);
    printf("\n   txid = %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);

    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    sign = DIDDocument_GetProofSignature(doc, 0);
    signs[1] = alloca(strlen(sign) + 1);
    strcpy(signs[1], sign);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n-- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(resolvedoc));
    printf("\n   txid = %s\n-- resolve result: successfully!\n-- publish begin(update) again, waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update again
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    keyid = DIDURL_NewFromDid(&did, "key2");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    sign = DIDDocument_GetProofSignature(doc, 0);
    signs[2] = alloca(strlen(sign) + 1);
    strcpy(signs[2], sign);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n-- resolve begin(update) again", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(resolvedoc));

    printf("\n   txid = %s\n-- resolve result: successfully!\n------------------------------------------------------------\n", txid);
    DIDDocument_Destroy(resolvedoc);

    //DIDBiography
    DIDBiography *biography = DID_ResolveBiography(&did);
    CU_ASSERT_PTR_NOT_NULL_FATAL(biography);
    CU_ASSERT_EQUAL(3, DIDBiography_GetTransactionCount(biography));
    CU_ASSERT_EQUAL(0, DIDBiography_GetStatus(biography));

    DID *owner = DIDBiography_GetOwner(biography);
    CU_ASSERT_PTR_NOT_NULL_FATAL(owner);
    CU_ASSERT_TRUE_FATAL(DID_Equals(&did, owner));

    for (i = 0; i < 3; i++) {
        doc = DIDBiography_GetDocumentByIndex(biography, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
        CU_ASSERT_STRING_EQUAL(signs[2-i], DIDDocument_GetProofSignature(doc, 0));
        DIDDocument_Destroy(doc);
    }
    DIDBiography_Destroy(biography);
}

static void test_idchain_publishdid_without_txid(void)
{
    DIDURL *signkey;
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);
    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    signkey = DIDDocument_GetDefaultPublicKey(doc);
    CU_ASSERT_PTR_NOT_NULL(signkey);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, signkey, false, storepass);
    CU_ASSERT_TRUE_FATAL(success);
    DIDDocument_Destroy(doc);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    printf("\n   txid = %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);

    rc = DIDMetadata_SetTxid(metadata, "");
    CU_ASSERT_NOT_EQUAL(rc, -1);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_STRING_EQUAL(txid, "");

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    printf("\n   txid = %s\n-- resolve result: successfully!\n-- publish begin(update) again, waiting...\n", txid);
    metadata = DIDDocument_GetMetadata(resolvedoc);
    rc = DIDMetadata_SetTxid(metadata, "");
    CU_ASSERT_NOT_EQUAL(rc, -1);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update again
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    keyid = DIDURL_NewFromDid(&did, "key2");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update) again", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(resolvedoc));

    printf("\n   txid: %s\n-- resolve result: successfully!\n------------------------------------------------------------\n", txid);
    DIDDocument_Destroy(resolvedoc);
}

static void test_idchain_publishdid_without_signature(void)
{
    DIDURL *signkey;
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);
    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    signkey = DIDDocument_GetDefaultPublicKey(doc);
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, signkey, false, storepass);
    CU_ASSERT_TRUE_FATAL(success);
    DIDDocument_Destroy(doc);

    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDMetadata_SetPrevSignature(metadata, resolvedoc->proofs.proofs[0].signatureValue);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    rc = DIDMetadata_SetSignature(metadata, "");
    CU_ASSERT_NOT_EQUAL(rc, -1);
    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update) again, waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update again
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    keyid = DIDURL_NewFromDid(&did, "key2");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update) again", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(resolvedoc));

    printf("\n   txid: %s\n-- resolve result: successfully!\n------------------------------------------------------------\n", txid);
    DIDDocument_Destroy(resolvedoc);
}

static void test_idchain_publishdid_without_prevsignature(void)
{
    DIDURL *signkey;
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    signkey = DIDDocument_GetDefaultPublicKey(doc);
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, signkey, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);

    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDMetadata_SetPrevSignature(metadata, "");
    CU_ASSERT_NOT_EQUAL(rc, -1);
    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update) again, waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update again
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    keyid = DIDURL_NewFromDid(&did, "key2");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);
    const char *signature = DIDMetadata_GetPrevSignature(metadata);
    CU_ASSERT_PTR_NOT_NULL(signature);
    CU_ASSERT_STRING_EQUAL(signature, "");

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update) again", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(resolvedoc));

    printf("\n   txid: %s\n-- resolve result: successfully!\n------------------------------------------------------------\n", txid);
    DIDDocument_Destroy(resolvedoc);
}

static void test_idchain_publishdid_without_prevsignature_and_signature(void)
{
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDMetadata_SetSignature(metadata, ""));
    CU_ASSERT_NOT_EQUAL(-1, DIDMetadata_SetPrevSignature(metadata, ""));
    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase));
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, doc));
    CU_ASSERT_NOT_EQUAL(1, DIDDocument_PublishDID(doc, NULL, false, storepass));
    CU_ASSERT_STRING_EQUAL("Missing signatures information, DID SDK dosen't know how to handle it, use force mode to ignore checks.",
           DIDError_GetLastErrorMessage());
    DIDDocument_Destroy(doc);
}

static void test_force_updatedid_without_prevsignature_and_signature(void)
{
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDMetadata_SetSignature(metadata, ""));
    CU_ASSERT_NOT_EQUAL(-1, DIDMetadata_SetPrevSignature(metadata, ""));
    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase));
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, doc));

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, true, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(resolvedoc));

    printf("\n   txid: %s\n-- resolve result: successfully!\n------------------------------------------------------------\n", txid);
    DIDDocument_Destroy(resolvedoc);
}

static void test_updatedid_with_diffprevsignature_only(void)
{
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDMetadata_SetPrevSignature(metadata, "123456789");
    CU_ASSERT_NOT_EQUAL(rc, -1);
    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n-- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(resolvedoc));

    DIDDocument_Destroy(resolvedoc);
}

static void test_updatedid_with_diffsignature_only(void)
{
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n    txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDMetadata_SetPrevSignature(metadata, resolvedoc->proofs.proofs[0].signatureValue);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    rc = DIDMetadata_SetSignature(metadata, "123456789");
    CU_ASSERT_NOT_EQUAL(rc, -1);
    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update) again, waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update again
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    keyid = DIDURL_NewFromDid(&did, "key2");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    rc = DIDStore_StoreDID(store, doc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update) again", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    CU_ASSERT_EQUAL(3, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(3, DIDDocument_GetAuthenticationCount(resolvedoc));

    DIDDocument_Destroy(resolvedoc);
    printf("\n-- resolve result: successfully!\n------------------------------------------------------------\n");
}

static void test_updatedid_with_diff_prevsignature_and_signature(void)
{
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    rc = DIDMetadata_SetSignature(metadata, "12345678");
    CU_ASSERT_NOT_EQUAL(rc, -1);
    rc = DIDMetadata_SetPrevSignature(metadata, "12345678");
    CU_ASSERT_NOT_EQUAL(rc, -1);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase));
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, doc));
    CU_ASSERT_NOT_EQUAL(1, DIDDocument_PublishDID(doc, NULL, false, storepass));
    CU_ASSERT_STRING_EQUAL("Current copy not based on the lastest on-chain copy.",
            DIDError_GetLastErrorMessage());
    DIDDocument_Destroy(doc);
}

static void test_force_updatedid_with_wrongsignature(void)
{
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDMetadata_SetSignature(metadata, "12345678"));
    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));

    printf("\n  txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase));
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, doc));

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, true, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(resolvedoc));

    DIDDocument_Destroy(resolvedoc);
    printf("\n-- resolve result: successfully!\n------------------------------------------------------------\n");
}

static void test_idchain_publishdid_with_credential(void)
{
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid;
    Credential *cred;
    bool success;
    DID did;
    int i = 0, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    doc = RootIdentity_NewDID(rootidentity, storepass, "littlefish", false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    DIDURL *credid = DIDURL_NewFromDid(&did, "cred-1");
    CU_ASSERT_PTR_NOT_NULL(credid);

    const char *types[] = {"BasicProfileCredential", "SelfClaimedCredential"};

    Property props[1];
    props[0].key = "name";
    props[0].value = "John";

    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddSelfProclaimedCredential(builder, credid, types, 2, props, 1, 0, NULL, storepass));

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    DIDDocumentBuilder_Destroy(builder);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, doc));

    cred = DIDDocument_GetCredential(doc, credid);
    CU_ASSERT_PTR_NOT_NULL(cred);

    success = DIDDocument_PublishDID(doc, NULL, true, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    printf("\n-- resolve result: successfully!\n------------------------------------------------------------\n");

    cred = DIDDocument_GetCredential(resolvedoc, credid);
    CU_ASSERT_PTR_NOT_NULL(cred);

    DIDURL_Destroy(credid);
    DIDDocument_Destroy(resolvedoc);
}

static void test_idchain_deactivedid_after_create(void)
{
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid;
    bool success;
    DID did;
    int i = 0, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    doc = RootIdentity_NewDID(rootidentity, storepass, "littlefish", false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    const char *data1 = DIDDocument_ToJson(doc, true);
    const char *data2 = DIDDocument_ToJson(resolvedoc, true);
    DIDDocument_Destroy(resolvedoc);
    CU_ASSERT_STRING_EQUAL(data1, data2);
    free((void*)data1);
    free((void*)data2);

    success = DIDDocument_DeactivateDID(doc, NULL, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE(success);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);
    CU_ASSERT_EQUAL(status, DIDStatus_Deactivated);

    printf("\n-- resolve result: successfully!\n------------------------------------------------------------\n");
    DIDDocument_Destroy(resolvedoc);
    return;
}

static void test_idchain_deactivedid_after_update(void)
{
    DIDURL *signkey;
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *doc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    bool success;
    DID did;
    int i = 0, rc, status;

    mnemonic = Mnemonic_Generate(language);

    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    //create
    doc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    RootIdentity_Destroy(rootidentity);
    CU_ASSERT_PTR_NOT_NULL(doc);

    signkey = DIDDocument_GetDefaultPublicKey(doc);
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    DID_Copy(&did, DIDDocument_GetSubject(doc));

    printf("\n------------------------------------------------------------\n-- publish begin(create), waiting....\n");
    success = DIDDocument_PublishDID(doc, signkey, false, storepass);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);
    CU_ASSERT_NOT_EQUAL(status, DIDStatus_Deactivated);

    rc = DIDStore_StoreDID(store, resolvedoc);
    CU_ASSERT_NOT_EQUAL(rc, -1);
    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    const char *nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    CU_ASSERT_STRING_EQUAL(DIDDocument_GetProofSignature(doc, 0), DIDDocument_GetProofSignature(resolvedoc, 0));
    DIDDocument_Destroy(doc);

    printf("\n   txid: %s\n-- resolve result: successfully!\n-- publish begin(update), waiting...\n", txid);
    DIDDocument_Destroy(resolvedoc);
    resolvedoc = NULL;

    //update
    doc = DIDStore_LoadDID(store, &did);
    CU_ASSERT_PTR_NOT_NULL(doc);

    DIDDocumentBuilder *builder = DIDDocument_Edit(doc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(doc);

    keybase = Generater_Publickey(publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);
    DIDURL *keyid = DIDURL_NewFromDid(&did, "key1");
    CU_ASSERT_PTR_NOT_NULL(keyid);
    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, keybase));
    DIDURL_Destroy(keyid);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(doc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(doc));
    DIDDocumentBuilder_Destroy(builder);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, doc));

    metadata = DIDDocument_GetMetadata(doc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    nalias = DIDMetadata_GetAlias(metadata);
    CU_ASSERT_PTR_NOT_NULL(nalias);
    CU_ASSERT_STRING_EQUAL(alias, nalias);

    success = DIDDocument_PublishDID(doc, NULL, false, storepass);
    DIDDocument_Destroy(doc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(update)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);
    CU_ASSERT_NOT_EQUAL(status, DIDStatus_Deactivated);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(resolvedoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(resolvedoc));
    printf("\n-- resolve result: successfully!\n-- deactive did begin, waiting...\n");

    success = DIDDocument_DeactivateDID(resolvedoc, NULL, storepass);
    CU_ASSERT_TRUE_FATAL(success);
    DIDDocument_Destroy(resolvedoc);
    printf("-- deactive did result:\n   did = %s\n -- resolve begin(deactive)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);
    CU_ASSERT_EQUAL(status, DIDStatus_Deactivated);

    printf("\n-- resolve result: successfully!\n------------------------------------------------------------\n");
    DIDDocument_Destroy(resolvedoc);
    return;
}

static void test_idchain_deactivedid_with_authorization1(void)
{
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc, *targetdoc, *authorizordoc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *alias = "littlefish";
    DID controller, did;
    PublicKey *pks[1];
    bool success;
    int i = 0, status;

    mnemonic = Mnemonic_Generate(language);
    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    authorizordoc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    CU_ASSERT_PTR_NOT_NULL(authorizordoc);

    DID_Copy(&controller, DIDDocument_GetSubject(authorizordoc));

    printf("\n------------------------------------------------------------\n-- publish authorization did begin(create), waiting....\n");
    success = DIDDocument_PublishDID(authorizordoc, NULL, false, storepass);
    DIDDocument_Destroy(authorizordoc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", controller.idstring);

    authorizordoc = DID_Resolve(&controller, &status, true);
    CU_ASSERT_PTR_NOT_NULL(authorizordoc);
    CU_ASSERT_NOT_EQUAL(status, DIDStatus_Deactivated);
    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, authorizordoc));

    metadata = DIDDocument_GetMetadata(authorizordoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    printf("\n   txid: %s\n-- resolve authorization result: successfully!\n", txid);

    targetdoc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    CU_ASSERT_PTR_NOT_NULL(targetdoc);

    DID_Copy(&did, DIDDocument_GetSubject(targetdoc));

    DIDDocumentBuilder *builder = DIDDocument_Edit(targetdoc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);
    DIDDocument_Destroy(targetdoc);

    DIDURL *keyid = DIDURL_NewFromDid(&did, "recovery");
    CU_ASSERT_PTR_NOT_NULL(keyid);

    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AuthorizeDid(builder, keyid, &controller, NULL));
    DIDURL_Destroy(keyid);

    targetdoc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL_FATAL(targetdoc);
    DIDDocumentBuilder_Destroy(builder);
    CU_ASSERT_EQUAL(1, DIDDocument_GetAuthorizationCount(targetdoc));

    CU_ASSERT_EQUAL(1, DIDDocument_GetAuthorizationKeys(targetdoc, pks, sizeof(pks)));
    CU_ASSERT_TRUE(DID_Equals(&did, &pks[0]->id.did));

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, targetdoc));

    printf("-- publish target did begin(create), waiting....\n");
    success = DIDDocument_PublishDID(targetdoc, NULL, false, storepass);
    DIDDocument_Destroy(targetdoc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);
    CU_ASSERT_NOT_EQUAL(status, DIDStatus_Deactivated);

    metadata = DIDDocument_GetMetadata(resolvedoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, resolvedoc));
    DIDDocument_Destroy(resolvedoc);
    printf("\n-- resolve authorization result: successfully!\n");

    success = DIDDocument_DeactivateDIDByAuthorizor(authorizordoc, &did, NULL, storepass);
    CU_ASSERT_TRUE(success);
    DIDDocument_Destroy(authorizordoc);
    printf("-- deactive did result:\n   did = %s\n -- resolve begin(deactive)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);
    CU_ASSERT_EQUAL(status, DIDStatus_Deactivated);

    printf("\n-- resolve target result: successfully!\n------------------------------------------------------------\n");
    RootIdentity_Destroy(rootidentity);
    DIDDocument_Destroy(resolvedoc);
    return;
}

static void test_idchain_deactivedid_with_authorization2(void)
{
    char publickeybase58[PUBLICKEY_BASE58_BYTES];
    RootIdentity *rootidentity;
    DIDDocument *resolvedoc = NULL, *authorizordoc, *targetdoc;
    DIDMetadata *metadata;
    const char *mnemonic, *txid, *keybase, *alias = "littlefish";
    HDKey _dkey, *dkey;
    DID controller, did;
    PublicKey *pks[1];
    bool equal, success;
    int i = 0, status;

    mnemonic = Mnemonic_Generate(language);
    rootidentity = RootIdentity_Create(mnemonic, "", true, store, storepass);
    CU_ASSERT_PTR_NOT_NULL(rootidentity);
    Mnemonic_Free((void*)mnemonic);

    authorizordoc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    CU_ASSERT_PTR_NOT_NULL(authorizordoc);

    DID_Copy(&controller, DIDDocument_GetSubject(authorizordoc));

    DIDDocumentBuilder *builder = DIDDocument_Edit(authorizordoc, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(builder);
    DIDDocument_Destroy(authorizordoc);

    dkey = Generater_KeyPair(&_dkey);
    keybase = HDKey_GetPublicKeyBase58(dkey, publickeybase58, sizeof(publickeybase58));
    CU_ASSERT_PTR_NOT_NULL(keybase);

    DIDURL *signkey = DIDURL_NewFromDid(&controller, "key-2");
    CU_ASSERT_PTR_NOT_NULL(signkey);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StorePrivateKey(store, storepass, signkey,
            HDKey_GetPrivateKey(dkey), PRIVATEKEY_BYTES));

    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddAuthenticationKey(builder, signkey, keybase));

    authorizordoc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL(authorizordoc);
    DIDDocumentBuilder_Destroy(builder);

    CU_ASSERT_EQUAL(2, DIDDocument_GetPublicKeyCount(authorizordoc));
    CU_ASSERT_EQUAL(2, DIDDocument_GetAuthenticationCount(authorizordoc));

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, authorizordoc));

    printf("\n------------------------------------------------------------\n-- publish authorization did begin(create), waiting....\n");
    success = DIDDocument_PublishDID(authorizordoc, NULL, false, storepass);
    DIDDocument_Destroy(authorizordoc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", controller.idstring);

    authorizordoc = DID_Resolve(&controller, &status, true);
    CU_ASSERT_PTR_NOT_NULL(authorizordoc);
    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, authorizordoc));

    metadata = DIDDocument_GetMetadata(authorizordoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);

    printf("\n   txid: %s\n-- resolve authorization result: successfully!\n", txid);

    targetdoc = RootIdentity_NewDID(rootidentity, storepass, alias, false);
    CU_ASSERT_PTR_NOT_NULL(targetdoc);

    builder = DIDDocument_Edit(targetdoc, NULL);
    CU_ASSERT_PTR_NOT_NULL(builder);

    DID_Copy(&did, DIDDocument_GetSubject(targetdoc));
    DIDDocument_Destroy(targetdoc);

    DIDURL *keyid = DIDURL_NewFromDid(&did, "recovery");
    CU_ASSERT_PTR_NOT_NULL(keyid);

    CU_ASSERT_NOT_EQUAL(-1, DIDDocumentBuilder_AddAuthorizationKey(builder, keyid, &controller, keybase));

    targetdoc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL(targetdoc);
    DIDDocumentBuilder_Destroy(builder);
    CU_ASSERT_EQUAL(1, DIDDocument_GetAuthorizationCount(targetdoc));

    size_t size = DIDDocument_GetAuthorizationKeys(targetdoc, pks, sizeof(pks));
    CU_ASSERT_EQUAL(1, size);
    equal = DID_Equals(&did, &pks[0]->id.did);
    CU_ASSERT_TRUE(equal);

    CU_ASSERT_NOT_EQUAL(-1, DIDStore_StoreDID(store, targetdoc));

    printf("-- publish target did begin(create), waiting....\n");
    success = DIDDocument_PublishDID(targetdoc, NULL, false, storepass);
    DIDDocument_Destroy(targetdoc);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- publish result:\n   did = %s\n -- resolve begin(create)", did.idstring);

    targetdoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(targetdoc);
    CU_ASSERT_NOT_EQUAL(status, DIDStatus_Deactivated);

    metadata = DIDDocument_GetMetadata(targetdoc);
    CU_ASSERT_PTR_NOT_NULL(metadata);
    txid = DIDMetadata_GetTxid(metadata);
    CU_ASSERT_PTR_NOT_NULL(txid);
    printf("\n   txid: %s\n-- resolve target result: successfully!", txid);
    DIDDocument_Destroy(targetdoc);

    success = DIDDocument_DeactivateDIDByAuthorizor(authorizordoc, &did, signkey, storepass);
    CU_ASSERT_TRUE_FATAL(success);
    printf("-- deactive did result:\n   did = %s\n -- resolve begin(deactive)", did.idstring);

    resolvedoc = DID_Resolve(&did, &status, true);
    CU_ASSERT_PTR_NOT_NULL(resolvedoc);
    CU_ASSERT_EQUAL(status, DIDStatus_Deactivated);

    printf("\n-- resolve result: successfully!\n------------------------------------------------------------\n");
    DIDDocument_Destroy(resolvedoc);
    DIDDocument_Destroy(authorizordoc);
    DIDURL_Destroy(signkey);
    DIDURL_Destroy(keyid);
    RootIdentity_Destroy(rootidentity);
}

static int idchain_dummyadapter_test_suite_init(void)
{
    store = TestData_SetupStore(true);
    if (!store)
        return -1;

    return 0;
}

static int idchain_dummyadapter_test_suite_cleanup(void)
{
    TestData_Free();
    return 0;
}

static CU_TestInfo cases[] = {
    { "test_idchain_publishdid",                                      test_idchain_publishdid                                     },
    { "test_idchain_publishdid_without_txid",                         test_idchain_publishdid_without_txid                        },
    { "test_idchain_publishdid_without_signature",                    test_idchain_publishdid_without_signature                   },
    { "test_idchain_publishdid_without_prevsignature",                test_idchain_publishdid_without_prevsignature               },
    { "test_idchain_publishdid_without_prevsignature_and_signature",  test_idchain_publishdid_without_prevsignature_and_signature },
    { "test_force_updatedid_without_prevsignature_and_signature",     test_force_updatedid_without_prevsignature_and_signature    },
    { "test_updatedid_with_diffprevsignature_only",                   test_updatedid_with_diffprevsignature_only                  },
    { "test_updatedid_with_diffsignature_only",                       test_updatedid_with_diffsignature_only                      },
    { "test_updatedid_with_diff_prevsignature_and_signature",         test_updatedid_with_diff_prevsignature_and_signature        },
    { "test_force_updatedid_with_wrongsignature",                     test_force_updatedid_with_wrongsignature                    },
    { "test_idchain_publishdid_with_credential",                      test_idchain_publishdid_with_credential                     },
    { "test_idchain_deactivedid_after_create",                        test_idchain_deactivedid_after_create                       },
    { "test_idchain_deactivedid_after_update",                        test_idchain_deactivedid_after_update                       },
    { "test_idchain_deactivedid_with_authorization1",                 test_idchain_deactivedid_with_authorization1                },
    { "test_idchain_deactivedid_with_authorization2",                 test_idchain_deactivedid_with_authorization2                },
    {  NULL,                                                          NULL                                                        }
};

static CU_SuiteInfo suite[] = {
    { "idchain dummyadapter test", idchain_dummyadapter_test_suite_init, idchain_dummyadapter_test_suite_cleanup, NULL, NULL, cases },
    {  NULL,                      NULL,                              NULL,                                 NULL, NULL, NULL  }
};

CU_SuiteInfo* idchain_dummyadapter_test_suite_info(void)
{
    return suite;
}
