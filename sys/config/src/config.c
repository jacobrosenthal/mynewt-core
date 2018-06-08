/*
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

#include <string.h>
#include <stdio.h>

#include "os/mynewt.h"
#include "base64/base64.h"

#include "config/config.h"
#include "config_priv.h"

struct conf_handler_head conf_handlers;

static os_event_fn conf_ev_fn_load;

static struct os_mutex conf_mtx;

/* OS event - causes persisted config values to be loaded at startup. */
static struct os_event conf_ev_load = {
    .ev_cb = conf_ev_fn_load,
};

void
conf_init(void)
{
    int rc;

    os_mutex_init(&conf_mtx);

    SLIST_INIT(&conf_handlers);
    conf_store_init();

    (void)rc;

#if MYNEWT_VAL(CONFIG_CLI)
    rc = conf_cli_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
#if MYNEWT_VAL(CONFIG_NEWTMGR)
    rc = conf_nmgr_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    /* Delay loading the configuration until the default event queue is
     * processed.  This gives main() a chance to configure the underlying
     * storage first.
     */
    os_eventq_put(os_eventq_dflt_get(), &conf_ev_load);
}

void
conf_lock(void)
{
    os_mutex_pend(&conf_mtx, 0xFFFFFFFF);
}

void
conf_unlock(void)
{
    os_mutex_release(&conf_mtx);
}

int
conf_register(struct conf_handler *handler)
{
    conf_lock();
    SLIST_INSERT_HEAD(&conf_handlers, handler, ch_list);
    conf_unlock();
    return 0;
}

static void
conf_ev_fn_load(struct os_event *ev)
{
    conf_load();
}

/*
 * Find conf_handler based on name.
 */
struct conf_handler *
conf_handler_lookup(char *name)
{
    struct conf_handler *ch;

    SLIST_FOREACH(ch, &conf_handlers, ch_list) {
        if (!strcmp(name, ch->ch_name)) {
            return ch;
        }
    }
    return NULL;
}

/*
 * Separate string into argv array.
 */
int
conf_parse_name(char *name, int *name_argc, char *name_argv[])
{
    char *tok;
    char *tok_ptr;
    char *sep = CONF_NAME_SEPARATOR;
    int i;

    tok = strtok_r(name, sep, &tok_ptr);

    i = 0;
    while (tok) {
        name_argv[i++] = tok;
        tok = strtok_r(NULL, sep, &tok_ptr);
    }
    *name_argc = i;

    return 0;
}

struct conf_handler *
conf_parse_and_lookup(char *name, int *name_argc, char *name_argv[])
{
    int rc;

    rc = conf_parse_name(name, name_argc, name_argv);
    if (rc) {
        return NULL;
    }
    return conf_handler_lookup(name_argv[0]);
}

int
conf_value_from_str(char *val_str, enum conf_type type, void *vp, int maxlen)
{
    int32_t val;
    int64_t val64;
    char *eptr;

    if (!val_str) {
        goto err;
    }
    switch (type) {
    case CONF_INT8:
    case CONF_INT16:
    case CONF_INT32:
    case CONF_BOOL:
        val = strtol(val_str, &eptr, 0);
        if (*eptr != '\0') {
            goto err;
        }
        if (type == CONF_BOOL) {
            if (val < 0 || val > 1) {
                goto err;
            }
            *(bool *)vp = val;
        } else if (type == CONF_INT8) {
            if (val < INT8_MIN || val > UINT8_MAX) {
                goto err;
            }
            *(int8_t *)vp = val;
        } else if (type == CONF_INT16) {
            if (val < INT16_MIN || val > UINT16_MAX) {
                goto err;
            }
            *(int16_t *)vp = val;
        } else if (type == CONF_INT32) {
            *(int32_t *)vp = val;
        }
        break;
    case CONF_INT64:
        val64 = strtoll(val_str, &eptr, 0);
        if (*eptr != '\0') {
            goto err;
        }
        *(int64_t *)vp = val64;
        break;
    case CONF_STRING:
        val = strlen(val_str);
        if (val + 1 > maxlen) {
            goto err;
        }
        strcpy(vp, val_str);
        break;
    default:
        goto err;
    }
    return 0;
err:
    return OS_INVALID_PARM;
}

int
conf_bytes_from_str(char *val_str, void *vp, int *len)
{
    int tmp;

    if (base64_decode_len(val_str) > *len) {
        return OS_INVALID_PARM;
    }
    tmp = base64_decode(val_str, vp);
    if (tmp < 0) {
        return OS_INVALID_PARM;
    }
    *len = tmp;
    return 0;
}

char *
conf_str_from_value(enum conf_type type, void *vp, char *buf, int buf_len)
{
    int32_t val;

    if (type == CONF_STRING) {
        return vp;
    }
    switch (type) {
    case CONF_INT8:
    case CONF_INT16:
    case CONF_INT32:
    case CONF_BOOL:
        if (type == CONF_BOOL) {
            val = *(bool *)vp;
        } else if (type == CONF_INT8) {
            val = *(int8_t *)vp;
        } else if (type == CONF_INT16) {
            val = *(int16_t *)vp;
        } else {
            val = *(int32_t *)vp;
        }
        snprintf(buf, buf_len, "%ld", (long)val);
        return buf;
    case CONF_INT64:
        snprintf(buf, buf_len, "%lld", *(long long *)vp);
        return buf;
    default:
        return NULL;
    }
}

char *
conf_str_from_bytes(void *vp, int vp_len, char *buf, int buf_len)
{
    if (BASE64_ENCODE_SIZE(vp_len) > buf_len) {
        return NULL;
    }
    base64_encode(vp, vp_len, buf, 1);
    return buf;
}

int
conf_set_value(char *name, char *val_str)
{
    int name_argc;
    char *name_argv[CONF_MAX_DIR_DEPTH];
    struct conf_handler *ch;
    int rc;

    conf_lock();
    ch = conf_parse_and_lookup(name, &name_argc, name_argv);
    if (!ch) {
        rc = OS_INVALID_PARM;
        goto out;
    }
    rc = ch->ch_set(name_argc - 1, &name_argv[1], val_str);
out:
    conf_unlock();
    return rc;
}

/*
 * Get value in printable string form. If value is not string, the value
 * will be filled in *buf.
 * Return value will be pointer to beginning of that buffer,
 * except for string it will pointer to beginning of string.
 */
char *
conf_get_value(char *name, char *buf, int buf_len)
{
    int name_argc;
    char *name_argv[CONF_MAX_DIR_DEPTH];
    struct conf_handler *ch;
    char *rval = NULL;

    conf_lock();
    ch = conf_parse_and_lookup(name, &name_argc, name_argv);
    if (!ch) {
        goto out;
    }

    if (!ch->ch_get) {
        goto out;
    }
    rval = ch->ch_get(name_argc - 1, &name_argv[1], buf, buf_len);
out:
    conf_unlock();
    return rval;
}

int
conf_commit(char *name)
{
    int name_argc;
    char *name_argv[CONF_MAX_DIR_DEPTH];
    struct conf_handler *ch;
    int rc;
    int rc2;

    conf_lock();
    if (name) {
        ch = conf_parse_and_lookup(name, &name_argc, name_argv);
        if (!ch) {
            rc = OS_INVALID_PARM;
            goto out;
        }
        if (ch->ch_commit) {
            rc = ch->ch_commit();
        } else {
            rc = 0;
        }
    } else {
        rc = 0;
        SLIST_FOREACH(ch, &conf_handlers, ch_list) {
            if (ch->ch_commit) {
                rc2 = ch->ch_commit();
                if (!rc) {
                    rc = rc2;
                }
            }
        }
    }
out:
    conf_unlock();
    return rc;
}
