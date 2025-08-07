#include "quickjs.h"
#include "quickjs-libc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double x;
    double y;
    double z;
} EField;

// Class ID for QuickJS
static JSClassID js_efield_data_class_id;

// Finalizer: called when JS object is GC'd
static void js_efield_data_finalizer(JSRuntime *rt, JSValue val) {
    EField *p = JS_GetOpaque(val, js_efield_data_class_id);
    if (p) free(p);
}

// Getter
static JSValue js_efield_data_get(JSContext *ctx, JSValueConst this_val, int magic) {
    EField *p = JS_GetOpaque2(ctx, this_val, js_efield_data_class_id);
    if (!p) return JS_UNDEFINED;
    switch (magic) {
        case 0: return JS_NewFloat64(ctx, p->x);
        case 1: return JS_NewFloat64(ctx, p->y);
        case 2: return JS_NewFloat64(ctx, p->z);
    }
    return JS_UNDEFINED;
}

// Setter
static JSValue js_efield_data_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic) {
    EField *p = JS_GetOpaque2(ctx, this_val, js_efield_data_class_id);
    if (!p) return JS_EXCEPTION;
    double v;
    if (JS_ToFloat64(ctx, &v, val))
        return JS_EXCEPTION;
    switch (magic) {
        case 0: p->x = v; break;
        case 1: p->y = v; break;
        case 2: p->z = v; break;
    }
    return JS_UNDEFINED;
}

// Constructor
static JSValue js_efield_data_ctor(JSContext *ctx, JSValueConst new_target,
                                   int argc, JSValueConst *argv) {
    EField *p = malloc(sizeof(*p));
    if (!p) return JS_EXCEPTION;

    p->x = (argc > 0) ? JS_ToFloat64(ctx, &p->x, argv[0]) == 0 ? p->x : 0 : 0;
    p->y = (argc > 1) ? JS_ToFloat64(ctx, &p->y, argv[1]) == 0 ? p->y : 0 : 0;
    p->z = (argc > 2) ? JS_ToFloat64(ctx, &p->z, argv[2]) == 0 ? p->z : 0 : 0;

    JSValue obj = JS_NewObjectClass(ctx, js_efield_data_class_id);
    if (JS_IsException(obj)) {
        free(p);
        return obj;
    }

    JS_SetOpaque(obj, p);
    return obj;
}

// Property definitions
static const JSCFunctionListEntry js_efield_data_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("x", js_efield_data_get, js_efield_data_set, 0),
    JS_CGETSET_MAGIC_DEF("y", js_efield_data_get, js_efield_data_set, 1),
    JS_CGETSET_MAGIC_DEF("z", js_efield_data_get, js_efield_data_set, 2),
};

// Register the class
static int js_init_efield_data_class(JSContext *ctx, JSValue global_obj) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(rt, &js_efield_data_class_id);

    JSClassDef def = { "EField", .finalizer = js_efield_data_finalizer };
    JS_NewClass(JS_GetRuntime(ctx), js_efield_data_class_id, &def);

    // Prototype object
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_efield_data_proto_funcs,
                               sizeof(js_efield_data_proto_funcs) / sizeof(JSCFunctionListEntry));

    // Constructor function
    JSValue ctor = JS_NewCFunction2(ctx, js_efield_data_ctor,
                                    "EField", 3,
                                    JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto);
    JS_SetClassProto(ctx, js_efield_data_class_id, proto);

    // Add to global object
    JS_SetPropertyStr(ctx, global_obj, "EField", ctor);
    return 0;
}


// Read JS code from file
char* read_file(const char *filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    return buffer;
}


// Main
int main() {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue global_obj = JS_GetGlobalObject(ctx);
    js_init_efield_data_class(ctx, global_obj);
    js_std_add_helpers(ctx, 0, NULL);

    // Test JavaScript
    const char *js_code =
        "let e = new EField(1.1, 2.2, 3.3);\n"
        "console.log('Efield:', e.x, e.y, e.z);\n"
        "e.x = 42.0;\n"
        "console.log('Updated x:', e.x);";

    JSValue result = JS_Eval(ctx, js_code, strlen(js_code), "<eval>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        const char *err = JS_ToCString(ctx, exception);
        fprintf(stderr, "JS Exception: %s\n", err);
        JS_FreeCString(ctx, err);
        JS_FreeValue(ctx, exception);
    }
    JS_FreeValue(ctx, result);


    // Call js function
    char* js_code_1 = read_file("efield-test.js");
    JS_Eval(ctx, js_code_1, strlen(js_code_1), "efield-test.js", JS_EVAL_TYPE_GLOBAL);
    free(js_code_1);

	JSValue fn = JS_GetPropertyStr(ctx, global_obj, "efield");
	if (!JS_IsFunction(ctx, fn)) {
		fprintf(stderr, "'efield' is not a function\n");
		JS_FreeValue(ctx, fn);
		JS_FreeValue(ctx, global_obj);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	} 

    JSValue arg[1];
    for (int i=0; i<=100; ++i) {
        // Prepare arguments
        double t = (double)(i);
        arg[0] = JS_NewFloat64(ctx, t);

        // Call the function
        JSValue result = JS_Call(ctx, fn, JS_UNDEFINED, 1, arg);

        // Handle result
        if (JS_IsException(result)) {
            JSValue ex = JS_GetException(ctx);
            const char *err = JS_ToCString(ctx, ex);
            fprintf(stderr, "Exception: %s\n", err);
            JS_FreeCString(ctx, err);
            JS_FreeValue(ctx, ex);
        } else {
            EField* efield = JS_GetOpaque(result, js_efield_data_class_id);
            if (efield) {
                printf("Returned Efield: x=%.2lf y=%.2lf z=%.2lf\n", efield->x, efield->y, efield->z);
            } else {
                printf("Returned value is not JSEfieldData\n");
                return 2;
            }
        }

        JS_FreeValue(ctx, result);
    }

    JS_FreeValue(ctx, arg[0]);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global_obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);


    printf("JS_NAN = %x\n", JS_NAN);
    printf("JS_FLOAT64_TAG_ADDEND = %x\n", (0x7ff80000 + 9 + 1));
    return 0;
}
