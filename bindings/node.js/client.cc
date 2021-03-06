// Copyright (c) 2013, Cornell University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of HyperDex nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C
#include <cstring>
#include <cstdlib>

// STL
#include <map>

// e
#include <e/intrusive_ptr.h>

// HyperDex
#include "hyperdex/client.h"
#include "hyperdex/datastructures.h"

// Node
#include <node.h>
#include <node_buffer.h>

#pragma GCC visibility push(hidden)

namespace hyperdex
{
namespace nodejs
{

class Operation;

class HyperDexClient : public node::ObjectWrap
{
    public:
        static v8::Persistent<v8::Function> ctor;
        static v8::Persistent<v8::Function> global_err;
        static void Init(v8::Handle<v8::Object> exports);
        static v8::Handle<v8::Value> New(const v8::Arguments& args);

    public:
        HyperDexClient(const char* host, uint16_t port);
        ~HyperDexClient() throw ();

    public:
        static v8::Handle<v8::Value> loop(const v8::Arguments& args);
        static v8::Handle<v8::Value> loop_error(const v8::Arguments& args);

#include "client.declarations.cc"

    public:
        hyperdex_client* client() { return m_cl; }
        void add(int64_t reqid, e::intrusive_ptr<Operation> op);
        void loop(int timeout);
        void poll_start();
        void poll_stop();

    public:
        static void poll_callback(uv_poll_t* watcher, int status, int revents);
        static void poll_cleanup(uv_handle_t* handle);

    private:
        hyperdex_client* m_cl;
        std::map<int64_t, e::intrusive_ptr<Operation> > m_ops;
        v8::Persistent<v8::Object> m_callback;
        uv_poll_t* m_poll;
        bool m_poll_active;
};

class Operation
{
    public:
        Operation(v8::Handle<v8::Object>& c1, HyperDexClient* c2);
        ~Operation() throw ();

    public:
        bool set_callback(v8::Handle<v8::Function>& func, unsigned func_sz);
        bool convert_cstring(v8::Handle<v8::Value>& _cstring,
                             const char** cstring);
        bool convert_type(v8::Handle<v8::Value>& _value,
                          const char** value, size_t* value_sz,
                          hyperdatatype* datatype);

        bool convert_spacename(v8::Handle<v8::Value>& _spacename,
                               const char** spacename);
        bool convert_key(v8::Handle<v8::Value>& _key,
                         const char** key, size_t* key_sz);
        bool convert_attributes(v8::Handle<v8::Value>& _attributes,
                                const hyperdex_client_attribute** attrs,
                                size_t* attrs_sz);
        bool convert_predicates(v8::Handle<v8::Value>& _predicates,
                                const hyperdex_client_attribute_check** checks,
                                size_t* checks_sz);
        bool convert_mapattributes(v8::Handle<v8::Value>& _predicates,
                                   const hyperdex_client_map_attribute** checks,
                                   size_t* checks_sz);
        bool convert_sortby(v8::Handle<v8::Value>& _sortby,
                            const char** sortby);
        bool convert_limit(v8::Handle<v8::Value>& _limit,
                           uint64_t* limit);
        bool convert_maxmin(v8::Handle<v8::Value>& _maxmin,
                            int* maximize);

        v8::Local<v8::Value> build_string(const char* value, size_t value_sz);
        bool build_attribute(const hyperdex_client_attribute* attr,
                             v8::Local<v8::Value>& retval,
                             v8::Local<v8::Value>& error);
        void build_attributes(v8::Local<v8::Value>& retval,
                              v8::Local<v8::Value>& error);

        void encode_asynccall_status();
        void encode_asynccall_status_attributes();
        void encode_asynccall_status_count();
        void encode_asynccall_status_description();
        void encode_iterator_status_attributes();

    public:
        void make_callback2(v8::Handle<v8::Value>& first,
                            v8::Handle<v8::Value>& second);
        void make_callback3(v8::Handle<v8::Value>& first,
                            v8::Handle<v8::Value>& second,
                            v8::Handle<v8::Value>& third);
        static v8::Local<v8::Value> error_from_status(hyperdex_client* client,
                                                      hyperdex_client_returncode status);
        v8::Local<v8::Value> error_from_status();
        v8::Local<v8::Value> error_message(const char* msg);
        v8::Local<v8::Value> error_out_of_memory();
        void callback_error(v8::Handle<v8::Value>& err);
        void callback_error_from_status();
        void callback_error_message(const char* msg);
        void callback_error_out_of_memory();

    public:
        HyperDexClient* client;
        int64_t reqid;
        enum hyperdex_client_returncode status;
        const struct hyperdex_client_attribute* attrs;
        size_t attrs_sz;
        const char* description;
        uint64_t count;
        bool finished;
        void (Operation::*encode_return)();

    private:
        void inc() { ++m_ref; }
        void dec() { if (--m_ref == 0) delete this; }
        friend class e::intrusive_ptr<Operation>;

    private:
        size_t m_ref;
        hyperdex_ds_arena* m_arena;
        v8::Persistent<v8::Object> m_client;
        v8::Persistent<v8::Object> m_callback;
        unsigned m_callback_sz;
};

v8::Persistent<v8::Function> HyperDexClient::ctor;
v8::Persistent<v8::Function> HyperDexClient::global_err;

void
HyperDexClient :: Init(v8::Handle<v8::Object> target)
{
    v8::Local<v8::FunctionTemplate> tpl
        = v8::FunctionTemplate::New(HyperDexClient::New);
    tpl->SetClassName(v8::String::NewSymbol("Client"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "loop", HyperDexClient::loop);
#include "client.prototypes.cc"

    ctor = v8::Persistent<v8::Function>::New(tpl->GetFunction());
    target->Set(v8::String::NewSymbol("Client"), ctor);

    v8::Local<v8::FunctionTemplate> tpl_err
        = v8::FunctionTemplate::New(HyperDexClient::loop_error);
    global_err = v8::Persistent<v8::Function>::New(tpl_err->GetFunction());
}

v8::Handle<v8::Value>
HyperDexClient :: New(const v8::Arguments& args)
{
    v8::HandleScope scope;

    if (!args.IsConstructCall())
    {
        v8::Local<v8::Value> argv[2] = {argv[0], argv[1]};
        return scope.Close(ctor->NewInstance(2, argv));
    }

    if (args.Length() != 2)
    {
        return v8::ThrowException(v8::String::New("constructor requires (host, port) arguments"));
    }

    if (args[0].IsEmpty() || !args[0]->IsString())
    {
        return v8::ThrowException(v8::String::New("constructor requires that the \"host\" argument be a string"));
    }

    if (args[1].IsEmpty() || !args[1]->IsNumber())
    {
        return v8::ThrowException(v8::String::New("constructor requires that the \"port\" argument be a number"));
    }

    v8::Local<v8::String> _host = args[0].As<v8::String>();
    v8::Local<v8::Integer> _port = args[1].As<v8::Integer>();
    v8::String::AsciiValue s(_host);

    const char* host = *s;
    uint16_t port = _port->Value();

    HyperDexClient* c = new HyperDexClient(host, port);
    c->Wrap(args.This());
    return scope.Close(args.This());
}

HyperDexClient :: HyperDexClient(const char* host, uint16_t port)
    : m_cl(hyperdex_client_create(host, port))
    , m_poll(NULL)
    , m_poll_active(false)
{
    if (m_cl)
    {
        m_poll = new uv_poll_t;
        uv_poll_init_socket(uv_default_loop(), m_poll, hyperdex_client_poll(m_cl));
        m_poll->data = this;
    }

    v8::Local<v8::Function> func(v8::Local<v8::Function>::New(global_err));
    v8::Local<v8::Object> cbobj = v8::Object::New();
    cbobj->Set(v8::String::NewSymbol("callback"), func);
    m_callback = v8::Persistent<v8::Object>::New(cbobj);
}

HyperDexClient :: ~HyperDexClient() throw ()
{
    if (m_cl)
    {
        hyperdex_client_destroy(m_cl);
    }

    if (m_poll)
    {
        m_poll->data = NULL;
        poll_stop();
        uv_close(reinterpret_cast<uv_handle_t*>(m_poll), poll_cleanup);
    }

    if (!m_callback.IsEmpty())
    {
        m_callback.Dispose();
        m_callback.Clear();
    }
}

void
HyperDexClient :: add(int64_t reqid, e::intrusive_ptr<Operation> op)
{
    m_ops[reqid] = op;
    poll_start();
}

void
HyperDexClient :: loop(int timeout)
{
    enum hyperdex_client_returncode rc;
    int64_t ret = hyperdex_client_loop(m_cl, timeout, &rc);

    if (ret < 0 && timeout == 0 && rc == HYPERDEX_CLIENT_TIMEOUT)
    {
        return;
    }

    if (ret < 0)
    {
        v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(m_callback);
        v8::Local<v8::Function> callback
            = obj->Get(v8::String::NewSymbol("callback")).As<v8::Function>();
        v8::Local<v8::Value> err = Operation::error_from_status(m_cl, rc);
        v8::Handle<v8::Value> argv[] = { err };
        node::MakeCallback(v8::Context::GetCurrent()->Global(), callback, 1, argv);
        return;
    }

    std::map<int64_t, e::intrusive_ptr<Operation> >::iterator it;
    it = m_ops.find(ret);
    assert(it != m_ops.end());
    (*it->second.*it->second->encode_return)();

    if (it->second->finished)
    {
        m_ops.erase(it);
    }

    if (m_ops.empty())
    {
        poll_stop();
    }
}

void
HyperDexClient :: poll_start()
{
    if (m_poll_active || !m_poll)
    {
        return;
    }

    m_poll_active = true;
    uv_poll_start(m_poll, UV_READABLE, poll_callback);
}

void
HyperDexClient :: poll_stop()
{
    if (!m_poll_active || !m_poll)
    {
        return;
    }

    uv_poll_stop(m_poll);
    m_poll_active = false;
}

void
HyperDexClient :: poll_callback(uv_poll_t* watcher, int status, int revents)
{
    HyperDexClient* cl = reinterpret_cast<HyperDexClient*>(watcher->data);

    if (cl)
    {
        cl->loop(0);
    }
}

void
HyperDexClient :: poll_cleanup(uv_handle_t* handle)
{
    uv_poll_t* poll = reinterpret_cast<uv_poll_t*>(handle);

    if (handle)
    {
        delete poll;
    }
}

v8::Handle<v8::Value>
HyperDexClient :: loop(const v8::Arguments& args)
{
    v8::HandleScope scope;
    int timeout = -1;

    if (args.Length() > 0 && args[0]->IsNumber())
    {
        timeout = args[0]->IntegerValue();
    }

    HyperDexClient* client = node::ObjectWrap::Unwrap<HyperDexClient>(args.This());
    client->loop(timeout);
    return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value>
HyperDexClient :: loop_error(const v8::Arguments& args)
{
    v8::HandleScope scope;

    if (args.Length() > 0)
    {
        v8::ThrowException(args[0]);
    }

    return scope.Close(v8::Undefined());
}

#include "client.definitions.cc"

Operation :: Operation(v8::Handle<v8::Object>& c1, HyperDexClient* c2)
    : client(c2)
    , reqid(-1)
    , status(HYPERDEX_CLIENT_GARBAGE)
    , attrs(NULL)
    , attrs_sz(0)
    , description(NULL)
    , count(0)
    , finished(false)
    , encode_return()
    , m_ref(0)
    , m_arena(hyperdex_ds_arena_create())
    , m_client()
    , m_callback()
    , m_callback_sz(0)
{
    m_client = v8::Persistent<v8::Object>::New(c1);
    v8::Local<v8::Object> cbobj = v8::Object::New();
    m_callback = v8::Persistent<v8::Object>::New(cbobj);
}

Operation :: ~Operation() throw ()
{
    if (m_arena)
    {
        hyperdex_ds_arena_destroy(m_arena);
    }

    if (attrs)
    {
        hyperdex_client_destroy_attrs(attrs, attrs_sz);
    }

    if (description)
    {
        free((void*)description);
    }

    if (!m_client.IsEmpty())
    {
        m_client.Dispose();
        m_client.Clear();
    }

    if (!m_callback.IsEmpty())
    {
        m_callback.Dispose();
        m_callback.Clear();
    }
}

bool
Operation :: set_callback(v8::Handle<v8::Function>& func, unsigned func_sz)
{
    v8::Local<v8::Object>::New(m_callback)->Set(v8::String::NewSymbol("callback"), func);
    m_callback_sz = func_sz;
    return true;
}

bool
Operation :: convert_cstring(v8::Handle<v8::Value>& _cstring,
                             const char** cstring)
{
    v8::String::Utf8Value s(_cstring);

    if (s.length() == 0 && *s == NULL)
    {
        this->callback_error_message("cannot convert object to string");
        return false;
    }

    hyperdex_ds_returncode error;
    size_t sz;

    if (hyperdex_ds_copy_string(m_arena, *s, s.length() + 1, &error, cstring, &sz) < 0)
    {
        this->callback_error_out_of_memory();
        return false;
    }

    return true;
}

bool
Operation :: convert_type(v8::Handle<v8::Value>& _value,
                          const char** value, size_t* value_sz,
                          hyperdatatype* datatype)
{
    // XXX support other types
    hyperdex_ds_returncode error;

    if (node::Buffer::HasInstance(_value->ToObject()))
    {
        size_t sz = node::Buffer::Length(_value->ToObject());
        char* data = node::Buffer::Data(_value->ToObject());

        if (hyperdex_ds_copy_string(m_arena, data, sz, &error, value, value_sz) < 0)
        {
            this->callback_error_out_of_memory();
            return false;
        }
    }
    else
    {
        v8::String::Utf8Value s(_value);

        if (s.length() == 0 && *s == NULL)
        {
            this->callback_error_message("cannot convert object to string");
            return false;
        }

        if (hyperdex_ds_copy_string(m_arena, *s, s.length(), &error, value, value_sz) < 0)
        {
            this->callback_error_out_of_memory();
            return false;
        }
    }

    *datatype = HYPERDATATYPE_STRING;
    return true;
}

bool
Operation :: convert_spacename(v8::Handle<v8::Value>& _spacename,
                               const char** spacename)
{
    return convert_cstring(_spacename, spacename);
}

bool
Operation :: convert_key(v8::Handle<v8::Value>& _key,
                         const char** key, size_t* key_sz)
{
    hyperdatatype datatype;
    return convert_type(_key, key, key_sz, &datatype);
}

bool
Operation :: convert_attributes(v8::Handle<v8::Value>& x,
                                const hyperdex_client_attribute** _attrs,
                                size_t* _attrs_sz)
{
    if (!x->IsObject())
    {
        this->callback_error_message("attributes must be an object");
        return false;
    }

    v8::Local<v8::Object> attributes = x->ToObject();
    v8::Local<v8::Array> arr = attributes->GetPropertyNames();
    size_t ret_attrs_sz = arr->Length();
    struct hyperdex_client_attribute* ret_attrs;
    ret_attrs = hyperdex_ds_allocate_attribute(m_arena, ret_attrs_sz);

    if (!ret_attrs)
    {
        this->callback_error_out_of_memory();
        return false;
    }

    *_attrs = ret_attrs;
    *_attrs_sz = ret_attrs_sz;

    for (size_t i = 0; i < arr->Length(); ++i)
    {
        v8::Local<v8::Value> key = arr->Get(i);
        v8::Local<v8::Value> val = attributes->Get(key);

        if (!convert_cstring(key, &ret_attrs[i].attr) ||
            !convert_type(val, &ret_attrs[i].value,
                               &ret_attrs[i].value_sz,
                               &ret_attrs[i].datatype))
        {
            return false;
        }
    }

    return true;
}

bool
Operation :: convert_predicates(v8::Handle<v8::Value>& x,
                                const hyperdex_client_attribute_check** _checks,
                                size_t* _checks_sz)
{
    v8::ThrowException(v8::String::New("predicates not yet supported")); // XXX
    *_checks = NULL;
    *_checks_sz = 0;
    return true;
}

bool
Operation :: convert_mapattributes(v8::Handle<v8::Value>& _predicates,
                                   const hyperdex_client_map_attribute** _mapattrs,
                                   size_t* _mapattrs_sz)
{
    v8::ThrowException(v8::String::New("mapattributes not yet supported")); // XXX
    *_mapattrs = NULL;
    *_mapattrs_sz = 0;
    return false;
}

bool
Operation :: convert_sortby(v8::Handle<v8::Value>& _sortby,
                            const char** sortby)
{
    return convert_cstring(_sortby, sortby);
}

bool
Operation :: convert_limit(v8::Handle<v8::Value>& _limit,
                           uint64_t* limit)
{
    if (!_limit->IsNumber())
    {
        this->callback_error_message("limit must be numeric");
        return false;
    }

    *limit = _limit->IntegerValue();
    return true;
}

bool
Operation :: convert_maxmin(v8::Handle<v8::Value>& _maxmin,
                            int* maximize)
{
    *maximize = _maxmin->IsTrue() ? 1 : 0;
    return true;
}

v8::Local<v8::Value>
Operation :: build_string(const char* value, size_t value_sz)
{
    v8::HandleScope scope;
    node::Buffer* buf = node::Buffer::New(value_sz);
    memmove(node::Buffer::Data(buf), value, value_sz);
    v8::Local<v8::Object> global = v8::Context::GetCurrent()->Global();
    v8::Local<v8::Function> buf_ctor
        = v8::Local<v8::Function>::Cast(global->Get(v8::String::NewSymbol("Buffer")));
    v8::Local<v8::Integer> A(v8::Integer::New(value_sz));
    v8::Local<v8::Integer> B(v8::Integer::New(0));
    v8::Handle<v8::Value> argv[3] = { buf->handle_, A, B };
    v8::Local<v8::Object> jsbuf = buf_ctor->NewInstance(3, argv);
    return scope.Close(jsbuf);
}

bool
Operation :: build_attribute(const hyperdex_client_attribute* attr,
                             v8::Local<v8::Value>& retval,
                             v8::Local<v8::Value>& error)
{
    struct hyperdex_ds_iterator iter;
    const char* tmp_str = NULL;
    size_t tmp_str_sz = 0;
    const char* tmp_str2 = NULL;
    size_t tmp_str2_sz = 0;
    int64_t tmp_i = 0;
    int64_t tmp_i2 = 0;
    double tmp_d = 0;
    double tmp_d2 = 0;
    int result = 0;

    switch (attr->datatype)
    {
        case HYPERDATATYPE_STRING:
            retval = build_string(attr->value, attr->value_sz);
            return true;
        case HYPERDATATYPE_INT64:
        case HYPERDATATYPE_FLOAT:
        case HYPERDATATYPE_LIST_STRING:
        case HYPERDATATYPE_LIST_INT64:
        case HYPERDATATYPE_LIST_FLOAT:
        case HYPERDATATYPE_SET_STRING:
        case HYPERDATATYPE_SET_INT64:
        case HYPERDATATYPE_SET_FLOAT:
        case HYPERDATATYPE_MAP_STRING_STRING:
        case HYPERDATATYPE_MAP_STRING_INT64:
        case HYPERDATATYPE_MAP_STRING_FLOAT:
        case HYPERDATATYPE_MAP_INT64_STRING:
        case HYPERDATATYPE_MAP_INT64_INT64:
        case HYPERDATATYPE_MAP_INT64_FLOAT:
        case HYPERDATATYPE_MAP_FLOAT_STRING:
        case HYPERDATATYPE_MAP_FLOAT_INT64:
        case HYPERDATATYPE_MAP_FLOAT_FLOAT:
            error = this->error_message("node.js only supports type String");
            return false;
        case HYPERDATATYPE_GENERIC:
        case HYPERDATATYPE_LIST_GENERIC:
        case HYPERDATATYPE_SET_GENERIC:
        case HYPERDATATYPE_MAP_GENERIC:
        case HYPERDATATYPE_MAP_STRING_KEYONLY:
        case HYPERDATATYPE_MAP_INT64_KEYONLY:
        case HYPERDATATYPE_MAP_FLOAT_KEYONLY:
        case HYPERDATATYPE_GARBAGE:
        default:
            error = this->error_message("server sent malformed attributes");
            return false;
    }
}

void
Operation :: build_attributes(v8::Local<v8::Value>& retval,
                              v8::Local<v8::Value>& error)
{
    v8::Local<v8::Object> obj(v8::Object::New());

    for (size_t i = 0; i < attrs_sz; ++i)
    {
        v8::Local<v8::Value> val;

        if (!build_attribute(attrs + i, val, error))
        {
            retval = v8::Local<v8::Value>::New(v8::Undefined());
            return;
        }

        obj->Set(v8::String::New(attrs[i].attr), val);
    }

    if (attrs)
    {
        hyperdex_client_destroy_attrs(attrs, attrs_sz);
        attrs = NULL;
        attrs_sz = 0;
    }

    retval = obj;
    error = v8::Local<v8::Value>::New(v8::Undefined());
}

void
Operation :: encode_asynccall_status()
{
    v8::Local<v8::Value> retval;
    v8::Local<v8::Value> error;

    if (status == HYPERDEX_CLIENT_SUCCESS)
    {
        retval = v8::Local<v8::Value>::New(v8::True());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else if (status == HYPERDEX_CLIENT_NOTFOUND)
    {
        retval = v8::Local<v8::Value>::New(v8::Null());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else if (status == HYPERDEX_CLIENT_CMPFAIL)
    {
        retval = v8::Local<v8::Value>::New(v8::False());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else
    {
        retval = v8::Local<v8::Value>::New(v8::Undefined());
        error = v8::Local<v8::Value>::New(this->error_from_status());
    }

    finished = true;
    this->make_callback2(retval, error);
}

void
Operation :: encode_asynccall_status_attributes()
{
    v8::Local<v8::Value> retval;
    v8::Local<v8::Value> error;

    if (status == HYPERDEX_CLIENT_SUCCESS)
    {
        this->build_attributes(retval, error);
    }
    else if (status == HYPERDEX_CLIENT_NOTFOUND)
    {
        retval = v8::Local<v8::Value>::New(v8::Null());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else if (status == HYPERDEX_CLIENT_CMPFAIL)
    {
        retval = v8::Local<v8::Value>::New(v8::False());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else
    {
        retval = v8::Local<v8::Value>::New(v8::Undefined());
        error = v8::Local<v8::Value>::New(this->error_from_status());
    }

    finished = true;
    this->make_callback2(retval, error);
}

void
Operation :: encode_asynccall_status_count()
{
    v8::Local<v8::Value> retval;
    v8::Local<v8::Value> error;

    if (status == HYPERDEX_CLIENT_SUCCESS)
    {
        retval = v8::Local<v8::Value>::New(v8::Integer::New(count));
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else if (status == HYPERDEX_CLIENT_NOTFOUND)
    {
        retval = v8::Local<v8::Value>::New(v8::Null());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else if (status == HYPERDEX_CLIENT_CMPFAIL)
    {
        retval = v8::Local<v8::Value>::New(v8::False());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else
    {
        retval = v8::Local<v8::Value>::New(v8::Undefined());
        error = v8::Local<v8::Value>::New(this->error_from_status());
    }

    finished = true;
    this->make_callback2(retval, error);
}

void
Operation :: encode_asynccall_status_description()
{
    v8::Local<v8::Value> retval;
    v8::Local<v8::Value> error;

    if (status == HYPERDEX_CLIENT_SUCCESS)
    {
        retval = v8::Local<v8::Value>::New(v8::String::New(description));
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else if (status == HYPERDEX_CLIENT_NOTFOUND)
    {
        retval = v8::Local<v8::Value>::New(v8::Null());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else if (status == HYPERDEX_CLIENT_CMPFAIL)
    {
        retval = v8::Local<v8::Value>::New(v8::False());
        error = v8::Local<v8::Value>::New(v8::Undefined());
    }
    else
    {
        retval = v8::Local<v8::Value>::New(v8::Undefined());
        error = v8::Local<v8::Value>::New(this->error_from_status());
    }

    finished = true;
    this->make_callback2(retval, error);
}

void
Operation :: encode_iterator_status_attributes()
{
    v8::Local<v8::Value> retval;
    v8::Local<v8::Value> error;
    v8::Local<v8::Value> done;

    if (status == HYPERDEX_CLIENT_SEARCHDONE)
    {
        finished = true;
        retval = v8::Local<v8::Value>::New(v8::Undefined());
        error = v8::Local<v8::Value>::New(v8::Undefined());
        done = v8::Local<v8::Value>::New(v8::True());
    }
    else if (status == HYPERDEX_CLIENT_SUCCESS)
    {
        this->build_attributes(retval, error);
        done = v8::Local<v8::Value>::New(v8::False());
    }
    else if (status == HYPERDEX_CLIENT_NOTFOUND)
    {
        retval = v8::Local<v8::Value>::New(v8::Null());
        error = v8::Local<v8::Value>::New(v8::Undefined());
        done = v8::Local<v8::Value>::New(v8::False());
    }
    else if (status == HYPERDEX_CLIENT_CMPFAIL)
    {
        retval = v8::Local<v8::Value>::New(v8::False());
        error = v8::Local<v8::Value>::New(v8::Undefined());
        done = v8::Local<v8::Value>::New(v8::False());
    }
    else
    {
        retval = v8::Local<v8::Value>::New(v8::Undefined());
        error = v8::Local<v8::Value>::New(this->error_from_status());
        done = v8::Local<v8::Value>::New(v8::False());
    }

    this->make_callback3(retval, done, error);
}

void
Operation :: make_callback2(v8::Handle<v8::Value>& first,
                            v8::Handle<v8::Value>& second)
{
    v8::Local<v8::Function> callback = v8::Local<v8::Object>::New(m_callback)->Get(v8::String::NewSymbol("callback")).As<v8::Function>();
    v8::Handle<v8::Value> argv[] = { first, second };
    node::MakeCallback(v8::Context::GetCurrent()->Global(), callback, 2, argv);
}

void
Operation :: make_callback3(v8::Handle<v8::Value>& first,
                            v8::Handle<v8::Value>& second,
                            v8::Handle<v8::Value>& third)
{
    v8::Local<v8::Function> callback = v8::Local<v8::Object>::New(m_callback)->Get(v8::String::NewSymbol("callback")).As<v8::Function>();
    v8::Handle<v8::Value> argv[] = { first, second, third };
    node::MakeCallback(v8::Context::GetCurrent()->Global(), callback, 3, argv);
}

v8::Local<v8::Value>
Operation :: error_from_status(hyperdex_client* client,
                               hyperdex_client_returncode status)
{
    v8::Local<v8::Object> obj(v8::Object::New());
    obj->Set(v8::String::New("msg"),
             v8::String::New(hyperdex_client_error_message(client)));
    obj->Set(v8::String::New("sym"),
             v8::String::New(hyperdex_client_returncode_to_string(status)));
    return obj;
}

v8::Local<v8::Value>
Operation :: error_from_status()
{
    return Operation::error_from_status(client->client(), status);
}

v8::Local<v8::Value>
Operation :: error_message(const char* msg)
{
    v8::Local<v8::Object> obj(v8::Object::New());
    obj->Set(v8::String::New("msg"), v8::String::New(msg));
    return obj;
}

v8::Local<v8::Value>
Operation :: error_out_of_memory()
{
    return error_message("out of memory");
}

void
Operation :: callback_error(v8::Handle<v8::Value>& err)
{
    v8::Local<v8::Value> X(v8::Local<v8::Value>::New(v8::Undefined()));
    v8::Local<v8::Value> T(v8::Local<v8::Value>::New(v8::True()));
    assert(m_callback_sz == 2 || m_callback_sz == 3);

    if (m_callback_sz == 2)
    {
        return make_callback2(X, err);
    }

    if (m_callback_sz == 3)
    {
        return make_callback3(X, T, err);
    }
}

void
Operation :: callback_error_from_status()
{
    v8::Local<v8::Value> err
        = v8::Local<v8::Value>::New(this->error_from_status());
    callback_error(err);
}

void
Operation :: callback_error_message(const char* msg)
{
    v8::Local<v8::Value> err
        = v8::Local<v8::Value>::New(this->error_message(msg));
    callback_error(err);
}

void
Operation :: callback_error_out_of_memory()
{
    v8::Local<v8::Value> err
        = v8::Local<v8::Value>::New(this->error_out_of_memory());
    callback_error(err);
}

void
Init(v8::Handle<v8::Object> target)
{
    HyperDexClient::Init(target);
}

} // namespace nodejs
} // namespace hyperdex

#pragma GCC visibility pop

NODE_MODULE(hyperdex_client, hyperdex::nodejs::Init)
