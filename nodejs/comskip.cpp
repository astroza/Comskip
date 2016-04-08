#include <node.h>
#include <v8.h>
#include <uv.h>
#include <vector>
#include <thread>
#include <dlfcn.h>
#include <string>
#include <queue>
#include "../comskip.h"

using namespace v8;

typedef struct {
    unsigned long start;
    unsigned long stop;
} comm;

typedef struct {
    uv_work_t request;
    std::string input_file;
    Persistent<Function> callback;
    std::queue<comm> comms;
} comskip_work;

static void work_async(uv_work_t* req)
{
    comskip_decode_state state;
    comskip_work* work = static_cast<comskip_work*>(req->data);
    void* comskip_handle = dlopen("../.libs/libcomskip.so", RTLD_GLOBAL | RTLD_NOW);

    if (!comskip_handle) {
        fputs("libcomskip.so not found", stderr);
        exit(1);
    }

    void (*comskip_decode_init)(comskip_decode_state*, int, char**) = reinterpret_cast<void (*)(comskip_decode_state*, int, char**)>(dlsym(comskip_handle, "comskip_decode_init"));
    void (*comskip_decode_loop)(comskip_decode_state*) = reinterpret_cast<void (*)(comskip_decode_state*)>(dlsym(comskip_handle, "comskip_decode_loop"));
    void (*comskip_decode_finish)(comskip_decode_state*) = reinterpret_cast<void (*)(comskip_decode_state*)>(dlsym(comskip_handle, "comskip_decode_finish"));

    char const* args[] = { "node-comskip", work->input_file.data(), "output" };
    comskip_decode_init(&state, 3, (char**)args);
    comskip_decode_loop(&state);
    comskip_decode_finish(&state);
}

static void work_complete(uv_work_t* req, int status)
{
    Isolate* isolate = Isolate::GetCurrent();

    v8::HandleScope handleScope(isolate);
    comskip_work* work = static_cast<comskip_work*>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc] = { String::NewFromUtf8(isolate, "algo") };

    Local<Function>::New(isolate, work->callback)->Call(isolate->GetCurrentContext()->Global(), argc, argv);
    work->callback.Reset();
    delete work;
}

void comskip_run(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    Isolate* isolate = args.GetIsolate();

    comskip_work* work = new comskip_work();
    work->input_file = "/media/psf/Home/download.mpg";
    work->request.data = work;

    Local<Function> callback = Local<Function>::Cast(args[1]);
    work->callback.Reset(isolate, callback);

    // kick of the worker thread
    uv_queue_work(uv_default_loop(), &work->request, work_async, work_complete);

    args.GetReturnValue().Set(Undefined(isolate));
}

void init(Handle<Object> exports, Handle<Object> module)
{
    NODE_SET_METHOD(exports, "run", comskip_run);
}

NODE_MODULE(comskip, init)
