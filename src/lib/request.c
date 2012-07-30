#include <gc.h>
#include <string.h>
#include "lib/request.h"
#include "lib/dict.h"

typedef struct {
    SLVAL method;
    SLVAL uri;
    SLVAL path_info;
    SLVAL query_string;
    SLVAL remote_addr;
    SLVAL headers;
    SLVAL env;
    SLVAL get;
    SLVAL post;
    SLVAL post_data;
    SLVAL params;
}
sl_request_internal_opts_t;

static int Request_;
static int Request_opts;

static sl_request_internal_opts_t*
request(sl_vm_t* vm)
{
    return (sl_request_internal_opts_t*)sl_get_ptr(sl_vm_store_get(vm, &Request_opts));
}

static void
parse_query_string(sl_vm_t* vm, SLVAL dict, size_t len, uint8_t* query_string)
{
    uint8_t *key = query_string, *value = NULL;
    size_t key_len = 0, value_len = 0;
    size_t i;
    SLVAL addee = dict, k, v;
    int bracket_mode = 0, in_bracket = 0;
    for(i = 0; i <= len; i++) {
        if(query_string[i] == '&' || i == len) {
            if(key_len > 0) {
                k = sl_string_url_decode(vm, sl_make_string(vm, key, key_len));
                if(value) {
                    v = sl_string_url_decode(vm, sl_make_string(vm, value, value_len));
                }
                sl_dict_set(vm, addee, k, value ? v : vm->lib.nil);
            }
            key = query_string + i + 1;
            key_len = 0;
            value = NULL;
            value_len = 0;
            addee = dict;
            in_bracket = 0;
            bracket_mode = 0;
            continue;
        }
        if(query_string[i] == '=' && !value) {
            value = query_string + i + 1;
            continue;
        }
        if(value) {
            value_len++;
        } else {
            if(query_string[i] == '[') {
                k = sl_make_string(vm, key, key_len);
                key = query_string + i + 1;
                key_len = 0;
                if(!sl_is_a(vm, sl_dict_get(vm, addee, k), vm->lib.Dict)) {
                    sl_dict_set(vm, addee, k, sl_make_dict(vm, 0, NULL));
                }
                addee = sl_dict_get(vm, addee, k);
                in_bracket = 1;
                bracket_mode = 1;
                continue;
            }
            if(query_string[i] == ']' && in_bracket) {
                in_bracket = 0;
                continue;
            }
            if(bracket_mode && !in_bracket) {
                /* skip until \0, & or = */
                while(i + 1 < len && query_string[i + 1] != '&' && query_string[i + 1] != '=') {
                    i++;
                }
                continue;
            }
            key_len++;
        }
    }
}

void
sl_request_set_opts(sl_vm_t* vm, sl_request_opts_t* opts)
{
    size_t i;
    SLVAL n, v;
    sl_request_internal_opts_t* req = GC_MALLOC(sizeof(sl_request_internal_opts_t));
    req->method       = sl_make_cstring(vm, opts->method);
    req->uri          = sl_make_cstring(vm, opts->uri);
    req->path_info    = sl_make_cstring(vm, opts->path_info);
    req->query_string = sl_make_cstring(vm, opts->query_string ? opts->query_string : "");
    req->remote_addr  = sl_make_cstring(vm, opts->remote_addr);
    req->headers      = sl_make_dict(vm, 0, NULL);
    req->env          = sl_make_dict(vm, 0, NULL);
    req->get          = sl_make_dict(vm, 0, NULL);
    req->post         = sl_make_dict(vm, 0, NULL);
    req->post_data    = sl_make_string(vm, (uint8_t*)opts->post_data, opts->post_length);
    for(i = 0; i < opts->header_count; i++) {
        n = sl_make_cstring(vm, opts->headers[i].name);
        v = sl_make_cstring(vm, opts->headers[i].value);
        sl_dict_set(vm, req->headers, n, v);
    }
    for(i = 0; i < opts->env_count; i++) {
        n = sl_make_cstring(vm, opts->env[i].name);
        v = sl_make_cstring(vm, opts->env[i].value);
        sl_dict_set(vm, req->env, n, v);
    }
    if(opts->query_string) {
        parse_query_string(vm, req->get, strlen(opts->query_string), (uint8_t*)opts->query_string);
    }
    if(opts->content_type && strcmp(opts->content_type, "application/x-www-form-urlencoded") == 0) {
        parse_query_string(vm, req->post, opts->post_length, (uint8_t*)opts->post_data);
    }
    req->params = sl_dict_merge(vm, req->get, req->post);
    sl_vm_store_put(vm, &Request_opts, sl_make_ptr((sl_object_t*)req));
}

static SLVAL
request_get(sl_vm_t* vm)
{
    return request(vm)->get;
}

static SLVAL
request_post(sl_vm_t* vm)
{
    return request(vm)->post;
}

static SLVAL
request_post_data(sl_vm_t* vm)
{
    return request(vm)->post_data;
}

static SLVAL
request_headers(sl_vm_t* vm)
{
    return request(vm)->headers;
}

static SLVAL
request_env(sl_vm_t* vm)
{
    return request(vm)->env;
}

static SLVAL
request_method(sl_vm_t* vm)
{
    return request(vm)->method;
}

static SLVAL
request_uri(sl_vm_t* vm)
{
    return request(vm)->uri;
}

static SLVAL
request_path_info(sl_vm_t* vm)
{
    return request(vm)->path_info;
}

static SLVAL
request_query_string(sl_vm_t* vm)
{
    return request(vm)->query_string;
}

static SLVAL
request_remote_addr(sl_vm_t* vm)
{
    return request(vm)->remote_addr;
}

static SLVAL
request_index(sl_vm_t* vm, SLVAL self, SLVAL name)
{
    (void)self;
    return sl_dict_get(vm, request(vm)->params, name);
}

void
sl_init_request(sl_vm_t* vm)
{
    SLVAL Request = sl_new(vm, vm->lib.Object, 0, NULL);
    sl_vm_store_put(vm, &Request_, Request);
    sl_define_singleton_method(vm, Request, "get", 0, request_get);
    sl_define_singleton_method(vm, Request, "post", 0, request_post);
    sl_define_singleton_method(vm, Request, "post_data", 0, request_post_data);
    sl_define_singleton_method(vm, Request, "headers", 0, request_headers);
    sl_define_singleton_method(vm, Request, "env", 0, request_env);
    sl_define_singleton_method(vm, Request, "method", 0, request_method);
    sl_define_singleton_method(vm, Request, "uri", 0, request_uri);
    sl_define_singleton_method(vm, Request, "path_info", 0, request_path_info);
    sl_define_singleton_method(vm, Request, "query_string", 0, request_query_string);
    sl_define_singleton_method(vm, Request, "remote_addr", 0, request_remote_addr);
    sl_define_singleton_method(vm, Request, "[]", 1, request_index);
    
    sl_class_set_const(vm, vm->lib.Object, "Request", Request);
}