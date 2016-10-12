/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <os/os.h>
#include <os/endian.h>

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <mgmt/mgmt.h>
#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>
#include <json/json.h>
#include <base64/base64.h>
#include <hal/hal_bsp.h>

#include "split/split.h"
#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

static void
imgr_hash_jsonstr(struct json_encoder *enc, char *key, uint8_t *hash)
{
    struct json_value jv;
    char hash_str[IMGMGR_HASH_STR + 1];

    base64_encode(hash, IMGMGR_HASH_LEN, hash_str, 1);
    JSON_VALUE_STRING(&jv, hash_str);
    json_encode_object_entry(enc, key, &jv);
}

int
imgr_boot2_read(struct mgmt_jbuf *njb)
{
    int rc;
    struct json_encoder *enc;
    struct image_version ver;
    struct json_value jv;
    uint8_t hash[IMGMGR_HASH_LEN];
    int test_slot;
    int main_slot;
    int active_slot;

    enc = &njb->mjb_enc;

    json_encode_object_start(enc);

    test_slot = -1;
    main_slot = -1;
    active_slot = -1;

    /* Temporary hack to preserve old behavior. */
    if (boot_split_app_active_get()) {
        if (split_mode_get() == SPLIT_MODE_TEST_APP) {
            test_slot = 0;
        }
        main_slot = 0;
        active_slot = 1;
    } else {
        boot_vect_read_test(&test_slot);
        boot_vect_read_main(&main_slot);
        active_slot = boot_current_slot;
    }

    if (test_slot != -1) {
        rc = imgr_read_info(test_slot, &ver, hash, NULL);
        if (rc >= 0) {
            imgr_hash_jsonstr(enc, "test", hash);
        }
    }

    if (main_slot != -1) {
        rc = imgr_read_info(main_slot, &ver, hash, NULL);
        if (rc >= 0) {
            imgr_hash_jsonstr(enc, "main", hash);
        }
    }

    if (active_slot != -1) {
        rc = imgr_read_info(active_slot, &ver, hash, NULL);
        if (rc >= 0) {
            imgr_hash_jsonstr(enc, "active", hash);
        }
    }

    JSON_VALUE_INT(&jv, MGMT_ERR_EOK);
    json_encode_object_entry(enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;
}

int
imgr_boot2_write(struct mgmt_jbuf *njb)
{
    char hash_str[IMGMGR_HASH_STR + 1];
    uint8_t hash[IMGMGR_HASH_LEN];
    const struct json_attr_t boot_write_attr[2] = {
        [0] = {
            .attribute = "test",
            .type = t_string,
            .addr.string = hash_str,
            .len = sizeof(hash_str),
        },
        [1] = {
            .attribute = NULL
        }
    };
    struct json_encoder *enc;
    struct json_value jv;
    int rc;
    struct image_version ver;

    rc = json_read_object(&njb->mjb_buf, boot_write_attr);
    if (rc) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    base64_decode(hash_str, hash);
    rc = imgr_find_by_hash(hash, &ver);
    if (rc >= 0) {
        rc = boot_vect_write_test(rc);
        if (rc) {
            rc = MGMT_ERR_EUNKNOWN;
            goto err;
        }
        rc = 0;
    } else {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    enc = &njb->mjb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_INT(&jv, MGMT_ERR_EOK);
    json_encode_object_entry(&njb->mjb_enc, "rc", &jv);

    json_encode_object_finish(enc);

    return 0;

err:
    mgmt_jbuf_setoerr(njb, rc);

    return 0;
}