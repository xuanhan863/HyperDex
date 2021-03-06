# Copyright (c) 2013-2014, Cornell University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of HyperDex nor the names of its contributors may be
#       used to endorse or promote products derived from this software without
#       specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys

BASE = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BASE)

import bindings
import bindings.c
import bindings.nodejs

DOCS_IN = {(bindings.AsyncCall, bindings.SpaceName): 'The name of the space as a string or buffer.'
          ,(bindings.AsyncCall, bindings.Key): 'The key for the operation as a Node type'
          ,(bindings.AsyncCall, bindings.Attributes): 'An object specifying attributes '
           'to modify and their respective values.'
          ,(bindings.AsyncCall, bindings.MapAttributes): 'An object specifying map '
           'attributes to modify and their respective key/values.'
          ,(bindings.AsyncCall, bindings.Predicates): 'An object of predicates '
           'to check against.'
          ,(bindings.Iterator, bindings.SpaceName): 'The name of the space as string or buffer.'
          ,(bindings.Iterator, bindings.SortBy): 'The attribute to sort by.'
          ,(bindings.Iterator, bindings.Limit): 'The number of results to return.'
          ,(bindings.Iterator, bindings.MaxMin): 'Maximize or minimize '
          '(e.g., "max" or "min").'
          ,(bindings.Iterator, bindings.Predicates): 'An object of predicates '
           'to check against.'
          }

def generate_worker_declarations(xs):
    calls = set([])
    for x in xs:
        call = bindings.call_name(x)
        if call in calls:
            continue
        assert x.form in (bindings.AsyncCall, bindings.Iterator)
        fptr = bindings.c.generate_func_ptr(x)
        yield 'static v8::Handle<v8::Value> {0}({1}, const v8::Arguments& args);'.format(call, fptr)
        calls.add(call)

def generate_declaration(x):
    assert x.form in (bindings.AsyncCall, bindings.Iterator)
    func = 'static v8::Handle<v8::Value> {0}(const v8::Arguments& args);'
    return func.format(x.name)

def generate_worker_definitions(xs):
    calls = set([])
    for x in xs:
        call = bindings.call_name(x)
        if call in calls:
            continue
        assert x.form in (bindings.AsyncCall, bindings.Iterator)
        fptr = bindings.c.generate_func_ptr(x)
        func  = 'v8::Handle<v8::Value>\nHyperDexClient :: '
        func += '{0}({1}, const v8::Arguments& args)\n'.format(call, fptr)
        func += '{\n'
        func += '    v8::HandleScope scope;\n'
        func += '    v8::Local<v8::Object> client_obj = args.This();\n'
        func += '    HyperDexClient* client = node::ObjectWrap::Unwrap<HyperDexClient>(client_obj);\n'
        func += '    e::intrusive_ptr<Operation> op(new Operation(client_obj, client));\n'
        for idx, arg in enumerate(x.args_in):
            for p, n in arg.args:
                func += '    ' + p + ' in_' + n + ';\n'
            args = ', '.join(['&in_' + n for p, n in arg.args])
            func += '    v8::Local<v8::Value> {0} = args[{1}];\n'.format(arg.__name__.lower(), idx)
            func += '    if (!op->convert_{0}({0}, {1})) return scope.Close(v8::Undefined());\n'.format(arg.__name__.lower(), args)
        func += '    v8::Local<v8::Function> func = args[{0}].As<v8::Function>();\n'.format(len(x.args_in))
        func += '\n    if (func.IsEmpty() || !func->IsFunction())\n'.format(len(x.args_in))
        func += '    {\n'
        func += '        v8::ThrowException(v8::String::New("Callback must be a function"));\n'
        func += '        return scope.Close(v8::Undefined());\n'
        func += '    }\n\n'
        if x.form == bindings.AsyncCall:
            func += '    if (!op->set_callback(func, 2)) { return scope.Close(v8::Undefined()); }\n'
        if x.form == bindings.Iterator:
            func += '    if (!op->set_callback(func, 3)) { return scope.Close(v8::Undefined()); }\n'
        func += '    op->reqid = f(client->client(), {0}, {1});\n\n'.format(', '.join(['in_' + n for p, n in sum([list(a.args) for a in x.args_in], [])]),
                                                                            ', '.join(['&op->' + n for p, n in sum([list(a.args) for a in x.args_out], [])]))
        func += '    if (op->reqid < 0)\n    {\n'
        func += '        op->callback_error_from_status();\n'
        func += '        return scope.Close(v8::Undefined());\n'
        func += '    }\n\n'
        args = '_'.join([arg.__name__.lower() for arg in x.args_out])
        form = x.form.__name__.lower()
        func += '    op->encode_return = &Operation::encode_{0}_{1};\n'.format(form, args)
        func += '    client->add(op->reqid, op);\n'
        func += '    return scope.Close(v8::Undefined());\n'
        func += '}\n'
        yield func
        calls.add(call)

def generate_definition(x):
    assert x.form in (bindings.AsyncCall, bindings.Iterator)
    func  = 'v8::Handle<v8::Value>\n'
    func += 'HyperDexClient :: ' + x.name
    func += '(const v8::Arguments& args)\n{\n'
    func += '    return {0}(hyperdex_client_{1}, args);\n'.format(bindings.call_name(x), x.name)
    func += '}\n'
    return func

def generate_prototype(x):
    assert x.form in (bindings.AsyncCall, bindings.Iterator)
    return 'NODE_SET_PROTOTYPE_METHOD(tpl, "{0}", HyperDexClient::{0});'.format(x.name)

def generate_api_func(x, cls):
    assert x.form in (bindings.AsyncCall, bindings.Iterator)
    func = 'Client :: %s(' % x.name
    padd = ' ' * 16
    func += ', '.join([str(arg).lower()[17:-2] for arg in x.args_in])
    func += ')\n'
    return func

def generate_api_block(x, cls='Client'):
    block  = ''
    block += '\\subsubsection{{\code{{{0}}}}}\n'.format(bindings.LaTeX(x.name))
    block += '\\label{{api:nodejs:{0}}}\n'.format(x.name)
    block += '\\index{{{0}!Node.js API}}\n'.format(bindings.LaTeX(x.name))
    block += '\\begin{javascriptcode}\n'
    block += generate_api_func(x, cls=cls)
    block += '\\end{javascriptcode}\n'
    block += '\\funcdesc \input{{\\topdir/api/desc/{0}}}\n\n'.format(x.name)
    block += '\\noindent\\textbf{Parameters:}\n'
    block += bindings.doc_parameter_list(x.form, x.args_in, DOCS_IN,
                                         label_maker=bindings.parameters_script_style)
    block += '\n\\noindent\\textbf{Returns:}\n'
    if x.args_out == (bindings.Status,):
        if bindings.Predicates in x.args_in:
            block += 'True if predicate, False if not predicate.  Raises exception on error.'
        else:
            block += 'True.  Raises exception on error.'
    elif x.args_out == (bindings.Status, bindings.Attributes):
        block += 'Object if found, nil if not found.  Raises exception on error.'
    elif x.args_out == (bindings.Status, bindings.Count):
        block += 'Number of objects found.  Raises exception on error.'
    elif x.args_out == (bindings.Status, bindings.Description):
        block += 'Description of search.  Raises exception on error.'
    else:
        assert False
    block += '\n'
    return block

def generate_client_declarations():
    fout = open(os.path.join(BASE, 'bindings/node.js/client.declarations.cc'), 'w')
    fout.write(bindings.copyright('/', '2014'))
    fout.write('\n// This file is generated by bindings/nodejs.py\n\n')
    fout.write('\n'.join(generate_worker_declarations(bindings.Client)))
    fout.write('\n\n')
    fout.write('\n'.join([generate_declaration(c) for c in bindings.Client]))

def generate_client_definitions():
    fout = open(os.path.join(BASE, 'bindings/node.js/client.definitions.cc'), 'w')
    fout.write(bindings.copyright('/', '2014'))
    fout.write('\n// This file is generated by bindings/nodejs.py\n\n')
    fout.write('\n'.join(generate_worker_definitions(bindings.Client)))
    fout.write('\n\n')
    fout.write('\n'.join([generate_definition(c) for c in bindings.Client]))

def generate_client_prototypes():
    fout = open(os.path.join(BASE, 'bindings/node.js/client.prototypes.cc'), 'w')
    fout.write(bindings.copyright('/', '2014'))
    fout.write('\n// This file is generated by bindings/nodejs.py\n\n')
    fout.write('\n'.join([generate_prototype(c) for c in bindings.Client]))

def generate_client_doc():
    fout = open(os.path.join(BASE, 'doc/api/node.js.client.tex'), 'w')
    fout.write(bindings.copyright('%', '2014'))
    fout.write('\n% This LaTeX file is generated by bindings/nodejs.py\n\n')
    fout.write('\n'.join([generate_api_block(c) for c in bindings.Client]))

if __name__ == '__main__':
    generate_client_declarations()
    generate_client_definitions()
    generate_client_prototypes()
    generate_client_doc()
