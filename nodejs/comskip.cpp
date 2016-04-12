/* (C) 2016 NED
 * Author: Felipe Astroza (fastroza@ned.cl)
 */
#include <node.h>
#include <v8.h>
#include <uv.h>
#include <vector>
#include <thread>
#include <dlfcn.h>
#include <string>
#include <list>
#include <queue>
#include <comskip.h>

using namespace v8;

typedef struct {
    double start;
    double end;
} segment;

typedef struct {
    uv_work_t request;
    uv_async_t commercial_notifier;
    std::list<segment> commercials; // storage for partial results
    uv_mutex_t updates_mutex; // protects integrity of updates queue
    std::queue<segment> updates; 
    std::string input_file;
    Persistent<Function> complete_callback;
    Persistent<Function> commercial_callback;
    comskip_decode_state state;
} comskip_work;

static bool add_segment(std::list<segment> *commercials, segment latest_segment)
{
    for(auto i = commercials->begin(); i != commercials->end(); i++) {
        if(i->start == latest_segment.start) {
            if(i->end == latest_segment.end)
                return false;
            i->end = latest_segment.end;
            return true;
        }
    }
    commercials->push_back(latest_segment);
    return true;
}

static void comskip_work_async(uv_work_t *req)
{

    comskip_work *work = static_cast<comskip_work *>(req->data);
    char const* args[] = { "node-comskip", work->input_file.data(), "output"};

    comskip_decode_init(&work->state, 3, (char**)args);
    comskip_decode_loop(&work->state);
    comskip_decode_finish(&work->state);
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
    uv_unref((uv_handle_t *)&work->commercial_notifier); // takes away this handle from the event loop
    
    delete work;
}

void js_commercial_cb(uv_async_t *notifier)
{
    const unsigned argc = 2;
    Isolate *isolate = Isolate::GetCurrent();
    comskip_work *work = (comskip_work *)notifier->data;
    v8::HandleScope handleScope(isolate);

    uv_mutex_lock(&work->updates_mutex);
    while(!work->updates.empty()) {
        segment sg = work->updates.front();
        if(add_segment(&work->commercials, sg)) {
            // It calls JS update callback only if the segment is a new one or it was updated
            Local<Value> argv[argc] = { Number::New(isolate, sg.start), Number::New(isolate, sg.end) };
            Local<Function>::New(isolate, work->commercial_callback)->Call(isolate->GetCurrentContext()->Global(), argc, argv);
        }
        work->updates.pop();
    }
    uv_mutex_unlock(&work->updates_mutex);
}

void comskip_update_cb(double start, double end, comskip_work *work)
{
    segment sg;
    sg.start = start;
    sg.end = end;
    uv_mutex_lock(&work->updates_mutex);
    work->updates.push(sg);
    uv_mutex_unlock(&work->updates_mutex);
    
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
    uv_mutex_init(&work->updates_mutex);
        
    v8::String::Utf8Value param0(args[0]->ToString());

    work->input_file = std::string(*param0); 
    work->request.data = work;

    callback = Local<Function>::Cast(args[1]);
    work->complete_callback.Reset(isolate, callback);

    callback = Local<Function>::Cast(args[2]);
    work->commercial_callback.Reset(isolate, callback);

    set_output_callback((void (*)(double, double, void *))comskip_update_cb, (void *)work);
    uv_async_init(uv_default_loop(), &work->commercial_notifier, js_commercial_cb);

    work->commercial_notifier.data = work;

    // kick of the worker thread
    uv_queue_work(uv_default_loop(), &work->request, comskip_work_async, comskip_work_complete);

    args.GetReturnValue().Set(Undefined(isolate));
}

void init(Handle<Object> exports, Handle<Object> module)
{
    NODE_SET_METHOD(exports, "run", comskip_run);
}

NODE_MODULE(comskip, init)
