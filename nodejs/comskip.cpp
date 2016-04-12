#include <node.h>
#include <v8.h>
#include <uv.h>
#include <vector>
#include <thread>
#include <dlfcn.h>
#include <string>
#include <queue>
#include "../comskip.h"
#include <unordered_map>

using namespace v8;

typedef struct {
    double start;
    double end;
} segment;

typedef struct {
    uv_work_t request;
    uv_async_t commercial_notifier;
    std::string input_file;
    Persistent<Function> complete_callback;
    Persistent<Function> commercial_callback;
    std::queue<segment> commercials;
    comskip_decode_state state;
} comskip_work;

static void (*_comskip_decode_init)(comskip_decode_state *, int, char **);
static void (*_comskip_decode_loop)(comskip_decode_state *);
static void (*_comskip_decode_finish)(comskip_decode_state *);
static void (*_set_output_callback)(void (*)(double, double, comskip_work *), comskip_work *);

static void comskip_work_async(uv_work_t *req)
{

    comskip_work *work = static_cast<comskip_work *>(req->data);
    char const* args[] = { "node-comskip", work->input_file.data(), "output"};

    _comskip_decode_init(&work->state, 3, (char**)args);
    _comskip_decode_loop(&work->state);
    _comskip_decode_finish(&work->state);
}

static void comskip_work_complete(uv_work_t* req, int status)
{
    Isolate* isolate = Isolate::GetCurrent();

    v8::HandleScope handleScope(isolate);
    comskip_work* work = static_cast<comskip_work*>(req->data);

    const unsigned argc = 1;
    Local<Value> argv[argc] = { Number::New(isolate, work->state.result) };

    Local<Function>::New(isolate, work->complete_callback)->Call(isolate->GetCurrentContext()->Global(), argc, argv);
    work->complete_callback.Reset();
    work->commercial_callback.Reset();
    delete work;
}

void js_commercial_cb(uv_async_t *notifier)
{
    const unsigned argc = 2;
    Isolate *isolate = Isolate::GetCurrent();
    comskip_work *work = (comskip_work *)notifier->data;
    v8::HandleScope handleScope(isolate);

    segment sg = work->commercials.front();
    work->commercials.pop();

    Local<Value> argv[argc] = {Number::New(isolate, sg.start), Number::New(isolate, sg.end)};
    Local<Function>::New(isolate, work->commercial_callback)->Call(isolate->GetCurrentContext()->Global(), argc, argv);
}

void comskip_update_cb(double start, double end, comskip_work *work)
{
    segment sg;
    sg.start = start;
    sg.end = end;
    work->commercials.push(sg);
    uv_async_send(&work->commercial_notifier);
}

void comskip_run(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    Isolate* isolate = args.GetIsolate();
    Local<Function> callback;

    if(args.Length() < 3) {
    	isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Wrong number of arguments")));
    	return;
    }

    comskip_work *work = new comskip_work();

    v8::String::Utf8Value param0(args[0]->ToString());

    work->input_file = std::string(*param0); 
    work->request.data = work;

    callback = Local<Function>::Cast(args[1]);
    work->complete_callback.Reset(isolate, callback);

    callback = Local<Function>::Cast(args[2]);
    work->commercial_callback.Reset(isolate, callback);

    _set_output_callback(comskip_update_cb, work);

    uv_async_init(uv_default_loop(), &work->commercial_notifier, js_commercial_cb);

    work->commercial_notifier.data = work;

    // kick of the worker thread
    uv_queue_work(uv_default_loop(), &work->request, comskip_work_async, comskip_work_complete);

    args.GetReturnValue().Set(Undefined(isolate));
}

void init(Handle<Object> exports, Handle<Object> module)
{
    void *comskip_handle = dlopen("../.libs/libcomskip.so", RTLD_GLOBAL | RTLD_NOW);

    if (!comskip_handle) {
        fputs("libcomskip.so not found", stderr);
        exit(1);
    }

    _comskip_decode_init = reinterpret_cast<void (*)(comskip_decode_state *, int, char **)>(dlsym(comskip_handle, "comskip_decode_init"));
    _comskip_decode_loop = reinterpret_cast<void (*)(comskip_decode_state *)>(dlsym(comskip_handle, "comskip_decode_loop"));
    _comskip_decode_finish = reinterpret_cast<void (*)(comskip_decode_state *)>(dlsym(comskip_handle, "comskip_decode_finish"));
    _set_output_callback = reinterpret_cast<void (*)(void (*)(double, double, comskip_work *), comskip_work *)>(dlsym(comskip_handle, "set_output_callback"));

    NODE_SET_METHOD(exports, "run", comskip_run);
}

NODE_MODULE(comskip, init)
