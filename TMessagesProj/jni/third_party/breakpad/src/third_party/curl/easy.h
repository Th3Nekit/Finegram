#ifndef __CURL_EASY_H
#define __CURL_EASY_H
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2008, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * $Id: easy.h,v 1.14 2008-05-12 21:43:28 bagder Exp $
 ***************************************************************************/
#ifdef  __cplusplus
extern "C" {
#endif

CURL_EXTERN CURL *curl_easy_init(void);
CURL_EXTERN CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...);
CURL_EXTERN CURLcode curl_easy_perform(CURL *curl);
CURL_EXTERN void curl_easy_cleanup(CURL *curl);

CURL_EXTERN CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...);

CURL_EXTERN CURL* curl_easy_duphandle(CURL *curl);

CURL_EXTERN void curl_easy_reset(CURL *curl);

CURL_EXTERN CURLcode curl_easy_recv(CURL *curl, void *buffer, size_t buflen,
                                    size_t *n);

CURL_EXTERN CURLcode curl_easy_send(CURL *curl, const void *buffer,
                                    size_t buflen, size_t *n);

#ifdef  __cplusplus
}
#endif

#endif
