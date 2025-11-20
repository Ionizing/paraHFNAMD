#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <getopt.h>
#include "../quickjs/quickjs.h"
#include "../quickjs/quickjs-libc.h"

//#define SNPRINTF_BUF_SIZE ((1<<10) + 1)
//static char* buf[SNPRINTF_BUF_SIZE];

typedef struct {
    double x;
    double y;
    double z;
} EField;

static std::vector<EField> efields;
static JSRuntime* RT = nullptr;
static JSContext* CTX = nullptr;
static JSValue GLOBAL_OBJ = JS_NULL;
static JSValue FN = JS_NULL;

// Class ID for QuickJS
static JSClassID js_efield_data_class_id;

// Finalizer: called when JS object is GC'd
static void js_efield_data_finalizer(JSRuntime *rt, JSValue val) {
    EField* p = (EField*)JS_GetOpaque(val, js_efield_data_class_id);
    if (p) free(p);
}

// Getter
static JSValue js_efield_data_get(JSContext *ctx, JSValueConst this_val, int magic) {
    EField* p = (EField*)JS_GetOpaque2(ctx, this_val, js_efield_data_class_id);
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
    EField *p = (EField*)JS_GetOpaque2(ctx, this_val, js_efield_data_class_id);
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
    EField *p = (EField*)malloc(sizeof(*p));
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

    //JSClassDef def = { "EField", .finalizer = js_efield_data_finalizer };
    JSClassDef def = {"EField", js_efield_data_finalizer};
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
static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Optical field file '%s' not valid.\n", filename);
        std::exit(EXIT_FAILURE);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* buffer = (char*)malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    return buffer;
}


static void init_engine(const std::string& fname) {
    if (!RT) { RT = JS_NewRuntime(); }
    if (!CTX) { CTX = JS_NewContext(RT); }
    if (JS_IsNull(GLOBAL_OBJ)) { GLOBAL_OBJ = JS_GetGlobalObject(CTX); }

    js_init_efield_data_class(CTX, GLOBAL_OBJ);
    js_std_add_helpers(CTX, 0, NULL);

    char* jscode = read_file(fname.c_str());
    JS_Eval(CTX, jscode, strlen(jscode), fname.c_str(), JS_EVAL_TYPE_GLOBAL);
    free(jscode);

    if (JS_IsNull(FN)) {
        FN = JS_GetPropertyStr(CTX, GLOBAL_OBJ, "efield");
    }
    
    // check if 'efield' is a function
    if (!JS_IsFunction(CTX, FN)) {
		fprintf(stderr, "'efield' is not a function\n");
		JS_FreeValue(CTX, FN);
		JS_FreeValue(CTX, GLOBAL_OBJ);
		JS_FreeContext(CTX);
		JS_FreeRuntime(RT);
        exit(1);
    }
}

static void destroy_engine() {
    if (!JS_IsNull(FN)) { JS_FreeValue(CTX, FN); }
    if (!JS_IsNull(GLOBAL_OBJ)) { JS_FreeValue(CTX, GLOBAL_OBJ); }
    if (!CTX) { JS_FreeContext(CTX); }
    if (!RT) { JS_FreeRuntime(RT); }
}

static std::vector<EField> get_efield_array(const std::vector<double>& ts) {
    size_t len = ts.size();
    std::vector<EField> ret;
    ret.reserve(len);

    JSValue arg[1];
    JSValue result = JS_NULL;
    for (double t: ts) {
        arg[0] = JS_NewFloat64(CTX, t);
        result = JS_Call(CTX, FN, JS_UNDEFINED, 1, arg);
        if (JS_IsException(result)) {
            JSValue ex = JS_GetException(CTX);
            const char *err = JS_ToCString(CTX, ex);
            fprintf(stderr, "Exception: %s\n", err);
            JS_FreeCString(CTX, err);
            JS_FreeValue(CTX, ex);
            exit(2);
        } else {
            EField* efield = (EField*)JS_GetOpaque(result, js_efield_data_class_id);
            if (efield) {
                ret.emplace_back(*efield);
            } else {
                fprintf(stderr, "Returned value is not JSEfieldData\n");
                exit(3);
            }
        }
    }

    JS_FreeValue(CTX, result);
    JS_FreeValue(CTX, arg[0]);

    return ret;
}

static void init_efield(const std::string& jsfname, int namdtim, int neleint, double iontime) {
    int veclength = namdtim * neleint;

    std::vector<double> ts = std::vector<double>(veclength, 0.0);
    int cnt = 0;
    double timestep = iontime / double(neleint);    // Time step for each electron time
    for (int inamdtim=0; inamdtim<namdtim; ++inamdtim) {
        for (int iele=0; iele<neleint; ++iele) {
            ts[cnt] = inamdtim * iontime + iele * timestep;
            ++cnt;
        }
    }

    init_engine(jsfname);
    efields = get_efield_array(ts);     // efield.h:  extern efields
    destroy_engine();
}


void write_efield(const std::string& fname, int namdtim, int neleint, double iontime) {
    FILE* fp = fopen(fname.c_str(), "w");
    if (nullptr == fp) {
        std::cerr << "Cannot open " << fname << " to write efields data." << std::endl;
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "#  Time(fs)  |   Ex           Ey          Ez  (V/A)  |\n");

    const double dt = iontime / neleint;
    int cnt = 0;
    for (int t_ion=0; t_ion!=namdtim; ++t_ion) {
        for (int t_ele=0; t_ele!=neleint; ++t_ele) {
            const double t = t_ion * iontime + t_ele * dt;
            fprintf(fp, "%12.3lf   %12.6lf %12.6lf %12.6lf\n",
                    t, efields[cnt].x, efields[cnt].y, efields[cnt].z);
            ++cnt;
        }
    }

    fclose(fp);
}


struct CmdArgs {
    std::string jsfile  = {"efield.js"};
    std::string txtfile = {"EFIELD.txt"};
    int namdtim         = 3000;
    int neleint         = 10;
    double iontime      = 1.0;
};


void print_help() {
    puts(
        "Usage:\n"
        "  -j,--jsfile  <file>    (default: efiled.js)\n"
        "  -t,--txtfile <file>    (default: EFIELD.txt)\n"
        "  -n,--namdtim <integer> (default: 3000)\n"
        "  -e,--neleint <integer> (default: 10)\n"
        "  -i,--iontime <real>    (default: 0.1)\n"
        "  -h,--help\n"
    );
}


CmdArgs parse_args(int argc, char** argv) {
    if (1 == argc) {
        fprintf(stderr, "No arguments provided.\n\n");
        print_help();
        exit(0);
    }


    CmdArgs args;  // Default argument defined in CmdArgs' definition

    const struct option long_options[] = {
        {"jsfile",   required_argument, 0, 'j'},
        {"txtfile",  required_argument, 0, 't'},
        {"namdtim",  required_argument, 0, 'n'},
        {"neleint",  required_argument, 0, 'e'},
        {"iontime",  required_argument, 0, 'i'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "j:t:n:e:i:h", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'j':
            args.jsfile = optarg;
            break;
        case 't':
            args.txtfile = optarg;
            break;
        case 'n':
            args.namdtim = std::atoi(optarg);
            break;
        case 'e':
            args.neleint = std::atoi(optarg);
            break;
        case 'i':
            args.iontime = std::atof(optarg);
            break;
        case 'h':
            print_help();
            exit(0);
        default:
            print_help();
            exit(EXIT_FAILURE);
        }
    }

    return args;
}



int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(true);

    CmdArgs args = parse_args(argc, argv);
    const std::string& jsfile = args.jsfile;
    const std::string& txtfile = args.txtfile;
    const int namdtim = args.namdtim;
    const int neleint = args.neleint;
    const double iontime = args.iontime;

    printf("Converting \"%s\" to \"%s\" ...\n", jsfile.c_str(), txtfile.c_str());
    init_efield(jsfile, namdtim, neleint, iontime);
    write_efield(txtfile, namdtim, neleint, iontime);
    printf("\"%s\" written.\n", txtfile.c_str());
    return 0;
}
