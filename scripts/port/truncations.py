#!/usr/bin/env python3
"""List every cast of a pointer to an integer narrower than a pointer in the port's build.

    truncations.py [--build port/build/pc] [--libclang PATH] [--out docs/port/truncations.md]
                   [--json FILE] [--jobs N]

The game casts pointers to int (MWCC's 32-bit ABI). The port links PIE and maps its arenas wherever
the system puts them, above 4 GiB, as arm64 macOS must, so every cast whose value comes back as a
pointer and can run has to be widened in port/src. This tool finds the casts with
libclang, using the compile commands of the port's build (CMAKE_EXPORT_COMPILE_COMMANDS is on), and
sorts them. A ps2/src unit is read as it is, not as the copy the port compiles without the
definitions port/src overrides, so the bodies port/src replaces are listed too:

- round-trip: the value comes back as a pointer: cast back, stored or passed into an int that is
  cast back somewhere (followed across units by USR), returned from a function whose result is
  cast back, or an absolute address added to a pointer as an offset.
- low-bits: only the low bits matter: masks, modulo, shifts, a difference of two truncated
  pointers, a truthiness test, a pointer field that holds a file offset added to a base.
- escapes: stored where the tool cannot follow it (memory, an aggregate, an unknown callee) and
  never seen cast back; read by hand.

Each site carries the pointer's origin where it can be derived (image, stack, arena, parameter,
field, ...) and the state of its function in the linked darkcloud: port (port/src's own code),
replaced (a ps2/src body a PC_OVERRIDE definition in port/src replaces; the port never compiles
it), retail (a ps2/src body that is linked) or dead (dropped by --gc-sections; never runs).
An optimised build inlines functions and drops the bodies, which would then count as dead, so the
build has to be a Debug one.

Needs the clang Python bindings: `pip install libclang` (bundles the library) or the `clang`
package with a system libclang, found through --libclang, LIBCLANG or the usual LLVM paths.
"""

import argparse
import collections
import glob
import json
import multiprocessing
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE_DIRS = ("ps2/src/", "port/src/", "ps2/include/", "port/include/")

ci = None

# Calls and globals whose pointers come from the port's arenas (dataset.cpp, dataalloc2_1.cpp).
ARENA_CALLS = {"Alloc", "Alloc64", "ArenaBlock", "GetPackFile", "GetPackFileExt", "LoadPackFile"}
ARENA_GLOBALS = {"read_buffer", "packfile_buff", "GlobalDataBuffer", "VisualData", "MotionData",
                 "TextureData", "WaterData", "ActiveData0", "ActiveData1", "workbuffer", "WorkBuffer"}


def load_libclang(explicit):
    global ci
    import clang.cindex as cindex

    ci = cindex
    candidates = [explicit, os.environ.get("LIBCLANG")]
    try:
        libdir = subprocess.run(["llvm-config", "--libdir"], capture_output=True, text=True).stdout.strip()
        candidates += glob.glob(os.path.join(libdir, "libclang*.so*")) if libdir else []
    except OSError:
        pass
    candidates += sorted(glob.glob("/usr/lib/llvm-*/lib/libclang-*.so*"), reverse=True)
    candidates += ["/opt/homebrew/opt/llvm/lib/libclang.dylib", "/usr/local/opt/llvm/lib/libclang.dylib"]
    for candidate in candidates:
        if candidate and os.path.exists(candidate):
            try:
                cindex.Config.set_library_file(candidate)
                cindex.Index.create()
                return
            except Exception:
                cindex.Config.loaded = False
    cindex.Index.create()  # the bundled library of `pip install libclang`


def rel(path):
    path = os.path.realpath(path)
    return os.path.relpath(path, ROOT) if path.startswith(ROOT + os.sep) else path


def ours(path):
    return path is not None and rel(path).startswith(SOURCE_DIRS)


def is_cast(cursor):
    K = ci.CursorKind
    return cursor.kind in (K.CSTYLE_CAST_EXPR, K.CXX_STATIC_CAST_EXPR, K.CXX_REINTERPRET_CAST_EXPR,
                           K.CXX_FUNCTIONAL_CAST_EXPR)


def pointerish(type_):
    T = ci.TypeKind
    kind = type_.get_canonical().kind
    return kind in (T.POINTER, T.CONSTANTARRAY, T.INCOMPLETEARRAY, T.VARIABLEARRAY, T.DEPENDENTSIZEDARRAY,
                    T.FUNCTIONPROTO, T.FUNCTIONNOPROTO, T.NULLPTR)


def integral(type_):
    T = ci.TypeKind
    canonical = type_.get_canonical()
    if canonical.kind == T.ENUM:
        return True
    return canonical.kind in (T.CHAR_U, T.UCHAR, T.CHAR16, T.CHAR32, T.USHORT, T.UINT, T.ULONG, T.ULONGLONG,
                              T.CHAR_S, T.SCHAR, T.WCHAR, T.SHORT, T.INT, T.LONG, T.LONGLONG, T.BOOL)


def narrow(type_):
    return integral(type_) and 0 < type_.get_canonical().get_size() < 8


def operand(cast):
    children = [c for c in cast.get_children() if c.kind != ci.CursorKind.TYPE_REF and
                not c.kind.is_reference() and c.kind != ci.CursorKind.NAMESPACE_REF]
    exprs = [c for c in children if c.kind.is_expression()]
    return exprs[-1] if exprs else None


def peel(cursor):
    K = ci.CursorKind
    while cursor is not None and cursor.kind in (K.PAREN_EXPR, K.UNEXPOSED_EXPR):
        children = list(cursor.get_children())
        if len(children) != 1:
            break
        cursor = children[0]
    return cursor


def text(cursor, limit=120):
    tokens = [t.spelling for t in cursor.get_tokens()]
    joined = " ".join(tokens)
    joined = re.sub(r" ?([()\[\],.;]) ?", r"\1", joined).replace("-> ", "->").replace(" ->", "->")
    return joined if len(joined) <= limit else joined[: limit - 3] + "..."


def unary_op(cursor):
    tokens = [t.spelling for t in cursor.get_tokens()]
    return tokens[0] if tokens else ""


def binop(cursor):
    try:
        return cursor.binary_operator.name
    except Exception:
        return "Unknown"


def is_global_var(decl):
    K = ci.CursorKind
    if decl.kind != K.VAR_DECL:
        return False
    if decl.storage_class in (ci.StorageClass.STATIC, ci.StorageClass.EXTERN):
        return True
    parent = decl.semantic_parent
    return parent is not None and parent.kind in (K.TRANSLATION_UNIT, K.NAMESPACE, K.STRUCT_DECL,
                                                   K.CLASS_DECL, K.LINKAGE_SPEC)


def origin(cursor, depth=0):
    """Where the pointer the cursor evaluates to points."""
    K = ci.CursorKind
    cursor = peel(cursor)
    if cursor is None or depth > 4:
        return "unknown"
    kind = cursor.kind
    if kind == K.STRING_LITERAL:
        return "image (literal)"
    if kind == K.CXX_THIS_EXPR:
        return "this"
    if is_cast(cursor):
        inner = operand(cursor)
        return origin(inner, depth + 1) if inner is not None and pointerish(inner.type) else "integer"
    if kind == K.UNARY_OPERATOR and unary_op(cursor) == "&":
        inner = [c for c in cursor.get_children()]
        return address_of(inner[0], depth) if inner else "unknown"
    if kind == K.BINARY_OPERATOR and binop(cursor) in ("Add", "Sub"):
        for child in cursor.get_children():
            if pointerish(child.type):
                return origin(child, depth + 1)
    if kind == K.ARRAY_SUBSCRIPT_EXPR:
        # An element that is itself a pointer was loaded from memory; an array row decays in place.
        if cursor.type.get_canonical().kind == ci.TypeKind.POINTER:
            return "field"
        return origin(next(cursor.get_children()), depth + 1)
    if kind == K.UNARY_OPERATOR and unary_op(cursor) == "*":
        return "field"
    if kind == K.CONDITIONAL_OPERATOR:
        parts = list(cursor.get_children())[1:]
        kinds = {origin(p, depth + 1) for p in parts}
        return kinds.pop() if len(kinds) == 1 else "mixed"
    if kind == K.CALL_EXPR:
        name = cursor.spelling or "?"
        return "arena (%s)" % name if name in ARENA_CALLS else "call (%s)" % name
    if kind == K.DECL_REF_EXPR:
        decl = cursor.referenced
        if decl is None:
            return "unknown"
        if decl.kind == K.FUNCTION_DECL or decl.kind == K.CXX_METHOD:
            return "image (function)"
        if decl.kind == K.PARM_DECL:
            return "parameter"
        if decl.kind == K.VAR_DECL:
            array = decl.type.get_canonical().kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY)
            if decl.spelling in ARENA_GLOBALS:
                return "arena (%s)" % decl.spelling
            if is_global_var(decl):
                return "image" if array else "global pointer"
            if array:
                return "stack"
            init = [c for c in decl.get_children() if c.kind.is_expression()]
            if init:
                inner = origin(init[-1], depth + 1)
                return inner if inner != "unknown" else "local pointer"
            return "local pointer"
        return "unknown"
    if kind == K.MEMBER_REF_EXPR:
        decl = cursor.referenced
        if decl is not None and decl.spelling in ("base", "block") or cursor.spelling in ARENA_GLOBALS:
            return "arena (%s)" % cursor.spelling
        if decl is not None and decl.type.get_canonical().kind in (ci.TypeKind.CONSTANTARRAY,):
            children = list(cursor.get_children())
            return "member array of " + (origin_of_object(children[0], depth) if children else "this")
        return "field"
    return "unknown"


def origin_of_object(cursor, depth):
    """Where an object expression (the left of a member access) lives."""
    cursor = peel(cursor)
    K = ci.CursorKind
    if cursor is None:
        return "unknown"
    if cursor.kind == K.DECL_REF_EXPR and cursor.referenced is not None:
        decl = cursor.referenced
        if decl.kind == K.VAR_DECL:
            if pointerish(decl.type):
                return origin(cursor, depth + 1)
            return "image" if is_global_var(decl) else "stack"
        if decl.kind == K.PARM_DECL:
            return "parameter"
    if cursor.kind == K.CXX_THIS_EXPR:
        return "this"
    return origin(cursor, depth + 1)


def address_of(cursor, depth):
    K = ci.CursorKind
    cursor = peel(cursor)
    if cursor is None:
        return "unknown"
    if cursor.kind == K.DECL_REF_EXPR and cursor.referenced is not None:
        decl = cursor.referenced
        if decl.kind in (K.FUNCTION_DECL, K.CXX_METHOD):
            return "image (function)"
        if decl.kind == K.VAR_DECL:
            if decl.spelling in ARENA_GLOBALS:
                return "image (%s)" % decl.spelling
            return "image" if is_global_var(decl) else "stack"
        if decl.kind == K.PARM_DECL:
            return "stack (parameter)"
    if cursor.kind == K.ARRAY_SUBSCRIPT_EXPR:
        return origin(next(cursor.get_children()), depth + 1)
    if cursor.kind == K.MEMBER_REF_EXPR:
        children = list(cursor.get_children())
        if not children:
            return "this"
        base = peel(children[0])
        if base is not None and pointerish(base.type):
            return origin(base, depth + 1)
        return origin_of_object(base, depth)
    if cursor.kind == K.UNARY_OPERATOR and unary_op(cursor) == "*":
        return origin(next(cursor.get_children()), depth + 1)
    return "unknown"


def refs(cursor):
    """USRs of the integer-typed variables, fields and calls an expression reads."""
    K = ci.CursorKind
    found = set()
    stack = [cursor]
    while stack:
        node = stack.pop()
        if node.kind in (K.DECL_REF_EXPR, K.MEMBER_REF_EXPR):
            decl = node.referenced
            if decl is not None and decl.kind in (K.VAR_DECL, K.PARM_DECL, K.FIELD_DECL) and integral(decl.type):
                usr = decl.get_usr()
                if usr:
                    found.add(usr)
        elif node.kind == K.CALL_EXPR:
            decl = node.referenced
            if decl is not None and integral(node.type):
                usr = decl.get_usr()
                if usr:
                    found.add("ret:" + usr)
        stack.extend(node.get_children())
    return found


def target_of(cursor):
    """The USR an assignment's left side names, or None for memory the tool cannot follow."""
    K = ci.CursorKind
    cursor = peel(cursor)
    if cursor is None:
        return None
    if cursor.kind in (K.DECL_REF_EXPR, K.MEMBER_REF_EXPR):
        decl = cursor.referenced
        if decl is not None and decl.kind in (K.VAR_DECL, K.PARM_DECL, K.FIELD_DECL):
            return decl.get_usr() or None
    if cursor.kind == K.ARRAY_SUBSCRIPT_EXPR:
        base = peel(next(cursor.get_children()))
        if base is not None and base.kind in (K.DECL_REF_EXPR, K.MEMBER_REF_EXPR):
            decl = base.referenced
            if decl is not None and decl.type.get_canonical().kind == ci.TypeKind.CONSTANTARRAY:
                return decl.get_usr() or None
    return None


def callee_param(call, argument):
    decl = call.referenced
    if decl is None:
        return None, call.spelling
    args = list(call.get_arguments())
    index = next((i for i, a in enumerate(args) if a == argument), None)
    params = list(decl.get_arguments()) if decl.kind in (ci.CursorKind.FUNCTION_DECL, ci.CursorKind.CXX_METHOD,
                                                         ci.CursorKind.CONSTRUCTOR) else []
    if index is None or index >= len(params):
        return None, decl.spelling
    return params[index].get_usr() or None, decl.spelling


def contains_truncation(cursor):
    stack = [cursor]
    while stack:
        node = stack.pop()
        if is_cast(node) and narrow(node.type):
            inner = operand(node)
            if inner is not None and pointerish(inner.type):
                return True
        stack.extend(node.get_children())
    return False


def narrowed_later(ancestors):
    """A pointer cast to a full-width integer that an implicit conversion then narrows."""
    K = ci.CursorKind
    for parent in reversed(ancestors):
        if parent.kind in (K.PAREN_EXPR, K.UNEXPOSED_EXPR) or is_cast(parent) or (
                parent.kind == K.BINARY_OPERATOR and binop(parent) in ("Add", "Sub", "And", "Or", "Xor", "Shr",
                                                                        "Shl", "Rem", "Mul", "Div")):
            if integral(parent.type) and narrow(parent.type):
                return True
            if not integral(parent.type):
                return False
            continue
        if parent.kind == K.VAR_DECL or (parent.kind == K.BINARY_OPERATOR and binop(parent) == "Assign"):
            return narrow(parent.type)
        return False
    return False


def context(cast, ancestors, function_usr, ops):
    """Where the truncated value goes: (kind, detail, usr or None); ops collects the operators on the way."""
    K = ci.CursorKind
    node = cast
    for parent in reversed(ancestors):
        kind = parent.kind
        if kind in (K.PAREN_EXPR, K.UNEXPOSED_EXPR):
            node = parent
            continue
        if is_cast(parent):
            if pointerish(parent.type):
                return "cast back", text(parent, 60), None
            if integral(parent.type):
                node = parent
                continue
            return "converted", parent.type.spelling, None
        if kind == K.BINARY_OPERATOR:
            op = binop(parent)
            children = list(parent.get_children())
            other = children[1] if children and children[0] == node else (children[0] if children else None)
            if op in ("Add", "Sub") and other is not None and pointerish(other.type):
                return "pointer offset", text(parent, 60), None
            if op == "Sub" and other is not None and contains_truncation(other):
                return "difference", "", None
            if op in ("And", "Rem", "Shr", "Shl"):
                ops.append(op)
                node = parent
                continue
            if op in ("Add", "Sub", "Or", "Xor", "Mul", "Div"):
                ops.append(op)
                node = parent
                continue
            if op in ("LT", "GT", "LE", "GE", "EQ", "NE"):
                if other is not None and contains_truncation(other):
                    return "compare truncated", op, None
                return "compare", op, None
            if op in ("LAnd", "LOr"):
                return "test", "", None
            if op == "Assign":
                if children and children[0] == node:
                    return "other", "assigned to", None
                usr = target_of(children[0])
                return ("store", text(children[0], 60), usr) if usr else ("store to memory", text(children[0], 60), None)
            if op == "Comma":
                return "discarded", "", None
            return "other", op, None
        if kind == K.COMPOUND_ASSIGNMENT_OPERATOR:
            children = list(parent.get_children())
            if children and pointerish(children[0].type):
                return "pointer offset", text(parent, 60), None
            usr = target_of(children[0]) if children else None
            return ("store", text(children[0], 60), usr) if usr else ("store to memory", text(parent, 60), None)
        if kind == K.ARRAY_SUBSCRIPT_EXPR:
            return "index", text(parent, 60), None
        if kind == K.VAR_DECL:
            return "store", parent.spelling, parent.get_usr() or None
        if kind == K.CALL_EXPR:
            usr, name = callee_param(parent, node)
            return ("argument", name, usr) if usr else ("argument to unknown", name or "?", None)
        if kind == K.RETURN_STMT:
            return "return", "", "ret:" + function_usr if function_usr else None
        if kind in (K.IF_STMT, K.WHILE_STMT, K.DO_STMT, K.FOR_STMT, K.SWITCH_STMT):
            return "test", "", None
        if kind == K.CONDITIONAL_OPERATOR:
            children = list(parent.get_children())
            if children and children[0] == node:
                return "test", "", None
            node = parent
            continue
        if kind == K.UNARY_OPERATOR:
            op = unary_op(parent)
            if op == "!":
                return "test", "", None
            node = parent
            continue
        if kind in (K.INIT_LIST_EXPR, K.CXX_CONSTRUCT_EXPR):
            return "store to memory", "initialiser", None
        if kind == K.COMPOUND_STMT:
            return "discarded", "", None
        return "other", str(kind).split(".")[-1], None
    return "other", "", None


FUNCTION_KINDS = None


def walk(path, args):
    global FUNCTION_KINDS
    K = ci.CursorKind
    FUNCTION_KINDS = (K.FUNCTION_DECL, K.CXX_METHOD, K.CONSTRUCTOR, K.DESTRUCTOR, K.CONVERSION_FUNCTION,
                      K.FUNCTION_TEMPLATE)
    index = ci.Index.create()
    try:
        tu = index.parse(path, args=args, options=ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES * 0)
    except ci.TranslationUnitLoadError as error:
        return {"file": path, "error": str(error)}
    errors = [str(d) for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
    casts = []
    back = []
    sinks = set()
    edges = []
    definitions = []
    in_port = rel(path).startswith("port/src/")

    def visit(cursor, ancestors, function):
        kind = cursor.kind
        if kind in FUNCTION_KINDS and cursor.is_definition():
            function = cursor
            if in_port and cursor.location.file is not None and rel(cursor.location.file.name).startswith("port/src/"):
                mangled = cursor.mangled_name
                if mangled:
                    definitions.append(mangled)
        if is_cast(cursor):
            inner = operand(cursor)
            if inner is not None:
                if narrow(cursor.type) and pointerish(inner.type):
                    record(cursor, inner, ancestors, function)
                elif integral(cursor.type) and pointerish(inner.type) and narrowed_later(ancestors):
                    record(cursor, inner, ancestors, function)
                elif pointerish(cursor.type) and integral(inner.type):
                    read = refs(inner)
                    sinks.update(read)
                    if read and cursor.location.file is not None:
                        back.append({
                            "file": rel(cursor.location.file.name),
                            "line": cursor.location.line,
                            "column": cursor.location.column,
                            "expression": text(cursor),
                            "refs": sorted(read),
                            "function": qualified(function) if function is not None else "(file scope)",
                            "mangled": function.mangled_name if function is not None else "",
                            "static": function is not None and function.linkage == ci.LinkageKind.INTERNAL,
                        })
        elif kind == K.COMPOUND_ASSIGNMENT_OPERATOR or (kind == K.BINARY_OPERATOR and binop(cursor) == "Assign"):
            children = list(cursor.get_children())
            if len(children) == 2 and integral(children[0].type):
                target = target_of(children[0])
                if target:
                    edges.extend((source, target) for source in refs(children[1]))
            if len(children) == 2 and pointerish(children[0].type) and kind == K.COMPOUND_ASSIGNMENT_OPERATOR:
                sinks.update(refs(children[1]))
        elif kind == K.VAR_DECL and integral(cursor.type):
            init = [c for c in cursor.get_children() if c.kind.is_expression()]
            if init and cursor.get_usr():
                edges.extend((source, cursor.get_usr()) for source in refs(init[-1]))
        elif kind == K.CALL_EXPR and cursor.referenced is not None:
            params = list(cursor.referenced.get_arguments()) if cursor.referenced.kind in FUNCTION_KINDS else []
            for argument, param in zip(cursor.get_arguments(), params):
                if integral(param.type) and param.get_usr():
                    edges.extend((source, param.get_usr()) for source in refs(argument))
        elif kind == K.RETURN_STMT and function is not None and integral(function.result_type):
            for child in cursor.get_children():
                edges.extend((source, "ret:" + function.get_usr()) for source in refs(child))
        elif kind == K.BINARY_OPERATOR and binop(cursor) in ("Add", "Sub"):
            children = list(cursor.get_children())
            if len(children) == 2 and any(pointerish(c.type) for c in children):
                for child in children:
                    if integral(child.type) and not contains_truncation(child):
                        sinks.update("offset:" + usr for usr in refs(child))
        ancestors.append(cursor)
        for child in cursor.get_children():
            location = child.location
            if location.file is not None and not ours(location.file.name):
                continue
            visit(child, ancestors, function)
        ancestors.pop()

    def record(cast, inner, ancestors, function):
        location = cast.location
        if location.file is None:
            return
        function_usr = function.get_usr() if function is not None else None
        ops = []
        kind, detail, usr = context(cast, ancestors, function_usr, ops)
        casts.append({
            "file": rel(location.file.name),
            "line": location.line,
            "column": location.column,
            "expression": text(cast),
            "type": cast.type.spelling,
            "origin": origin(inner),
            "context": kind,
            "detail": detail,
            "usr": usr,
            "ops": ops,
            "function": qualified(function) if function is not None else "(file scope)",
            "mangled": function.mangled_name if function is not None else "",
            "static": function is not None and function.linkage == ci.LinkageKind.INTERNAL,
        })

    for child in tu.cursor.get_children():
        location = child.location
        if location.file is not None and ours(location.file.name):
            visit(child, [], None)
    return {"file": rel(path), "casts": casts, "back": back, "sinks": sorted(sinks), "edges": edges,
            "definitions": definitions, "errors": errors[:5]}


def qualified(cursor):
    parts = []
    while cursor is not None and cursor.kind != ci.CursorKind.TRANSLATION_UNIT:
        if cursor.spelling:
            parts.append(cursor.spelling)
        cursor = cursor.semantic_parent
    return "::".join(reversed(parts))


def unit_entries(build):
    """The compile commands of the game's units and port/src's, each ps2/src unit under its own path."""
    with open(os.path.join(build, "compile_commands.json")) as f:
        entries = json.load(f)
    copies = os.path.join(os.path.realpath(build), "ps2_src") + os.sep
    for entry in entries:
        path = os.path.realpath(os.path.join(entry["directory"], entry["file"]))
        if path.startswith(copies):
            args = shlex.split(entry.pop("command")) if "command" in entry else entry["arguments"]
            entry["arguments"] = [a for a in args if os.path.realpath(os.path.join(entry["directory"], a)) != path]
            entry["file"] = os.path.join(ROOT, "ps2", "src", path[len(copies):])
    return [e for e in entries if rel(e["file"]).startswith(("ps2/src/", "port/src/")) and
            "/tests/" not in e["file"]]


def debug_build(build):
    try:
        with open(os.path.join(build, "CMakeCache.txt")) as f:
            return re.search(r"^CMAKE_BUILD_TYPE:\w+=Debug$", f.read(), re.MULTILINE) is not None
    except OSError:
        return False


def command_args(entry):
    args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
    out = []
    skip = False
    for arg in args[1:]:
        if skip:
            skip = False
            continue
        if arg in ("-o", "-c"):
            skip = arg == "-o"
            continue
        if os.path.realpath(os.path.join(entry["directory"], arg)) == os.path.realpath(entry["file"]):
            continue
        out.append(arg)
    # libclang needs to find its own resource headers through the same compiler.
    resource = subprocess.run([args[0], "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
    if resource:
        out += ["-resource-dir", resource]
    return out


def work(job):
    entry, libclang = job
    load_libclang(libclang)
    os.chdir(entry["directory"])
    return walk(entry["file"], command_args(entry))


def linked_symbols(build):
    """Mangled name -> the source files defining it in the linked darkcloud (empty without debug info)."""
    executable = os.path.join(build, "darkcloud")
    try:
        output = subprocess.run(["nm", "-l", "--defined-only", executable], capture_output=True, text=True).stdout
    except OSError:
        return {}
    linked = collections.defaultdict(set)
    for line in output.splitlines():
        symbol, _, where = line.partition("\t")
        fields = symbol.split()
        if len(fields) >= 3:
            linked[fields[2]].add(rel(where.rsplit(":", 1)[0]) if where else "")
    return linked


def classify(site, sinks, reach):
    context = site["context"]
    origin = site["origin"]
    # A mask or a remainder keeps only the low bits; shifts can be undone on the way back.
    masked = any(op in ("And", "Rem") for op in site["ops"])
    if context in ("cast back", "compare"):
        return "round-trip"
    if context == "pointer offset":
        absolute = not (origin in ("field", "integer", "unknown") or origin.startswith("member array"))
        return "round-trip" if absolute else "low-bits"
    if context in ("store", "argument", "return"):
        if site["usr"] and site["usr"] in reach and not masked:
            return "round-trip"
        return "low-bits" if masked else "escapes"
    if context in ("store to memory", "argument to unknown", "other", "converted"):
        return "low-bits" if masked else "escapes"
    return "low-bits"


def reaching(sinks, edges):
    """Every USR whose value flows, through integer assignments, into a cast back to a pointer."""
    backward = collections.defaultdict(set)
    for source, target in edges:
        backward[target].add(source)
    reach = set(s for s in sinks if not s.startswith("offset:"))
    reach.update(s[len("offset:"):] for s in sinks if s.startswith("offset:"))
    stack = list(reach)
    while stack:
        usr = stack.pop()
        for source in backward.get(usr, ()):
            if source not in reach:
                reach.add(source)
                stack.append(source)
    return reach


def carriers(sites, edges):
    """The integers that hold a truncated pointer on its way back, named after the first that took it."""
    forward = collections.defaultdict(set)
    for source, target in edges:
        forward[source].add(target)
    carried = {}
    for site in sites:
        if site["class"] != "round-trip" or not site["usr"]:
            continue
        name = site["detail"] or site["function"]
        stack = [site["usr"]]
        while stack:
            usr = stack.pop()
            if usr in carried:
                continue
            carried[usr] = name
            stack.extend(forward.get(usr, ()))
    return carried


def resolve_at_readers(sites, edges, returns, totals):
    """A stored round trip whose value only becomes a pointer again in code that never runs (a
    replacement resolves it, or the reader is dead) is harmless: class "resolved"."""
    forward = collections.defaultdict(set)
    for source, target in edges:
        forward[source].add(target)
    for site in sites:
        if site["class"] != "round-trip" or not site["usr"]:
            continue
        closure = set()
        stack = [site["usr"]]
        while stack:
            usr = stack.pop()
            if usr not in closure:
                closure.add(usr)
                stack.extend(forward.get(usr, ()))
        readers = [r for r in returns if closure.intersection(r["refs"])]
        site["readers"] = sorted({r["function"] for r in readers if r["state"] == "retail"})
        if readers and all(r["state"] in ("replaced", "dead") for r in readers):
            totals[(site["class"], site["state"])] -= 1
            site["class"] = "resolved"
            site["detail"] += " (read back only in " + ", ".join(sorted({r["function"] for r in readers})) + ")"
            totals[(site["class"], site["state"])] += 1


def status(site, port_definitions, linked):
    if site["file"].startswith(("port/src/", "port/include/")):
        return "port"
    mangled = site["mangled"]
    if not mangled:
        return "file scope"
    files = linked.get(mangled)
    if files is None:
        return "dead"
    if site["static"]:
        # Statics share mangled names across units; the debug info's file tells them apart.
        return "retail" if site["file"] in files or files == {""} else "dead"
    return "replaced" if mangled in port_definitions else "retail"


def markdown(sites, totals, returns, out):
    todo = []
    groups = collections.OrderedDict()
    for site in sites:
        if site["class"] == "round-trip" and site["state"] == "retail":
            groups.setdefault(site["function"], []).append(site)
    for function, group in groups.items():
        readers = sorted({r for g in group for r in g.get("readers", ()) if r != function})
        todo.append((function, group, readers))
    lines = []
    w = lines.append
    w("# Pointer truncations")
    w("")
    w("Generated by `scripts/port/truncations.py` from the port's compile commands; do not edit by hand.")
    w("Every cast of a pointer to an integer narrower than a pointer in `ps2/src`, `port/src` and their")
    w("headers. `darkcloud` is PIE and its arenas are ordinary mappings, all above 4 GiB as on arm64")
    w("macOS (whose low 4 GiB are its hard page zero), so a **round-trip** site that runs faults at")
    w("once; each one is widened in `port/src`. `--check` (CI) fails while one is listed or while this")
    w("file is stale.")
    w("")
    w("- **round-trip**: the value comes back as a pointer (cast back, stored or passed into an int that is")
    w("  cast back somewhere, returned from a function whose result is cast back, or an absolute address")
    w("  added to a pointer as an offset).")
    w("- **low-bits**: only the low bits are used (masks, modulo, shifts, differences of two truncated")
    w("  pointers, tests, comparisons, a pointer field holding a file offset added to a base).")
    w("- **escapes**: stored into memory, an aggregate or an unknown callee, and never seen cast back.")
    w("- **resolved**: a round trip whose value only becomes a pointer again in a replaced or dead function")
    w("  (the replacement recovers the pointer, e.g. `PortImagePointer` for image globals).")
    w("")
    w("State of the enclosing function in the linked `darkcloud`: **port** (port/src's own code),")
    w("**replaced** (a ps2/src body a `PC_OVERRIDE` definition in port/src replaces; never compiled),")
    w("**retail** (a ps2/src body that is linked and can run), **dead** (dropped by `--gc-sections`; never")
    w("runs).")
    w("")
    w("Origin is where the pointer points, where the expression shows it: **image** (a global, a literal,")
    w("a function), **stack**, **arena** (an arena allocation or global), **parameter**, **field**,")
    w("**global pointer**, **local pointer**, **call**, **this**.")
    w("")
    w("## Counts")
    w("")
    states = ["retail", "port", "replaced", "dead", "file scope"]
    classes = ["round-trip", "resolved", "escapes", "low-bits"]
    w("| class | " + " | ".join(states) + " | total |")
    w("|---|" + "---|" * (len(states) + 1))
    for cls in classes:
        row = [totals[(cls, s)] for s in states]
        w("| %s | %s | %d |" % (cls, " | ".join(str(n) for n in row), sum(row)))
    w("")
    live = [s for s in sites if s["state"] in ("retail", "port")]
    origins = collections.Counter(s["origin"].split(" (")[0] for s in live if s["class"] == "round-trip")
    w("Round-trip sites that can run (retail and port), by origin: " +
      (", ".join("%s %d" % (k, v) for k, v in origins.most_common()) or "none") + ".")
    w("")
    w("## Retail functions with round trips that can run (%d)" % len(todo))
    w("")
    w("Retail functions holding round-trip sites that can run, with the retail functions that turn the")
    w("value back into a pointer. Replacing either side in `port/src` with the pointer kept whole (or")
    w("recovered, as `CCharacter::ClothStep` does with `PortImagePointer`) fixes the round trip.")
    w("")
    w("| function | sites | origin | value comes back in |")
    w("|---|---|---|---|")
    for function, group, readers in todo:
        w("| %s | %s | %s | %s |" % (esc(function), ", ".join("%s:%d" % (g["file"], g["line"]) for g in group),
                                    esc(", ".join(sorted({g["origin"] for g in group}))),
                                    esc(", ".join(readers)) if readers else "same function"))
    w("")
    never = ("replaced", "dead", "file scope")
    for title, classes_shown, states_shown in (
            ("Round-trip sites that can run", ("round-trip",), ("retail", "port")),
            ("Escaping sites that can run", ("escapes",), ("retail", "port")),
            ("Low-bits sites that can run", ("low-bits",), ("retail", "port")),
            ("Round trips resolved where the value comes back", ("resolved",), ("retail", "port")),
            ("Sites that never run", ("round-trip", "resolved", "escapes", "low-bits"), never)):
        rows = [s for s in sites if s["class"] in classes_shown and s["state"] in states_shown]
        w("## %s (%d)" % (title, len(rows)))
        w("")
        w("| site | function | state | class | expression | origin | use |")
        w("|---|---|---|---|---|---|---|")
        for s in rows:
            use = s["context"] + (": " + s["detail"] if s["detail"] else "")
            w("| %s:%d | %s | %s | %s | `%s` | %s | %s |" % (
                s["file"], s["line"], esc(s["function"]), s["state"], s["class"],
                esc(s["expression"]).replace("`", "'"), esc(s["origin"]), esc(use)))
        w("")
    w("## Where round-trip values become pointers again (%d)" % len(returns))
    w("")
    w("Casts from an integer to a pointer that read an integer a round-trip site above stored into, followed")
    w("through integer assignments, arguments and returns. A round trip is fixed at either end: keep the")
    w("pointer where it is stored, or resolve the integer where it comes back.")
    w("")
    w("| site | function | state | expression | carries |")
    w("|---|---|---|---|---|")
    for s in returns:
        w("| %s:%d | %s | %s | `%s` | %s |" % (s["file"], s["line"], esc(s["function"]), s["state"],
                                             esc(s["expression"]).replace("`", "'"), esc(s["carrier"])))
    w("")
    with open(out, "w") as f:
        f.write("\n".join(lines))


def check(sites, totals, returns, out):
    import tempfile
    with tempfile.TemporaryDirectory() as scratch:
        fresh = os.path.join(scratch, "truncations.md")
        markdown(sites, totals, returns, fresh)
        with open(fresh) as f:
            wanted = f.read()
    try:
        with open(out) as f:
            committed = f.read()
    except OSError:
        committed = None
    status = 0
    if committed != wanted:
        print("truncations.py: %s is stale; run scripts/port/truncations.py and commit it" % rel(out),
              file=sys.stderr)
        status = 1
    for site in sites:
        if site["class"] == "round-trip" and site["state"] in ("retail", "port"):
            print("truncations.py: %s:%d: %s: round trip that can run: %s" % (
                site["file"], site["line"], site["function"], site["expression"]), file=sys.stderr)
            status = 1
    return status


def esc(value):
    return value.replace("|", "\\|")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--build", default=os.path.join(ROOT, "port/build/pc"))
    parser.add_argument("--libclang")
    parser.add_argument("--out", default=os.path.join(ROOT, "docs/port/truncations.md"))
    parser.add_argument("--json")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--only", help="regular expression over source paths, for a quick run")
    parser.add_argument("--check", action="store_true",
                        help="write nothing; fail if --out differs from what would be written or if a "
                             "round trip can run")
    options = parser.parse_args()

    try:
        load_libclang(options.libclang)
    except ImportError:
        sys.exit("truncations.py: the clang Python bindings are missing (pip install libclang)")
    if not debug_build(options.build):
        sys.exit("truncations.py: %s is not a Debug build" % options.build)
    entries = unit_entries(options.build)
    if options.only:
        entries = [e for e in entries if re.search(options.only, e["file"])]
    with multiprocessing.Pool(options.jobs) as pool:
        results = pool.map(work, [(e, options.libclang) for e in entries], chunksize=1)

    sinks = set()
    edges = []
    port_definitions = set()
    seen = {}
    backs = {}
    for result in results:
        if "error" in result:
            print("truncations.py: %s: %s" % (result["file"], result["error"]), file=sys.stderr)
            continue
        for error in result["errors"]:
            print("truncations.py: %s: %s" % (result["file"], error), file=sys.stderr)
        sinks.update(result["sinks"])
        edges.extend(result["edges"])
        port_definitions.update(result["definitions"])
        for site in result["casts"]:
            seen.setdefault((site["file"], site["line"], site["column"]), site)
        for site in result["back"]:
            backs.setdefault((site["file"], site["line"], site["column"]), site)
    reach = reaching(sinks, edges)
    linked = linked_symbols(options.build)
    if not linked:
        print("truncations.py: no darkcloud in %s; every retail site counts as retail" % options.build,
              file=sys.stderr)
    sites = sorted(seen.values(), key=lambda s: (s["file"], s["line"], s["column"]))
    totals = collections.Counter()
    for site in sites:
        site["class"] = classify(site, sinks, reach)
        site["state"] = status(site, port_definitions, linked) if linked else (
            "port" if site["file"].startswith(("port/src/", "port/include/")) else "retail")
        totals[(site["class"], site["state"])] += 1
    carried = carriers(sites, edges)
    returns = []
    for site in sorted(backs.values(), key=lambda s: (s["file"], s["line"], s["column"])):
        names = [usr for usr in site["refs"] if usr in carried]
        if names:
            site["carrier"] = ", ".join(sorted({carried[usr] for usr in names}))
            site["state"] = status(site, port_definitions, linked) if linked else "retail"
            returns.append(site)
    resolve_at_readers(sites, edges, returns, totals)
    if options.check:
        return check(sites, totals, returns, options.out)
    os.makedirs(os.path.dirname(os.path.abspath(options.out)), exist_ok=True)
    markdown(sites, totals, returns, options.out)
    if options.json:
        with open(options.json, "w") as f:
            json.dump(sites, f, indent=1)
    for cls in ("round-trip", "resolved", "escapes", "low-bits"):
        print("%-10s retail %4d  port %4d  replaced %4d  dead %4d" % (
            cls, totals[(cls, "retail")], totals[(cls, "port")], totals[(cls, "replaced")], totals[(cls, "dead")]))


if __name__ == "__main__":
    sys.exit(main())
