/*
 * Copyright (c) 2019 Elastos Foundation
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

#ifndef __DOC_TEST_SUITES_H__
#define __DOC_TEST_SUITES_H__

DECL_TESTSUITE(diddoc_fromjson_test);
DECL_TESTSUITE(diddoc_getelem_test);
DECL_TESTSUITE(diddoc_arelem_test);
DECL_TESTSUITE(diddoc_base_test);
DECL_TESTSUITE(diddoc_sign_test);

#define DEFINE_DOC_TESTSUITES \
    DEFINE_TESTSUITE(diddoc_fromjson_test), \
    DEFINE_TESTSUITE(diddoc_getelem_test), \
    DEFINE_TESTSUITE(diddoc_arelem_test), \
    DEFINE_TESTSUITE(diddoc_base_test), \
    DEFINE_TESTSUITE(diddoc_sign_test)

#endif /* __DOC_TEST_SUITES_H__ */