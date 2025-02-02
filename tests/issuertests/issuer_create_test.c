#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <limits.h>
#include <CUnit/Basic.h>
#include <crystal.h>

#include "ela_did.h"
#include "loader.h"
#include "constant.h"
#include "diddocument.h"

static DIDDocument *issuerdoc;
static DID *issuerid;
static DIDURL *signkey;
static DIDStore *store;

static void test_issuer_create(void)
{
    Issuer *issuer;

    issuer = Issuer_Create(issuerid, signkey, store);
    CU_ASSERT_PTR_NOT_NULL_FATAL(issuer);

    CU_ASSERT_TRUE(DID_Equals(issuerid, Issuer_GetSigner(issuer)));
    CU_ASSERT_TRUE(DIDURL_Equals(signkey, Issuer_GetSignKey(issuer)));

    Issuer_Destroy(issuer);
}

static void test_issuer_create_without_key(void)
{
    Issuer *issuer;

    issuer = Issuer_Create(issuerid, NULL, store);
    CU_ASSERT_PTR_NOT_NULL_FATAL(issuer);

    CU_ASSERT_TRUE(DID_Equals(issuerid, Issuer_GetSigner(issuer)));
    CU_ASSERT_TRUE(DIDURL_Equals(signkey, Issuer_GetSignKey(issuer)));

    Issuer_Destroy(issuer);
}

static void test_issuer_create_with_invalidkey1(void)
{
    char pkbase[PUBLICKEY_BASE58_BYTES];
    const char *publickeybase;
    DIDDocumentBuilder *builder;
    DIDURL *keyid;
    Issuer *issuer;
    DIDDocument *doc;
    int rc;

    builder = DIDDocument_Edit(issuerdoc, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(builder);

    publickeybase = Generater_Publickey(pkbase, sizeof(pkbase));
    CU_ASSERT_PTR_NOT_NULL_FATAL(publickeybase);

    keyid = DIDURL_NewFromDid(DIDDocument_GetSubject(issuerdoc), "testkey");
    CU_ASSERT_PTR_NOT_NULL_FATAL(keyid);

    rc = DIDDocumentBuilder_AddAuthenticationKey(builder, keyid, publickeybase);
    CU_ASSERT_NOT_EQUAL_FATAL(rc, -1);

    doc = DIDDocumentBuilder_Seal(builder, storepass);
    CU_ASSERT_PTR_NOT_NULL(doc);
    CU_ASSERT_TRUE(DIDDocument_IsValid(doc));
    DIDDocumentBuilder_Destroy(builder);
    DIDDocument_Destroy(doc);

    issuer = Issuer_Create(issuerid, keyid, store);
    DIDURL_Destroy(keyid);

    CU_ASSERT_PTR_NULL(issuer);
    Issuer_Destroy(issuer);
}

static void test_issuer_create_with_invalidkey2(void)
{
    DIDURL *key;
    Issuer *issuer;

    key = DIDURL_NewFromDid(issuerid, "key2");
    CU_ASSERT_PTR_NOT_NULL_FATAL(key);

    issuer = Issuer_Create(issuerid, key, store);
    CU_ASSERT_PTR_NULL(issuer);

    Issuer_Destroy(issuer);
    DIDURL_Destroy(key);
}

static void test_issuer_create_by_cid(void)
{
    Issuer *issuer;

    CU_ASSERT_PTR_NOT_NULL(TestData_GetDocument("issuer", NULL, 0));
    CU_ASSERT_PTR_NOT_NULL(TestData_GetDocument("document", NULL, 0));

    DIDDocument *customized_doc = TestData_GetDocument("customized-did", NULL, 0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(customized_doc);

    DIDDocument *doc = TestData_GetDocument("document", NULL, 0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc);

    DIDURL *signkey = DIDURL_NewFromDid(&doc->did, "key3");
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    issuer = Issuer_Create(&customized_doc->did, signkey, store);
    CU_ASSERT_PTR_NOT_NULL_FATAL(issuer);
    CU_ASSERT_TRUE(DID_Equals(&customized_doc->did, Issuer_GetSigner(issuer)));
    CU_ASSERT_TRUE(DIDURL_Equals(signkey, Issuer_GetSignKey(issuer)));
    DIDURL_Destroy(signkey);
    Issuer_Destroy(issuer);

    signkey = DIDURL_NewFromDid(&customized_doc->did, "k1");
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    issuer = Issuer_Create(&customized_doc->did, signkey, store);
    CU_ASSERT_PTR_NOT_NULL_FATAL(issuer);
    CU_ASSERT_TRUE(DID_Equals(&customized_doc->did, Issuer_GetSigner(issuer)));
    CU_ASSERT_TRUE(DIDURL_Equals(signkey, Issuer_GetSignKey(issuer)));
    DIDURL_Destroy(signkey);
    Issuer_Destroy(issuer);

    signkey = DIDDocument_GetDefaultPublicKey(doc);
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    issuer = Issuer_Create(&customized_doc->did, NULL, store);
    CU_ASSERT_PTR_NOT_NULL_FATAL(issuer);
    CU_ASSERT_TRUE(DID_Equals(&customized_doc->did, Issuer_GetSigner(issuer)));
    CU_ASSERT_TRUE(DIDURL_Equals(signkey, Issuer_GetSignKey(issuer)));
    Issuer_Destroy(issuer);
}

static void test_issuer_create_by_multicid(void)
{
    Issuer *issuer;
    DID controller1, controller2, controller3;
    ssize_t size;

    CU_ASSERT_PTR_NOT_NULL(TestData_GetDocument("issuer", NULL, 0));
    CU_ASSERT_PTR_NOT_NULL(TestData_GetDocument("controller", NULL, 0));
    CU_ASSERT_PTR_NOT_NULL(TestData_GetDocument("document", NULL, 0));

    DIDDocument *customized_doc = TestData_GetDocument("customized-multisigone", NULL, 0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(customized_doc);

    DID *controllers[3] = {0};
    size = DIDDocument_GetControllers(customized_doc, controllers, 3);
    CU_ASSERT_EQUAL(3, size);
    DID_Copy(&controller1, controllers[0]);
    DID_Copy(&controller2, controllers[1]);
    DID_Copy(&controller3, controllers[2]);

    DIDURL *signkey = DIDURL_NewFromDid(&controller1, "key3");
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    issuer = Issuer_Create(&customized_doc->did, signkey, store);
    CU_ASSERT_PTR_NOT_NULL_FATAL(issuer);
    CU_ASSERT_TRUE(DID_Equals(&customized_doc->did, Issuer_GetSigner(issuer)));
    CU_ASSERT_TRUE(DIDURL_Equals(signkey, Issuer_GetSignKey(issuer)));
    DIDURL_Destroy(signkey);
    Issuer_Destroy(issuer);

    signkey = DIDURL_NewFromDid(&controller2, "pk1");
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    issuer = Issuer_Create(&customized_doc->did, signkey, store);
    CU_ASSERT_PTR_NOT_NULL_FATAL(issuer);
    CU_ASSERT_TRUE(DID_Equals(&customized_doc->did, Issuer_GetSigner(issuer)));
    CU_ASSERT_TRUE(DIDURL_Equals(signkey, Issuer_GetSignKey(issuer)));
    DIDURL_Destroy(signkey);
    Issuer_Destroy(issuer);

    signkey = DIDURL_NewFromDid(&customized_doc->did, "k1");
    CU_ASSERT_PTR_NOT_NULL_FATAL(signkey);

    issuer = Issuer_Create(&customized_doc->did, signkey, store);
    CU_ASSERT_PTR_NULL_FATAL(issuer);
    DIDURL_Destroy(signkey);

    issuer = Issuer_Create(&customized_doc->did, NULL, store);
    CU_ASSERT_PTR_NULL(issuer);
}

static int issuer_create_test_suite_init(void)
{
    int rc;

    store = TestData_SetupStore(true);
    if (!store)
        return -1;

    issuerdoc = TestData_GetDocument("issuer", NULL, 0);
    if (!issuerdoc) {
        TestData_Free();
        return -1;
    }

    rc = DIDStore_StoreDID(store, issuerdoc);
    if (rc < 0) {
        TestData_Free();
        return rc;
    }

    issuerid = DIDDocument_GetSubject(issuerdoc);
    if (!issuerid) {
        TestData_Free();
        return -1;
    }

    signkey = DIDDocument_GetDefaultPublicKey(issuerdoc);
    if (!signkey) {
        TestData_Free();
        return -1;
    }

    return 0;
}

static int issuer_create_test_suite_cleanup(void)
{
    TestData_Free();
    return 0;
}

static CU_TestInfo cases[] = {
    { "test_issuer_create",                     test_issuer_create                     },
    { "test_issuer_create_without_key",         test_issuer_create_without_key         },
    { "test_issuer_create_with_invalidkey1",    test_issuer_create_with_invalidkey1    },
    { "test_issuer_create_with_invalidkey2",    test_issuer_create_with_invalidkey2    },
    { "test_issuer_create_by_cid",              test_issuer_create_by_cid              },
    { "test_issuer_create_by_multicid",         test_issuer_create_by_multicid         },
    { NULL,                                     NULL                                   }
};

static CU_SuiteInfo suite[] = {
    { "issuer create test", issuer_create_test_suite_init, issuer_create_test_suite_cleanup, NULL, NULL, cases },
    {  NULL,                NULL,                          NULL,                             NULL, NULL, NULL  }
};


CU_SuiteInfo* issuer_create_test_suite_info(void)
{
    return suite;
}
